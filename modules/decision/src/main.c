#include <stdio.h>

int main()
{
    int bpm, vol;
    float s, sbpm, svol;
    printf("Please enter BPM (60-240): ");
    scanf("%d", &bpm);
    printf("\nBPM: %d\nPlease enter volume (0-100%): ", bpm);
    scanf("%d", &vol);
    printf("\nVolume: %d", vol);
    sbpm = bpm*.5 - 20;
    svol = vol*.4 + 10;
    printf("\nStress (BPM): %.1f  Stress (Volume): %.1f", sbpm, svol);
    if(svol < 50){ //need to adjust/optimize
        s = (sbpm+svol)*.5;
    }
    else{
        s = sbpm;
    }
    
    printf("\nStress level: %.1f", s);
}