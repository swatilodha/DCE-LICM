#include "available.h"

using namespace llvm;
using namespace std;

namespace llvm {
void WillBeAvailableExpressions::transferFn(struct bbProps *props) {
  BitVector tmp = this->anticipated[props->ref]->bbInput;
  tmp |= props->bbInput;
  props->bbOutput = props->killSet.flip();
  props->bbOutput &= tmp;
}

BitVector WillBeAvailableExpressions::meetFn(vector<BitVector> inputs) {
  size_t _sz = inputs.size();
  BitVector result = inputs[0];
  for (int itr = 1; itr < _sz; ++itr) {
    result &= inputs[itr];
  }
  return result;
}
}; // namespace llvm