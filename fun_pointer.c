#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "game.h"

struct game_state {
    void (*f)();
    bool should_call_f;
};

char bytes[2048];

static void f(){
    printf("\tf defined on %s:%d 0x%lx\n", __FILE__, __LINE__, (unsigned long int)f);
}

static struct game_state *game_init() {
    struct game_state *state = malloc(sizeof(*state));
    state->f = f;
    return state;
}

static void game_reload(struct game_state *state){
    state->should_call_f = true;
}

static void game_unload(struct game_state *state){
}

static void game_finalize(struct game_state *state){
    free(state);
}

static bool game_step(struct game_state *state){
    if(state->should_call_f){
        state->f();
        state->should_call_f = false;
    }
    return true;
}

const struct game_api GAME_API = {
    .init = game_init,
    .reload = game_reload,
    .step = game_step,
    .unload = game_unload,
    .finalize = game_finalize
};
