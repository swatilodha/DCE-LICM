// ECE/CS 5544 S22 Assignment 3: dominators.cpp
// Group: Swati Lodha, Abhijit Tripathy

////////////////////////////////////////////////////////////////////////////////

#include "dominators.h"

using namespace llvm;
using namespace std;

namespace llvm {

Dominators::Dominators() : FunctionPass(ID) {}

bool Dominators::runOnFunction(Function &F) {

  vector<string> domain;
  for (BasicBlock &BB : F) {
    domain.push_back(BB.getName().str());
  }

  populateInfoMap(F, domain);

  BitVector boundaryCond(domain.size(), false);
  BitVector initCond(domain.size(), true);

  Analysis *dfa = new Analysis(domain.size(), FORWARD, boundaryCond, initCond);
  dfa->run(F, infoMap);

  generateDomMap(dfa->result);

  printResults();

  return false;
}

void Dominators::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
  AU.addRequired<LoopInfoWrapperPass>();
}

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
  }
}

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

bool Dominators::contains(set<string> set1, set<string> set2) {
  for (string str : set1) {
    if (set2.find(str) == set2.end()) {
      return false;
    }
  }

  return true;
}

bool Dominators::isSubset(string key, set<string> smallSet,
                          set<string> bigSet) {
  set<string> _set = smallSet;
  _set.erase(key);
  return contains(_set, bigSet);
}

void Dominators::populateInfoMap(Function &F, vector<string> domain) {
  BitVector empty(domain.size(), false);
  int idx = 0;

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
    info->genSet.set(domainToBitMap[BB.getName().str()]);
    infoMap[&BB] = info;
  }
}

map<string, set<string>> Dominators::getDomMap() { return this->domMap; }

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

void Dominators::Analysis::transferFn(struct bbProps *props) {
  BitVector tmp = props->bbInput;
  props->bbInput |= props->genSet;
  props->bbOutput = props->bbInput;
  props->bbInput = tmp;
}

BitVector Dominators::Analysis::meetFn(vector<BitVector> inputs) {
  size_t _sz = inputs.size();
  BitVector result = inputs[0];
  for (int itr = 1; itr < _sz; ++itr) {
    result &= inputs[itr];
  }
  return result;
}

char Dominators::ID = 0;
RegisterPass<Dominators> dom("dominators", "ECE/CS 5544 Dominators Pass");
} // namespace llvm