

#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include <llvm/ADT/SmallVector.h>
#include <llvm/Analysis/CFGPrinter.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/Instruction.h>
#include <llvm/Transforms/Utils/SSAUpdater.h>

#include "landing-pad.h"

#include <llvm/Analysis/LoopPass.h>

#include <map>

using namespace std;

namespace llvm {

LandingPadTransform::LandingPadTransform() : LoopPass(ID) {}

void LandingPadTransform::updatePhiNotInLoop(
    Loop &loop, SmallVector<Instruction *, 10> &previousPhiUsers, PHINode *from,
    PHINode *to) {
  std::vector<BasicBlock *> &loopBlocks = loop.getBlocksVector();
  for (Instruction *I : previousPhiUsers) {
    BasicBlock *usersBlock = I->getParent();
    if ((std::find(loopBlocks.begin(), loopBlocks.end(), usersBlock) ==
         loopBlocks.end())) {
      if (I != to) {
        I->replaceUsesOfWith(from, to);
      }
    }
  }
}

void LandingPadTransform::unifyPhiAtExit(BasicBlock *newtest,
                                         BasicBlock *unifiedExit,
                                         BasicBlock *loopexit,
                                         BasicBlock *header,
                                         BasicBlock *lastBody, Loop *L) {
  SmallVector<PHINode *, 10> insertedPhi;
  SmallVector<Instruction *, 10> previousPhiUsers;

  for (Instruction &I : *header) {
    if (PHINode *phi = dyn_cast<PHINode>(&I)) {

      for (User *U : phi->users()) {
        if (Instruction *UI = dyn_cast<Instruction>(U)) {
          previousPhiUsers.push_back(UI);
        }
      }

      PHINode *exitphi = PHINode::Create(I.getType(), 0);
      exitphi->addIncoming(phi, lastBody);
      exitphi->addIncoming(phi->getIncomingValue(0), newtest);
      loopexit->getInstList().push_front(exitphi);
      insertedPhi.push_back(phi);

      updatePhiNotInLoop(*L, previousPhiUsers, phi, exitphi);
    }
  }
}

void LandingPadTransform::removePhiDependencies(BasicBlock *preHeader,
                                                BasicBlock *header) {
  for (Instruction &I : *header) {
    if (PHINode *phi = dyn_cast<PHINode>(&I)) {
      Value *incomingValue = phi->getIncomingValue(0);
      for (Instruction &newtestI : *preHeader) {
        for (auto OP = newtestI.op_begin(); OP != newtestI.op_end(); ++OP) {
          if (OP->get() == &I) {
            OP->set(incomingValue);
          }
        }
      }
    }
  }
}

/**
 * @brief This function performs three subroutines:
 * 1. Remove terminator instruction from the loop latch.
 * 2. Clone non-Phi instructions from header to loop latch. If a branch
 * instruction is being cloned, the edge to loop body must be updated with the
 * loop header.
 * 3. If there are any non-Phi instruction in header that uses variables
 * defined in header as operand, update to their cloned variable names, in
 * loop latch.
 *
 * @param loopLatch
 * @param header
 */
void LandingPadTransform::updateLatch(BasicBlock *loopLatch,
                                      BasicBlock *header) {

  /* Since we are removing the out-edge from loop header to exit,
   * we need to modify loop latch to fork out-edges to loop header
   * and loop exit. Therefore, we are first removing the terminator
   * instruction of loop latch.
   */
  loopLatch->getTerminator()->eraseFromParent();

  // map to store original non-Phi instructions in loop header(key) and
  // their clones in loop latch(value)
  map<Value *, Value *> cloneMap;

  for (auto it = header->getFirstInsertionPt(); it != header->end(); it++) {
    Instruction *clone = it->clone();
    cloneMap[&*it] = clone;

    /* Branch instructions are always terminators as they transfer control to a
     * different basic block. If we encounter the terminator instruction of loop
     * header, it will have two outward edges: loop body and loop exit. However,
     * in the transformed CFG, we want loop latch to have two outward edges:
     * loop header and loop exit. Therefore, for the cloned branch instruction
     * in loop latch, we update the loop body out-edge with the loop header
     * out-edge.
     */
    if (BranchInst *terminator = dyn_cast<BranchInst>(clone)) {
      terminator->setOperand(2, header);
    }

    // Pushing cloned instruction from loop header to loop latch.
    loopLatch->getInstList().push_back(clone);
  }

  /* As instructions are cloned, an assignment instruction will have a
   * new variable name. Therefore, there might be cloned instructions in the
   * loop latch that may be referring to variables declared in loop header in
   * their operands. Therefore, we update the operands in the loop latch to
   * point to the cloned variable names.
   */
  for (Instruction &I : *loopLatch) {
    for (auto op = I.op_begin(); op != I.op_end(); ++op) {

      auto itr = cloneMap.find(*op);
      if (itr != cloneMap.end()) {
        I.setOperand(op - I.op_begin(), itr->second);
      }
    }
  }
}

/*
 */
void LandingPadTransform::moveCondFromHeaderToPreheader(BasicBlock *preHeader,
                                                        BasicBlock *header) {
  preHeader->getTerminator()->eraseFromParent();
  preHeader->getInstList().splice(preHeader->end(), header->getInstList(),
                                  header->getFirstNonPHI()->getIterator(),
                                  header->end());
}

bool LandingPadTransform::runOnLoop(Loop *L, LPPassManager &LPM) {
  BasicBlock *preHeader = L->getLoopPreheader();
  BasicBlock *header = L->getHeader();

  LoopInfo &loopInfo = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();

  if (preHeader != nullptr) {
    BasicBlock *landingPad =
        preHeader->splitBasicBlock(preHeader->getTerminator(), ".landingpad");

    if (Loop *parent = L->getParentLoop()) {
      parent->addBasicBlockToLoop(landingPad, loopInfo);
    }

    BasicBlock *loopLatch = L->getLoopLatch();

    updateLatch(loopLatch, header);

    moveCondFromHeaderToPreheader(preHeader, header);

    removePhiDependencies(preHeader, header);

    BranchInst *phTerminator = cast<BranchInst>(preHeader->getTerminator());

    BasicBlock *loopBody = cast<BasicBlock>(phTerminator->getOperand(2));

    preHeader->getTerminator()->setOperand(2, landingPad);

    BranchInst *bi = BranchInst::Create(loopBody, header);

    BasicBlock *loopExit = L->getExitBlock(); 

    BasicBlock *unifiedExit =
        loopExit->splitBasicBlock(loopExit->begin(), ".unitedexit");

    if (Loop *parent = L->getParentLoop()) {
      parent->addBasicBlockToLoop(unifiedExit, loopInfo);
    }

    unifyPhiAtExit(preHeader, unifiedExit, loopExit, header, loopLatch, L);
  }

  return true;
}

void LandingPadTransform::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<LoopInfoWrapperPass>();
}

char LandingPadTransform::ID = 2;
RegisterPass<LandingPadTransform> lPad("landing-pad", "ECE 5984 Landing Pad");
} // namespace llvm