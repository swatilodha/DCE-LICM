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

// enum to denote passDirection
enum passDirection {
  FORWARD = 0,
  BACKWARD = 1,
};

// enum to denote BasicBlock type
enum bbType {
  ENTRY = 0,
  EXIT = 1,
  REGULAR = 2,
};

/* This struct is used to abstract the details that a client of this API needs
 * to be provide about each BasicBlock. This contains a reference to the
 * BasicBlock, its genSet, and its killSet.
 */
struct bbInfo {
  BasicBlock *ref;   // Pointer to the BasicBlock
  BitVector genSet;  // Gen Set
  BitVector killSet; // Kill set
};

/* This struct is used to abstract all relevant details of a BasicBlock that
 * this API needs to run an iterative dataflow analysis pass.
 */
struct bbProps {
  enum bbType type;             // BasicBlock type
  BasicBlock *ref;              // Pointer to the BasicBlock
  BitVector bbInput;            // Input BitVector
  BitVector bbOutput;           // Output BitVector
  BitVector genSet;             // Gen Set
  BitVector killSet;            // Kill set
  vector<BasicBlock *> pBlocks; // Predecessor Blocks
  vector<BasicBlock *> sBlocks; // Successor Blocks
};

class Dataflow {

private:
  int domainSize;         // Size of the domain ~ Length of the Domain BitVector
  enum passDirection dir; // Pass Direction
  vector<BasicBlock *> poTraversal;  // Post-order traversal vector
  vector<BasicBlock *> rpoTraversal; // Reverse Post-order traversal vector

  void populateEdges(BasicBlock *BB, struct bbProps *props);
  void initialize(Function &F, map<BasicBlock *, struct bbInfo *> infoMap);
  void initializeBlocks(struct bbProps *block);
  virtual void populateTraversal(Function &F);
  
public:
  map<BasicBlock *, struct bbProps *> result;
  BitVector initCond; // Initial Condition for the framework
  BitVector boundaryCond; // Boundary Condition for the framework

  Dataflow(int domainSize, enum passDirection dir, BitVector boundaryCond,
           BitVector initCond) {
    this->domainSize = domainSize;
    this->dir = dir;
    this->boundaryCond = boundaryCond;
    this->initCond = initCond;
  }

  void displayVector(BitVector b);
  // abstract function to define the meet operator
  virtual BitVector meetFn(vector<BitVector> input) = 0;

  // abstract Transfer function
  virtual void transferFn(struct bbProps *block) = 0;

  // Execution of the Dataflow Analysis algorithm.
  void run(Function &F, map<BasicBlock *, struct bbInfo *> infoMap);
};
} // namespace llvm

#endif
