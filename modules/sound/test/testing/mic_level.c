#include <stdio.h>
#include <stdlib.h>
#include <math.h>   //math library
#include <alsa/asoundlib.h>  // audio library

// Important note: this code will not work on our stuff. this is only to understand.
// to run gcc mic_level -o mic_level.c then ./mic_level I think...
// arecord -D pulse -f S16_LE -r 44100 -c 1 -d 2 /dev/null (to cold start). then ./mic_level

#define SAMPLE_RATE 44100
#define CHANNELS 1
#define FRAMES 1024
#define THRESHOLD 50
#define DB_FLOOR -60.0
#define DB_CEIL   0.0

double compute_dbfs(short *buffer, snd_pcm_uframes_t frames, int channels) 
// this is where rms is turned into decibels
{
    long long sum_sq = 0;
    int total_samples = frames * channels;

    for (int i = 0; i < total_samples; i++) {
        sum_sq += (long long)buffer[i] * buffer[i];
    }

    double rms = sqrt((double)sum_sq / total_samples);
    if (rms < 1.0) rms = 1.0;

    return 20.0 * log10(rms / 32768.0);
}

int dbfs_to_level(double dbfs) 
// this is where db has a floor and a ceiling
{
    if (dbfs < DB_FLOOR) dbfs = DB_FLOOR;
    if (dbfs > DB_CEIL) dbfs = DB_CEIL;

    double level = (dbfs - DB_FLOOR) / (DB_CEIL - DB_FLOOR) * 100.0;
    return (int)(level + 0.5);
}

void print_bar(int level) 
// this is where db is visiualized on terminal
{
    printf("\r[");
    int bar_width = 50;
    int filled = (level * bar_width) / 100;
    for (int i = 0; i < bar_width; i++) {
        putchar(i < filled ? '#' : ' ');
    }
    printf("] %3d", level);
    if (level >= THRESHOLD) {
        printf("  <-- TRIGGERED (>= %d)   ", THRESHOLD);
    } else {
        printf("                          ");
    }
    fflush(stdout);
}

int main() 
// the main function
{
    int err;
    snd_pcm_t *capture_handle;
    snd_pcm_hw_params_t *hw_params;

// this is input for device

    const char *device = "pulse";
    unsigned int sample_rate = SAMPLE_RATE;
    int channels = CHANNELS;
    snd_pcm_uframes_t frames = FRAMES;

    if ((err = snd_pcm_open(&capture_handle, device, SND_PCM_STREAM_CAPTURE, 0)) < 0) {
        fprintf(stderr, "cannot open audio device %s (%s)\n", device, snd_strerror(err));
        return 1;
    }

    snd_pcm_hw_params_alloca(&hw_params);
    snd_pcm_hw_params_any(capture_handle, hw_params);
    snd_pcm_hw_params_set_access(capture_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(capture_handle, hw_params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(capture_handle, hw_params, channels);
    snd_pcm_hw_params_set_rate_near(capture_handle, hw_params, &sample_rate, 0);
    snd_pcm_hw_params_set_period_size_near(capture_handle, hw_params, &frames, 0);

    if ((err = snd_pcm_hw_params(capture_handle, hw_params)) < 0) {
        fprintf(stderr, "cannot set hw params (%s)\n", snd_strerror(err));
        snd_pcm_close(capture_handle);
        return 1;
    }

    snd_pcm_prepare(capture_handle);

    short *buffer = malloc(frames * channels * sizeof(short));
    if (!buffer) {
        fprintf(stderr, "failed to allocate buffer\n");
        snd_pcm_close(capture_handle);
        return 1;
    }

    printf("Listening on '%s'. Press Ctrl+C to stop.\n", device);
    printf("Level scale: 0 = silence, 100 = max input. Threshold = %d\n\n", THRESHOLD);

    while (1) {
        err = snd_pcm_readi(capture_handle, buffer, frames);

        if (err == -EPIPE) {
            fprintf(stderr, "\nbuffer overrun, recovering...\n");
            snd_pcm_prepare(capture_handle);
            continue;
        } else if (err < 0) {
            fprintf(stderr, "\nread error: %s\n", snd_strerror(err));
            break;
        }

        double dbfs = compute_dbfs(buffer, frames, channels);
        int level = dbfs_to_level(dbfs);

        print_bar(level);
    }

    free(buffer);
    snd_pcm_close(capture_handle);
    return 0;
}
