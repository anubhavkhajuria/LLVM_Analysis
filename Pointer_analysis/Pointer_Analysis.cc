#include <map>
#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Analysis/CFG.h"

using namespace llvm;

using ValueSet = SmallPtrSet<const Value *, 8>;

class PointerAnalysis
{

	const Function &Fn;
	SmallPtrSet<const Value *, 32> memObj;
	SmallVector<std::pair<const Value *, const Value *>, 128> copy, load, store;

	const Value *getPtrObj(const Value *val)
	{
		if (!val)
			return nullptr;
		const Value *base = val->stripPointerCasts();

		if (memObj.count(base))
			return base;

		return nullptr;
	}

	bool insertIfAbsent(SmallPtrSet<const Value *, 8> &set, const Value *val){
		if (!set.count(val))
		{
			set.insert(val);
			return true;
		}
		return false;
	}

	SmallPtrSet<const Value *, 16> getPtsToSet(const Value *src, const llvm::DenseMap<const Value *, SmallPtrSet<const Value *, 8>> &pointsTo)
	{
		SmallPtrSet<const Value *, 16> am;
		if (const Value *direct = getPtrObj(src))
			am.insert(direct);
		auto it = pointsTo.find(src);
		if (it != pointsTo.end())
			am.insert(it->second.begin(), it->second.end());

		return am;
	}

	Type *getType(const Value *memObj)
	{
		if (const AllocaInst *alloc = dyn_cast<AllocaInst>(memObj))
			return alloc->getAllocatedType();
		else if (const GlobalVariable *globalvar = dyn_cast<GlobalVariable>(memObj))
			return globalvar->getValueType();
		return nullptr;
	}

	const Value *getPtrOpd(const Constant *cons)
	{
		if (!cons)
			return nullptr;
		if (auto *constt = dyn_cast<ConstantExpr>(cons))
		{
			if (constt->getOpcode() == Instruction::BitCast || constt->getOpcode() == Instruction::GetElementPtr)
			{
				if (auto *op = dyn_cast<Constant>(constt->getOperand(0)))
					return getPtrOpd(op);
			}
		}
		else if (getPtrObj(cons))
		{
			return cons;
		}
		return nullptr;
	}

	bool pCopy(llvm::DenseMap<const Value *, SmallPtrSet<const Value *, 8>> &pointsTo)
	{
		bool modified = false;
		for (auto &[dest, src] : copy)
		{
			auto targets = getPtsToSet(src, pointsTo);
			auto &dstSet = pointsTo[dest];
			for (const Value *t : targets)
				modified |= insertIfAbsent(dstSet, t);
		}
		return modified;
	}
	// copy working fine and needs some additional testing

	bool pLoad(llvm::DenseMap<const Value *, SmallPtrSet<const Value *, 8>> &pointsTo)
	{
		bool modified = false;
		for (auto &[dest, srcPtr] : load)
		{
			auto ptrTargets = getPtsToSet(srcPtr, pointsTo);
			auto &destSet = pointsTo[dest];
			for (const Value *memObj : ptrTargets)
			{
				auto it = pointsTo.find(memObj);
				if (it == pointsTo.end())
					continue;
				for (const Value *t : it->second)
				{
					modified |= insertIfAbsent(destSet, t);
				}
			}
		}
		return modified;
	}

