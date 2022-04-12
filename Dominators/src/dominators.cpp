// ECE/CS 5544 S22 Assignment 3: dominators.cpp
// Group: Swati Lodha, Abhijit Tripathy

////////////////////////////////////////////////////////////////////////////////

#include "dominators.h"

using namespace llvm;
using namespace std;

namespace llvm {

Dominators::Dominators() : FunctionPass(ID) {}

// Overriden function from FunctionPass. Runs for each function encountered
// during the LLVM Pass.
bool Dominators::runOnFunction(Function &F) {

  vector<string> domain; // Holds the list of all BasicBlock names.
  for (BasicBlock &BB : F) {
    domain.push_back(BB.getName().str());
  }

  populateInfoMap(F, domain);

  // Boundary Condition is Empty Set
  BitVector boundaryCond(domain.size(), false);
  // Init Condition is Universal Set (All BasicBlocks)
  BitVector initCond(domain.size(), true);

  // Initialize the Dataflow Analysis Framework.
  Analysis *dfa = new Analysis(domain.size(), FORWARD, boundaryCond, initCond);

  // Run the Dataflow Analysis algorithm on the given function, for the given
  // info map.
  dfa->run(F, infoMap);

  // Transform the Dataflow Result into a dominator map.
  generateDomMap(dfa->result);

  // Print the Immediate Dominators of each BasicBlock in a loop.
  printResults();

  // Does not modify the CFG.
  return false;
}

void Dominators::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
  AU.addRequired<LoopInfoWrapperPass>();
}

// For each loop encountered, we print the BasicBlocks contained within the loop
// and their immediate dominators.
void Dominators::printResults() {
  LoopInfo &loop_info = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
  int ct = 0;
  for (Loop *loop : loop_info) {
    outs() << "Loop " << ct << " :\n";
    for (BasicBlock *bb : loop->getBlocksVector()) {
      outs() << "Basic Block : " << bb->getName() << "\n";
      outs() << "Immediate Dominator : "
             << getImmediateDominator(bb->getName().str()) << "\n";
      outs() << "\n";
    }
    ++ct;
  }
}

/* A basic block n has an immediate dominator m, such that the following
 * condition holds true:
 * Let D(n) = {d_i | Set of dominators of n}
 * Then, for all d_i, such that d_i not equals n, the following must hold true :
 * d_i dom m
 *
 * We use the following algorithm to find immediate dominators.
 * For Block basicBlock:
 *    for Block : D(basicBlock) {
 *      if(Block != basicBlock) {
 *          if (D(basicBlock) - {basicBlock} is a subset of D(Block)) {
 *            return Block as Immediate Dominator
 *          }
 *      }
 *    }
 */
string Dominators::getImmediateDominator(string basicBlock) {
  set<string> doms = domMap[basicBlock];
  for (string dom : doms) {
    if (dom.compare(basicBlock) != 0) {
      set<string> doms2 = domMap[dom];
      if (isSubset(basicBlock, doms, doms2)) {
        return dom;
      }
    }
  }

  return "";
}

// Checks if set1 is a subset of set2
bool Dominators::contains(set<string> set1, set<string> set2) {
  for (string str : set1) {
    if (set2.find(str) == set2.end()) {
      return false;
    }
  }

  return true;
}

// This function compares two sets after performing the following operations.
// It deletes the provided key from the small set, and then checks if the small
// set is contained within the bigger set.
bool Dominators::isSubset(string key, set<string> smallSet,
                          set<string> bigSet) {
  set<string> _set = smallSet;
  _set.erase(key);
  return contains(_set, bigSet);
}

// Evaluates the Gen and Kill set for each BasicBlock.
void Dominators::populateInfoMap(Function &F, vector<string> domain) {
  BitVector empty(domain.size(), false); // Empty Set
  int idx = 0;

  // Create a bi-directional mapping between BasicBlocks and their positions in
  // the BitVectors.
  for (string blk : domain) {
    domainToBitMap[blk] = idx;
    bitToDomainMap[idx] = blk;
    ++idx;
  }

  for (BasicBlock &BB : F) {
    struct bbInfo *info = new bbInfo();
    info->ref = &BB;
    info->genSet = empty;
    info->killSet = empty;
    // For Dominator Pass, the gen-set of each BasicBlock just comprises of
    // single element, the BasicBlock itself.
    info->genSet.set(domainToBitMap[BB.getName().str()]);
    infoMap[&BB] = info;
  }
}

map<string, set<string>> Dominators::getDomMap() { return this->domMap; }

// Transforms the BitVector representation of Dataflow Analysis framework into a
// map a BasicBlock names and their set of dominators.
void Dominators::generateDomMap(map<BasicBlock *, struct bbProps *> dfaResult) {
  for (map<BasicBlock *, struct bbProps *>::iterator itr = dfaResult.begin();
       itr != dfaResult.end(); ++itr) {
    string bbName = itr->first->getName().str();
    set<string> dominators;
    for (int idx = 0; idx < itr->second->bbOutput.size(); ++idx) {
      if (itr->second->bbOutput[idx]) {
        dominators.insert(bitToDomainMap[idx]);
      }
    }
    domMap[bbName] = dominators;
  }
}

/* For Dominators pass, OUT[BB] = F(IN[BB]) = gen[BB] U IN[BB]
 * Here gen[BB] is a singleton set containing {BB}.
 */
void Dominators::Analysis::transferFn(struct bbProps *props) {
  BitVector tmp = props->bbInput;
  props->bbInput |= props->genSet;
  props->bbOutput = props->bbInput;
  props->bbInput = tmp;
}

// For Dominators pass, the meet operator is Intersection
BitVector Dominators::Analysis::meetFn(vector<BitVector> inputs) {
  size_t _sz = inputs.size();
  BitVector result = inputs[0];
  for (int itr = 1; itr < _sz; ++itr) {
    result &= inputs[itr];
  }
  return result;
}

char Dominators::ID = 1;
RegisterPass<Dominators> dom("dominators", "ECE/CS 5544 Dominators Pass");
} // namespace llvm