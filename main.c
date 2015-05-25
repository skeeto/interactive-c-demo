#define _BSD_SOURCE // usleep()
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include "game.h"

#ifdef _WIN32
#include <windows.h>
const char *GAME_LIBRARY = "./libgame.dll";
#else
#include <unistd.h>
const char *GAME_LIBRARY = "./libgame.so";
#endif


struct game {
    void *handle;
    time_t id;
    struct game_api api;
    struct game_state *state;
};

static void game_load(struct game *game)
{
    struct stat attr;
    if ((stat(GAME_LIBRARY, &attr) == 0) && (game->id != attr.st_mtime)) {
        if (game->handle) {
            game->api.unload(game->state);
            dlclose(game->handle);
        }
        void *handle = dlopen(GAME_LIBRARY, RTLD_NOW);
        if (handle) {
            game->handle = handle;
            game->id = attr.st_mtime;
            const struct game_api *api = dlsym(game->handle, "GAME_API");
            if (api != NULL) {
                game->api = *api;
                if (game->state == NULL)
                    game->state = game->api.init();
                game->api.reload(game->state);
            } else {
                dlclose(game->handle);
                game->handle = NULL;
                game->id = 0;
            }
        } else {
            game->handle = NULL;
            game->id = 0;
        }
    }
}

void game_unload(struct game *game)
{
    if (game->handle) {
        game->api.finalize(game->state);
        game->state = NULL;
        dlclose(game->handle);
        game->handle = NULL;
        game->id = 0;
    }
}

int main(void)
{
    struct game game = {0};
    for (;;) {
        game_load(&game);
        if (game.handle)
            if (!game.api.step(game.state))
                break;
#ifdef _WIN32            
        Sleep(100);
#else
        usleep(100000);
#endif
    }
    game_unload(&game);
    return 0;
}
