#include "bluetooth.h"
#include "tetris.hpp"

extern "C" void app_main(void)
{
    init_ble();
    while(1)
    {
        Game game;
        game.reset();
        game.run();
    }
}
