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
/**
 * @brief Dominators class is an implementation of the Dominator Dataflow
 * Analysis Pass. It traverses the Control flow graph in post-order fashion, and
 * for each block, it calculates the set of dominators. A node X is said to be
 * dominating Y (denoted as x dom y), if there is no path from ENTRY to Y, that
 * does not go through X.
 *
 */
class Dominators : public FunctionPass {
public:
  static char ID;
  Dominators();
  virtual bool runOnFunction(Function &F);
  virtual void getAnalysisUsage(AnalysisUsage &AU) const;

  /**
   * @brief API to be used by other LLVM Passes, to request dominator
   * information. The result is returned in the form of map<string,
   * set<string>>, where the key is the name of the BasicBlock, and the value is
   * the set of names of all dominators of the key.
   *
   * @return map<string, set<string>> Returns domMap, a map where the key is the
   * name of the BasicBlock, and the value is the set of names of all dominators
   * of the key.
   */
  map<string, set<string>> getDomMap();

private:
  // Map of BasicBlock name and its position in the BitVectors.
  map<BasicBlock *, struct bbInfo *> infoMap;
  // Map of BasicBlock name and their position in the BitVector.
  map<string, int> domainToBitMap;
  // Reverse mapping of positions in the BitVector to their
  // corresponding BasicBlock name.
  map<int, string> bitToDomainMap;

  map<string, set<string>> domMap; // map of basic blocks and their dominators.

  void populateInfoMap(Function &F, vector<string> domain);
  void generateDomMap(map<BasicBlock *, struct bbProps *>);
  bool contains(set<string> set1, set<string> set2);
  bool isSubset(string key, set<string> smallSet, set<string> bigSet);
  string getImmediateDominator(string basicBlock);
  void printResults();

  // Inner class that extends the Dataflow Framework, and implements its own
  // meet and transfer function.
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