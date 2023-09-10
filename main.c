#include <stdio.h>

#include <assert.h>
#include <pthread.h>
#include <raylib.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 450

#define FPS 60

#define ARRAY_LENGTH(x) (sizeof(x) / sizeof(x[0]))

static unsigned int SAMPLE_SIZE = 0;
static unsigned int CHANNELS = 0;

#define SAMPLE_BUFFER_MAX_SIZE (1024 * 10)
static int32_t SAMPLE_BUFFER[SAMPLE_BUFFER_MAX_SIZE] = {0};
static unsigned int SAMPLE_BUFFER_COUNT = 0;
#define SAMPLE_BUFFER_COUNT_BYTES (SAMPLE_BUFFER_COUNT * 4)
#define SAMPLE_BUFFER_CAPACITY                                                 \
  (((long)SAMPLE_BUFFER_MAX_SIZE - (long)SAMPLE_BUFFER_COUNT < 0)              \
       ? 0                                                                     \
       : (SAMPLE_BUFFER_MAX_SIZE - SAMPLE_BUFFER_COUNT))
#define SAMPLE_BUFFER_CAPACITY_BYTES (SAMPLE_BUFFER_CAPACITY * 4)
#define FRAMES_TO_SAMPLES(frames) (frames * CHANNELS)
#define SAMPLES_TO_FRAMES(frames) (frames / CHANNELS)
static pthread_mutex_t BUFFER_LOCK;

void fillSampleBuffer(void *buffer, unsigned int frames) {
  pthread_mutex_lock(&BUFFER_LOCK);
  unsigned int samples = FRAMES_TO_SAMPLES(frames);
  unsigned int bytes_available = (SAMPLE_SIZE / 8) * samples;
  unsigned int to_fill = (SAMPLE_BUFFER_CAPACITY_BYTES < bytes_available)
                             ? SAMPLE_BUFFER_CAPACITY_BYTES
                             : bytes_available;
  if (to_fill > 0) {
    memcpy(SAMPLE_BUFFER, buffer, to_fill);
    SAMPLE_BUFFER_COUNT = to_fill / (SAMPLE_SIZE / 8);
  }
  // printf("bytes_available %u\n", bytes_available);
  // printf("samples=%u\n", samples);
  // printf("to fill %u\n", to_fill);
  // printf("Buffer capacity bytes %u\n", SAMPLE_BUFFER_CAPACITY_BYTES);
  // printf("Buffer count bytes %u\n", SAMPLE_BUFFER_COUNT_BYTES);
  // printf("Buffer count  %u\n", SAMPLE_BUFFER_COUNT);
  // if (SAMPLE_BUFFER_COUNT >= SAMPLE_BUFFER_MAX_SIZE)
  //   exit(1);
  pthread_mutex_unlock(&BUFFER_LOCK);
}

void drawWave(void) {
  long step = 0;
  switch (SAMPLE_SIZE) {
  case 16:
    step = 1;
    break;
  case 32:
    step = 2;
    break;
  default:
    assert(false && "Sample size is not supported.");
  }

  printf("--------------- START SAMPLES -----------------\n");
  printf("Sample buffer count %u\n", SAMPLE_BUFFER_COUNT);
  pthread_mutex_lock(&BUFFER_LOCK);
  for (long i = 0; i < (long)SAMPLE_BUFFER_COUNT - (step - 1); i += step) {
    int32_t sample_1;
    int32_t sample_2;
    switch (SAMPLE_SIZE) {
    case 16:
      sample_1 = (SAMPLE_BUFFER[i] >> 16);
      sample_2 = SAMPLE_BUFFER[i] & (int32_t)65535;
      break;
    case 32:
      sample_1 = SAMPLE_BUFFER[i];
      sample_2 = SAMPLE_BUFFER[i + 1];
      break;
    default:
      assert(false && "Sample size is not supported.");
    }
    printf("(1 [%d], ", sample_1);
    printf("2 [%d]) ", sample_2);
  }
  printf("\n");
  SAMPLE_BUFFER_COUNT = 0;
  pthread_mutex_unlock(&BUFFER_LOCK);
  printf("--------------- END SAMPLES -----------------\n");
}

char *getFirstFile(FilePathList files) {
  assert(files.count > 0 && "No files found");
  return files.paths[0];
}

Music playMusicFromFile(void) {
  FilePathList files = LoadDroppedFiles();
  char *file_path = getFirstFile(files);
  Music music = LoadMusicStream(file_path);
  PlayMusicStream(music);
  UnloadDroppedFiles(files);
  AttachAudioStreamProcessor(music.stream, &fillSampleBuffer);
  printf("Frame count: %u\n", music.frameCount);
  printf("Sample rate: %u\n", music.stream.sampleRate);
  CHANNELS = music.stream.channels;
  assert(CHANNELS == 2 && "Does only support music with 2 channels.");
  SAMPLE_SIZE = music.stream.sampleSize;
  return music;
}

int main(void) {
  if (pthread_mutex_init(&BUFFER_LOCK, NULL) != 0) {
    printf("\n mutex init failed\n");
    return 1;
  }

  InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "musializer");
  InitAudioDevice();

  // Music that will be played
  Music music;
  // AttachAudioMixedProcessor(&fillSampleBuffer);

  float timePlayed = 0.0f; // Time played normalized [0.0f..1.0f]

  SetTargetFPS(FPS); // Set our game to run at 30 frames-per-second
  //--------------------------------------------------------------------------------------

  // Main game loop
  while (!WindowShouldClose()) // Detect window close button or ESC key
  {

    // Update
    //----------------------------------------------------------------------------------
    if (IsFileDropped()) {
      music = playMusicFromFile();
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

      drawWave();

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
