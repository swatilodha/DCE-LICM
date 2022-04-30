#ifndef __POSTPONABLE_H___
#define __POSTPONABLE_H___

#include "dataflow.h"

using namespace std;
using namespace llvm;

namespace llvm {
class PostponableExpressions : public Dataflow {
private:
  map<BasicBlock *, BitVector> earliest;

public:
  PostponableExpressions(int domainSize, BitVector boundaryCond,
                         BitVector initCond,
                         map<BasicBlock *, BitVector> earliest,
                         enum passDirection dir = FORWARD)
      : Dataflow(domainSize, dir, boundaryCond, initCond) {
    this->earliest = earliest;
  }

  virtual void transferFn(struct bbProps *props);
  virtual BitVector meetFn(vector<BitVector> inputs);
};
} // namespace llvm

#endif