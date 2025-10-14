#include<iostream>
#include<queue>
#include <map>
#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Analysis/CFG.h"

using namespace llvm;

namespace {

//
// Range : Represent a range of integer values
//
class Range {
private:
    int lower, upper;   // Lower and Upper bounds (both inclusive)

public:
    // Constructors
    Range(int a) : lower(a), upper(a) {};
    Range(int lower, int upper) : lower(lower), upper(upper) {};
    Range();

    // Functions to perform arithmetic on Ranges
    static Range addRange(Range r1, Range r2);
    static Range subRange(Range r1, Range r2);
    static Range mulRange(Range r1, Range r2);
    static Range mergeRange(Range r1, Range r2);
    static Range intersectRange(Range r1, Range r2);

    void print(raw_ostream &os);
    // Comparison operators for Ranges
    friend bool operator< (const Range& lhs, const Range& rhs);
    friend bool operator==(const Range& lhs, const Range& rhs);
    friend bool operator!=(const Range& lhs, const Range& rhs);
};

// Special Ranges to represent Top and Bottom of the Lattice
static Range FULL_RANGE(INT_MIN, INT_MAX);
static Range EMPTY_RANGE(INT_MAX, INT_MIN);

Range::Range() {
    lower = EMPTY_RANGE.lower;
    upper = EMPTY_RANGE.upper;
}

int64_t reality_check(int64_t val) {
    if (val > INT_MAX) 
        return INT_MAX;
    if (val < INT_MIN) 
        return INT_MIN;
    return val;
}

void Range::print(raw_ostream &os) { 
    if (*this == EMPTY_RANGE) os << "[EMPTY]";
    else os << "[" << lower << "," << upper << "]";
}

Range Range::addRange(Range r1, Range r2) {
    if (r1 == EMPTY_RANGE || r2 == EMPTY_RANGE) 
        return EMPTY_RANGE;
    int64_t safe_lane,off_lane;
    safe_lane = reality_check((int64_t)r1.lower + r2.lower);
    off_lane = reality_check((int64_t)r1.upper + r2.upper);
    return Range(safe_lane, off_lane);
}

Range Range::subRange(Range r1, Range r2) {
    if (r1 == EMPTY_RANGE || r2 == EMPTY_RANGE) 
        return EMPTY_RANGE;
    
    int64_t safe_lane,off_lane;

    safe_lane = reality_check((int64_t)r1.lower - r2.upper);
    
    off_lane = reality_check((int64_t)r1.upper - r2.lower);
    return Range(safe_lane, off_lane);
}

Range Range::mulRange(Range r1, Range r2) {
    if (r1 == EMPTY_RANGE || r2 == EMPTY_RANGE) 
        return EMPTY_RANGE;
    int64_t p1 = reality_check((int64_t)r1.lower * r2.lower);
    int64_t p2 = reality_check((int64_t)r1.lower * r2.upper);
    int64_t p3 = reality_check((int64_t)r1.upper * r2.lower);
    int64_t p4 = reality_check((int64_t)r1.upper * r2.upper);

    int64_t safe_lane = std::min({p1, p2, p3, p4});
    int64_t off_lane = std::max({p1, p2, p3, p4});
    return Range(safe_lane, off_lane);
}

Range Range::mergeRange(Range r1, Range r2) {
    if (r1 == EMPTY_RANGE) return r2;
    if (r2 == EMPTY_RANGE) return r1;
    int64_t safe_lane = std::min(r1.lower, r2.lower);
    int64_t off_lane = std::max(r1.upper, r2.upper);
    return Range(safe_lane, off_lane);
}

Range Range::intersectRange(Range r1, Range r2) {
    if (r1 == EMPTY_RANGE || r2 == EMPTY_RANGE) return EMPTY_RANGE;
    int64_t safe_lane = std::max(r1.lower, r2.lower);
    int64_t off_lane = std::min(r1.upper, r2.upper);
    if (safe_lane > off_lane) return EMPTY_RANGE;
    return Range(safe_lane, off_lane);
}

/// Helper function to print Range
raw_ostream& operator<<(raw_ostream& os, Range r) { r.print(os); return os; }

// Comparison operators for Ranges
bool operator< (const Range& lhs, const Range& rhs) {
    if (lhs == EMPTY_RANGE) 
        return true;
    if (rhs == EMPTY_RANGE) return false;
    return std::tie(lhs.lower, lhs.upper) < std::tie(rhs.lower, rhs.upper);
}
bool operator==(const Range& lhs, const Range& rhs) {
    return std::tie(lhs.lower, lhs.upper) == std::tie(rhs.lower, rhs.upper);
}
bool operator!=(const Range& lhs, const Range& rhs) {
    return std::tie(lhs.lower, lhs.upper) != std::tie(rhs.lower, rhs.upper);
}


// BasicBlockState : Represent the data flow facts of a Basic Block

class BasicBlockState {
private:
  

public:
    std::map<Value*, Range> varRanges;

