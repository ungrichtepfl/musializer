#include <dlfcn.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>

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

  if (pthread_mutex_init(plugins->BUFFER_LOCK, NULL) != 0) {
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
        DetachAudioStreamProcessor(music.stream, plugins->fillSampleBuffer);

      music = playMusicFromFile();

      *(plugins->CHANNELS) = music.stream.channels;

      // Reset buffer
      *(plugins->FRAME_BUFFER_SIZE) = 0;
      // Attach to new music stream
      AttachAudioStreamProcessor(music.stream, plugins->fillSampleBuffer);
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

      plugins->drawMusic();

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
  DetachAudioStreamProcessor(music.stream, plugins->fillSampleBuffer);
  UnloadMusicStream(music);
  CloseAudioDevice();
  CloseWindow();
  pthread_mutex_destroy(plugins->BUFFER_LOCK);
  dlclose(libplug);
  return 0;
}
