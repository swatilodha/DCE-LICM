#include <stdio.h>

int test(int a, int b, int x, int y) {
  int c = 0;
  int d;
  if (b > c) {
    d = a + b;
    c = x + y;
  } else {
    d = 12;
  }

  int e = a + b;
  int f = x + y;
  return e * a * f * b * d;
}

int main() {
  int a = 3;
  int b = 5;
  int x = 6;
  int y = 10;
  test(a, b, x, y);
  return 0;
}