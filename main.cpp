#include <Windows.h>
#include "Game.h"

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    Game game;
    game.Initialize();

    while (!game.IsEndRequested()) {
        game.Update();
        game.Draw();
    }

    game.Finalize();
    return 0;
}