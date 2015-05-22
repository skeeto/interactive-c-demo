#include <stdlib.h>
#include "rlutil.h"
#include "game.h"

struct game_state {
    int width, height;
    int select;
    char cells[];
};

static char
get(struct game_state *state, int x, int y)
{
    return state->cells[state->select * state->width * state->height +
                        y * state->width + x];
}

static void
set(struct game_state *state, int x, int y, char c)
{
    state->cells[!state->select * state->width * state->height +
                 y * state->width + x] = c;
}

static void
flip(struct game_state *state)
{
    state->select = !state->select;
}

static void randomize(struct game_state *state)
{
    for (int y = 0; y < state->height; y++)
        for (int x = 0; x < state->width; x++)
            set(state, x, y, rand() % 2);
    flip(state);
}

static struct game_state *game_init()
{
    int width, height;
    cls();
    width = tcols();
    height = trows();
    struct game_state *state = malloc(sizeof(*state) + width * height * 2);
    state->select = 0;
    state->width = width;
    state->height = height;
    randomize(state);
    return state;
}

static void game_reload(struct game_state *state)
{
    cls();
}

static void game_unload(struct game_state *state)
{
}

static void game_finalize(struct game_state *state)
{
    free(state);
}

static int count(struct game_state *state, int x, int y)
{
    int sum = 0;
    for (int yy = -1; yy <= 1; yy++)
        for (int xx = -1; xx <= 1; xx++)
            if (xx != 0 || yy != 0)
                sum += get(state,
                           (x + xx + state->width) % state->width,
                           (y + yy + state->height) % state->height);
    return sum;
}

static void iterate(struct game_state *state)
{
    for (int y = 0; y < state->height; y++) {
        for (int x = 0; x < state->width; x++) {
            int sum = count(state, x, y);
            set(state, x, y, sum == 3 || (get(state, x, y) && sum == 2));
        }
    }
    flip(state);
}

static void draw(struct game_state *state)
{
    gotoxy(0, 0);
    for (int y = 0; y < state->height; y++)
        for (int x = 0; x < state->width; x++) {
            gotoxy( x, y ); 
            putch( get(state, x, y) ? 'o' : '.' );
        }
}

static bool game_step(struct game_state *state)
{
    if(kbhit())
    switch (getch()) {
    case 'r':
        randomize(state);
        break;
    case 'q':
        return false;
    }
    iterate(state);
    draw(state);
    return true;
}

#ifdef _MSC_VER
__declspec(dllexport)
#endif
const struct game_api GAME_API = {
    .init = game_init,
    .reload = game_reload,
    .step = game_step,
    .unload = game_unload,
    .finalize = game_finalize
};
