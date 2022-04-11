// ECE/CS 5544 S22 Assignment 3: deadCodeElimination.cpp
// Group: Swati Lodha, Abhijit Tripathy

////////////////////////////////////////////////////////////////////////////////

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
//#include "llvm/Transforms/Utils/BasicBlockUtils.h"

#include "dataflow.h"

using namespace llvm;
using namespace std;

namespace {

class DeadCodeElimination : public FunctionPass {
public:
  static char ID;
  DeadCodeElimination() : FunctionPass(ID) {}

  bool isLive(Instruction *I) {
    return (I->isTerminator() || isa<DbgInfoIntrinsic>(I) ||
            isa<LandingPadInst>(I) || I->mayHaveSideEffects());
  }

  virtual bool runOnFunction(Function &F) {

    // set up the domain
    setupDomain(F);

    // intialize gen and kill set for each basic block
    populateInfoMap(F);

    // Sets the boundary and init conditions, which is
    // the set of all variables
    BitVector boundaryCond(domain.size(), true);
    BitVector initCond(domain.size(), true);

    // create a new DCE analysis variable
    DeadCodeEliminationAnalysis *dce = new DeadCodeEliminationAnalysis(
        domain.size(), BACKWARD, boundaryCond, initCond);
    // Run the Dataflow pass
    dce->run(F, infoMap);

    // This function eliminates the dead code
    eliminateDeadCode(F, dce->result);

    return false;
  }

  virtual void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.setPreservesAll();
    AU.addRequired<LoopInfoWrapperPass>();
  }

private:
  map<BasicBlock *, struct bbInfo *> infoMap;
  map<Instruction *, int> domainToBitMap;
  map<int, Instruction *> bitToDomainMap;
  vector<Instruction *> domain;

  void eliminateDeadCode(Function &F,
                         map<BasicBlock *, struct bbProps *> result) {
    for (Function::iterator itr = F.begin(); itr != F.end(); ++itr) {
      BasicBlock *blk = dyn_cast<BasicBlock>(&*itr);

      // check the faint value for this basic block
      BitVector faintVal = result[blk]->bbInput;
      // store instructions to delete
      vector<Instruction *> insToDel;

      for (BasicBlock::reverse_iterator rItr = itr->rbegin();
           rItr != itr->rend(); ++rItr) {
        Instruction *I = &(*rItr);

        // we do not need live instructions
        if (isLive(I))
          continue;

        // store all instructions
        auto foundInDomain = find(domain.begin(), domain.end(), I);
        if (foundInDomain != domain.end()) {
          int instructionIdx = domainToBitMap[I];
          if (faintVal[instructionIdx]) {
            insToDel.push_back(I);
          }
        }
      }

      // delete the instruction from code
      for (auto ins : insToDel) {
        if (!ins->use_empty())
          continue;
        errs() << "Instruction deleted: " << *(Value *)&*ins << "\n";
        ins->replaceAllUsesWith(UndefValue::get(ins->getType()));
        ins->eraseFromParent();
      }
    }
  }

  /**
   * This function populates the map we maintain to
   * keep track of the instructions and their respective indices.
   *
   * It also initializes the gen and kill set for each
   * basic block in the domain. In Faint analysis, we need to
   * special handle the phi nodes too.
   */
  void populateInfoMap(Function &F) {
    // BitVector empty(domain.size(), false);
    int counter = 0;

    for (auto I : domain) {
      domainToBitMap[I] = counter;
      bitToDomainMap[counter] = I;
      ++counter;
    }

    // In Faint analysis, we do in the backward direction.
    // since we want to visit all successors before the node,
    // we do a post order traversal

    for (po_iterator<BasicBlock *> itr = po_begin(&F.getEntryBlock());
         itr != po_end(&F.getEntryBlock()); ++itr) {
      BasicBlock *basicBlk = *itr;
      struct bbInfo *info = new bbInfo();
      // initialize empty for now, for each basic block
      info->ref = basicBlk;
      BitVector empty(domain.size(), false);
      info->genSet = empty;
      info->killSet = empty;

      for (BasicBlock::reverse_iterator rItr = basicBlk->rbegin();
           rItr != basicBlk->rend(); ++rItr) {
        Instruction *I = &(*rItr);

        // phi nodes

        for (int operands = 0; operands < I->getNumOperands(); ++operands) {
          Value *val = I->getOperand(operands);

          if (PHINode *p = dyn_cast<PHINode>(val)) {
            for (int i = 0; i < p->getNumIncomingValues(); ++i) {
              auto v = p->getIncomingValue(i);

              auto foundInDomain = find(domain.begin(), domain.end(), v);
              if (foundInDomain != domain.end()) {
                info->killSet.set(domainToBitMap[(Instruction *)v]);
              }
            }
          }
        }

        // gen
        auto foundInDomain = find(domain.begin(), domain.end(), I);
        if (foundInDomain != domain.end() &&
            !(info->killSet[domainToBitMap[I]])) {
          info->genSet.set(domainToBitMap[I]);
        }

        // kill
        for (int operands = 0; operands < I->getNumOperands(); ++operands) {
          Value *val = I->getOperand(operands);

          auto foundInDomain = find(domain.begin(), domain.end(), val);
          if (foundInDomain != domain.end()) {
            info->killSet.set(domainToBitMap[(Instruction *)val]);
          }
        }
      }

      // populate the info map
      infoMap.insert({basicBlk, info});
    }
  }

  void setupDomain(Function &F) {
    for (auto &BB : F) {
      for (auto &I : BB) {
        Instruction *ins = &I;
        if (!isLive(ins))
          domain.push_back(ins);
      }
    }
  }

  class DeadCodeEliminationAnalysis : public Dataflow {
  public:
    DeadCodeEliminationAnalysis(int domainSize, enum passDirection dir,
                                BitVector boundaryCond, BitVector initCond)
        : Dataflow(domainSize, dir, boundaryCond, initCond) {}

    /**
     * Transfer function for Faint analysis
     */
    virtual void transferFn(struct bbProps *props) {

      auto kill = props->killSet;
      auto gen = props->genSet;
      auto out = props->bbOutput;
      auto in = props->bbInput;

      // For faint analysis, the transfer function is:
      // F(x) = (X - Kill) U Gen
      out = kill;
      out.flip(); // -Kill
      out &= in;
      out |= gen; // union with gen

      props->bbOutput = out;
    }

    /**
     * Meet operator for the Faint analysis is INTERSECTION
     */
    virtual BitVector meetFn(vector<BitVector> inputs) {
      size_t _sz = inputs.size();
      BitVector result = inputs[0];
      for (int itr = 1; itr < _sz; ++itr) {
        result &= inputs[itr];
      }
      return result;
    }
  };
};

char DeadCodeElimination::ID = 0;
RegisterPass<DeadCodeElimination> X("dead-code-elimination",
                                    "ECE/CS 5544 Dead-Code-Elimination Pass");
} // namespace