    BasicBlockState () {}

   
    bool meet(const BasicBlockState &other) {
        bool changed = false;
        for (auto const& [val, other_range] : other.varRanges) {
            if (varRanges.find(val) == varRanges.end()) {
                varRanges[val] = other_range;
                changed = true;
            } else {
                Range oldRange = varRanges[val];
                Range newRange = Range::mergeRange(oldRange, other_range);
                if (oldRange != newRange) {
                    varRanges[val] = newRange;
                    changed = true;
                }
            }
        }
        return changed;
    }

    bool operator!=(const BasicBlockState &other) const {
        if (varRanges.size() != other.varRanges.size()) return true;
        for (auto const& [val, range] : varRanges) {
            auto it = other.varRanges.find(val);
            if (it == other.varRanges.end() || it->second != range) {
                return true;
            }
        }
        return false;
    }

    void print(raw_ostream &os) {
        // Print data for debugging
        os << "Basic Block State:\n";
        for (auto const& [val, range] : varRanges) {
            os << "  " << val->getName() << " : " << range << "\n";
        }
    }
};

//
// Intra-procedural Data flow Analysis
//
void analyseFunction(Function &F) {
    std::map<BasicBlock*, BasicBlockState*> BBState;

    // Kildall's algorithm to analyse function
    if (F.isDeclaration()) return;

    std::map<BasicBlock*, BasicBlockState> in, out;
    std::queue<BasicBlock*> wk;
    std::set<BasicBlock*> wk_set; 

    SmallVector<std::pair<const BasicBlock*, const BasicBlock*>, 8> llvmBackEdges;
    FindFunctionBackedges(F, llvmBackEdges);
    std::set<std::pair<BasicBlock*, BasicBlock*>> backEdges;
    for (const auto& edge : llvmBackEdges) {
        backEdges.insert({const_cast<BasicBlock*>(edge.first), const_cast<BasicBlock*>(edge.second)});
    }

    for (BasicBlock &basicblock : F) {
        wk.push(&basicblock);
        wk_set.insert(&basicblock);
    }

    auto getRangeForValue = [&](Value *V, BasicBlockState &state) -> Range {
        if (ConstantInt *const_int = dyn_cast<ConstantInt>(V)) {
            return Range(const_int->getSExtValue(), const_int->getSExtValue());
        }
        if (LoadInst* LI = dyn_cast<LoadInst>(V)) {
            Value* ptr = LI->getPointerOperand();
            if (state.varRanges.count(ptr)) return state.varRanges[ptr];
        }
        if (state.varRanges.count(V)) {
            return state.varRanges[V];
        }
        return FULL_RANGE; 
    };

    auto getRangeFromPred = [&](Value *V, BasicBlock* pred) -> Range {
        if (ConstantInt *const_int = dyn_cast<ConstantInt>(V)) {
            return Range(const_int->getSExtValue(), const_int->getSExtValue());
        }
        if (out[pred].varRanges.count(V)) {
            return out[pred].varRanges[V];
        }
        if(LoadInst* LI = dyn_cast<LoadInst>(V)){
            Value* ptr = LI->getPointerOperand();
            if (out[pred].varRanges.count(ptr)) return out[pred].varRanges[ptr];
        }
        return FULL_RANGE;
    };

    while (!wk.empty()) {
        BasicBlock *basicblock = wk.front();
        wk.pop();
        wk_set.erase(basicblock);

        BasicBlockState inState;
        if (basicblock == &F.getEntryBlock()) {
            for (Argument &arg : F.args()) {
                inState.varRanges[&arg] = FULL_RANGE;
            }
        } else {
            for (BasicBlock *pred : predecessors(basicblock)) {
                BasicBlockState predOutState = out[pred];

                if (BranchInst *br = dyn_cast<BranchInst>(pred->getTerminator())) {
                    if (br->isConditional()) {
                        if (ICmpInst *compare = dyn_cast<ICmpInst>(br->getCondition())) {
                            
                            Value* var_val = compare->getOperand(0); Value* const_val = compare->getOperand(1);
                            
                            
                            Value* var_ptr = nullptr; ConstantInt* K = nullptr;
                            
                            ICmpInst::Predicate p = compare->getPredicate();

                            if (isa<LoadInst>(var_val) && isa<ConstantInt>(const_val)) {
                                var_ptr = dyn_cast<LoadInst>(var_val)->getPointerOperand(); K = dyn_cast<ConstantInt>(const_val);
                            } else if (isa<LoadInst>(const_val) && isa<ConstantInt>(var_val)) {
                                var_ptr = dyn_cast<LoadInst>(const_val)->getPointerOperand(); K = dyn_cast<ConstantInt>(var_val);
                                p = CmpInst::getSwappedPredicate(p);
                            }

                            if (var_ptr && K && predOutState.varRanges.count(var_ptr)) {
                                Range islerange = predOutState.varRanges[var_ptr]; Range crange = FULL_RANGE;
                                int64_t k_val = K->getSExtValue();
                                

                                BasicBlock *trueSucc  = br->getSuccessor(0);
                                BasicBlock *falseSucc = br->getSuccessor(1);

                                bool isTrueBranch = (basicblock == trueSucc);

                                if (!isTrueBranch) { p = CmpInst::getInversePredicate(p); }

                                switch (p) {
                                    case ICmpInst::ICMP_SGT: crange = Range(k_val + 1, INT_MAX); break;
                                    case ICmpInst::ICMP_SGE: crange = Range(k_val, INT_MAX); break;
                                    case ICmpInst::ICMP_SLT: crange = Range(INT_MIN, k_val - 1); break;
                                    case ICmpInst::ICMP_SLE: crange = Range(INT_MIN, k_val); break;
                                    default: break;
                                }
                                predOutState.varRanges[var_ptr] = Range::intersectRange(islerange, crange);
                            }
                        }
                    }
                }

                if (backEdges.count({pred, basicblock})) {
                    for (auto const& [val, oldRange] : in[basicblock].varRanges) {
                        if (predOutState.varRanges.count(val)) {
                            Range newRange = predOutState.varRanges[val];
                            if (oldRange != newRange) { predOutState.varRanges[val] = FULL_RANGE; }
                        }
                    }
                }
                inState.meet(predOutState);
            }
        }
        in[basicblock] = inState;

        BasicBlockState oldOutState = out[basicblock];
        BasicBlockState nmistate = in[basicblock];
        for (Instruction &I : *basicblock) {

            if (PHINode *PN = dyn_cast<PHINode>(&I)) {
                Range phirnage = EMPTY_RANGE;
                for (unsigned i = 0; i < PN->getNumIncomingValues(); ++i) {
                    Value* val = PN->getIncomingValue(i);
                    BasicBlock* pred = PN->getIncomingBlock(i);
                    phirnage = Range::mergeRange(phirnage, getRangeFromPred(val, pred));
                }
                nmistate.varRanges[PN] = phirnage;
            } else if (AllocaInst *AI = dyn_cast<AllocaInst>(&I)) {
                nmistate.varRanges[AI] = FULL_RANGE; 
            } else if (StoreInst *SI = dyn_cast<StoreInst>(&I)) {
                Value *valToStore = SI->getValueOperand(); Value *ptr = SI->getPointerOperand();
                nmistate.varRanges[ptr] = getRangeForValue(valToStore, nmistate);
            } else if (CallInst *const_int = dyn_cast<CallInst>(&I)) {

                Function *calledFunc = const_int->getCalledFunction();

                 if (calledFunc) {
                    for (unsigned i = 0; i < const_int->arg_size(); ++i) {
                        Value *arg = const_int->getArgOperand(i);
                        if (arg->getType()->isPointerTy()) {

                            nmistate.varRanges[arg] = FULL_RANGE;
                        }
                    }
                }
            } 
            else if (BinaryOperator *BO = dyn_cast<BinaryOperator>(&I)) {
                Range r1 = getRangeForValue(BO->getOperand(0), nmistate); 
                Range r2 = getRangeForValue(BO->getOperand(1), nmistate);
                switch (BO->getOpcode()) {
                    case Instruction::Add: nmistate.varRanges[BO] = Range::addRange(r1, r2); break;
                    case Instruction::Sub: nmistate.varRanges[BO] = Range::subRange(r1, r2); break;
                    case Instruction::Mul: nmistate.varRanges[BO] = Range::mulRange(r1, r2); break;
                    default: nmistate.varRanges[BO] = FULL_RANGE;
                }
            }
        }
        out[basicblock] = nmistate;

        if (oldOutState != nmistate) {
            for (BasicBlock *succ : successors(basicblock)) {
                if (wk_set.find(succ) == wk_set.end()) {
                    wk.push(succ); wk_set.insert(succ);
                }
            }
        }
    }

    if (false) { // Set to true for debugging
        for (BasicBlock &bb: F) {
            errs() << bb << "\n";
            BBState[&bb]->print(errs());
            errs() << "\n";
        }
    }

    //
    // Print the results
    //
    errs() << "Function " << F.getName() << "\n";
    // The below code snippet iterates over the Debug Info records to find out the names
    // of local variables, and adds then to programVariables map
    std::map<StringRef, Value*> programVariables;
    for (BasicBlock &bb : F)
        for (Instruction &inst: bb)
            for (DbgVariableRecord &DVR : filterDbgVars(inst.getDbgRecordRange()))
                programVariables[DVR.getVariable()->getName()] = DVR.getAddress();
    for (auto& arg : F.args()) {
        if(arg.hasName()) { programVariables[arg.getName()] = &arg; }
    }

    BasicBlockState finalState;
    for (BasicBlock &bb : F) {
        if (succ_size(&bb) == 0) {
            finalState.meet(out[&bb]);
        }
    }

    for (BasicBlock &bb : F) {
        if (succ_size(&bb) == 0) {
            for (auto entry: programVariables) {
                StringRef variableName = entry.first;
                Value *variableValue = entry.second;
                errs() << variableName << " : ";
                //Printing the Range computed by the analysis for the given variable
                Range finalRange = FULL_RANGE;
                if (finalState.varRanges.count(variableValue)) {
                   finalRange = finalState.varRanges[variableValue];
                   if (finalRange == EMPTY_RANGE) { finalRange = FULL_RANGE; }
                }
                finalRange.print(errs());
                errs() << "\n";
            }
        }
    }
    errs() << "\n";
}

//
// Registering the Function Pass
//
class RangeAnalysisPass : public PassInfoMixin<RangeAnalysisPass> {
public:
    static bool isRequired() { return true; }

    PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {
        analyseFunction(F);
        return PreservedAnalyses::all();
    };
};

}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
    return {
        .APIVersion = LLVM_PLUGIN_API_VERSION,
        .PluginName = "Range Analysis Pass",
        .PluginVersion = "v0.1",
        .RegisterPassBuilderCallbacks = [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM, ArrayRef<PassBuilder::PipelineElement>) {
                    if (Name == "range-analysis") {
                        FPM.addPass(RangeAnalysisPass());
                        return true;
                    }
                    return false;
                });
        }
    };
}
