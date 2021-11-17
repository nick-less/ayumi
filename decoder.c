/* Author: Peter Sovietov */

#include "ayumi.h"
#include "load_text.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WAVE_FORMAT_IEEE_FLOAT 3
// WAVE_FORMAT_IEEE_FLOAT are 32 bit or 64 bit floating-point numbers that vary
// between 1 and -1.
#define WAVE_FORMAT_PCM 1
// WAVE_FORMAT_PCM or WAVE_FORMAT_EXTENSIBLE, nBlockAlign must be equal to the
// product of nChannels and wBitsPerSample divided by 8 (bits per byte

#define SAMPLES_PER_BLOCK 44100
#define CHANNELS 2

static short int
    audio_buffer[SAMPLES_PER_BLOCK * sizeof(short int) * CHANNELS + 1];

void update_ayumi_state(struct ayumi *ay, int *r) {
  ayumi_set_tone(ay, 0, (r[1] << 8) | r[0]);
  ayumi_set_tone(ay, 1, (r[3] << 8) | r[2]);
  ayumi_set_tone(ay, 2, (r[5] << 8) | r[4]);
  ayumi_set_noise(ay, r[6]);
  ayumi_set_mixer(ay, 0, r[7] & 1, (r[7] >> 3) & 1, r[8] >> 4);
  ayumi_set_mixer(ay, 1, (r[7] >> 1) & 1, (r[7] >> 4) & 1, r[9] >> 4);
  ayumi_set_mixer(ay, 2, (r[7] >> 2) & 1, (r[7] >> 5) & 1, r[10] >> 4);
  ayumi_set_volume(ay, 0, r[8] & 0xf);
  ayumi_set_volume(ay, 1, r[9] & 0xf);
  ayumi_set_volume(ay, 2, r[10] & 0xf);
  ayumi_set_envelope(ay, (r[12] << 8) | r[11]);
  if (r[13] != 255) {
    ayumi_set_envelope_shape(ay, r[13]);
  }
}

void set_default_text_data(struct text_data *t) {
  memset(t, 0, sizeof(struct text_data));
  t->sample_rate = 44100;
  t->eqp_stereo_on = 0;
  t->dc_filter_on = 1;
  t->is_ym = 1;
  t->clock_rate = 2000000;
  t->frame_rate = 50;
}

struct context_t {
  int frame;
  double isr_step;
  double isr_counter;
  long int samples;
  short int *buffer;
} context;

struct user_data {
  struct context_t *ctx;
  struct ayumi *ay;
  struct text_data *t;
} userdata;

void ayumi_render(struct context_t *ctx, struct ayumi *ay,
                  struct text_data *t) {

  printf("frame  %d / %d \n", ctx->frame, t->frame_count);

  while ((ctx->samples == 0 || ctx->samples % SAMPLES_PER_BLOCK != 0) &&
         ctx->frame < t->frame_count) {
    ctx->isr_counter += ctx->isr_step;
    if (ctx->isr_counter >= 1) {
      ctx->isr_counter -= 1;
      update_ayumi_state(ay, &t->frame_data[ctx->frame * 16]);
      ctx->frame += 1;
    }
    ayumi_process(ay);
    if (t->dc_filter_on) {
      ayumi_remove_dc(ay);
    }
    ctx->buffer[0] = (int)(ay->left * t->volume * 32768);
    ctx->buffer[1] = (int)(ay->right * t->volume * 32768);
    ctx->buffer += 2;
    ctx->samples++;
  }
  ctx->samples++;

  printf("%ld samples %lu\n", ctx->samples, sizeof(float));
}

/* The audio function callback takes the following parameters:
      stream:  A pointer to the audio buffer to be filled
      len:     The length (in bytes) of the audio buffer
   */
void fill_audio(void *udata, Uint8 *stream, int len) {

  struct user_data *ud = (struct user_data *)udata;

  /* Mix as much data as possible */
  // len = ( len > audio_len ? audio_len : len );
  printf("fill_audio  %p %p %d \n", udata, stream, len);

  SDL_memset(stream, 0, len); // make sure this is silence.
  ud->ctx->buffer = (short int *)&audio_buffer;
  ayumi_render(ud->ctx, ud->ay, ud->t);

  SDL_MixAudio(stream, (const Uint8 *)&audio_buffer, len, SDL_MIX_MAXVOLUME);
}

int main(int argc, char **argv) {
  int sample_count;
  struct text_data t;
  struct ayumi ay;
  if (argc != 2) {
    printf("ayumi_render input.txt\n");
    return 1;
  }
  set_default_text_data(&t);
  if (!load_text_file(argv[1], &t)) {
    printf("Load error\n");
    return 1;
  }
  sample_count = (int)((t.sample_rate / t.frame_rate) * t.frame_count);
  if (sample_count == 0) {
    printf("No frames\n");
    return 1;
  }
  if (!ayumi_configure(&ay, t.is_ym, t.clock_rate, t.sample_rate)) {
    printf("ayumi_configure error (wrong sample rate?)\n");
    return 1;
  }
  if (t.pan[0] >= 0) {
    ayumi_set_pan(&ay, 0, t.pan[0], t.eqp_stereo_on);
  }
  if (t.pan[1] >= 0) {
    ayumi_set_pan(&ay, 1, t.pan[1], t.eqp_stereo_on);
  }
  if (t.pan[2] >= 0) {
    ayumi_set_pan(&ay, 2, t.pan[2], t.eqp_stereo_on);
  }

  SDL_AudioSpec wanted;

  /* Set the audio format */
  wanted.freq = 44100;
  wanted.format = AUDIO_S16;
  wanted.channels = 2;                /* 1 = mono, 2 = stereo */
  wanted.samples = SAMPLES_PER_BLOCK; /* Good low-latency value for callback */
  wanted.callback = fill_audio;
  wanted.userdata = &userdata;

  context.frame = 0;
  context.isr_step = t.frame_rate / t.sample_rate;
  context.isr_counter = 1;
  context.samples = 0;
  userdata.ctx = &context;
  userdata.ay = &ay;
  userdata.t = &t;

  /* Open the audio device, forcing the desired format */
  if (SDL_OpenAudio(&wanted, NULL) < 0) {
    fprintf(stderr, "Couldn't open audio: %s\n", SDL_GetError());
    return (-1);
  }

  SDL_PauseAudio(0);

  while (context.frame < t.frame_count) {

    SDL_Delay(200); /* Sleep 1/10 second */
  }

  SDL_CloseAudio();
  return 0;
}
