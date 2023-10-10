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

// https://pages.mtu.edu/~suits/NoteFreqCalcs.html
#define EQUAL_TEMPERED_FACTOR 1.059463094359

#define ARRAY_LENGTH(x) (sizeof(x) / sizeof(x[0]))

static unsigned int CHANNELS = 2;

typedef struct Frames {
  // TODO: Check which is left and which is right
  float left;
  float right;
} Frames;

#define FRAME_BUFFER_CAPACITY (2 << 13)
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

#define SMOOTHED_BUFFER_SIZE SCREEN_WIDTH

#define FFT_SIZE (2 * FRAME_BUFFER_CAPACITY)

#define max(a, b) (a > b ? a : b)

static Music MUSIC;

typedef struct State {
  bool finished;
  bool reload;
  float timePlayedSeconds;
  char musicFile[255];
  Vector2 window_pos;
  float max;
  bool use_wave;
} State;

static State *STATE;

typedef struct LinearColor_s {
  float r;
  float g;
  float b;
  float a;
} LinearColor;

static inline float color_u8_to_f32(const unsigned char x) { return x / 255.0; }

static inline unsigned char color_f32_to_u8(const float x) { return x * 255.0; }

static inline float to_linear(const unsigned char x) {
  const float f = color_u8_to_f32(x);
  if (f <= 0.04045)
    return f / 12.92;
  else
    return pow((f + 0.055) / 1.055, 2.4);
}

static inline LinearColor srgb_to_linear(const Color color) {
  return (LinearColor){
      .r = to_linear(color.r),
      .g = to_linear(color.g),
      .b = to_linear(color.b),
      .a = color_u8_to_f32(color.a),
  };
}

static inline unsigned char to_srgb(const float x) {
  const float f =
      (x <= 0.0031308) ? x * 12.92 : 1.055 * pow(x, 1.0 / 2.4) - 0.055;
  return color_f32_to_u8(f);
}

static LinearColor lerp_color(const LinearColor color1,
                              const LinearColor color2, const float t) {
  const float vec1[] = {color1.r, color1.g, color1.b, color1.a};
  const float vec2[] = {color2.r, color2.g, color2.b, color2.a};
  float res[] = {0, 0, 0, 0};
  for (int i = 0; i < 4; i++)
    res[i] = vec1[i] + (vec2[i] - vec1[i]) * t;
  return (LinearColor){
      .r = res[0],
      .g = res[1],
      .b = res[2],
      .a = res[3],
  };
}

static inline Color linear_to_srgb(const LinearColor color) {
  return (Color){.r = to_srgb(color.r),
                 .g = to_srgb(color.g),
                 .b = to_srgb(color.b),
                 .a = color_f32_to_u8(color.a)};
}

static Color lerp_color_gamma_corrected(const Color color1, const Color color2,
                                        const float t) {
  const LinearColor c1 = srgb_to_linear(color1);
  const LinearColor c2 = srgb_to_linear(color2);
  const LinearColor c = lerp_color(c1, c2, t);
  return linear_to_srgb(c);
}

static Color next_rainbow_color(int i, int n, bool reversed) {
  // RED
  // ORANGE
  // YELLOW
  // GREEN
  // BLUE
  // INDIGO
  // VIOLET
  if (reversed)
    i = n - 1 - i;

  const int buckets = n / 5;
  int start;
  int stop;
  Color startColor;
  Color stopColor;
  if (i < buckets) {
    start = 0;
    stop = buckets;
    startColor = RED;
    stopColor = ORANGE;
  } else if (i < 2 * buckets) {
    start = buckets;
    stop = 2 * buckets;
    startColor = ORANGE;
    stopColor = YELLOW;
  } else if (i < 3 * buckets) {
    start = 2 * buckets;
    stop = 3 * buckets;
    startColor = YELLOW;
    stopColor = GREEN;
  } else if (i < 4 * buckets) {
    start = 3 * buckets;
    stop = 4 * buckets;
    startColor = GREEN;
    stopColor = BLUE;
  } else {
    start = 4 * buckets;
    stop = n;
    startColor = BLUE;
    stopColor = PURPLE;
  }

  const float t = (float)(i - start) / (stop - start);
  return lerp_color_gamma_corrected(startColor, stopColor, t);
}

static bool lockBuffer(void) {
  int err;
  // Locking mutex
  if ((err = pthread_mutex_lock(&BUFFER_LOCK)) != 0) {
    printf("WARNING: Could not lock mutex: %s.", strerror(err));
    return false;
  }
  return true;
}

