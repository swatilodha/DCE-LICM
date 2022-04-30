#include "postponable.h"

namespace llvm {
void PostponableExpressions::transferFn(struct bbProps *props) {
  props->bbOutput = this->earliest[props->ref];
  props->bbOutput |= props->bbInput;
  BitVector genSetComplement = props->genSet.flip();
  props->bbOutput &= genSetComplement;
}

BitVector PostponableExpressions::meetFn(vector<BitVector> inputs) {
  size_t _sz = inputs.size();
  BitVector result = inputs[0];
  for (int itr = 1; itr < _sz; ++itr) {
    result &= inputs[itr];
  }
  return result;
}
}; // namespace llvm