#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

#include "anticipated.h"
#include "available.h"
#include "postponable.h"
#include "pre-support.h"
#include "used.h"

#include <map>
#include <queue>
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
  map<BasicBlock *, BitVector> toInsert;
  map<BasicBlock *, BitVector> toReplace;

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
  void getOptimalComputationPoints(Function &);
  void getRedundantOccurences(Function &);
  void lazyCodeMotion(Function &);

  void _TopologicalSortAndReplaceRO(Function &,
                                    map<BasicBlock *, map<int, Value *>>);
  map<BasicBlock *, map<int, Value *>> _InsertOCP(Function &);
  void printResults(Function &);
  void printBitVector(BitVector);
};

char PRE::ID = 0;
static RegisterPass<PRE> Y("pre", "Partial Redundancy Elimination via LCM",
                           false,
                           false /* transformation, not just analysis */);

bool PRE::runOnFunction(Function &F) {

  outs() << "Running PRE Pass\n";
  Init(F);

  BitVector empty(domain.size(), false);
  BitVector full(domain.size(), true);

  getAnticipated(F, empty, full);

  getWillBeAvailable(F, empty, full);

  getEarliest(F);

  getPostponable(F, empty, full);

  getLatest(F);

  getUsed(F, empty, empty);

  getOptimalComputationPoints(F);

  getRedundantOccurences(F);

  lazyCodeMotion(F);

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

  // Transform criticial edges

  set<pair<BasicBlock *, BasicBlock *>> toSplit;

  for (BasicBlock &BB : F) {
    if (BB.hasNPredecessorsOrMore(2)) {
      for (BasicBlock *pred : predecessors(&BB)) {
        toSplit.insert(make_pair(pred, &BB));
      }
    }
  }

  for (set<pair<BasicBlock *, BasicBlock *>>::iterator itr = toSplit.begin();
       itr != toSplit.end(); ++itr) {
    SplitEdge((*itr).first, (*itr).second);
  }
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

void PRE::getAnticipated(Function &F, BitVector empty, BitVector full) {
  AnticipatedExpressions *antPass =
      new AnticipatedExpressions(domain.size(), empty, full);

  antPass->run(F, infoMap);
  this->anticipated = antPass->result;
}

void PRE::getWillBeAvailable(Function &F, BitVector empty, BitVector full) {
  WillBeAvailableExpressions *wbaPass = new WillBeAvailableExpressions(
      domain.size(), empty, full, this->anticipated);

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

void PRE::getPostponable(Function &F, BitVector empty, BitVector full) {
  PostponableExpressions *postPass =
      new PostponableExpressions(domain.size(), empty, full, this->earliest);

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

void PRE::getUsed(Function &F, BitVector empty, BitVector full) {
  UsedExpressions *usedPass =
      new UsedExpressions(domain.size(), empty, full, this->latest);

  usedPass->run(F, infoMap);

  this->used = usedPass->result;
}

void PRE::getOptimalComputationPoints(Function &F) {
  for (BasicBlock &BB : F) {
    BitVector tmp = this->used[&BB]->bbOutput;
    tmp &= this->latest[&BB];
    this->toInsert[&BB] = tmp;
  }
}

void PRE::getRedundantOccurences(Function &F) {
  for (BasicBlock &BB : F) {
    BitVector tmp = this->latest[&BB].flip();
    tmp |= this->used[&BB]->bbOutput;
    tmp &= this->infoMap[&BB]->genSet;
    this->toReplace[&BB] = tmp;
  }
}

map<BasicBlock *, map<int, Value *>> PRE::_InsertOCP(Function &F) {
  map<BasicBlock *, map<int, Value *>> _inserted;

  for (BasicBlock &BB : F) {

    BitVector insert = toInsert[&BB];
    for (int i = 0; i < insert.size(); ++i)
      if (insert[i]) {

        Expression &exp = bitToDomainMap[i];

        BinaryOperator *bop =
            BinaryOperator::Create(exp.op, exp.operand1, exp.operand2, "T",
                                   &*(BB.getFirstInsertionPt()));

        _inserted[&BB][i] = bop;
      }
  }

  return _inserted;
}

void PRE::_TopologicalSortAndReplaceRO(
    Function &F, map<BasicBlock *, map<int, Value *>> inserted) {

  /**
   * @brief This variable stores reaching definitions of temporary variables.
   * This helps us join multiple temporaries for the same expression at their
   * dominance frontier(DF+).
   * The structure of this map is as follows:
   * map<BasicBlock,
   *       map<index in domain (Represents an Expression),
   *             vector<pair<Definition of Temporary Variable,
   *                         BasicBlock from which definition is reaching>>>>
   *
   */
  map<BasicBlock *, map<int, vector<pair<Value *, BasicBlock *>>>> _state;

  queue<BasicBlock *> Q;

  map<BasicBlock *, int> inDeg; // In-degrees for each BasicBlock in the CFG

  // Calculate in-degrees. For BasicBlock BB, in-degree[BB] = count of
  // predecessors of BB
  for (BasicBlock &BB : F) {
    int preds = 0;
    for (BasicBlock *pred : predecessors(&BB)) {
      preds++;
    }
    inDeg[&BB] = preds;
  }

  // Start Toplogical Sort
  Q.push(&F.getEntryBlock());

  while (Q.size() > 0) {

    BasicBlock &currBlock = *Q.front();
    Q.pop();

    /* For each Temporary variable inserted in the BasicBlock, update State.
     * i.first refers to the index of the Expression in the domain BitVector.
     * i.second refers to the inserted Temporary value.
     */
    for (auto &i : inserted[&currBlock]) {
      _state[&currBlock][i.first].push_back(make_pair(i.second, &currBlock));
    }

    map<int, Value *>
        replaceWith; // Mapping of each Expression index in the domain, to the
                     // Value it needs to be replaced with in the current block.

    for (int i = 0; i < domainToBitMap.size();
         ++i) { // For each Expression in Domain

      if (_state[&currBlock][i].size() > 0) {
        Value *value;

        // For a given Expression represented by i, check if definitions from
        // multiple values reach this block.
        if (_state[&currBlock][i].size() > 1) {

          // If multiple definitions for an Expression is reaching the current
          // block, create a Phi Node and join the conflicting definitions.
          PHINode *phiNode =
              PHINode::Create(_state[&currBlock][i][0].first->getType(),
                              _state[&currBlock][i].size(), Twine(),
                              currBlock.getFirstNonPHI());

          for (int j = 0; j < _state[&currBlock][i].size(); ++j) {
            phiNode->addIncoming(_state[&currBlock][i][j].first,
                                 _state[&currBlock][i][j].second);
          }
          // When we look for Expressions to replace with, we will use this Phi
          // Instruction
          value = phiNode;
        } else {
          value = _state[&currBlock][i][0].first;
        }
        replaceWith[i] = value;
      }
    }

    // Check if there are Expressions that are Redundant Occurences and thus
    // need to be replaced.
    for (auto it = currBlock.begin(); it != currBlock.end(); ) {
      Instruction &I = *it;
      ++it;
      if (I.isBinaryOp()) {
        if (find(domain.begin(), domain.end(), Expression(&I)) == domain.end())
          continue;
        Expression exp = Expression(&I);
        int index = domainToBitMap[exp];

        // toReplace represents the BitVector representation of Redundant
        // Occurences
        if (this->toReplace[&currBlock][domainToBitMap[exp]]) {
          if (replaceWith.find(index) != replaceWith.end()) {
            // If the Instruction itself is the Temporary Instruction, we skip
            // an iteration
            if (replaceWith[index] == &I)
              continue;

            BasicBlock::iterator at(&I);
            ReplaceInstWithValue(currBlock.getInstList(), at,
                                 replaceWith[index]);
          }
        }
      }
    }

    for (BasicBlock *next : successors(&currBlock)) {
      // We have visted the current block, therefore, we reduce the in-degree of
      // its successors. If the in-degree becomes 0 for any successor, we add it
      // to Topological Sort queue. This ensures that we don't visit a node
      // without visiting all its predecessors first.
      if ((--inDeg[next]) == 0) {
        Q.push(next);
      }

      // Propagate the definition of Temporary variable inserted, to all the successors.
      for (int i = 0; i < domainToBitMap.size(); ++i) {
        if (replaceWith.find(i) != replaceWith.end()) {
          _state[next][i].push_back(make_pair(replaceWith[i], &currBlock));
        }
      }
    }
  }
}
void PRE::lazyCodeMotion(Function &F) {

  map<BasicBlock *, map<int, Value *>> _inserted = _InsertOCP(F);
  _TopologicalSortAndReplaceRO(F, _inserted);
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