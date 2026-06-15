// ============================================================================
// main.cpp - MegaMan 10 Remake — SDL3 bootstrap (C++)
// ============================================================================
// Subsystems:
//   1. VIDEO  — SDL3 window + renderer (hardware-accelerated 2D)
//   2. AUDIO  — SDL3 audio playback device + stream
//   3. MIDI   — BASSMIDI software synthesizer (.sf2)
//   4. SFX    — Short WAV playback mixed into the audio feed
//
// Controls:
//   ESC      Quit
//   M        Start / stop MIDI playback
//   Space    Pause / resume MIDI
//   W        Play a sound effect (one-shot)
// ============================================================================

#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test_font.h>

#include "midi_player.h"

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

#define WINDOW_TITLE    "MegaMan 10 Remake"
#define SCREEN_WIDTH    640
#define SCREEN_HEIGHT   480

// ---------------------------------------------------------------------------
// SDL3 — Video
// ---------------------------------------------------------------------------

static SDL_Window   *g_window   = NULL;
static SDL_Renderer *g_renderer = NULL;

static bool init_video(void)
{
    g_window = SDL_CreateWindow(WINDOW_TITLE, SCREEN_WIDTH, SCREEN_HEIGHT,
                                SDL_WINDOW_RESIZABLE);
    if (!g_window) {
        fprintf(stderr, "[VIDEO] SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }

    g_renderer = SDL_CreateRenderer(g_window, NULL);
    if (!g_renderer) {
        fprintf(stderr, "[VIDEO] SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(g_window);
        g_window = NULL;
        return false;
    }

    printf("[VIDEO] Window + Renderer created (%dx%d)\n", SCREEN_WIDTH, SCREEN_HEIGHT);
    return true;
}

static void destroy_video(void)
{
    if (g_renderer) { SDL_DestroyRenderer(g_renderer); g_renderer = NULL; }
    if (g_window)   { SDL_DestroyWindow(g_window);     g_window   = NULL; }
}

// ---------------------------------------------------------------------------
// SDL3 — Audio (MIDI stream)
// ---------------------------------------------------------------------------

static SDL_AudioDeviceID g_audio_dev_id  = 0;
static SDL_AudioStream   *g_audio_stream = NULL;

static bool init_audio(void)
{
    SDL_AudioSpec spec;
    SDL_zero(spec);

    g_audio_dev_id = SDL_OpenAudioDevice(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec);
    if (g_audio_dev_id == 0) {
        fprintf(stderr, "[AUDIO] SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
        return false;
    }
    printf("[AUDIO] Opened device — freq=%d fmt=0x%x ch=%d\n",
           spec.freq, spec.format, spec.channels);

    SDL_AudioSpec src_spec;
    SDL_zero(src_spec);
    src_spec.format   = SDL_AUDIO_S16;
    src_spec.channels = 2;
    src_spec.freq     = 44100;

    g_audio_stream = SDL_CreateAudioStream(&src_spec, NULL);
    if (!g_audio_stream) {
        fprintf(stderr, "[AUDIO] SDL_CreateAudioStream failed: %s\n", SDL_GetError());
        SDL_CloseAudioDevice(g_audio_dev_id);
        g_audio_dev_id = 0;
        return false;
    }

    if (!SDL_BindAudioStream(g_audio_dev_id, g_audio_stream)) {
        fprintf(stderr, "[AUDIO] SDL_BindAudioStream failed: %s\n", SDL_GetError());
        SDL_DestroyAudioStream(g_audio_stream);
        g_audio_stream = NULL;
        SDL_CloseAudioDevice(g_audio_dev_id);
        g_audio_dev_id = 0;
        return false;
    }
    printf("[AUDIO] Stream bound to device (input: S16/44100/stereo)\n");
    return true;
}

static void destroy_audio(void)
{
    if (g_audio_stream) {
        SDL_UnbindAudioStream(g_audio_stream);
        SDL_DestroyAudioStream(g_audio_stream);
        g_audio_stream = NULL;
    }
    if (g_audio_dev_id) {
        SDL_CloseAudioDevice(g_audio_dev_id);
        g_audio_dev_id = 0;
    }
}

// Forward declaration — mix_sfx() is defined in the SFX section below.
static void mix_sfx(short *buf, int buf_frames, int limit_frames);

// ---------------------------------------------------------------------------
// Audio feed — called every frame to keep the audio buffer filled
// ---------------------------------------------------------------------------
// BASSMIDI uses a decode stream (no automatic playback), so we pull PCM
// data here and push it into SDL's audio stream.
//
// The amount decoded is based on elapsed wall-clock time since the last
// frame, smoothed with a minimum queue threshold to avoid underruns.
//
// IMPORTANT: SFX position must advance at real-time rate (time_based_frames),
// NOT the possibly-inflated buf_frames, otherwise short effects finish
// too quickly.
// ---------------------------------------------------------------------------

static Uint64 g_last_audio_tick = 0;

static void feed_audio(void)
{
    if (!g_audio_stream) return;

    if (g_last_audio_tick == 0) {
        g_last_audio_tick = SDL_GetTicks();
        return;
    }

    Uint64 now = SDL_GetTicks();
    double elapsed = (now - g_last_audio_tick) / 1000.0;
    g_last_audio_tick = now;

    int time_based_frames = (int)(elapsed * 44100.0 + 0.5);
    if (time_based_frames < 1) time_based_frames = 1;
    if (time_based_frames > 8192) time_based_frames = 8192;

    int buf_frames = time_based_frames;
    int queued_b = SDL_GetAudioStreamQueued(g_audio_stream);
    int queued_frames = queued_b / (2 * (int)sizeof(short));
    int min_frames = 2048;
    if (queued_frames + buf_frames < min_frames)
        buf_frames = min_frames - queued_frames;

    short buf[8192 * 2];
    MidiPlayer_Render(buf, buf_frames, 44100);
    mix_sfx(buf, buf_frames, time_based_frames);
    SDL_PutAudioStreamData(g_audio_stream, buf,
                           buf_frames * 2 * (int)sizeof(short));
}

// ---------------------------------------------------------------------------
// SFX — Short WAV sound effects played on-demand
// ---------------------------------------------------------------------------
// The WAV is loaded once and converted to S16/44100/stereo.  On each frame,
// mix_sfx() blends any active SFX samples into the MIDI buffer before it
// is sent to the audio device.
// ---------------------------------------------------------------------------

static short   *g_sfx_data      = NULL;   // converted PCM (interleaved S16)
static Uint32   g_sfx_nsamples  = 0;      // total sample count (= frames × 2)
static bool     g_sfx_playing   = false;  // currently active?
static Uint32   g_sfx_offset    = 0;      // next sample to mix

// Load a .wav file, convert to S16/44100/stereo, and store in g_sfx_data.
static bool init_sfx(const char *wav_path)
{
    SDL_AudioSpec spec;
    Uint8 *raw = NULL;
    Uint32 raw_len = 0;

    if (!SDL_LoadWAV(wav_path, &spec, &raw, &raw_len)) {
        fprintf(stderr, "[SFX] LoadWAV failed: %s\n", SDL_GetError());
        return false;
    }

    // Target format: S16, 44100 Hz, stereo
    SDL_AudioSpec dst_spec;
    SDL_zero(dst_spec);
    dst_spec.format   = SDL_AUDIO_S16;
    dst_spec.channels = 2;
    dst_spec.freq     = 44100;

    Uint8 *converted = NULL;
    int converted_len = 0;
    if (!SDL_ConvertAudioSamples(&spec, raw, (int)raw_len,
                                 &dst_spec, &converted, &converted_len)) {
        fprintf(stderr, "[SFX] Convert failed: %s\n", SDL_GetError());
        SDL_free(raw);
        return false;
    }
    SDL_free(raw);

    g_sfx_data    = (short *)converted;
    g_sfx_nsamples = (Uint32)(converted_len / (int)sizeof(short));

    printf("[SFX] Loaded: %s  (%u samples = %u frames)\n",
           wav_path, g_sfx_nsamples, g_sfx_nsamples / 2);
    return true;
}

static void destroy_sfx(void)
{
    SDL_free(g_sfx_data);
    g_sfx_data    = NULL;
    g_sfx_nsamples = 0;
    g_sfx_playing = false;
    g_sfx_offset  = 0;
}

// Mix active SFX into the stereo-S16 buffer (in-place).
// limit_frames is the number of frames to advance the SFX offset (based on
// real elapsed time), clamped to buf_frames so we never write past the end.
static void mix_sfx(short *buf, int buf_frames, int limit_frames)
{
    if (!g_sfx_playing || !g_sfx_data) return;

    if (limit_frames > buf_frames) limit_frames = buf_frames;

    for (int i = 0; i < limit_frames; i++) {
        for (int ch = 0; ch < 2; ch++) {
            if (g_sfx_offset >= g_sfx_nsamples) break;
            int mixed = (int)buf[i * 2 + ch] + (int)g_sfx_data[g_sfx_offset];
            if (mixed >  32767) mixed =  32767;
            if (mixed < -32768) mixed = -32768;
            buf[i * 2 + ch] = (short)mixed;
            g_sfx_offset++;
        }
        if (g_sfx_offset >= g_sfx_nsamples)
            break;
    }

    if (g_sfx_offset >= g_sfx_nsamples) {
        g_sfx_playing = false;
        g_sfx_offset  = 0;
    }
}

// ---------------------------------------------------------------------------
// Controls overlay text
// ---------------------------------------------------------------------------

static void draw_overlay(void)
{
    char lines[8][64];
    int  num_lines = 0;

    strcpy(lines[num_lines++], "[ Controls ]");
    strcpy(lines[num_lines++], "  M     Play / Stop");
    strcpy(lines[num_lines++], "  Space Pause / Resume");
    strcpy(lines[num_lines++], "  W     Sound effect");
    strcpy(lines[num_lines++], "  ESC   Quit");

    const char *status = "Stopped";
    if (MidiPlayer_IsPlaying())      status = "Playing...";
    else if (MidiPlayer_IsPaused())  status = "Paused";
    snprintf(lines[num_lines], sizeof(lines[num_lines]),
             "MIDI: %s", status);
    num_lines++;

    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 160);
    SDL_FRect bg_rect = { 4, 4, 210, (float)(num_lines * 18 + 8) };
    SDL_RenderFillRect(g_renderer, &bg_rect);

    SDL_SetRenderDrawColor(g_renderer, 255, 255, 255, 255);
    float tx = 10.0f, ty = 10.0f;
    for (int i = 0; i < num_lines; i++) {
        SDLTest_DrawString(g_renderer, tx, ty, lines[i]);
        ty += 18.0f;
    }
}

// ---------------------------------------------------------------------------
// Game loop
// ---------------------------------------------------------------------------

static bool process_events(void)
{
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {

        case SDL_EVENT_QUIT:
            return false;

        case SDL_EVENT_KEY_DOWN:
            if (ev.key.repeat) break; // ignore key-repeat
            switch (ev.key.key) {
            case SDLK_ESCAPE:
                return false;

            case SDLK_M:
                if (MidiPlayer_IsPlaying() || MidiPlayer_IsPaused())
                    MidiPlayer_Stop();
                else
                    MidiPlayer_Play(true);
                break;

            case SDLK_SPACE:
                if (MidiPlayer_IsPlaying())
                    MidiPlayer_Pause();
                else if (MidiPlayer_IsPaused())
                    MidiPlayer_Resume();
                break;

            case SDLK_W:
                if (g_sfx_data) {
                    g_sfx_playing = true;
                    g_sfx_offset  = 0;
                }
                break;

            default:
                break;
            }

        default:
            break;
        }
    }
    return true;
}

static void render_frame(void)
{
    SDL_SetRenderDrawColor(g_renderer, 16, 24, 48, 255);
    SDL_RenderClear(g_renderer);

    draw_overlay();

    SDL_RenderPresent(g_renderer);
}

// ---------------------------------------------------------------------------
// Headless test
// ---------------------------------------------------------------------------

static int run_headless_test(void)
{
    char sf2_path[1024], mid_path[1024];
    {
        const char *base = SDL_GetBasePath();
        if (!base) base = ".";
        SDL_snprintf(sf2_path, sizeof(sf2_path), "%sMM10.sf2", base);
        SDL_snprintf(mid_path, sizeof(mid_path), "%s14_StrikeMan.mid", base);
    }

    fprintf(stderr, "\n=== HEADLESS MIDI TEST ===\n");
    fprintf(stderr, "SF2: %s\n", sf2_path);

    if (!MidiPlayer_Init(sf2_path)) {
        fprintf(stderr, "FAIL: MidiPlayer_Init\n");
        return 1;
    }
    fprintf(stderr, "PASS: MidiPlayer_Init\n");

    if (!MidiPlayer_Load(mid_path)) {
        fprintf(stderr, "FAIL: MidiPlayer_Load\n");
        MidiPlayer_Quit();
        return 1;
    }
    fprintf(stderr, "PASS: MidiPlayer_Load\n");

    MidiPlayer_Play(true);
    fprintf(stderr, "PASS: Play started\n");

    short buf[44100 * 2];
    memset(buf, 0, sizeof(buf));
    MidiPlayer_Render(buf, 44100, 44100);

    int non_zero = 0;
    for (int i = 0; i < 44100 * 2; i++) {
        if (buf[i] != 0) { non_zero = 1; break; }
    }
    fprintf(stderr, "Render test (1s): non_zero=%d\n", non_zero);

    MidiPlayer_Quit();
    fprintf(stderr, "=== TEST DONE ===\n");
    return non_zero ? 0 : 1;
}

// ---------------------------------------------------------------------------
// main()
// ---------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    if (argc >= 2 && strcmp(argv[1], "--test") == 0)
        return run_headless_test();

    SDL_SetAppMetadata("MegaMan 10 Remake", "0.1", "com.example.megaman10");

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        fprintf(stderr, "[INIT] SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    printf("[INIT] SDL3 subsystems initialised\n");

    char sf2_path[1024], mid_path[1024], sfx_path[1024];
    {
        const char *base = SDL_GetBasePath();
        if (!base) base = ".";
        SDL_snprintf(sf2_path, sizeof(sf2_path), "%sMM10.sf2", base);
        SDL_snprintf(mid_path, sizeof(mid_path), "%s14_StrikeMan.mid", base);
        SDL_snprintf(sfx_path, sizeof(sfx_path), "%sSoundEffect.wav", base);
        printf("[INIT] Asset path: %s\n", base);
    }

    if (!MidiPlayer_Init(sf2_path)) {
        fprintf(stderr, "[INIT] MidiPlayer_Init failed\n");
        SDL_Quit();
        return 1;
    }
    printf("[INIT] MIDI player ready\n");

    if (!MidiPlayer_Load(mid_path))
        fprintf(stderr, "[INIT] Failed to load MIDI\n");

    init_sfx(sfx_path);

    if (!init_video()) {
        MidiPlayer_Quit();
        SDL_Quit();
        return 1;
    }

    init_audio();

    printf("[MAIN] Auto-starting StrikeMan theme...\n");
    MidiPlayer_Play(true);

    printf("[MAIN] Entering game loop (ESC to quit)\n");

    bool running = true;
    while (running) {
        running = process_events();
        feed_audio();
        render_frame();
        SDL_Delay(16);
    }

    printf("[MAIN] Shutting down...\n");
    destroy_audio();
    destroy_video();
    destroy_sfx();
    MidiPlayer_Quit();
    SDL_Quit();
    printf("[MAIN] Goodbye.\n");
    return 0;
}
