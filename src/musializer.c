#include <complex.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <raylib.h>

#ifndef _WIN32
#include <signal.h> // needed for sigaction()
#endif              // _WIN32

#include "./hotreload.h"

int main(int argc, char **argv) {
  FilePathList init_tracks = {0};
  if (argc == 2) {
    FILE *fptr = fopen(argv[1], "r");
    if (fptr == NULL) {
      printf("ERROR: Unable to open file %s\n", argv[1]);
      goto INVALID_PLAYLIST;
    }
    printf("INFO: Opened file %s\n", argv[1]);
    int ct = 0, max_len = 0, curr_len = 0;
    bool reading = false;
    char c = 'x';
    while (fread(&c, 1, 1, fptr)) {
      if (reading && c == '[') {
        printf("ERROR: Encountered '[' while reading track no. %d\n", ct + 1);
        goto INVALID_PLAYLIST;
      } else if (c == '[') {
        reading = true;
        continue;
      }
      if (reading && c == ']') {
        max_len = (max_len < curr_len ? curr_len : max_len);
        reading = false;
        curr_len = 0;
        ct++;
      } else if (reading)
        curr_len++;
    }
    fseek(fptr, 0, SEEK_SET);
    init_tracks.capacity = init_tracks.count = ct;
    init_tracks.paths = malloc(sizeof(char *) * ct);
    for (int i = 0; i < ct; i++) {
      init_tracks.paths[i] = malloc(sizeof(char) * max_len);
      c = ' ';
      while (c != '[')
        fread(&c, 1, 1, fptr);

      fread(&c, 1, 1, fptr);
      curr_len = 0;
      while (c != ']') {
        curr_len++;
        fread(&c, 1, 1, fptr);
      }
      fseek(fptr, -curr_len - 1, SEEK_CUR);
      fread(init_tracks.paths[i], sizeof(char), curr_len, fptr);
      printf("INFO: Track path %s read from playlist\n", init_tracks.paths[i]);
      fread(&c, 1, 1, fptr);
    }
    goto PLAYLIST_END;
  INVALID_PLAYLIST:
    printf("ERROR: Invalid playlist provided\n");
    if (fptr)
      fclose(fptr);
  } else if (argc > 2) {
    printf("Usage : %s [playlist filepath]\n", argv[0]);
    return 1;
  }
PLAYLIST_END:
#ifndef _WIN32
  // NOTE: This is needed because if the pipe between Musializer and FFmpeg
  // breaks Musializer will receive SIGPIPE on trying to write into it. While
  // such behavior makes sense for command line utilities, Musializer is a
  // relatively friendly GUI application that is trying to recover from such
  // situations.
  struct sigaction act = {0};
  act.sa_handler = SIG_IGN;
  sigaction(SIGPIPE, &act, NULL);
#endif // _WIN32

  if (!reload_libplug())
    return 1;

  SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_ALWAYS_RUN);
  size_t factor = 80;
  InitWindow(factor * 16, factor * 9, "Musializer");
  {
    const char *file_path = "./resources/logo/logo-256.png";
    size_t data_size;
    void *data = plug_load_resource(file_path, &data_size);
    Image logo =
        LoadImageFromMemory(GetFileExtension(file_path), data, data_size);
    SetWindowIcon(logo);
    plug_free_resource(data);
  }
  SetExitKey(KEY_NULL);
  InitAudioDevice();

  plug_init(init_tracks);
  while (!WindowShouldClose()) {
    if (IsKeyPressed(KEY_H)) {
      void *state = plug_pre_reload();
      if (!reload_libplug())
        return 1;
      plug_post_reload(state);
    }
    plug_update();
  }

  CloseAudioDevice();
  CloseWindow();

  return 0;
}
