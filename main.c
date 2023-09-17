#include <dlfcn.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>

#include "fft.h"
#include "plug.h"
#include <assert.h>
#include <pthread.h>
#include <raylib.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 450

#define FPS 24

#define ARRAY_LENGTH(x) (sizeof(x) / sizeof(x[0]))

static unsigned int CHANNELS = 2;

typedef struct Frames {
  // TODO: Check which is left and which is right
  float left;
  float right;
} Frames;

#define FRAME_BUFFER_CAPACITY (2 * SCREEN_WIDTH)
static Frames FRAME_BUFFER[FRAME_BUFFER_CAPACITY] = {0};
static unsigned int FRAME_BUFFER_SIZE = 0;
#define FRAME_BUFFER_SIZE_BYTES (FRAME_BUFFER_SIZE * sizeof(Frames))
#define FRAME_BUFFER_UNINITIALIZED_ELEMS                                       \
  (((long)FRAME_BUFFER_CAPACITY - (long)FRAME_BUFFER_SIZE < 0)                 \
       ? 0                                                                     \
       : (FRAME_BUFFER_CAPACITY - FRAME_BUFFER_SIZE))
#define FRAME_BUFFER_UNINITIALIZED_BYTES                                       \
  (FRAME_BUFFER_UNINITIALIZED_ELEMS * sizeof(Frames))
#define FRAMES_TO_SAMPLES(frames) (frames * CHANNELS)
#define SAMPLES_TO_FRAMES(frames) (frames / CHANNELS)
static pthread_mutex_t BUFFER_LOCK;

// NOTE: From raudio.c:1269 (LoadMusicStream) of raylib:
//  We are loading samples are 32bit float normalized data, so,
//  we configure the output audio stream to also use float 32bit
void fillSampleBuffer(void *buffer, unsigned int frames) {
  if (frames == 0)
    return; // Nothing to do! TODO: Check if this even can happen.
  assert(CHANNELS == 2 && "Does only support music with 2 channels.");

  unsigned int frames_to_copy = 0;
  unsigned int offset = 0;
  const unsigned int available_frames_to_fill =
      FRAME_BUFFER_CAPACITY - FRAME_BUFFER_SIZE;

  if (frames >= FRAME_BUFFER_CAPACITY) {
    // Just fill all up with new values.
    offset = 0;
    frames_to_copy = FRAME_BUFFER_CAPACITY;
  } else if (frames > available_frames_to_fill) {
    // Just start again with the newest frames
    offset = 0;
    frames_to_copy = frames;
  } else {
    // Append the frames to the buffer
    offset = FRAME_BUFFER_SIZE;
    frames_to_copy = frames;
  }

  int err;
  // lock mutex
  if ((err = pthread_mutex_trylock(&BUFFER_LOCK)) != 0) {
    if (err != EBUSY) {
      printf("WARNING could not lock mutex. Error code %d. Returning from "
             "function.",
             err);
    }
    // Do not waste time. Just try next time.
    return;
  }
  memcpy(FRAME_BUFFER + offset, buffer,
         frames_to_copy *
             sizeof(Frames)); // HACK: This only works for two channel audio
                              //  because Frames contains 2 floats.
  FRAME_BUFFER_SIZE = offset + frames_to_copy;

  // Unlock mutex
  if ((err = pthread_mutex_unlock(&BUFFER_LOCK)) != 0) {
    printf("ERROR: Could not unlock mutex. Error code: %d. Quitting program.",
           err);
    exit(err);
  }
}

#define FFT_SIZE 2048

#define max(a, b) (a > b ? a : b)

static inline float fmaxvf(const float x[], const size_t n) {
  assert(n > 0 && "Empty vector");
  float m = x[0];
  for (size_t i = 0; i < n; ++i) {
    m = max(m, x[i]);
  }
  return m;
}

static inline float cmaxvf(const float complex x[], const size_t n) {
  assert(n > 0 && "Empty vector");
  float m = cabsf(x[0]);
  for (size_t i = 0; i < n; ++i) {
    m = max(m, cabsf(x[i]));
  }
  return m;
}

void drawFrequency(void) {

  int err;
  // Locking mutex
  if ((err = pthread_mutex_lock(&BUFFER_LOCK)) != 0) {
    printf("WARNING could not lock mutex. Error code %d. Returning from "
           "function.",
           err);
    return;
  };

  float samples[FFT_SIZE] = {0};
  float complex frequencies[FFT_SIZE];
  assert(FFT_SIZE >= FRAME_BUFFER_SIZE && "You need to increase the FFT_SIZE");
  for (unsigned int i = 0; i < FRAME_BUFFER_SIZE; ++i) {
    samples[i] = FRAME_BUFFER[i].left; // only take left channel
  }
  FRAME_BUFFER_SIZE = 0;

  // Unlock mutex
  if ((err = pthread_mutex_unlock(&BUFFER_LOCK)) != 0) {
    printf("ERROR: Could not unlock mutex. Error code: %d. Quitting program.",
           err);
    exit(err);
  }

  // Compute FFT
  fft(samples, frequencies, FFT_SIZE);

  const float f_max = cmaxvf(frequencies, FFT_SIZE);
  for (unsigned int i = 0; i < SCREEN_WIDTH; ++i) {
    const float f = cabsf(frequencies[i]);
    const int h = (float)GetScreenHeight() / f_max * f;
    DrawRectangle(i, GetScreenHeight() - h, 1, h, RED);
  }
}

