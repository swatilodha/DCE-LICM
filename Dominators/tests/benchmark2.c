#include "stdio.h"

int test(int x)
{
    int p = 0;
    for (int i = 0; i < 100; i++)
    {
        if (i < 50)
        {
            for (int j = 0; j < 100; j++)
            {
                p = x * 2;
            }
        }
    }
    int q = 0;
    for (int k = 0; k < 100; k++)
    {
        q = x * 2;
    }
    return p * q;
}

int main()
{
    int res = test(4);
    printf("The result is %d\n", res);
    return 0;
}