#include <stdio.h>

int test(int a, int b) {
  int c = 0;
  int d;
  if (a + b > c) {
    d = a + b;
  } else {
    d = 12;
  }

  int e = a + b;
  return e * a * b * d;
}

int main() {
    int a = 3;
    int b = 5;
    test(a, b);
    return 0;
}