#ifndef __ANTICIPATED_H___
#define __ANTICIPATED_H___

#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

#include "anticipated.h"
#include "available.h"
#include "postponable.h"
#include "used.h"
#include "pre-support.h"

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
  vector<Expression *> expressions;

  map<Expression, int> domainToBitMap;
  map<int, Expression> bitToDomainMap;

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

  vector<Expression *> getExpressions(Function &);

  void printResults();
};
} // namespace llvm

#endif