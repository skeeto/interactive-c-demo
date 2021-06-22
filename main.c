#define _DEFAULT_SOURCE // usleep()
#include <stdio.h>
#include <unistd.h>
#include <dlfcn.h>
#include <signal.h>
#include <stdbool.h>
#include "game.h"

const char *GAME_LIBRARY = "./libgame.so";

struct game {
    void *handle;
    struct game_api api;
    struct game_state *state;
};

static bool global_reload_shared_library = true;
static void reload_shared_program_library(int signum)
{
    if(signum == SIGUSR1)
    {
        global_reload_shared_library = true;
    }
}

static void game_load(struct game *game)
{
    if (global_reload_shared_library) {
        if(game->handle) {
            game->api.unload(game->state);
            dlclose(game->handle);
        }
        void *handle = dlopen(GAME_LIBRARY, RTLD_NOW);
        if (handle) {
            game->handle = handle;
            const struct game_api *api = dlsym(game->handle, "GAME_API");
            if (api != NULL) {
                game->api = *api;
                if (game->state == NULL)
                    game->state = game->api.init();
                game->api.reload(game->state);
            } else {
                dlclose(game->handle);
                game->handle = NULL;
            }
        } else {
            game->handle = NULL;
        }
        global_reload_shared_library = false;
    }
}

void game_unload(struct game *game)
{
    if (game->handle) {
        game->api.unload(game->state);
        game->api.finalize(game->state);
        game->state = NULL;
        dlclose(game->handle);
        game->handle = NULL;
    }
}

int main(void)
{
    struct sigaction act = {0};
    act.sa_handler = *reload_shared_program_library;
    sigaction(SIGUSR1, &act, 0);

    struct game game = {0};
    for (;;) {
        game_load(&game);
        if (game.handle)
            if (!game.api.step(game.state))
                break;
        usleep(100000);
    }
    game_unload(&game);
    return 0;
}