void drawWave(void) {
  int err;
  // Locking mutex
  if ((err = pthread_mutex_lock(&BUFFER_LOCK)) != 0) {
    printf("WARNING could not lock mutex. Error code %d. Returning from "
           "function.",
           err);
    return;
  };
  for (unsigned int i = 0; i < FRAME_BUFFER_SIZE; ++i) {
    const float sleft = FRAME_BUFFER[i].left;
    const float sright = FRAME_BUFFER[i].right;
    (void)sright;
    if (sleft > 0)
      DrawRectangle(i, GetScreenHeight() / 2, 1, sleft * GetScreenHeight() / 2,
                    RED);
    else
      DrawRectangle(
          i, (float)GetScreenHeight() / 2 + sleft * GetScreenHeight() / 2, 1,
          -sleft * GetScreenHeight() / 2, RED);
  }
  FRAME_BUFFER_SIZE = 0;
  // Unlock mutex
  if ((err = pthread_mutex_unlock(&BUFFER_LOCK)) != 0) {
    printf("ERROR: Could not unlock mutex. Error code: %d. Quitting program.",
           err);
    exit(err);
  }
}

void drawMusic(void) {
  if (FRAME_BUFFER_SIZE == 0) {
    return; // Nothing to draw
  }

#ifndef USE_WAVE
  drawFrequency();
#else
  drawWave();
#endif
}

const char *getFirstFile(const FilePathList files) {
  assert(files.count > 0 && "No files found");
  return files.paths[0];
}

Music playMusicFromFile(void) {
  const FilePathList files = LoadDroppedFiles();
  const char *file_path = getFirstFile(files);
  const Music music = LoadMusicStream(file_path);
  PlayMusicStream(music);
  UnloadDroppedFiles(files);
  printf("Frame count: %u\n", music.frameCount);
  printf("Sample rate: %u\n", music.stream.sampleRate);
  printf("Frame size: %u\n", music.frameCount);
  CHANNELS = music.stream.channels;
  return music;
}

int main(void) {

  const char *libplug_file_name = "build/libplug.so";

  void *libplug = dlopen(libplug_file_name, RTLD_LAZY);

  if (libplug == NULL) {
    fprintf(stderr, "Could not load %s: %s\n", libplug_file_name, dlerror());
    return 1;
  }
  PLUG plugins = dlsym(libplug, PLUG_SYM);
  if (libplug == NULL) {
    fprintf(stderr, "Could not load %s from %s: %s\n", PLUG_SYM,
            libplug_file_name, dlerror());
    return 1;
  }
  plugins->plug_hello();

  dlclose(libplug);

  if (pthread_mutex_init(&BUFFER_LOCK, NULL) != 0) {
    printf("\n mutex init failed\n");
    return 1;
  }

  InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "musializer");
  InitAudioDevice();

  // Music that will be played
  Music music;

  float timePlayed = 0.0f; // Time played normalized [0.0f..1.0f]

  SetTargetFPS(FPS); // Set our game to run at 30 frames-per-second
  //--------------------------------------------------------------------------------------

  // Main game loop
  while (!WindowShouldClose()) // Detect window close button or ESC key
  {

    // Update
    //----------------------------------------------------------------------------------
    if (IsFileDropped()) {
      if (IsMusicReady(music))
        // Detach if already loaded
        DetachAudioStreamProcessor(music.stream, &fillSampleBuffer);

      music = playMusicFromFile();

      // Reset buffer
      FRAME_BUFFER_SIZE = 0;
      // Attach to new music stream
      AttachAudioStreamProcessor(music.stream, &fillSampleBuffer);
    }

    if (IsMusicReady(music)) {
      UpdateMusicStream(music); // Update music buffer with new stream data

      // Restart music playing (stop and play)
      if (IsKeyPressed(KEY_SPACE)) {
        StopMusicStream(music);
        PlayMusicStream(music);
      }

      // Pause/Resume music playing
      if (IsKeyPressed(KEY_P)) {

        if (IsMusicStreamPlaying(music))
          PauseMusicStream(music);
        else
          ResumeMusicStream(music);
      }

      // Get normalized time played for current music stream
      timePlayed = GetMusicTimePlayed(music) / GetMusicTimeLength(music);

      if (timePlayed > 1.0f)
        timePlayed = 1.0f; // Make sure time played is no longer than music
    }

    //----------------------------------------------------------------------------------

    // Draw
    //----------------------------------------------------------------------------------
    BeginDrawing();

    ClearBackground(BLACK);

    if (IsMusicReady(music)) {

      drawMusic();

      // DrawText("MUSIC SHOULD BE PLAYING!", 255, 150, 20, WHITE);
      //
      // DrawRectangle(200, 200, 400, 12, WHITE);
      // DrawRectangle(200, 200, (int)(timePlayed * 400.0f), 12, RED);
      // DrawRectangleLines(200, 200, 400, 12, LIGHTGRAY);
      //
      // DrawText("PRESS SPACE TO RESTART MUSIC", 215, 250, 20, WHITE);
      // DrawText("PRESS P TO PAUSE/RESUME MUSIC", 208, 280, 20, WHITE);
    } else {
      DrawText("DRAG AND DROP A MUSIC FILE", 235, 200, 20, WHITE);
    }

    EndDrawing();
    //----------------------------------------------------------------------------------
  }
  // Clean up
  DetachAudioStreamProcessor(music.stream, &fillSampleBuffer);
  UnloadMusicStream(music);
  CloseAudioDevice();
  CloseWindow();
  pthread_mutex_destroy(&BUFFER_LOCK);
  return 0;
}
