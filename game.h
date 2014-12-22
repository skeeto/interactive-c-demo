#pragma once

struct game_api {
    void (*init)(void);
    void (*step)(void);
};

extern const struct game_api GAME_API;
