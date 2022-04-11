#ifndef __CLASSICAL_DATAFLOW_H__
#define __CLASSICAL_DATAFLOW_H__

#include "llvm/ADT/BitVector.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/ValueMap.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include <algorithm>
#include <iostream>
#include <map>
#include <queue>
#include <stack>
#include <stdio.h>
#include <vector>

using namespace std;

namespace llvm {

enum passDirection {
  FORWARD = 0,
  BACKWARD = 1,
};

enum bbType {
  ENTRY = 0,
  EXIT = 1,
  REGULAR = 2,
};

struct bbInfo {
  BasicBlock *ref;
  BitVector genSet;
  BitVector killSet;
};

struct bbProps {
  enum bbType type;
  BasicBlock *ref;
  BitVector bbInput;
  BitVector bbOutput;
  BitVector genSet;             // Gen Set for each block
  BitVector killSet;            // Kill set for each block
  vector<BasicBlock *> pBlocks; // Predecessor Blocks
  vector<BasicBlock *> sBlocks; // Successor Blocks
};

class Dataflow {

private:
  int domainSize;
  enum passDirection dir;

  vector<BasicBlock *> poTraversal;
  vector<BasicBlock *> rpoTraversal;

  void populateEdges(BasicBlock *BB, struct bbProps *props);
  void initializeDfa(Function &F, map<BasicBlock *, struct bbInfo *> infoMap);
  void initializeBlocks(struct bbProps *block);
  virtual void populateTraversal(Function &F);

public:
  map<BasicBlock *, struct bbProps *> result;
  BitVector initCond;
  BitVector boundaryCond;

  Dataflow(int domainSize, enum passDirection dir, BitVector boundaryCond,
           BitVector initCond) {
    this->domainSize = domainSize;
    this->dir = dir;
    this->boundaryCond = boundaryCond;
    this->initCond = initCond;
  }
  virtual BitVector meetFn(vector<BitVector> input) = 0;
  virtual void transferFn(struct bbProps *block) = 0;

  void run(Function &F, map<BasicBlock *, struct bbInfo *> infoMap);
};
} // namespace llvm

#endif
