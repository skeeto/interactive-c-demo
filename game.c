#include <stdio.h>

static int xrand()
{
    return 6;
}

void step(void)
{
    printf("rand() == 0x%08x\n", rand());
}
