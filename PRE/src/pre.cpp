#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
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
  PRE() : FunctionPass(ID) {}
  virtual bool runOnFunction(Function &F);
  virtual void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.setPreservesCFG();
  }

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

  bool inDomain(Expression);

  void Init(Function &);

  void Preprocess(Function &);

  vector<Expression> getExpressions(Function &);

  void populateInfoMap(Function &, vector<Expression> &);
  void getAnticipated(Function &, BitVector, BitVector);
  void getWillBeAvailable(Function &, BitVector, BitVector);
  void getPostponable(Function &, BitVector, BitVector);
  void getUsed(Function &, BitVector, BitVector);
  void getEarliest(Function &);
  void getLatest(Function &);
  void lazyCodeMotion(Function &);

  void printResults(Function &);
  void printBitVector(BitVector);
};

char PRE::ID = 0;
static RegisterPass<PRE> Y("pre", "Partial Redundancy Elimination via LCM",
                           false,
                           false /* transformation, not just analysis */);

bool PRE::runOnFunction(Function &F) {
  outs() << "Initializing Pass"
         << "\n";
  Init(F);
  outs() << "Initializing Done."
         << "\n";

  BitVector boundaryCondition(domain.size(), false);
  BitVector initCondition(domain.size(), true);

  outs() << "Getting Anticipated Expressions"
         << "\n";
  getAnticipated(F, boundaryCondition, initCondition);
  outs() << "Anticipated Expressions done."
         << "\n";

  outs() << "Getting WillBeAvailable Expressions"
         << "\n";
  getWillBeAvailable(F, boundaryCondition, initCondition);
  outs() << "WillBeAvailable Expressions done."
         << "\n";

  getEarliest(F);

  outs() << "Getting Postponable Expressions"
         << "\n";
  getPostponable(F, boundaryCondition, initCondition);
  outs() << "Postponable Expressions done."
         << "\n";

  getLatest(F);

  outs() << "Getting Used Expressions\n";
  getUsed(F, boundaryCondition, boundaryCondition);
  outs() << "Used Expressions done.\n";

  printResults(F);
  lazyCodeMotion(F);

  // printResults(F);

  return false;
}

void PRE::Init(Function &F) {
  Preprocess(F);
  this->domain = getExpressions(F);
  populateInfoMap(F, this->domain);
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
  outs() << "Domain : ";
  printSet(exps);
  return exps;
}

