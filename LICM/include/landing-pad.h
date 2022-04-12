#ifndef __LANDINGPADTRANSFORM_H___
#define __LANDINGPADTRANSFORM_H___

#include "llvm/IR/Function.h"
#include "llvm/Pass.h"

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Support/raw_ostream.h"
#include <llvm/Analysis/LoopPass.h>

#include <map>
#include <vector>

using namespace llvm;
using namespace std;

namespace llvm {
/**
 * @brief Landing Pad transform inserts landing pad blocks between the loop
 * header and preheader, that can be used to hoist loop-invariant computations.
 * In addition to that, it rotates the loop into a do-until loop structure, and
 * adds the loop initial condition to the preheader, so that control only flows
 * into the loop body, if the condition for the loop running atleast once is
 * true.
 *
 */
class LandingPadTransform : public LoopPass {
private:
  void moveCondFromHeaderToLatch(BasicBlock *, BasicBlock *);
  void moveCondFromHeaderToPreheader(BasicBlock *, BasicBlock *, BasicBlock *);
  void updatePhiUsesOutsideLoop(Loop *, vector<Instruction *> &, PHINode *,
                          PHINode *);
  void joinPreheaderAndLatchAtExit(BasicBlock *, BasicBlock *, BasicBlock *, Loop *,
                      LoopInfo &);

public:
  static char ID;
  LandingPadTransform();
  virtual bool runOnLoop(Loop *L, LPPassManager &LPM) override;
  virtual void getAnalysisUsage(AnalysisUsage &AU) const override;
};
} // namespace llvm
#endif
