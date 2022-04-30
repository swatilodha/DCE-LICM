#include <stdio.h>

int main() {
  int a = 565;
  for(int i=0, j=a+1;i<100 && j<10;++i, ++j) {
      a++;
  }

  printf("Hello Sample Program! - %d", a);
  return 0;
}