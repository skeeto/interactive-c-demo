#define _BSD_SOURCE // usleep()
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dlfcn.h>

struct game {
    void *handle;
    time_t mtime;
    void (*step)(void);
};

static void game_load(struct game *game)
{
    struct stat attr;
    const char *path = "./libgame.so";
    if ((stat(path, &attr) == 0) && (game->mtime != attr.st_mtime)) {
        if (game->handle)
            dlclose(game->handle);
        void *handle = dlopen(path, RTLD_NOW);
        if (handle) {
            game->handle = handle;
            game->mtime = attr.st_mtime;
            game->step = dlsym(game->handle, "step");
        }
    }
}

int main(void)
{
    struct game game = {0};
    for (;;) {
        game_load(&game);
        game.step();
        usleep(100000);
    }
    return 0;
}
