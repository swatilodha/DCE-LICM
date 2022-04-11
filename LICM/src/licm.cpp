// Group Details

#include "llvm/Analysis/LoopPass.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/Pass.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

#include <map>
#include <queue>
#include <set>
#include <vector>

using namespace std;

namespace llvm {
class LICM : public LoopPass {
private:
  map<string, set<string>> dominators;
  vector<Value *> loopInvariantInstructions;

  /**
   * @brief This function checks if a given Instruction within a loop is
   * invariant. For each operand, it checks the following conditions:
   * Condition 1. Operand is Constant.
   * Condition 2. Operand is defined outside the loop.
   * Condition 3. Operand is defined inside the loop, but the definition itself
   * is loop-invariant.
   *
   * @param I
   * @param loopInstructions
   * @return true
   * @return false
   */
  bool isInvariant(Instruction *I, set<Value *> loopInstructions) {
    // Base condition for loop-invariance.
    bool preCheck = isSafeToSpeculativelyExecute(I) &&
                    !I->mayReadFromMemory() && !isa<LandingPadInst>(I);

    if (!preCheck) {
      return preCheck;
    }

    for (Instruction::op_iterator op = I->op_begin(); op != I->op_end(); ++op) {
      // Condition 1. Constant operands are treated as loop invariants. This can
      // give rise to two case: There is only one operand and it is a constant,
      // in which case we return true; There are two operands and both are
      // constant, in which case we return true;
      if (isa<ConstantInt>(*op)) {
        continue;
      }

      // Condition 2. If both operands of an instruction are defined outside the
      // loop, then we can this instruction as loop-invariant.
      if (loopInstructions.find(*op) == loopInstructions.end()) {
        continue;
      }

      // Condition 3. If an instruction's operands are defined inside the loop,
      // but their definitions are loop-invariant instructions, then the current
      // instruction is also treated as loop-invariant.
      if (find(loopInvariantInstructions.begin(),
               loopInvariantInstructions.end(),
               *op) == loopInvariantInstructions.end()) {
        return false;
      }
    }
    return true;
  }

  void populateLoopInvariantInstructions(Loop *L,
                                         set<Value *> loopInstructions) {
    // We iterate the loop in post-order fashion, so that the loop-invariant
    // instructions stored in vector are topologically queue.
    for (Loop::block_iterator bitr = L->block_begin(); bitr != L->block_end();
         ++bitr) {
      BasicBlock *BB = *bitr;
      for (BasicBlock::iterator itr = BB->begin(); itr != BB->end(); ++itr) {
        Instruction *I = &*itr;
        if (isa<BinaryOperator>(I) || isa<PHINode>(I)) {
          // Check for invariance.
          if (isInvariant(I, loopInstructions)) {
            loopInvariantInstructions.push_back(I);
          }
        }
      }
    }
  }

  set<Value *> getLoopInstructions(Loop *L) {
    set<Value *> loopInstructions; // Set of Loop Instructions

    for (Loop::block_iterator bitr = L->block_begin(); bitr != L->block_end();
         ++bitr) {
      BasicBlock *BB = *bitr;

      // Create a set of instructions contained within the loop. This set will
      // help identify if the operands of an instruction is defined inside the
      // loop.
      for (BasicBlock::iterator itr = BB->begin(); itr != BB->end(); ++itr) {
        if (isa<BinaryOperator>(*itr) || isa<PHINode>(*itr)) {
          Value *V = &*itr;
          loopInstructions.insert(V);
        }
      }
    }
    return loopInstructions;
  }

  vector<Value *> topologicalSort(vector<Value *> instructions) {
    map<Value *, set<Value *>> graph;
    map<Value *, int> inEdges;

    queue<Value *> queue;
    vector<Value *> sortedList;

    for (vector<Value *>::iterator itr = instructions.begin();
         itr != instructions.end(); ++itr) {
      inEdges[*itr] = 0;
    }
    for (vector<Value *>::iterator itr = instructions.begin();
         itr != instructions.end(); ++itr) {
      Instruction *I = dyn_cast<Instruction>(*itr);
      for (Instruction::op_iterator opItr = I->op_begin(); opItr != I->op_end();
           ++opItr) {
        if (find(instructions.begin(), instructions.end(), *opItr) !=
            instructions.end()) {
          inEdges[I]++;
        }
        graph[*opItr].insert(I);
      }
    }

    for(map<Value *, int>::iterator itr = inEdges.begin(); itr != inEdges.end(); ++itr) {
      if(itr->second == 0) {
        queue.push(itr->first);
      }
    }

    while(!queue.empty()) {
      Value *noDep = queue.front();
      sortedList.push_back(noDep);
      queue.pop();

      for(Value *deps : graph[noDep]) {
        inEdges[deps]--;
        if(inEdges[deps] < 1) {
          queue.push(deps);
        }
      }
    }

    return sortedList;
  }

public:
  static char ID;

  LICM() : LoopPass(ID) {}
  ~LICM() {}

  bool doInitialization(Loop *L, LPPassManager &LPM) override { return false; }

  bool doFinalization() override { return false; }

  void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequired<LoopInfoWrapperPass>();
  }

  bool runOnLoop(Loop *L, LPPassManager &LPM) override {

    BasicBlock *preHeader = L->getLoopPreheader();

    // Skip optimization if the loop does not have a preheader.
    if (preHeader == NULL) {
      outs() << "No loop preheader found. Skipping.\n";
      return false;
    }
    set<Value *> loopInstructions =
        getLoopInstructions(L); // Set of Loop Instructions
    populateLoopInvariantInstructions(L, loopInstructions);
    for (Value *val : loopInvariantInstructions) {
      Instruction *inv = dyn_cast<Instruction>(val);
      inv->moveBefore(preHeader->getTerminator());
    }

    return true;
  }
};
char LICM::ID = 2;
RegisterPass<LICM> L("loop-invariant-code-motion", "ECE 5544 LICM Pass");
} // namespace llvm