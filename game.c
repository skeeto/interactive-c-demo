#include <stdio.h>
#include <stdlib.h>
#include "game.h"

static void game_init(void)
{
}

static void game_step(void)
{
    printf("rand() == 0x%08x\n", rand());
}

struct game_api GAME_API = {
    .init = game_init,
    .step = game_step
};
