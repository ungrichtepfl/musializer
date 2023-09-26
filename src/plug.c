#include "plug.h"
#include "fft.h"
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <raylib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 450

#define FPS 60

#define ARRAY_LENGTH(x) (sizeof(x) / sizeof(x[0]))

unsigned int CHANNELS = 2;

typedef struct Frames {
  // TODO: Check which is left and which is right
  float left;
  float right;
} Frames;

#define FRAME_BUFFER_CAPACITY 800
static Frames FRAME_BUFFER[FRAME_BUFFER_CAPACITY] = {0};
unsigned int FRAME_BUFFER_SIZE = 0;
#define FRAME_BUFFER_SIZE_BYTES (FRAME_BUFFER_SIZE * sizeof(Frames))
#define FRAME_BUFFER_UNINITIALIZED_ELEMS                                       \
  (((long)FRAME_BUFFER_CAPACITY - (long)FRAME_BUFFER_SIZE < 0)                 \
       ? 0                                                                     \
       : (FRAME_BUFFER_CAPACITY - FRAME_BUFFER_SIZE))
#define FRAME_BUFFER_UNINITIALIZED_BYTES                                       \
  (FRAME_BUFFER_UNINITIALIZED_ELEMS * sizeof(Frames))
#define FRAMES_TO_SAMPLES(frames) (frames * CHANNELS)
#define SAMPLES_TO_FRAMES(frames) (frames / CHANNELS)

pthread_mutex_t BUFFER_LOCK;

#define FFT_SIZE 2048

#define max(a, b) (a > b ? a : b)

Music MUSIC;

typedef struct State {
  bool finished;
  bool reload;
  float timePlayedSeconds;
  char musicFile[255];
  Vector2 window_pos;
  float max;
} State;

State *STATE;

static const char *getFirstFile(const FilePathList files) {
  assert(files.count > 0 && "No files found");
  return files.paths[0];
}

static void playMusicFromFile(void) {
  const FilePathList files = LoadDroppedFiles();
  const char *file_path = getFirstFile(files);
  strcpy(STATE->musicFile, file_path);
  MUSIC = LoadMusicStream(file_path);
  PlayMusicStream(MUSIC);
  UnloadDroppedFiles(files);
  printf("Frame count: %u\n", MUSIC.frameCount);
  printf("Sample rate: %u\n", MUSIC.stream.sampleRate);
  printf("Frame size: %u\n", MUSIC.frameCount);
}

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

// #define USE_WAVE

#ifndef USE_WAVE
static void drawFrequency(void) {

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

  // Unlock mutex
  if ((err = pthread_mutex_unlock(&BUFFER_LOCK)) != 0) {
    printf("ERROR: Could not unlock mutex. Error code: %d. Quitting program.",
           err);
    exit(err);
  }

  // Compute FFT
  fft(samples, frequencies, FFT_SIZE);

  const float f_max = cmaxvf(frequencies, FFT_SIZE);
  STATE->max = max(STATE->max, f_max);
  for (int i = 0; i < GetScreenWidth(); ++i) {
    const float f = cabsf(frequencies[i]);
    const int h = (float)GetScreenHeight() / STATE->max * f;
    DrawRectangle(i, GetScreenHeight() - h, 1, h, BLUE);
  }
}

#else

static void drawWave(void) {
  if (FRAME_BUFFER_SIZE == 0)
    return; // Nothing to draw

  int err;
  // Locking mutex
  if ((err = pthread_mutex_lock(&BUFFER_LOCK)) != 0) {
    printf("WARNING could not lock mutex. Error code %d. Returning from "
           "function.",
           err);
    return;
  };
  for (long i = 0; i < FRAME_BUFFER_SIZE; ++i) {
    const float sleft = FRAME_BUFFER[i].left;
    if (sleft > 0)
      DrawRectangle(i, GetScreenHeight() / 2, 1, sleft * GetScreenHeight() / 2,
                    BLUE);
    else
      DrawRectangle(
          i, (float)GetScreenHeight() / 2 + sleft * GetScreenHeight() / 2, 1,
          -sleft * GetScreenHeight() / 2, RED);
  }
  // Unlock mutex
  if ((err = pthread_mutex_unlock(&BUFFER_LOCK)) != 0) {
    printf("ERROR: Could not unlock mutex. Error code: %d. Quitting program.",
           err);
    exit(err);
  }
}

#endif

static void drawMusic(void) {
  if (FRAME_BUFFER_SIZE == 0) {
    return; // Nothing to draw
  }

#ifndef USE_WAVE
  drawFrequency();
#else
  drawWave();
#endif
}