static bool tryLockBuffer(void) {
  int err;
  if ((err = pthread_mutex_trylock(&BUFFER_LOCK)) != 0) {
    if (err != EBUSY) {
      fprintf(stderr, "WARNING: Failed trying to lock mutex: %s.",
              strerror(err));
    }
    return false;
  }
  return true;
}

static void unlockBuffer(void) {
  int err;
  // Unlock mutex
  if ((err = pthread_mutex_unlock(&BUFFER_LOCK)) != 0) {
    fprintf(stderr, "FATAL: Could not unlock mutex. %s. Quitting program.",
            strerror(err));
    exit(EXIT_FAILURE); // panic
  }
}

static const char *getFirstFile(const FilePathList files) {
  assert(files.count > 0 && "No files found");
  return files.paths[0];
}

static void loadMusicFromFile(void) {
  const FilePathList files = LoadDroppedFiles();
  const char *file_path = getFirstFile(files);
  strcpy(STATE->musicFile, file_path);
  UnloadDroppedFiles(files);
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

static inline float hannWindow(float sample, unsigned int n, unsigned int N) {
  // Hann window: https://en.wikipedia.org/wiki/Window_function
  return 0.5 * (1 - cosf(2.0f * M_PI * n / N)) * sample;
}
static inline int nextFrequencyIndex(int k) {
  return (int)ceilf((float)k * EQUAL_TEMPERED_FACTOR);
}

static float SMOOTH_FREQUENCIES[SMOOTHED_BUFFER_SIZE] = {0};

static void drawFrequency(void) {

  if (FRAME_BUFFER_SIZE == 0)
    return; // Nothing to draw

  if (!lockBuffer())
    return;

  float samples[FFT_SIZE] = {0};
  float complex frequencies[FFT_SIZE];
  assert(FFT_SIZE >= FRAME_BUFFER_SIZE && "You need to increase the FFT_SIZE");
  for (unsigned int i = 0; i < FRAME_BUFFER_SIZE; ++i) {
    samples[i] = hannWindow(FRAME_BUFFER[i].left, i,
                            FRAME_BUFFER_SIZE); // only take left channel
  }

  unlockBuffer();

  int numFrequencyBuckets = 0;
  const int startIndex = 20;
  for (int k = startIndex; k < FFT_SIZE / 2; k = nextFrequencyIndex(k)) {
    ++numFrequencyBuckets;
  }

  const int w =
      numFrequencyBuckets > 0 ? SCREEN_WIDTH / numFrequencyBuckets : 1;

  // Compute FFT
  fft(samples, frequencies, FFT_SIZE);

  for (int i = 0, k = startIndex; i < numFrequencyBuckets && k < FFT_SIZE;
       ++i) {
    float f = 0;
    int n = 0;
    for (int j = k; j < nextFrequencyIndex(k); ++j) {
      f += cabsf(frequencies[j]);
      ++n;
    }

    f = logf(f / n);
    const float smoothFactor = 10.0f;

    SMOOTH_FREQUENCIES[i] +=
        (f - SMOOTH_FREQUENCIES[i]) * smoothFactor * GetFrameTime();

    if (SMOOTH_FREQUENCIES[i] < 0.0f)
      SMOOTH_FREQUENCIES[i] = 0.0f;

    f = SMOOTH_FREQUENCIES[i];

    const bool reversed_rainbow = true;
    const Color color =
        next_rainbow_color(i, numFrequencyBuckets, reversed_rainbow);

    STATE->max = max(STATE->max, f);
    const int h = (float)SCREEN_HEIGHT * f / STATE->max;
    const int x = w / 2 + i * w;
    const int radius = 5;
    const int lineWidth = 3;
    const float shrinkFactor = 0.9;
    for (int l = 0; l < lineWidth; ++l) {
      DrawLine(x - (l - lineWidth / 2), SCREEN_HEIGHT, x - (l - lineWidth / 2),
               SCREEN_HEIGHT - shrinkFactor * h, color);
    }
    DrawCircle(x, SCREEN_HEIGHT - shrinkFactor * h, radius, color);
    k = nextFrequencyIndex(k);
  }
}

static void drawWave(void) {
  if (FRAME_BUFFER_SIZE == 0)
    return; // Nothing to draw

  if (!lockBuffer())
    return;

  int j = 0;
  for (long i = FRAME_BUFFER_SIZE; i >= 0; --i) {
    if (j >= SCREEN_WIDTH)
      break;
    float sleft = FRAME_BUFFER[i].left;
    const float smoothFactor = 1.5f;
    SMOOTH_FREQUENCIES[j] +=
        (sleft - SMOOTH_FREQUENCIES[j]) * smoothFactor * GetFrameTime();

    sleft = SMOOTH_FREQUENCIES[j];

    STATE->max = max(STATE->max, sleft);
    sleft /= STATE->max;

    const bool reversed_rainbow = false;
    const Color color = next_rainbow_color(j, SCREEN_WIDTH, reversed_rainbow);
    const int lineWidth = 10;
    const float shrinkFactor = 0.5f;
    for (int l = 0; l < lineWidth; ++l) {
      const int diff = sleft * SCREEN_HEIGHT / 2;
      const int h = (float)SCREEN_HEIGHT / 2 - (l - (float)lineWidth / 2) +
                    shrinkFactor * diff;
      DrawPixel(j, h, color);
    }

    ++j;
  }

  unlockBuffer();
}

static void drawMusic(void) {
  if (FRAME_BUFFER_SIZE == 0) {
    return; // Nothing to draw
  }

  if (STATE->use_wave) {
    drawFrequency();
  } else {
    drawWave();
  }
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

  if (!tryLockBuffer())
    return;

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
  unlockBuffer();
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

#define STATE_MAX 0.01

bool init(void) {

  if (!initInternal()) {
    return false;
  }

  STATE = malloc(sizeof(State));

  // No need to initialize MUSIC otherwise it segfaults
  STATE->finished = false;
  STATE->reload = false;
  STATE->timePlayedSeconds = 0.0f;
  STATE->max = STATE_MAX;
  STATE->window_pos = GetWindowPosition();
  STATE->use_wave = false;

  return true;
}

static void startMusic(void) {
  STATE->max = STATE_MAX; // Start as if they are normalized

  if (!lockBuffer()) {
    exit(EXIT_FAILURE); // TODO: pass error to state
  }
  FRAME_BUFFER_SIZE = 0;
  unlockBuffer();

  if (strlen(STATE->musicFile) != 0) {
    MUSIC = LoadMusicStream(STATE->musicFile);
    CHANNELS = MUSIC.stream.channels;
    printf("Frame count: %u\n", MUSIC.frameCount);
    printf("Sample rate: %u\n", MUSIC.stream.sampleRate);
    printf("Frame size: %u\n", MUSIC.frameCount);
    PlayMusicStream(MUSIC);
    SeekMusicStream(MUSIC, STATE->timePlayedSeconds);
    AttachAudioStreamProcessor(MUSIC.stream, fillSampleBuffer);
  }
  // Reset filter
  for (int i = 0; i < SMOOTHED_BUFFER_SIZE; ++i) {
    SMOOTH_FREQUENCIES[i] = 0.0f;
  }
}

bool resume(State *state) {
  if (!initInternal()) {
    return false;
  }

  STATE = state;

  STATE->reload = false;
  startMusic();
  SetWindowPosition(STATE->window_pos.x, STATE->window_pos.y);
  return true;
}

State *getState(void) { return STATE; }

bool finished(void) { return STATE->finished; }

static void stopMusic(void) {
  if (IsMusicReady(MUSIC)) {
    DetachAudioStreamProcessor(MUSIC.stream, fillSampleBuffer);
    UnloadMusicStream(MUSIC);
  }
}

static void terminateInternal(void) {
  stopMusic();
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

  if (IsKeyPressed(KEY_W)) {
    STATE->use_wave = !STATE->use_wave;
    // Reset filter
    for (int i = 0; i < SMOOTHED_BUFFER_SIZE; ++i) {
      SMOOTH_FREQUENCIES[i] = 0.0f;
    }
    STATE->max = STATE_MAX;
  }

  if (IsFileDropped()) {
    stopMusic();
    loadMusicFromFile();
    STATE->timePlayedSeconds = 0.0f;
    startMusic();
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

      if (IsMusicStreamPlaying(MUSIC)) {
        PauseMusicStream(MUSIC);
      } else {
        // Reset filter
        for (int i = 0; i < SMOOTHED_BUFFER_SIZE; ++i) {
          SMOOTH_FREQUENCIES[i] = 0.0f;
        }
        STATE->max = STATE_MAX;
        ResumeMusicStream(MUSIC);
      }
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
