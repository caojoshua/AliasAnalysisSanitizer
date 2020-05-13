
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/TypeSize.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include "llvm/Analysis/MemoryBuiltins.h"
#include "llvm/Analysis/TargetLibraryInfo.h"

#include <stdio.h>
#include <string>

using namespace llvm;

namespace {
struct InstrumentationPass : public ModulePass {
  static char ID;

  static const std::string MemAllocHookName;
  static const std::string PointerDefHookName;
  static const std::string StoreHookName;

  Function *MemAllocHook;
  Function *PointerDefHook;
  Function *StoreHook;

  PointerType *PointerTy;

  InstrumentationPass() : ModulePass(ID) {}

  virtual bool runOnModule(Module &M);
  virtual bool doInitialization(Module &M);

  void instrumentStore(StoreInst *SI);
  void instrumentPointer(Module &M, BasicBlock::iterator &I);
  void instrumentAlloca(Module &M, AllocaInst *AI, BasicBlock::iterator &Loc);
};
} // namespace

char InstrumentationPass::ID = 0;
static RegisterPass<InstrumentationPass>
    X("memory-instrument", "Instrumentation Pass", false, false);

const std::string InstrumentationPass::MemAllocHookName = "MemAllocHook";
const std::string InstrumentationPass::PointerDefHookName = "PointerDefHook";
const std::string InstrumentationPass::StoreHookName = "StoreHook";

bool InstrumentationPass::doInitialization(Module &M) {
  Constant *C;

  C = M.getFunction(MemAllocHookName);
  assert(C);
  MemAllocHook = dyn_cast<Function>(C);
  assert(MemAllocHook);

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
  TargetLibraryInfoImpl TLII;
  TargetLibraryInfo TLI(TLII);

  for (Function &F : M) {
    if (F.getName().equals(MemAllocHookName) ||
        F.getName().equals(PointerDefHookName) ||
        F.getName().equals(StoreHookName)) {
      continue;
    }
    for (Function::iterator B = F.begin(), Fend = F.end(); B != Fend; ++B) {
      for (BasicBlock::iterator I = B->begin(), Bend = B->end(); I != Bend;
           ++I) {
        if (isAllocationFn(&*I, &TLI)) {
          // errs() << *I << "\n";
          // errs() << "isAllocationFn\n";
        }

        if (StoreInst *SI = dyn_cast<StoreInst>(I)) {
          instrumentStore(SI);
        }

        if (I->getType()->isPointerTy()) {
          instrumentPointer(M, I);
        }
      }
    }
  }

  return true;
}

void InstrumentationPass::instrumentStore(StoreInst *SI) {
  Value *ValueStored = SI->getValueOperand();
  if (ValueStored->getType()->isPointerTy()) {
    std::vector<Value *> Args;
    Args.push_back(new BitCastInst(ValueStored, PointerTy, "", SI));
    Args.push_back(new BitCastInst(SI->getPointerOperand(), PointerTy, "", SI));
    CallInst::Create(StoreHook, Args, "", SI);
  }
}

void InstrumentationPass::instrumentPointer(Module &M,
                                            BasicBlock::iterator &I) {
  BasicBlock::iterator Loc = I;
  // phis need to be grouped together, so we insert before the first
  // non-phi
  if (isa<PHINode>(I)) {
    do {
      ++Loc;
    } while (isa<PHINode>(I));
  }
  // insert instructions after the pointer definition
  else if (!I->isTerminator()) {
    ++Loc;
  } else if (isa<InvokeInst>(I)) {
    // InvokeInst is the only terminator that produce a value. We cannot
    // insert instructions after a terminator, because a terminator
    // needs to be at the end of a basic block. Probably need to create
    // a block right after the current block and insert the instructions
    // there.
    return;
  }

  if (AllocaInst *AI = dyn_cast<AllocaInst>(I)) {
    instrumentAlloca(M, AI, Loc);
  }

  std::vector<Value *> Args;
  Args.push_back(new BitCastInst(&*I, PointerTy, "", &*Loc));
  if (LoadInst *LI = dyn_cast<LoadInst>(I)) {
    Args.push_back(
        new BitCastInst(LI->getPointerOperand(), PointerTy, "", &*Loc));
  } else {
    Args.push_back(ConstantPointerNull::get(PointerTy));
  }
  CallInst::Create(PointerDefHook, Args, "", &*Loc);
  I = Loc;
}

void InstrumentationPass::instrumentAlloca(Module &M, AllocaInst *AI,
                                           BasicBlock::iterator &Loc) {
  Type *type = AI->getType()->getElementType();
  IntegerType *LongType = Type::getInt64Ty(M.getContext());
  ConstantInt *TypeSize =
      ConstantInt::get(LongType, M.getDataLayout().getTypeAllocSize(type));
  Value *ArrSize = new ZExtInst(AI->getArraySize(), LongType, "", &*Loc);
  Value *v =
      BinaryOperator::Create(Instruction::Mul, ArrSize, TypeSize, "", &*Loc);
  std::vector<Value *> Args;
  Args.push_back(new BitCastInst(&*AI, PointerTy, "", &*Loc));
  Args.push_back(v);
  CallInst::Create(MemAllocHook, Args, "", &*Loc);
}
