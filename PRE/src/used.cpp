#include "used.h"

namespace llvm {
void UsedExpressions::transferFn(struct bbProps *props) {
  BitVector tmp = props->genSet;
  tmp |= props->bbOutput;
  BitVector _latest = this->latest[props->ref];
  props->bbInput = _latest.flip();
  props->bbInput &= tmp;
}

BitVector UsedExpressions::meetFn(vector<BitVector> inputs) {
  size_t _sz = inputs.size();
  BitVector result = inputs[0];
  for (int itr = 1; itr < _sz; ++itr) {
    result |= inputs[itr];
  }
  return result;
}
}; // namespace llvm