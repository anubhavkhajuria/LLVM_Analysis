#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Metadata.h"
#include "llvm/Analysis/DependenceAnalysis.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/MemoryLocation.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Passes/OptimizationLevel.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/LegacyPassManagers.h"
#include "llvm/IR/DebugLoc.h"

#include <memory>
#include <vector>
#include <string>
#include <iomanip>

using namespace llvm;

static std::string locationForInst(const Instruction *I) {
    if (!I) return "<null>";
    const DebugLoc &DL = I->getDebugLoc();
    if (DL) {
        std::string Buf;
        raw_string_ostream OS(Buf);
        OS << *DL;
        return OS.str();
    }
    // Fallback: block name + instruction index
    const BasicBlock *BB = I->getParent();
    std::string bname = BB && BB->hasName() ? BB->getName().str() : "<bb>";
    unsigned idx = 0;
    for (const Instruction &J : *BB) {
        if (&J == I) break;
        ++idx;
    }
    return bname + ":" + std::to_string(idx);
}

static void printDependenceSummary(raw_ostream &OS, const Dependence &D, ScalarEvolution *SE) {
    // Dependence classification
    OS << (D.isFlow() ? "Flow " : "")
       << (D.isAnti()  ? "Anti "  : "")
       << (D.isOutput() ? "Output " : "")
       << (D.isInput() ? "Input " : "")
       << (D.isUnordered() ? "Unordered " : "");
    if (D.isConfused()) OS << "(Confused) ";
    if (D.isLoopIndependent()) OS << "(LoopIndependent) ";
    if (D.isConsistent()) OS << "(Consistent) ";
    OS << "\n";

    if (const FullDependence *FD = dyn_cast<FullDependence>(&D)) {
        OS << "FullDependence details:\n";
      
        const unsigned MAX_LEVELS = 8;
        for (unsigned lvl = 0; lvl < MAX_LEVELS; ++lvl) {
            unsigned Dir = FD->getDirection(lvl, false);
            const SCEV *Dist = FD->getDistance(lvl, false);
            OS << "    Level[" << lvl << "]: Dir=";
            // Direction is a bitmask of Dependence::DVEntry::Direction values; decode them:
            bool printed = false;
            if (Dir & Dependence::DVEntry::EQ)    { OS << "EQ"; printed = true; }
            if (Dir & Dependence::DVEntry::LT)    { if (printed) OS << "|"; OS << "LT"; printed = true; }
            if (Dir & Dependence::DVEntry::GT)    { if (printed) OS << "|"; OS << "GT"; printed = true; }
            if (Dir & Dependence::DVEntry::ALL)   { if (printed) OS << "|"; OS << "ALL"; printed = true; }
            if (!printed) OS << Dir;
            if (Dist) {
                OS << " dist=";
                if (SE) {
                    Dist->print(OS);
                } else {
                    OS << "<scev>";
                }
            }
            OS << "\n";
        }
    }
}

namespace {

class LoopDependenceAnalysisPass : public PassInfoMixin<LoopDependenceAnalysisPass> {
public:
    // Module-level run; use ModuleAnalysisManager to get FunctionAnalysisManager
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM) {
        // Get FunctionAnalysisManager from the ModuleAnalysisManager proxy
        FunctionAnalysisManager &FAM =
            MAM.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager();

        for (Function &F : M) {
            if (F.isDeclaration()) continue;
            analyzeFunction(F, FAM);
        }

        // This pass only analyzes (prints), not transforming IR.
        return PreservedAnalyses::all();
    }

private:
    void analyzeFunction(Function &F, FunctionAnalysisManager &FAM) {
        // Get the per-function analysis results we need
        DependenceInfo &DI = FAM.getResult<DependenceAnalysis>(F);
        LoopInfo &LI = FAM.getResult<LoopAnalysis>(F);
        ScalarEvolution &SE = FAM.getResult<ScalarEvolutionAnalysis>(F);


        errs() << "dependence analysis for function: " << F.getName() << " ===\n";

        // Iterate top-level loops
        for (Loop *TopL : LI) {
            analyzeLoopRecursively(TopL, DI, &SE, 0);
        }
    }

