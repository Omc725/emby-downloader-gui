# emby-downloader-gui

Native cross-platform desktop graphical user interface (GUI) for Emby media discovery, bulk downloading, and stream extraction. Built with C++17, GTK 3, and `emby-downloader-core`.

## Features

- **Native Desktop Toolkit:** No web browser engine, no Electron, and no Python. Compiles directly into a lightweight native binary using system widget renderers.
- **Asynchronous Poster Visualization:** Decodes and caches media posters locally (`~/.cache/emby-downloader/posters/`) using non-blocking background workers.
- **Bulk Item Selection:** Search movies and series by name, IMDb ID (`tt...`), or TMDb ID with batch checkbox selection.
- **Advanced Series, Season, and Episode Filtering:**
  - Entire series (all seasons) or targeted single season.
  - First episode of every season (`S*E01`).
  - Season finale of every season.
  - Specific episodes across seasons (e.g., episodes 3 and 4 of each season).
  - Custom episode index ranges (e.g., `1-3, 5, 8`).
- **Selective Stream Acquisition:**
  - Direct video stream (bit-for-bit, zero transcoding).
  - Audio track extraction via stream copy (`-c:a copy`) with language filtering.
  - Subtitle track extraction with language filtering.
- **Live Download Queue:**
  - Real-time progress percentage, transfer speed, and remaining time (ETA).
  - Pause, resume, and clear queue controls.
  - Direct output directory opener.
- **Server Profile Management:** Store and switch between multiple Emby server configurations.

## Dependencies

- C++17 compiler (GCC >= 9, Clang >= 10)
- `gtk+-3.0` development headers (`libgtk-3-dev`)
- `libcurl` development package
- `pthread`
- `emby-downloader-core` (vendored or built alongside in `../emby-downloader-core`)
- `ffmpeg` binary (runtime requirement for audio track extraction)

## Build Instructions

### Using Make

```bash
make
./emby-downloader-gui
```

### Using CMake

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
./emby-downloader-gui
```

## License

MIT License. See `LICENSE` for details.
