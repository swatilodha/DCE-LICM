#ifndef __WILLBEAVAILABLE_H___
#define __WILLBEAVAILABLE_H___

#include "dataflow.h"

namespace llvm {

class WillBeAvailableExpressions : public Dataflow {
private:
  map<BasicBlock *, struct bbProps *> anticipated;

public:
  WillBeAvailableExpressions(int domainSize, BitVector boundaryCond,
                             BitVector initCond,
                             map<BasicBlock *, struct bbProps *> anticipated,
                             enum passDirection dir = FORWARD)
      : Dataflow(domainSize, dir, boundaryCond, initCond) {
    this->anticipated = anticipated;
  }

  virtual void transferFn(struct bbProps *props);
  virtual BitVector meetFn(vector<BitVector> inputs);
};
}; // namespace llvm

#endif