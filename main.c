#include <stdio.h>

#include <assert.h>
#include <raylib.h>

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 450

#define FPS 60

char *getFirstFile(FilePathList files) {
  assert(files.count > 0 && "No files found");
  return files.paths[0];
}

int main(void) {

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

      FilePathList files = LoadDroppedFiles();
      char *file_path = getFirstFile(files);
      music = LoadMusicStream(file_path);
      PlayMusicStream(music);
      UnloadDroppedFiles(files);
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

      DrawText("MUSIC SHOULD BE PLAYING!", 255, 150, 20, WHITE);

      DrawRectangle(200, 200, 400, 12, WHITE);
      DrawRectangle(200, 200, (int)(timePlayed * 400.0f), 12, RED);
      DrawRectangleLines(200, 200, 400, 12, LIGHTGRAY);

      DrawText("PRESS SPACE TO RESTART MUSIC", 215, 250, 20, WHITE);
      DrawText("PRESS P TO PAUSE/RESUME MUSIC", 208, 280, 20, WHITE);
    } else {
      DrawText("DRAG AND DROP A MUSIC FILE", 235, 200, 20, WHITE);
    }

    EndDrawing();
    //----------------------------------------------------------------------------------
  }
  // Clean up
  UnloadMusicStream(music);
  CloseAudioDevice();
  CloseWindow();
  return 0;
}
