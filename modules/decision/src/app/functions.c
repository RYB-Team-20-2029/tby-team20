void inputs() {
    printf("Please enter BPM (60-240): ");
    scanf("%d", &bpm);
    printf("BPM: %d\nPlease enter volume (0-100%%): ", bpm);
    scanf("%d", &vol);
    printf("Volume: %d\n", vol); //whats up
}

void stress_calculation (bpm, vol) {
    float p_bpm = ((float)(bpm - 60) / 180)*100; //convert bpm to percentage value, the (float) cast is necessary to avoid the division always resulting in 0
    sbpm = 0.9*p_bpm + 10;
    svol = vol*0.4 + 10;

    s = (svol + sbpm)/2;
    /*
    *   create a new line based on sbpm and svol, then find intersection with the two stress lines to generate s
    *   float xs = ((svol-sbpm)/(vol-p_bpm))+(sbpm-((svol-sbpm)/(vol-p_bpm))*p_bpm);
    */


    // Print out calculations and seperate stress levels to keep track 
    printf("%.1f\n", s);
    printf("\nStress (BPM): %.1f  \nStress (Volume): %.1f", sbpm, svol);
    
    
    // Decide to include/adjust/exclude the svol based on the 50% threshold
    
    if(svol < 50){ //need to adjust/optimize
        s = (sbpm+svol)/2;
    }
    else{
        s = (50+sbpm)/2;
    }
    printf("\nStress level: %.1f\n", s);
    
    int k = (s/(100.0/9))+1;
    if(k > 9){
        k=9;
    }
    if(k <1){
        k=1;
    }
    printf("\nThe K level is: K%d\n", k);

}


void matrix_algorithm(s) {
    /* Cycle through the matrix orderly via columns or rows. Then store in the matrix the K levels if the progression succeeds.
    *   If the progression fails and the baby gets a panic jump, then skip that K-square for now and switch the cycle direction ( vertical vs horizontal)
    
   while(1)
    { */
        int current = matrix[row][col];
        printf("K%d at F%d, A%d\n", current, col + 1, row + 1);
        if(k == matrix[row][col]){
            printf("WE ARE HERE!^^^\n");
            freq = col+1;
            amp = row +1;
        }
        if(current == 1){
            break;
        }
        if (row > 0 && matrix[row - 1][col] < current) {
            row--;
        }
        else if (col > 0 && matrix[row][col - 1] < current) {
            col--;
        }
   // }
    printf("\nAmplitude: A%d", amp);
    printf("\nFrequency: F%d", freq);
    
    printf("\n");
}