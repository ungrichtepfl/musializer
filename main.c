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

static unsigned int CHANNELS = 0;

typedef struct Frames {
  // TODO: Check which is left and which is right
  float left;
  float right;
} Frames;

#define FRAME_BUFFER_CAPACITY (1024 * 10)
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
  assert(CHANNELS == 2 && "Does only support music with 2 channels.");
  pthread_mutex_lock(&BUFFER_LOCK);
  unsigned int frames_to_fill =
      (frames > FRAME_BUFFER_CAPACITY) ? FRAME_BUFFER_CAPACITY : frames;
  memcpy(FRAME_BUFFER, buffer,
         frames_to_fill *
             sizeof(Frames)); // HACK: This only works for two channel audio
                              //  because Frames contains 2 floats.
  FRAME_BUFFER_SIZE = frames_to_fill;
  pthread_mutex_unlock(&BUFFER_LOCK);
}

void drawWave(void) {

  pthread_mutex_lock(&BUFFER_LOCK);
  for (unsigned int i = 0; i < FRAME_BUFFER_SIZE; ++i) {
    float sleft = FRAME_BUFFER[i].left;
    float sright = FRAME_BUFFER[i].right;
    if (sleft > 0)
      DrawRectangle(i, GetScreenHeight() / 2, 1, sleft * GetScreenHeight() / 2,
                    RED);
    else
      DrawRectangle(
          i, (float)GetScreenHeight() / 2 + sleft * GetScreenHeight() / 2, 1,
          -sleft * GetScreenHeight() / 2, RED);
  }
  FRAME_BUFFER_SIZE = 0;
  pthread_mutex_unlock(&BUFFER_LOCK);
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
  printf("Frame size: %u\n", music.frameCount);
  CHANNELS = music.stream.channels;
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
