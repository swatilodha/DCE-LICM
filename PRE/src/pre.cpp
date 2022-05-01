#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

#include "anticipated.h"
#include "available.h"
#include "postponable.h"
#include "pre-support.h"
#include "used.h"

#include <map>
#include <set>
#include <vector>

using namespace llvm;
using namespace std;

namespace llvm {

class PRE : public FunctionPass {
public:
  static char ID;
  PRE();
  virtual bool runOnFunction(Function &F);
  virtual void getAnalysisUsage(AnalysisUsage &AU) const;

private:
  map<BasicBlock *, struct bbInfo *> infoMap;
  vector<Expression> domain;

  map<Expression, int> domainToBitMap;
  map<int, Expression> bitToDomainMap;

  map<BasicBlock *, struct bbProps *> anticipated;
  map<BasicBlock *, struct bbProps *> available;
  map<BasicBlock *, struct bbProps *> postponable;
  map<BasicBlock *, struct bbProps *> used;
  map<BasicBlock *, BitVector> earliest;
  map<BasicBlock *, BitVector> latest;

  void Init(Function &);

  /**
   * @brief Split each BasicBlock to contain only one instruction
   * Insert BasicBlocks before each BB that has multiple predecessors
   *
   *
   */
  void Preprocess(Function &);

  vector<Expression> getExpressions(Function &);

  void populateInfoMap(Function &, vector<Expression> &);
  void getAnticipated(Function &, BitVector, BitVector);
  void getWillBeAvailable(Function &, BitVector, BitVector);
  void getPostponable(Function &, BitVector, BitVector);
  void getUsed(Function &, BitVector, BitVector);
  void getEarliest(Function &);
  void getLatest(Function &);

  void printResults();

  class Test {};
};

bool PRE::runOnFunction(Function &F) {

  Init(F);

  BitVector boundaryCondition(domain.size(), false);
  BitVector initCondition(domain.size(), true);

  getAnticipated(F, boundaryCondition, initCondition);

  getWillBeAvailable(F, boundaryCondition, initCondition);

  getEarliest(F);

  return false;
}

void PRE::Init(Function &F) {
  Preprocess(F);
  domain = getExpressions(F);
  populateInfoMap(F, domain);
}

void PRE::Preprocess(Function &F) {
  // TODO: Ensure each statement is in a basic block of its own, except Phi
  // instructions. Split incoming edge for each basic block with multiple
  // predecessors
}
vector<Expression> PRE::getExpressions(Function &F) {
  vector<Expression> exps;
  for (inst_iterator I = inst_begin(F); I != inst_end(F); ++I) {
    Instruction *inst = &*I;
    if (inst->isBinaryOp()) {
      Expression exp = Expression(inst);
      if (find(exps.begin(), exps.end(), exp) == exps.end()) {
        exps.push_back(exp);
      }
    }
  }
  return exps;
}

void PRE::populateInfoMap(Function &F, vector<Expression> &domain) {

  BitVector empty(domain.size(), false);

  int idx = 0;
  for (Expression &exp : domain) {
    domainToBitMap[exp] = idx;
    bitToDomainMap[idx] = exp;
    ++idx;
  }

  ReversePostOrderTraversal<Function *> RPOT(&F);
  for (reverse_iterator<vector<BasicBlock>> itr = RPOT.begin();
       itr != RPOT.end(); ++itr) {
    BasicBlock *BB = &*itr;
    struct bbInfo *blockInfo = new bbInfo();
    blockInfo->ref = BB;
    blockInfo->genSet = empty;
    blockInfo->killSet = empty;

    for (BasicBlock::iterator iitr = BB->begin(); iitr != BB->end(); ++iitr) {
      Instruction *I = &*iitr;
      if (I->isBinaryOp()) {
        Expression currExp = Expression(I);
        if (find(domain.begin(), domain.end(), currExp) != domain.end()) {
          blockInfo->genSet.set(domainToBitMap[currExp]);
        }

        for (Expression e : domain) {
          if (I == e.operand1 || I == e.operand2) {
            blockInfo->killSet.set(domainToBitMap[e]);
            blockInfo->genSet.reset(domainToBitMap[e]);
          }
        }
      }
    }
    infoMap[BB] = blockInfo;
  }
}

void PRE::getAnticipated(Function &F, BitVector boundaryCondition,
                         BitVector initCondition) {
  AnticipatedExpressions *antPass = new AnticipatedExpressions(
      domain.size(), boundaryCondition, initCondition);

  antPass->run(F, infoMap);
  this->anticipated = antPass->result;
}

void PRE::getWillBeAvailable(Function &F, BitVector boundaryCondition,
                             BitVector initCondition) {
  WillBeAvailableExpressions *wbaPass = new WillBeAvailableExpressions(
      domain.size(), boundaryCondition, initCondition, this->anticipated);

  wbaPass->run(F, infoMap);

  this->available = wbaPass->result;
}

void PRE::getEarliest(Function &F) {
  for (BasicBlock &BB : F) {
    BitVector tmp = this->available[&BB]->bbInput.flip();
    tmp &= this->anticipated[&BB]->bbInput;
    this->earliest[&BB] = tmp;
  }
}

}; // namespace llvm