#include "anticipated.h"

using namespace std;
using namespace llvm;

namespace llvm {
void AnticipatedExpressions::transferFn(struct bbProps *props) {
  props->bbInput = props->killSet.flip();
  props->bbInput &= props->bbOutput;
  props->bbInput |= props->genSet;
}

BitVector AnticipatedExpressions::meetFn(vector<BitVector> inputs) {
  size_t _sz = inputs.size();
  BitVector result = inputs[0];
  for (int itr = 1; itr < _sz; ++itr) {
    result &= inputs[itr];
  }
  return result;
}
} // namespace llvm