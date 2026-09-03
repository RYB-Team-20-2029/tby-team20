/* Purpose: fixed electrical, signal and mapping constants for the RYB cradle.
 * Owning module: management (mgmt/) — values come from the 5ECW0 course brief. */

#ifndef RYB_CONFIG_H
#define RYB_CONFIG_H

#include <stdint.h>

/* ------------------------------------------------------------------------
 * PWM drive to the cradle (applies to both the F and the A channel)
 * ------------------------------------------------------------------------ */

#define RYB_PWM_CARRIER_HZ      1000u /* 1 kHz carrier                        */
#define RYB_PWM_AMPLITUDE_MV   12000u /* 12 V drive amplitude                 */
#define RYB_PWM_CURRENT_MIN_MA   800u /* driver must supply at least 0.8 A    */

/* HARD LIMIT. Duty cycle above this trips the cradle emergency light, which
 * ends the test slot. Clamp every computed duty against it before output. */
#define RYB_PWM_DUTY_MAX_PCT      90u
#define RYB_PWM_DUTY_MIN_PCT       0u

/* ------------------------------------------------------------------------
 * Region <-> duty cycle mapping
 *
 * Region  Duty range   Cradle frequency   Cradle amplitude
 *   1      0 - 10 %        0.20 Hz              20 %
 *   2     10 - 30 %        0.35 Hz              40 %
 *   3     30 - 50 %        0.50 Hz              60 %
 *   4     50 - 70 %        0.65 Hz              80 %
 *   5     70 - 90 %        0.70 Hz             100 %
 *
 * Regions are 1..5. Index the tables below with (region - 1).
 * Frequencies are in centihertz (Hz x 100) to keep float out of the drive
 * path — see docs/coding-standard.md §5.3.
 * ------------------------------------------------------------------------ */

#define RYB_REGION_COUNT 5u

/* Inclusive lower / exclusive upper duty bound of each region, in percent.
 * The upper bound of region 5 is RYB_PWM_DUTY_MAX_PCT and must not be crossed. */
#define RYB_REGION_DUTY_LO_PCT { 0u, 10u, 30u, 50u, 70u }
#define RYB_REGION_DUTY_HI_PCT { 10u, 30u, 50u, 70u, 90u }

/* A representative duty per region, for a module that must pick one value.
 * Chosen mid-band, and every entry is <= RYB_PWM_DUTY_MAX_PCT. */
#define RYB_REGION_DUTY_NOMINAL_PCT { 5u, 20u, 40u, 60u, 80u }

/* Resulting cradle behaviour per region. */
#define RYB_REGION_FREQ_CHZ { 20u, 35u, 50u, 65u, 70u } /* 0.20 .. 0.70 Hz */
#define RYB_REGION_AMPL_PCT { 20u, 40u, 60u, 80u, 100u }

/* ------------------------------------------------------------------------
 * Sensed quantities
 * ------------------------------------------------------------------------ */

#define RYB_HEART_BPM_MIN  60u
#define RYB_HEART_BPM_MAX 240u

#define RYB_STRESS_MIN_PCT  10u
#define RYB_STRESS_MAX_PCT 100u

/* The doll's crying volume is constant for 50 < S < 100 and only falls off
 * below S = 50. Loudness alone therefore cannot resolve high stress levels:
 * above this knee the decision module must lean on heart rate and on trend. */
#define RYB_STRESS_CRY_KNEE_PCT 50u

/* ------------------------------------------------------------------------
 * Stress matrix
 * ------------------------------------------------------------------------ */

/* 5x5 of (F, A) pairs. K9 sits at (F5, A5) and K1 at (F1, A1); exactly one
 * path of adjacent moves descends K9 -> K1, and a wrong move causes a panic
 * jump. The matrix contents are hidden and are not encoded here. */
#define RYB_MATRIX_DIM        5u
#define RYB_STRESS_LEVEL_MIN  1u /* K1 = calm    */
#define RYB_STRESS_LEVEL_MAX  9u /* K9 = maximum */

/* ------------------------------------------------------------------------
 * Pin assignments — NOT YET DECIDED
 *
 * TODO(mgmt): fill in once the RYB shield pinout and the backbone connector
 * are fixed. Deliberately left as comments rather than placeholder #defines,
 * so that nothing can build against a wrong-but-plausible value.
 *
 *   TODO(hb):   wrist LED / photodiode ADC channel
 *   TODO(snd):  microphone front-end ADC channel
 *   TODO(mot):  PWM output pin, F channel
 *   TODO(mot):  PWM output pin, A channel
 *   TODO(mgmt): backbone connector pins (all four boards)
 *   TODO(mgmt): 1.54" LCD SPI pins (all four boards)
 * ------------------------------------------------------------------------ */

/* ------------------------------------------------------------------------
 * ECG bonus (optional)
 * TODO: the sampling rate must be justified against Nyquist for the ECG
 * bandwidth actually used, and the analog front end must scale into 0..3.3 V
 * before the ADC. Neither is fixed yet.
 * ------------------------------------------------------------------------ */

#define RYB_ADC_VREF_MV 3300u

#endif /* RYB_CONFIG_H */