    void analyzeLoopRecursively(Loop *L, DependenceInfo &DI, ScalarEvolution *SE, unsigned depth) {
        // Analyze nested loops first
        for (Loop *Sub : L->getSubLoops())
            analyzeLoopRecursively(Sub, DI, SE, depth + 1);

        BasicBlock *Header = L->getHeader();
        if (!Header) return;

        std::vector<Instruction*> memInsts;
        memInsts.reserve(64);

        // Collect memory accesses in the loop (loads, stores, atomics)
        for (BasicBlock *BB : L->blocks()) {
            for (Instruction &I : *BB) {
                if (isa<LoadInst>(&I) || isa<StoreInst>(&I) ||
                    isa<AtomicCmpXchgInst>(&I) || isa<AtomicRMWInst>(&I)) {
                    memInsts.push_back(&I);
                }
            }
        }

        errs() << "Loop header: " << (Header->hasName() ? Header->getName() : StringRef("<unnamed>"))
               << " (depth=" << depth << ") - memory accesses: " << memInsts.size() << "\n";

        // Pairwise dependence test
        for (size_t i = 0; i < memInsts.size(); ++i) {
            Instruction *Src = memInsts[i];
            for (size_t j = 0; j < memInsts.size(); ++j) {
                Instruction *Dst = memInsts[j];

                // Skip identical instruction pairing that isn't meaningful
                if (Src == Dst) continue;

                // DependenceInfo::depends returns a unique_ptr<Dependence> also if there is no info then it will be NULL
                std::unique_ptr<Dependence> Dep = DI.depends(Src, Dst, /*PossiblyLoopIndependent=*/false);

                errs() << "  Pair: Src=" << locationForInst(Src)
                       << " (" << *Src->getType() << ")"
                       << "  Dst=" << locationForInst(Dst)
                       << " -> ";

                if (!Dep) {
                    errs() << "NO_DEPENDENCE\n";
                } else {
                    // There is a dependence now print classification & details
                    Dependence &Dref = *Dep;
                    errs() << "DEPENDENCE: ";
                    if (Dref.isFlow()) errs() << "[Flow] ";
                    if (Dref.isAnti())  errs() << "[Anti] ";
                    if (Dref.isOutput()) errs() << "[Output] ";
                    if (Dref.isInput()) errs() << "[Input] ";
                    if (Dref.isUnordered()) errs() << "[Unordered] ";
                    if (Dref.isConfused()) errs() << "[Confused] ";
                    if (Dref.isLoopIndependent()) errs() << "[LoopIndependent] ";
                    if (Dref.isConsistent()) errs() << "[Consistent] ";
                    errs() << "\n";

                    // If the dependence is FullDependence, we attempt to extract direction/distance
                    if (FullDependence *FD = dyn_cast<FullDependence>(Dep.release())) {
                        // Print per-level information (we attempt levels up to a safe limit)
                        const unsigned MAX_LEVELS = 8;
                        for (unsigned lvl = 0; lvl < MAX_LEVELS; ++lvl) {
                            // getDirection and getDistance are available on FullDependence
                            unsigned Dir = FD->getDirection(lvl, false);
                            const SCEV *Dist = FD->getDistance(lvl, false);
                            errs() << "    level[" << lvl << "] direction=";
                            bool printed = false;
                            if (Dir & Dependence::DVEntry::EQ) { errs() << "EQ"; printed = true; }
                            if (Dir & Dependence::DVEntry::LT) { if (printed) errs() << "|"; errs() << "LT"; printed = true; }
                            if (Dir & Dependence::DVEntry::GT) { if (printed) errs() << "|"; errs() << "GT"; printed = true; }
                            if (Dir & Dependence::DVEntry::ALL) { if (printed) errs() << "|"; errs() << "ALL"; printed = true; }
                            if (!printed) errs() << Dir;

                            if (Dist) {
                                errs() << " distance=";
                                Dist->print(errs());
                            }
                            errs() << "\n";
                        }
                        // free FD
                        delete FD;
                    } else {
                        // dependence confused / minimal info
                        errs() << "minimal info / confused analysis \n";
                    }
                }
            }
        }
    }
};

} 

// dependence-analysis plugin
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
    return {
        .APIVersion = LLVM_PLUGIN_API_VERSION,
        .PluginName = "LoopDependenceAnalysisPass",
        .PluginVersion = "v0.1",
        .RegisterPassBuilderCallbacks = [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &MPM, ArrayRef<PassBuilder::PipelineElement>) {
                    if (Name == "dependence-analysis") {
                        MPM.addPass(LoopDependenceAnalysisPass());
                        return true;
                    }
                    return false;
                });
        }
    };
}

