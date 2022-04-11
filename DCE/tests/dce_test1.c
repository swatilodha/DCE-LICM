#include <stdio.h>


int test(int x)
{ 
    int a = 0;
    int b = 0;
    
    for(int i=0; i<10; i++)
    {
        a = x * 2;
        b = a + 5; //dead code
    }
    return a;
}

int main()
{
    int r = test(3);
    printf("result is: %d\n",r);
    return 0;
}



