#include <stdio.h>
#include "./app/functions.c"
#include "ryb/ryb.h"
#include <stdint.h>
#include <unistd.h>
//2 32 values needed, for frequency and amplitude. must translate to binary
//merge binary values into one array and output that


static uint8_t bpm;
static uint8_t volume;
static int have_bpm;
static int have_volume;

void dec_receive_heartbeat(uint8_t value, ryb_trend_t trend)
{
    (void) trend;
    bpm = value;
    have_bpm = 1;
}

void dec_receive_sound(uint8_t value, ryb_trend_t trend)
{
    (void) trend;
    volume = value;
    have_volume = 1;
}


int matrix[5][5] = {
    1, 2, 3, 7, 9,
    5, 4, 4, 6, 9,
    6, 5, 5, 6, 9,
    9, 8, 7, 7, 9,
    9, 9, 8, 8, 9
};


//int bpm, vol;
//float s, sbpm, svol;
//int row = 4, col = 4;
//int freq = 0, amp = 0;



int main(void)
{   
    if (ryb_begin() != RYB_OK) {
        return 1;
    }
    unsigned loops = 0;
    for (;;) {
        ryb_update();
        if (loops++ % 50u == 0u && have_bpm && have_volume) {
            uint16_t row;
            uint16_t col;
            struct matrix_return stress_calc = stress_calculation(bpm, volume);
            struct matrix_return result = matrix_algorithm(stress_calc.freq, matrix);
            ryb_send_command(result.freq, result.amp);
        }
        usleep(10000);
    }
}