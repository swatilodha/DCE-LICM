

#include "pre.h"

using namespace llvm;
using namespace std;

namespace llvm {
bool PRE::runOnFunction(Function &F) {

  domain = getExpressions(F);
  return false;
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
} // namespace llvm