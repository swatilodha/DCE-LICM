#include <stdio.h>

int test(int a, int b) {
  int x = b * 10;
  x = x * b;
  int y = a * 10;
  int z = a * 10;
  if (z > 10) {

    z = b * 10;
    x = b * 10;
  }

  return x * y * z;
}

int main() {
  int a = 10;
  int b = 15;

  test(a, b);

  return 0;
}