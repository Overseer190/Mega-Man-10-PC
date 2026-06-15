# Mega Man 10 Remake

A Windows game project built with **SDL3**, **BASS/BASSMIDI 2.4**, and
**MinGW-w64** (C++17).

---

## Building

```bash
build.bat       # one step: compile + copy DLLs + assets to out/
make            # compile only
make run        # build + run
make clean      # remove build artifacts
```

You need **MinGW-w64 (x86_64, POSIX, SEH)** — get it from
[winlibs.com](https://winlibs.com/). Add its `bin/` to your `PATH`.

### What `build.bat` does

1. Runs `make` → produces `out/megaman10.exe`
2. Copies `SDL3.dll`, `bass.dll`, `bassmidi.dll`, SoundFont, MIDI, and
   WAV assets into `out/`

After building:
```bash
out\megaman10.exe
out\megaman10.exe --test   # headless test (no window)
```

---

## Controls (current test state)

| Key     | Action              |
|---------|---------------------|
| `M`     | Play / Stop MIDI    |
| `Space` | Pause / Resume MIDI |
| `W`     | Play sound effect   |
| `ESC`   | Quit                |

---

## Project layout

```
.
├── SDL3-3.4.10/            # SDL3 devel kit (mingw)
├── bass24/                 # BASS 2.4 — c/bass.h, c/x64/bass.{lib,dll}
├── bassmidi24/             # BASSMIDI 2.4 — c/bassmidi.h, c/x64/bassmidi.{lib,dll}
├── src/
│   ├── main.cpp            # Entry: window, game loop, audio feed, controls, SFX
│   ├── midi_player.h       # Public API: Init/Load/Play/Pause/Stop/Render
│   ├── midi_player.cpp     # BASSMIDI wrapper (decode stream, marker looping)
│   ├── Music/MIDI/         # SoundFonts + MIDI files
│   └── SFX/                # WAV sound effects
├── out/                    # Build output (exe + DLLs + assets)
├── build.bat
├── Makefile
└── README.md
```

`src/` uses `$(wildcard src/*.cpp)` so new files link automatically.

---

## Code walkthrough

### `src/main.cpp` — entry point

- Creates an SDL3 window + renderer
- Opens the default audio device and binds an audio stream (S16, 44100 Hz,
  stereo)
- Runs a fixed-step game loop at ~60 FPS (throttled via `SDL_GetTicks`)
- Each frame: pumps events → renders MIDI → mixes SFX → pushes to stream
- `--test` runs a headless MIDI render test and exits

### `src/midi_player.h` / `src/midi_player.cpp` — BASSMIDI wrapper

Key design choices:

| Choice | Why |
|--------|-----|
| **BASSMIDI** over TinySoundFont | Built-in MIDI parser, multi-track sync, SF2 rendering, marker support |
| **Decode stream** (`BASS_STREAM_DECODE`) | We pull PCM on our schedule, not BASS's; avoids threading issues |
| **Manual play/pause flags** | Decode streams ignore `BASS_ChannelPause` at the API level — we track `g_playing` / `g_paused` ourselves |
| **Marker looping** | Detects `loopStart` / `loopEnd` markers from the MIDI file; on stream end, seeks back with `BASS_MIDI_DECAYSEEK` for smooth note decay |

### SFX — short WAV playback

Sound effects are loaded once via `SDL_LoadWAV` and converted to
S16/44100/stereo by `SDL_ConvertAudioSamples` (any source format works).

On **W** press, `g_sfx_playing` is set and the sample offset resets to 0.
Each frame, `mix_sfx()` blends the SFX samples into the MIDI buffer before it
is sent to the audio device:

- **Sample-level mixing** — stereo channels are added with clamp-to-range
  to prevent overflow.
- **Real-time rate limiting** — SFX position advances only by the
  elapsed-time frame count (`time_based_frames`), not the possibly-inflated
  buffer count (`buf_frames`). This prevents the queue-management logic
  from consuming short effects faster than real time.
- **Correct sample/frame accounting** — `g_sfx_offset` is a **sample**
  index (incremented per left/right channel), compared against
  `g_sfx_nsamples` (total samples), not the frame count. This avoids
  cutting playback at the halfway point.

### Build system

- **`Makefile`**: `g++ -std=c++17 -Wall -Wextra -O2`, links SDL3, BASS,
  BASSMIDI
- **`build.bat`**: calls `make`, then copies DLLs + assets to `out/`
- Assets resolved relative to the exe directory via `SDL_GetBasePath()`

---

## What's next (placeholder → real game)

Everything in `src/` right now is scaffolding. The MIDI player and SFX
system are test code — the final game will wrap them behind a proper
audio manager. Expected additions:

- Sprite rendering / animation
- Tile maps and level loading
- Player movement and collision
- Game states (title, gameplay, pause, game over)

---

## Dependencies

| Library | Role | License |
|---------|------|---------|
| SDL3 | Windowing, input, audio stream | zlib |
| BASS 2.4 | Audio backend | Proprietary (freeware) |
| BASSMIDI 2.4 | MIDI/SF2 synthesis | Proprietary (freeware) |

SDL3, BASS, and BASSMIDI are included in the repo — no downloads needed
beyond MinGW-w64. See [un4seen.com](https://www.un4seen.com/) for BASS
licensing.
