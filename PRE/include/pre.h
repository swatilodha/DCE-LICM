#ifndef __ANTICIPATED_H___
#define __ANTICIPATED_H___

#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

#include "dataflow.h"
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

  void Preprocess(Function &);

  vector<Expression *> getExpressions(Function &);

  void printResults();
};
} // namespace llvm

#endif