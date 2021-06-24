#define _DEFAULT_SOURCE // usleep()
#include <stdio.h>
#include <unistd.h>
#include <dlfcn.h>
#include <signal.h>
#include <stdbool.h>
#include "game.h"

#include "reload_patch.c"       // Comment out to disable shared object symbol patching

char *global_game_library_file_name = 0;

struct game{
    void *handle;
    struct game_api api;
    struct game_state *state;
};

static bool global_reload_shared_library = true;
static void reload_shared_program_library(int signum){
    if(signum == SIGUSR1){
        global_reload_shared_library = true;
    }
}

static void game_reload(struct game *game){
    if(game->handle){
        game->api.unload(game->state);
        dlclose(game->handle);
    }

    void *handle = dlopen(global_game_library_file_name, RTLD_NOW);
    if(handle){
#if ENABLE_RELOAD_PATCH
        patch_so_symbols(handle);
#endif
        game->handle = handle;
        const struct game_api *api = dlsym(game->handle, "GAME_API");
        if(api != NULL){
            game->api = *api;
            if(game->state == NULL){
                game->state = game->api.init();
            }
            game->api.reload(game->state);
        }
        else{
            dlclose(game->handle);
            game->handle = NULL;
        }
    }
    else{
        game->handle = NULL;
    }
}

void game_unload(struct game *game){
    if(game->handle){
        game->api.unload(game->state);
        game->api.finalize(game->state);
        game->state = NULL;
        dlclose(game->handle);
        game->handle = NULL;
    }
}

int main(int argc, char *argv[]){
    global_game_library_file_name = "./libgame.so";
    if(argc > 1){
        char *target_fname = argv[1];
        if(access(target_fname, R_OK) != -1){
            global_game_library_file_name = target_fname;
        }
        else{
            fprintf(stderr, "Unable to load %s\n", target_fname);
            return -1;
        }
    }

    struct sigaction act = {0};
    act.sa_handler = *reload_shared_program_library;
    sigaction(SIGUSR1, &act, 0);

    struct game game = {0};
    while(true){
        if(global_reload_shared_library){
            game_reload(&game);
            global_reload_shared_library = false;
        }

        if(game.handle){
            if(!game.api.step(game.state)){
                break;
            }
        }
        usleep(100000);
    }
    game_unload(&game);
    return 0;
}
