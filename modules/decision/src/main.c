#include <stdio.h>
#include "./app/functions.c"
//2 32 values needed, for frequency and amplitude. must translate to binary
//merge binary values into one array and output that

    int matrix[5][5] = {
        1, 2, 3, 7, 9,
        5, 4, 4, 6, 9,
        6, 5, 5, 6, 9,
        9, 8, 7, 7, 9,
        9, 9, 8, 8, 9
    };

    int bpm, vol;
    float s, sbpm, svol;
    int row = 4, col = 4;
    int freq = 0, amp = 0;


int main()
{   
   inputs();
   // loop
   stress_calculation(bpm, vol);
   matrix_algorithm(s);
}
