#include "board.h"
#include "bootloader.h"
#include "tim_delay.h"

int main(void)
{
    board_lowlevel_init();
    board_init();
    tim_delay_init();

    bootloader_main();
}
