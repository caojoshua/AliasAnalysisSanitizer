
#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include "llvm/IR/Mangler.h"

#include <stdio.h>
#include <string>

using namespace llvm;

namespace {
struct InstrumentationPass : public ModulePass {
	static char ID;
	
	static const std::string PointerDefHookName;
	static const std::string StoreHookName;

	Function *PointerDefHook;
	Function *StoreHook;

	PointerType *PointerTy;

	InstrumentationPass() : ModulePass (ID) {}

	virtual bool runOnModule(Module &M);
	virtual bool doInitialization(Module &M);
};
}

char InstrumentationPass::ID = 0;
static RegisterPass<InstrumentationPass> X("hello", "Instrumentation Pass", false, false);

const std::string InstrumentationPass::PointerDefHookName = "PointerDefHook";
const std::string InstrumentationPass::StoreHookName = "StoreHook";

bool InstrumentationPass::doInitialization(Module& M) {
	Constant *C;

	C = M.getFunction(PointerDefHookName);
	assert(C);
	PointerDefHook = dyn_cast<Function>(C);
	assert(PointerDefHook);

	C = M.getFunction(StoreHookName);
	assert(C);
	StoreHook = dyn_cast<Function>(C);
	assert(StoreHook);

	PointerTy = PointerType::getUnqual(Type::getInt8Ty(M.getContext()));

	return true;
}

bool InstrumentationPass::runOnModule(Module &M) {
	for(Module::iterator F = M.begin(); F != M.end(); ++F) {
		if (F->getName().equals(PointerDefHookName) ||
				F->getName().equals(StoreHookName)) {
			continue;
		}
		for(Function::iterator B = F->begin(), Fend = F->end(); B != Fend; ++B) {
			for (BasicBlock::iterator I = B->begin(), Bend = B->end(); I != Bend; ++I) {
				
				if (I->getType()->isPointerTy()) {
					Instruction* Loc = &*I;
					// phis need to be grouped together, so we insert before the first non-phi
					if (isa<PHINode>(I)) {
						Loc = I->getParent()->getFirstNonPHI();
					}
					// insert instructions after the pointer definition
					else if (!I->isTerminator()) {
						Loc = I->getNextNonDebugInstruction();
					}
					else if (isa<InvokeInst>(I)) {
						// InvokeInst is the only terminator that produce a value. We cannot insert
						// instructions after a terminator, because a terminator needs to be at the
						// end of a basic block. Probably need to create a block right after the 
						// current block and insert the instructions there.
						continue;
					}

					std::vector<Value*> Args;
					Args.push_back(new BitCastInst(&*I, PointerTy, "", Loc));
					if(LoadInst *LI = dyn_cast<LoadInst>(I)) {
						Args.push_back(new BitCastInst(LI->getPointerOperand(), PointerTy, "", Loc));
					}
					else {
						Args.push_back(ConstantPointerNull::get(PointerTy));
					}
					CallInst::Create(PointerDefHook, Args, "", Loc);
					// set I equal to the iterator position associated with loc,
					// since we do not want to instrument instructions we just 
					// inserted. Is there a more elegant way to do this?
					while (&*I != Loc) {
						++I;
					}
				}

				else if (StoreInst *SI = dyn_cast<StoreInst>(I)) {
					Value *ValueStored = SI->getValueOperand();
					if (ValueStored->getType()->isPointerTy()) {
						std::vector<Value*> Args;
						Args.push_back(new BitCastInst(ValueStored, PointerTy, "", SI));
						Args.push_back(new BitCastInst(SI->getPointerOperand(), PointerTy, "", SI));
						CallInst::Create(StoreHook, Args, "", &*I);
					}
				}
			}
		}
	}

	return true;
}