// NOTE: From raudio.c:1269 (LoadMusicStream) of raylib:
//  We are loading samples are 32bit float normalized data, so,
//  we configure the output audio stream to also use float 32bit
static void fillSampleBuffer(void *buffer, unsigned int frames) {
  if (frames == 0)
    return; // Nothing to do! TODO: Check if this even can happen.
  assert(CHANNELS == 2 && "Does only support music with 2 channels.");
  const float *samples = (float *)buffer;

  const unsigned int available_frames_to_fill =
      FRAME_BUFFER_CAPACITY - FRAME_BUFFER_SIZE;

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

  if (frames <= available_frames_to_fill) {
    memcpy(FRAME_BUFFER + FRAME_BUFFER_SIZE, samples,
           frames *
               sizeof(Frames)); // HACK: This only works for two channel audio
                                //  because Frames contains 2 floats.
    FRAME_BUFFER_SIZE += frames;
  } else if (frames >= FRAME_BUFFER_CAPACITY) {
    memcpy(FRAME_BUFFER, &samples[frames - FRAME_BUFFER_CAPACITY],
           FRAME_BUFFER_CAPACITY *
               sizeof(Frames)); // HACK: This only works for two channel audio
                                //  because Frames contains 2 floats.
    FRAME_BUFFER_SIZE = FRAME_BUFFER_CAPACITY;
  } else {
    assert(FRAME_BUFFER_CAPACITY < frames + FRAME_BUFFER_SIZE);
    const unsigned int diff =
        frames + FRAME_BUFFER_SIZE - FRAME_BUFFER_CAPACITY;
    memmove(FRAME_BUFFER, &FRAME_BUFFER[diff],
            (FRAME_BUFFER_SIZE - diff) *
                sizeof(Frames)); // HACK: This only works for two channel audio
                                 //  because Frames contains 2 floats.)
    memcpy(&FRAME_BUFFER[FRAME_BUFFER_SIZE - diff], samples,
           frames *
               sizeof(Frames)); // HACK: This only works for two channel audio
                                //  because Frames contains 2 floats.
                                //  FRAME_BUFFER_SIZE = FRAME_BUFFER_CAPACITY;
    FRAME_BUFFER_SIZE = FRAME_BUFFER_CAPACITY;
  }

  // Unlock mutex
  if ((err = pthread_mutex_unlock(&BUFFER_LOCK)) != 0) {
    printf("ERROR: Could not unlock mutex. Error code: %d. Quitting program.",
           err);
    exit(err);
  }
}

static bool initInternal(void) {

  if (pthread_mutex_init(&BUFFER_LOCK, NULL) != 0) {
    printf("\n mutex init failed\n");
    return false;
  }
  InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "musializer");
  InitAudioDevice();

  SetTargetFPS(FPS); // Set our game to run at 30 frames-per-second
  return true;
}

bool init(void) {

  if (!initInternal()) {
    return false;
  }

  STATE = malloc(sizeof(State));

  // No need to initialize MUSIC otherwise it segfaults
  STATE->finished = false;
  STATE->reload = false;
  STATE->timePlayedSeconds = 0.0f; // Time played normalized [0.0f..1.0f]
  STATE->max = 1.0;                // Start as if they are normalized
  STATE->window_pos = GetWindowPosition();

  return true;
}

bool resume(State *state) {

  if (!initInternal()) {
    return false;
  }
  STATE = state;
  STATE->reload = false;
  STATE->max = 1.0; // Start as if they are normalized
  if (strlen(STATE->musicFile) != 0) {
    MUSIC = LoadMusicStream(STATE->musicFile);
    PlayMusicStream(MUSIC);
    SeekMusicStream(MUSIC, STATE->timePlayedSeconds);
    AttachAudioStreamProcessor(MUSIC.stream, fillSampleBuffer);
  }
  SetWindowPosition(STATE->window_pos.x, STATE->window_pos.y);
  return true;
}

State *getState(void) { return STATE; }

bool finished(void) { return STATE->finished; }

static void terminateInternal(void) {

  if (IsMusicReady(MUSIC)) {
    DetachAudioStreamProcessor(MUSIC.stream, fillSampleBuffer);
    UnloadMusicStream(MUSIC);
  }
  CloseAudioDevice();
  CloseWindow();
  pthread_mutex_destroy(&BUFFER_LOCK);
}

void terminate(void) {
  terminateInternal();

  free(STATE);
}

void pausePlugin(void) { terminateInternal(); }

bool reload() { return STATE->reload; }

void update(void) {

  // Main game loop
  if (WindowShouldClose()) // Detect window close button or ESC key
  {
    STATE->finished = true;
    return;
  }

  // Update
  //----------------------------------------------------------------------------------

  STATE->window_pos = GetWindowPosition();

  if (IsKeyPressed(KEY_R)) {
    // Reload plugins
    STATE->reload = true;
    return;
  }

  if (IsFileDropped()) {
    if (IsMusicReady(MUSIC))
      // Detach if already loaded
      DetachAudioStreamProcessor(MUSIC.stream, fillSampleBuffer);

    playMusicFromFile();

    CHANNELS = MUSIC.stream.channels;

    // Reset buffer
    FRAME_BUFFER_SIZE = 0;
    // Attach to new music stream
    AttachAudioStreamProcessor(MUSIC.stream, fillSampleBuffer);
  }

  if (IsMusicReady(MUSIC)) {
    UpdateMusicStream(MUSIC); // Update music buffer with new stream data

    // Restart music playing (stop and play)
    if (IsKeyPressed(KEY_SPACE)) {
      StopMusicStream(MUSIC);
      PlayMusicStream(MUSIC);
    }

    // Pause/Resume music playing
    if (IsKeyPressed(KEY_P)) {

      if (IsMusicStreamPlaying(MUSIC))
        PauseMusicStream(MUSIC);
      else
        ResumeMusicStream(MUSIC);
    }

    // Get normalized time played for current music stream
    STATE->timePlayedSeconds = GetMusicTimePlayed(MUSIC);
  }

  //----------------------------------------------------------------------------------

  // Draw
  //----------------------------------------------------------------------------------
  BeginDrawing();

  ClearBackground(BLACK);

  if (IsMusicReady(MUSIC)) {

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

const exports_t exports = {init,        update, getState, reload,
                           pausePlugin, resume, finished, terminate};
