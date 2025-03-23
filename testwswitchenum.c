#include <stdio.h>


typedef enum {
    test1,
    test2,
    test3,
    test4,
} TestEnum;

TestEnum functiontestEnum(TestEnum test)
{
    switch (test)
    {
        case test1:
            printf("test1\n");
            break;
        case test2:
            printf("test2\n");
            break;
        case test3:
            printf("test3\n");
            break;
        default:
            printf("default\n");
            break;
    }
    return test;
}

int main(void)
{
    functiontestEnum(test1);
    
    return 0;

}