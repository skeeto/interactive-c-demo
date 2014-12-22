#pragma once

#include <stdbool.h>

struct game_state;

struct game_api {
    /**
     * @return a fresh game state
     */
    struct game_state *(*init)();

    /**
     * Destroys a game state.
     */
    void (*finalize)(struct game_state *state);

    /**
     * Called exactly once when the game code is reloaded.
     */
    void (*reload)(struct game_state *state);

    /**
     * Called exactly once when the game code is about to be reloaded.
     */
    void (*unload)(struct game_state *state);

    /**
     * Called at a regular interval by the main program.
     * @return true if the program should continue
     */
    bool (*step)(struct game_state *state);
};

extern const struct game_api GAME_API;
