// ECE/CS 5544 Assignment 3: landing-pad.cpp
// Group: Swati Lodha, Abhijit Tripathy

#include "landing-pad.h"
#include <map>
#include <vector>

using namespace std;

namespace llvm {

LandingPadTransform::LandingPadTransform() : LoopPass(ID) {}

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
void LandingPadTransform::moveCondFromHeaderToLatch(BasicBlock *loopLatch,
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

/* A Phi instruction has variable assignment based on which incoming edge the
 * control enters from. Lets say, phi(BB) returns the incoming value from BB.
 * Moving the Loop condition from header to preheader is a three-step process:
 * 1. Remove the terminator instruction of preheader and replace it with the
 * non-Phi instructions of header.
 * 2. Loop condition statements in loop header, e.g. icmp, br, accept variables
 * from Phi instructions. If an instruction in Preheader, refers to a Phi
 * variable defined in the header, replace it with the incoming value from the
 * preheader block, i.e. phi(preheader)
 * 3. Link pre-header to landing-pad, and loop header to loop body.
 */
void LandingPadTransform::moveCondFromHeaderToPreheader(
    BasicBlock *preHeader, BasicBlock *header, BasicBlock *landingPad) {

  // Remove the terminator instruction of pre-header.
  preHeader->getTerminator()->eraseFromParent();

  // Move all non-Phi instructions from header to the end of the pre-header.
  preHeader->getInstList().splice(preHeader->end(), header->getInstList(),
                                  header->getFirstNonPHI()->getIterator(),
                                  header->end());

  /* If the copied instructions in the pre-header refer to a Phi variable
   * defined in header, replace that variable with the incoming value from the
   * Phi Instruction.
   */
  for (Instruction &I : *header) {
    if (PHINode *phi = dyn_cast<PHINode>(&I)) {
      // Incoming Value from the landing pad incoming edge.
      Value *incomingValue = phi->getIncomingValue(0);
      for (Instruction &preHeaderInst : *preHeader) {
        for (auto op = preHeaderInst.op_begin(); op != preHeaderInst.op_end();
             ++op) {

          // If operand is a Phi instruction, replace it with incoming value.
          if (op->get() == &I) {
            op->set(incomingValue);
          }
        }
      }
    }
  }
  // The terminator of pre-header is spliced from the header, so it is still
  // pointing to the loop body.
  BranchInst *phTerminator = cast<BranchInst>(preHeader->getTerminator());
  BasicBlock *loopBody = cast<BasicBlock>(phTerminator->getOperand(2));

  // Modify terminator of pre-header to point to landing pad block.
  preHeader->getTerminator()->setOperand(2, landingPad);

  // Join loop header and loop body.
  BranchInst *bi = BranchInst::Create(loopBody, header);
}

/* Prior to transformation, there was only one incoming edge at the loop exit,
 * i.e. from the loop header. After transformation, there are two incoming edges
 * at the loop exit, from loop latch and preheader.Therefore, definitions that
 * reached exit from loop header, can now reach from both loop latch and
 * preheader.This function unites the definitions coming from both these blocks,
 * at the loop exit.
 */
void LandingPadTransform::joinPreheaderAndLatchAtExit(BasicBlock *preHeader,
                                                      BasicBlock *header,
                                                      BasicBlock *loopLatch,
                                                      Loop *L,
                                                      LoopInfo &loopInfo) {

  BasicBlock *loopExit =
      dyn_cast<BasicBlock>(preHeader->getTerminator()->getOperand(1));

  // Insert a block before the loop exit. This new block will retain the name of
  // the loop-exit block, and the previous loop exit block will be renamed to
  // .commonexit. Therefore, loopExit will become loopExit->commonExit
  BasicBlock *commonExit =
      loopExit->splitBasicBlock(loopExit->begin(), ".commonexit");

  // Add the join block to the parent loop.
  if (Loop *parent = L->getParentLoop()) {
    parent->addBasicBlockToLoop(commonExit, loopInfo);
  }

  vector<Instruction *>
      prevPhiUsers; // Stores users for the Phi instructions in loop header.

  for (Instruction &I : *header) {
    if (PHINode *phiInHeader = dyn_cast<PHINode>(&I)) {

      // Store users of Phi variables from the loop header.
      for (User *U : phiInHeader->users()) {
        if (Instruction *UI = dyn_cast<Instruction>(U)) {
          prevPhiUsers.push_back(UI);
        }
      }

      // Create a Phi instruction in the new loop-exit block for every Phi
      // instruction in loop header. If the incoming edge is from loop latch,
      // the incoming value will be same as the Phi variable in loop-header. If
      // the incoming edge is from the pre-header, the incoming value wil be
      // same as header_phi_instruction->getIncomingValue(0).
      //
      PHINode *phiAtExit = PHINode::Create(I.getType(), 0);
      phiAtExit->addIncoming(phiInHeader, loopLatch);
      phiAtExit->addIncoming(phiInHeader->getIncomingValue(0), preHeader);

      // Push Phi node to the front of the new loop exit block.
      loopExit->getInstList().push_front(phiAtExit);

      // Since we unified definitions in the loop exit, we need to update the
      // uses of Phi instructions from the loop headers, to the new Phi
      // instructions inserted in loop exit. However, we only need to change the
      // users that are not inside the loop, to maintain correctness of the
      // original program.
      updatePhiUsesOutsideLoop(L, prevPhiUsers, phiInHeader, phiAtExit);
    }
  }
}

/* For all previous users of Phi instructions from the loop header that are not
 * part of the loop, we replace the uses of *from* PHINode with *to* PHINode.
 */
void LandingPadTransform::updatePhiUsesOutsideLoop(
    Loop *L, vector<Instruction *> &prevPhiUsers, PHINode *from, PHINode *to) {

  // List of BasicBlocks within a loop.
  vector<BasicBlock *> &loopBlocks = L->getBlocksVector();
  for (Instruction *I : prevPhiUsers) {
    BasicBlock *usersBlock = I->getParent();
    if ((find(loopBlocks.begin(), loopBlocks.end(), usersBlock) ==
         loopBlocks.end())) {
      if (I != to) {
        I->replaceUsesOfWith(from, to);
      }
    }
  }
}

bool LandingPadTransform::runOnLoop(Loop *L, LPPassManager &LPM) {
  BasicBlock *preHeader = L->getLoopPreheader();
  BasicBlock *header = L->getHeader();

  LoopInfo &loopInfo = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();

  if (preHeader != nullptr) {
    BasicBlock *landingPad =
        preHeader->splitBasicBlock(preHeader->getTerminator(), ".landingpad");

    // If a parent loop exists, add landingpad to the parent loop, so that the
    // algorithm can bubble out deeply nested loop invariant computations. Note:
    // LLVM Loop Pass execution starts from the deepest loops to the outer
    // loops, so inner loops are computed before outer loops.
    if (Loop *parent = L->getParentLoop()) {
      parent->addBasicBlockToLoop(landingPad, loopInfo);
    }

    BasicBlock *loopLatch = L->getLoopLatch();

    moveCondFromHeaderToLatch(loopLatch, header);

    moveCondFromHeaderToPreheader(preHeader, header, landingPad);

    joinPreheaderAndLatchAtExit(preHeader, header, loopLatch, L, loopInfo);

    return true;
  }

  return false;
}

void LandingPadTransform::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<LoopInfoWrapperPass>();
}

char LandingPadTransform::ID = 2;
RegisterPass<LandingPadTransform>
    lPad("landing-pad", "CS/ECE 5544 Landing Pad Transformation Pass");
} // namespace llvm