	bool pStore(llvm::DenseMap<const Value *, SmallPtrSet<const Value *, 8>> &pointsTo)
	{
		bool modified = false;

		for (auto &[ptrOp, srcVal] : store)
		{
			SmallPtrSet<const Value *, 16> ptrTargets = getPtsToSet(ptrOp, pointsTo);
			SmallPtrSet<const Value *, 16> srcTargets = getPtsToSet(srcVal, pointsTo);

			for (const Value *memObj : ptrTargets){
				Type *ht = getType(memObj);
				if (!ht || !ht->isPointerTy())
					continue;

				auto &memSet = pointsTo[memObj];
				for (const Value *t : srcTargets)
				{
					if (insertIfAbsent(memSet, t))
						modified = true;
				}
			}
		}

		return modified;
	}


public:
	PointerAnalysis(const Function &F) : Fn(F)
	{

		llvm::DenseMap<const Value *, SmallPtrSet<const Value *, 8>> pointsTo; /* You MUST use this Map*/

		// You don’t need to handle conversion from addresses back to variable names.
		// The code below will take care of that automatically as long as you correctly
		

		// MY implementation of Andersen's Analysis start here :-)

		const Module *M = Fn.getParent();
		for (auto &gVar : M->globals())
			memObj.insert(&gVar);

		for (auto &basicBlk : Fn)
		{
			for (auto &inst : basicBlk)
			{
				if (auto *A = dyn_cast<AllocaInst>(&inst))
					memObj.insert(A);
			}
		}

		for (auto *obj : memObj)
			pointsTo[obj];

		for (auto &gVar : M->globals())
		{
			if (gVar.hasInitializer())
			{
				const Constant *init = gVar.getInitializer();
				if (const Value *target = getPtrOpd(init))
				{
					pointsTo[&gVar].insert(target);
				}
			}
		}

		for (const BasicBlock &basicBlk : Fn)
		{
			for (const Instruction &inst : basicBlk)
			{
				if (auto *sInst = dyn_cast<StoreInst>(&inst))
				{ // Looks very clumsy >>> Need refactoring
					const Value *val = sInst->getValueOperand();
					const Value *ptr = sInst->getPointerOperand();
					const Value *memp, *memV;
					memp = getPtrObj(ptr);
					memV = getPtrObj(val);
					if (memp && memV)
						pointsTo[memp].insert(memV);
					else
						store.emplace_back(ptr, val);
					continue;
				}

				if (auto *loadInst = dyn_cast<LoadInst>(&inst)){
					const Value *ptr = loadInst->getPointerOperand();
					pointsTo[&inst];
					load.emplace_back(&inst, ptr);
					continue;
				}

				if (auto *bitcastInst = dyn_cast<BitCastInst>(&inst)){
					pointsTo[bitcastInst];
					copy.emplace_back(bitcastInst, bitcastInst->getOperand(0));
					continue;
				}

				if (auto *gepInst = dyn_cast<GetElementPtrInst>(&inst)){
					pointsTo[gepInst];
					copy.emplace_back(gepInst, gepInst->getPointerOperand());
					continue;
				}

				if (auto *itpInst = dyn_cast<IntToPtrInst>(&inst))
				{
					pointsTo[itpInst];
					for (const Value *mem : memObj)
						copy.emplace_back(itpInst, mem);

					continue;
				}

				if (auto *callInst = dyn_cast<CallInst>(&inst))
				{
					if (callInst->getType()->isPointerTy()){
						pointsTo[callInst];
						for (const Value *mem : memObj){
							copy.emplace_back(callInst, mem);
						}
					}

					SmallVector<const Value *, 8> ptrargs;
					for (const auto &arg : callInst->args())
					{
						if (arg->getType()->isPointerTy())
							ptrargs.push_back(arg.get());
					}

					for (const Value *a1 : ptrargs){
						for (const Value *a2 : ptrargs)
							store.emplace_back(a1, a2);
					}
					continue;
				}

				if (auto *phi = dyn_cast<PHINode>(&inst))
				{
					if (phi->getType()->isPointerTy())
					{
						pointsTo[phi];
						for (unsigned i = 0, n = phi->getNumIncomingValues(); i < n; ++i)
						{
							const Value *incomingVal = phi->getIncomingValue(i);
							if (incomingVal->getType()->isPointerTy())
								copy.emplace_back(phi, incomingVal);
						}
					}
					continue;
				}

				if (auto *selectInst = dyn_cast<SelectInst>(&inst))
				{
					if (selectInst->getType()->isPointerTy())
					{
						pointsTo[selectInst];
						const Value *trueVal = selectInst->getTrueValue();
						const Value *falseVal = selectInst->getFalseValue();
						if (trueVal->getType()->isPointerTy())
							copy.emplace_back(selectInst, trueVal);
						if (falseVal->getType()->isPointerTy())
							copy.emplace_back(selectInst, falseVal);
					}
					continue;
				}
			}
		}

		bool modified = true;
		while (modified)
		{
			modified = false;

			if (pCopy(pointsTo))
				modified = true;

			if (pStore(pointsTo))
				modified = true;

			if (pLoad(pointsTo))
				modified = true;
		}


		std::map<StringRef, Value *> programVariables;
		const Module *m = Fn.getParent();

		for (const GlobalVariable &gv : m->globals())
		{
			if (gv.hasName())
			{
				programVariables[gv.getName()] = const_cast<llvm::GlobalVariable *>(&gv);
			}
		}

		for (auto &BB : Fn)
		{
			for (auto &I : BB)
			{
				// Get all the memory obejects.
				for (DbgVariableRecord &DVR : filterDbgVars(I.getDbgRecordRange()))
					programVariables[DVR.getVariable()->getName()] = DVR.getAddress();
			}
		}

		StringRef varA = "a";
		StringRef varB = "b";

		const Value *valA = programVariables[varA];
		const Value *valB = programVariables[varB];

		auto itA = pointsTo.find(valA);
		auto itB = pointsTo.find(valB);

		if (itA != pointsTo.end() && itB != pointsTo.end())
		{
			const ValueSet &setA = itA->second;
			const ValueSet &setB = itB->second;

			// Collect intersection
			std::vector<std::string> names;
			for (const Value *v : setA)
			{
				if (setB.count(v))
				{
					if (v->hasName())
					{
						names.push_back(v->getName().str());
					}
					else
					{
						for (const auto &entry : programVariables)
						{
							if (entry.second == v)
							{
								names.push_back(entry.first.str());
								break;
							}
						}
					}
				}
			}

			std::sort(names.begin(), names.end());

			// Print
			llvm::errs() << "{ ";
			if (names.empty())
			{
				llvm::errs() << "}";
			}
			else
			{
				for (size_t i = 0; i < names.size(); i++)
				{
					llvm::errs() << names[i];
					if (i + 1 < names.size())
						llvm::errs() << " ";
				}
				llvm::errs() << " }";
			}
			llvm::errs() << "\n";
		}
		else
		{
			llvm::errs() << "Not sure what's happening here\n";
		}
	}
};

// Pointer Analysis
void analyseFunction(Module &M)
{

	std::map<StringRef, Value *> programVariables;
	for (Function &F : M)
	{
		PointerAnalysis andersen(F);
	}
}

// Registering the Function Pass 
class PointerAnalysisPass : public PassInfoMixin<PointerAnalysisPass>
{
public:
	static bool isRequired() { return true; }

	PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM)
	{
		analyseFunction(M);
		return PreservedAnalyses::all();
	};
};

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo()
{
	return {
		.APIVersion = LLVM_PLUGIN_API_VERSION,
		.PluginName = "Pointer Analysis Pass",
		.PluginVersion = "v0.1",
		.RegisterPassBuilderCallbacks = [](PassBuilder &PB)
		{
			PB.registerPipelineParsingCallback(
				[](StringRef Name, ModulePassManager &MPM, ArrayRef<PassBuilder::PipelineElement>)
				{
					if (Name == "pointer-analysis")
					{
						MPM.addPass(PointerAnalysisPass());
						return true;
					}
					return false;
				});
		}};
}
