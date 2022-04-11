#include "stdio.h"

int test(int x)
{
    int p = 0;
    for (int i = 0; i < 100; i++)
    {
        p = x * 2;
    }
    return p;
}

int main()
{
    int res = test(4);
    printf("The result is %d\n", res);
    return 0;
}