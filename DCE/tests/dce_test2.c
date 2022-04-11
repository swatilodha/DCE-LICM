#include <stdio.h>

int test(int x)
{ 
    int a = 5; 
    int b = 7; 
    
    b = a + 5; //dead code
    b = b + 1; //dead code
    
    return -1;
}

int main()
{
    int r = test(3);
    printf("result is: %d\n",r);
    return 0;
}
