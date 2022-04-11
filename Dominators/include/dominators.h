#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

#include "dataflow.h"

#include <map>
#include <set>
#include <vector>

using namespace llvm;
using namespace std;

namespace llvm {
class Dominators : public FunctionPass {
public:
  static char ID;
  Dominators();
  virtual bool runOnFunction(Function &F);
  virtual void getAnalysisUsage(AnalysisUsage &AU) const;
  map<string, set<string>> getDomMap();

private:
  map<BasicBlock *, struct bbInfo *> infoMap;
  map<string, int> domainToBitMap;
  map<int, string> bitToDomainMap;
  map<string, set<string>> domMap;
  vector<string> domain;

  void populateInfoMap(Function &F, vector<string> domain);
  void generateDomMap(map<BasicBlock *, struct bbProps *>);
  bool contains(set<string> set1, set<string> set2);
  bool isSubset(string key, set<string> smallSet, set<string> bigSet);
  string getImmediateDominator(string basicBlock);
  void printResults();

  class Analysis : public Dataflow {
  public:
    Analysis(int domainSize, enum passDirection dir, BitVector boundaryCond,
             BitVector initCond)
        : Dataflow(domainSize, dir, boundaryCond, initCond) {}

    virtual void transferFn(struct bbProps *props);
    virtual BitVector meetFn(vector<BitVector> inputs);
  };
};
} // namespace llvm