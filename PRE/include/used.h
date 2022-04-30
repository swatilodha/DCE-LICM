#ifndef __USED_H___
#define __USED_H___

#include "dataflow.h"

using namespace std;
using namespace llvm;

namespace llvm {
class UsedExpressions : public Dataflow {
private:
  map<BasicBlock *, BitVector> latest;

public:
  UsedExpressions(int domainSize, BitVector boundaryCond, BitVector initCond,
                  map<BasicBlock *, BitVector> latest,
                  enum passDirection dir = BACKWARD)
      : Dataflow(domainSize, dir, boundaryCond, initCond) {
    this->latest = latest;
  }

  virtual void transferFn(struct bbProps *props);
  virtual BitVector meetFn(vector<BitVector> inputs);
};
} // namespace llvm

#endif