#ifndef __ANTICIPATED_H___
#define __ANTICIPATED_H___

#include "dataflow.h"

using namespace std;
using namespace llvm;

namespace llvm {
class AnticipatedExpressions : public Dataflow {
  public:
    AnticipatedExpressions(int domainSize, BitVector boundaryCond,
                           BitVector initCond,
                           enum passDirection dir = BACKWARD)
        : Dataflow(domainSize, dir, boundaryCond, initCond) {}

    virtual void transferFn(struct bbProps *props);
    virtual BitVector meetFn(vector<BitVector> inputs);
  };
}

#endif