void PRE::populateInfoMap(Function &F, vector<Expression> &domain) {

  BitVector empty(domain.size(), false);

  int idx = 0;
  for (Expression exp : domain) {
    domainToBitMap[exp] = idx;
    bitToDomainMap[idx] = exp;
    ++idx;
  }

  ReversePostOrderTraversal<Function *> RPOT(&F);
  for (BasicBlock *BB : RPOT) {
    // BasicBlock *BB = &itr;
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

void PRE::getPostponable(Function &F, BitVector boundaryCondition,
                         BitVector initCondition) {
  PostponableExpressions *postPass = new PostponableExpressions(
      domain.size(), boundaryCondition, initCondition, this->earliest);

  postPass->run(F, infoMap);

  this->postponable = postPass->result;
}

void PRE::getLatest(Function &F) {
  for (BasicBlock &BB : F) {
    BitVector earliestExp = this->earliest[&BB];
    BitVector postExp = this->postponable[&BB]->bbInput;
    BitVector tmp = earliestExp;
    tmp |= postExp;

    BitVector tmp2(domain.size(), true);
    for (BasicBlock *succ : successors(&BB)) {
      BitVector tmp3 = this->postponable[succ]->bbInput;
      tmp3 |= this->earliest[succ];
      tmp2 &= tmp3;
    }

    tmp2 = tmp2.flip();
    tmp2 |= this->infoMap[&BB]->genSet;
    tmp &= tmp2;

    this->latest[&BB] = tmp;
  }
}

void PRE::getUsed(Function &F, BitVector boundaryCondition,
                  BitVector initCondition) {
  UsedExpressions *usedPass = new UsedExpressions(
      domain.size(), boundaryCondition, initCondition, this->latest);

  usedPass->run(F, infoMap);

  this->used = usedPass->result;
}

// void PRE::lazyCodeMotion(Function &F) {
//   outs() << "\n";
//   for (Expression exp : domain) {
//     set<Instruction *> toErase;
//     Instruction *inserted = nullptr;

//     ReversePostOrderTraversal<Function *> RPOT(&F);
//     for (BasicBlock *BB : RPOT) {
//       BitVector insert = this->used[BB]->bbOutput;
//       insert &= this->latest[BB];
//       if (insert[domainToBitMap[exp]]) {
//         BinaryOperator *bop =
//             BinaryOperator::Create(exp.op, exp.operand1, exp.operand2, "T",
//                                    &*(BB->getFirstInsertionPt()));
//         outs() << "Inserted " << getShortValueName(bop)
//                << " in the beginning of " << BB->getName().str() << "\n";
//         inserted = dyn_cast<Instruction>(bop);
//       }
//     }

//     for (BasicBlock *BB : RPOT) {
//       BitVector replace = this->latest[BB].flip();
//       replace |= this->used[BB]->bbOutput;
//       replace &= this->infoMap[BB]->genSet;

//       if (replace[domainToBitMap[exp]]) {
//         for (Instruction &I : *BB) {
//           if (I.isBinaryOp()) {
//             if (exp == Expression(&I) && inserted != nullptr &&
//                 &I != inserted) {

//               outs() << "Replacing " << getShortValueName(&I) << " with "
//                      << getShortValueName(inserted)
//                      << " in BB : " << BB->getName().str() << "\n";
//               I.replaceAllUsesWith(inserted);
//               toErase.insert(&I);
//             }
//           }
//         }
//       }
//     }

//     for (Instruction *I : toErase) {
//       I->eraseFromParent();
//     }
//   }
//   outs() << "\n";
// }

void PRE::lazyCodeMotion(Function &F) {
  map<BasicBlock *, map<int, Value *>> expList;

  // collect and insert all new exp evaluations
  map<Expression, Value *> expToValueMap;
  for (BasicBlock &BB : F) {

    // insert new evaluations at the beginning
    BitVector insert = this->used[&BB]->bbOutput;
    insert &= this->latest[&BB];

    IRBuilder<> ib(&BB);

    for (int i = 0; i < insert.size(); ++i)
      if (insert[i]) {
        outs() << "Inserts in BB : " << BB.getName().str() << " exp : " << bitToDomainMap[i].toString() << "\n";
        Expression &exp = bitToDomainMap[i];
        // dbgs() << "inserting expression " << exp2Int[exp] << ":\n" <<
        // *exp.instr << "\n";
        BinaryOperator *bop =
            BinaryOperator::Create(exp.op, exp.operand1, exp.operand2, "T",
                                   &*(BB.getFirstInsertionPt()));
        // ib.SetInsertPoint(&BB, BB.getFirstInsertionPt());

        // Value *istValue = ib.Insert(exp.instr->clone());
        expToValueMap[exp] = bop;
        expList[&BB][i] = bop;
        // dbgs() << "block after insertion :\n" << block << "\n";
      }
  }

  map<BasicBlock *, map<int, vector<pair<Value *, BasicBlock *>>>> expState;

  std::queue<BasicBlock *> q;
  std::map<BasicBlock *, int> ind;
  for (BasicBlock &block : F) {
    int preds = 0;
    for (BasicBlock *pred : predecessors(&block)) {
      preds++;
    }
    ind[&block] = preds;
  }
  q.push(&F.getEntryBlock());
  while (q.size() > 0) {

    BasicBlock &block = *q.front();
    q.pop();

    for (auto &ele : expList[&block]) {
      expState[&block][ele.first].push_back(make_pair(ele.second, &block));
    }

    map<int, Value *> expValueMap;
    for (int i = 0; i < domainToBitMap.size(); ++i)
      if (expState[&block][i].size() > 0) {
        Value *value;
        if (expState[&block][i].size() > 1) {
          PHINode *phiNode = PHINode::Create(
              expState[&block][i][0].first->getType(),
              expState[&block][i].size(), "", block.getFirstNonPHI());
          for (int j = 0; j < expState[&block][i].size(); ++j)
            phiNode->addIncoming(expState[&block][i][j].first,
                                 expState[&block][i][j].second);
          value = phiNode;
        } else
          value = expState[&block][i][0].first;
        expValueMap[i] = value;
      }

    for (auto it = block.begin(); it != block.end();) {
      auto &instr = *it;
      ++it;
      if (find(domain.begin(), domain.end(), Expression(&instr)) == domain.end())
        continue;
      Expression exp = Expression(&instr);
      int index = domainToBitMap[exp];

      if (expValueMap.find(index) != expValueMap.end()) {
        if (expValueMap[index] == &instr)
          continue;

        BasicBlock::iterator nit(&instr);
        ReplaceInstWithValue(block.getInstList(), nit, expValueMap[index]);
      }
    }

    for (auto next : successors(&block)) {

      if ((--ind[next]) == 0) {
        q.push(next);
      }
      for (int i = 0; i < domainToBitMap.size(); ++i)
        if (expValueMap.find(i) != expValueMap.end()) {
          expState[next][i].push_back(std::make_pair(expValueMap[i], &block));
        }
    }
  }


}

void PRE::printResults(Function &F) {
  for (BasicBlock &BB : F) {
    outs() << "BasicBlock : " << BB.getName().str() << "\n";
    outs() << "################################################################"
           << "\n";

    outs() << "Use_B : ";
    printBitVector(this->infoMap[&BB]->genSet);

    outs() << "Kill_B : ";
    printBitVector(this->infoMap[&BB]->killSet);

    outs() << "Anticipated.in : ";
    printBitVector(this->anticipated[&BB]->bbInput);

    outs() << "Will be Available.in : ";
    printBitVector(this->available[&BB]->bbInput);

    outs() << "Earliest : ";
    printBitVector(this->earliest[&BB]);

    outs() << "Postponable.in : ";
    printBitVector(this->postponable[&BB]->bbInput);

    outs() << "Latest : ";
    printBitVector(this->latest[&BB]);

    outs() << "Used.out : ";
    printBitVector(this->used[&BB]->bbOutput);

    outs() << "################################################################"
           << "\n";
  }
}

void PRE::printBitVector(BitVector arr) {
  vector<Expression> exps;

  int _sz = arr.size();
  for (int i = 0; i < _sz; i++) {
    if (arr[i]) {
      exps.push_back(bitToDomainMap[i]);
    }
  }

  printSet(exps);
}

}; // namespace llvm