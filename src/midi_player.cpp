#include "midi_player.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "bass.h"
#include "bassmidi.h"

// ---------------------------------------------------------------------------
// Internal state
// ---------------------------------------------------------------------------
// We track play/pause with manual flags instead of relying on
// BASS_ChannelIsActive because decode streams don't report paused state.
// ---------------------------------------------------------------------------

static HSTREAM    g_stream     = 0;
static HSOUNDFONT g_sfont      = 0;
static bool       g_loaded     = false;
static char       g_filename[512] = {0};
static QWORD      g_loop_start = 0;
static bool       g_loop_on    = false;
static bool       g_playing    = false;
static bool       g_paused     = false;

// ---------------------------------------------------------------------------
// Startup / shutdown
// ---------------------------------------------------------------------------

bool MidiPlayer_Init(const char *sf2_path)
{
    if (!BASS_Init(-1, 44100, 0, NULL, NULL)) {
        fprintf(stderr, "[MIDI] BASS_Init failed (error %d)\n", BASS_ErrorGetCode());
        return false;
    }

    g_sfont = BASS_MIDI_FontInit(sf2_path, BASS_MIDI_FONT_NOFX);
    if (!g_sfont) {
        fprintf(stderr, "[MIDI] Failed to load SoundFont: %s (error %d)\n",
                sf2_path, BASS_ErrorGetCode());
        BASS_Free();
        return false;
    }

    BASS_MIDI_FONT sf;
    sf.font = g_sfont;
    sf.preset = -1;
    sf.bank = 0;
    BASS_MIDI_StreamSetFonts(0, &sf, 1);

    printf("[MIDI] BASSMIDI ready — SoundFont loaded: %s\n", sf2_path);
    return true;
}

void MidiPlayer_Quit(void)
{
    if (g_stream) BASS_StreamFree(g_stream);
    if (g_sfont)  BASS_MIDI_FontFree(g_sfont);
    BASS_Free();
    g_stream = 0;
    g_sfont  = 0;
    g_loaded = false;
    g_playing = false;
    g_paused = false;
    g_filename[0] = '\0';
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Scan MIDI markers for a named marker (e.g. "loopStart") and return its
// byte position, or 0 if not found.
static QWORD find_marker(HSTREAM handle, const char *text)
{
    BASS_MIDI_MARK mark;
    for (int a = 0; BASS_MIDI_StreamGetMark(handle, BASS_MIDI_MARK_MARKER, a, &mark); a++)
        if (mark.text && strcmp(mark.text, text) == 0)
            return mark.pos;
    return 0;
}

// ---------------------------------------------------------------------------
// Loading / unloading MIDI files
// ---------------------------------------------------------------------------

bool MidiPlayer_Load(const char *mid_path)
{
    if (g_stream) BASS_StreamFree(g_stream);

    g_stream = BASS_MIDI_StreamCreateFile(FALSE, mid_path, 0, 0,
        BASS_STREAM_DECODE | BASS_MIDI_NOFX, 44100);
    if (!g_stream) {
        fprintf(stderr, "[MIDI] Failed to load MIDI: %s (error %d)\n",
                mid_path, BASS_ErrorGetCode());
        g_loaded = false;
        return false;
    }

    g_loop_start = find_marker(g_stream, "loopStart");

    strncpy(g_filename, mid_path, sizeof(g_filename) - 1);
    g_filename[sizeof(g_filename) - 1] = '\0';
    g_loaded = true;

    printf("[MIDI] Loaded: %s\n", mid_path);
    return true;
}

void MidiPlayer_Free(void)
{
    if (g_stream) BASS_StreamFree(g_stream);
    g_stream = 0;
    g_loaded = false;
    g_playing = false;
    g_paused = false;
    g_filename[0] = '\0';
}

// ---------------------------------------------------------------------------
// Playback control
// ---------------------------------------------------------------------------

void MidiPlayer_Play(bool loop)
{
    if (!g_stream) return;

    BASS_ChannelStop(g_stream);
    BASS_ChannelSetPosition(g_stream, 0, BASS_POS_BYTE);

    g_loop_on = loop;
    g_playing = true;
    g_paused = false;
    if (loop && !g_loop_start)
        g_loop_start = 0;
}

void MidiPlayer_Pause(void)
{
    if (g_stream) {
        g_paused = true;
    }
}

void MidiPlayer_Resume(void)
{
    if (g_stream) {
        g_paused = false;
    }
}

void MidiPlayer_Stop(void)
{
    if (g_stream) {
        BASS_ChannelStop(g_stream);
        BASS_ChannelSetPosition(g_stream, 0, BASS_POS_BYTE);
        g_playing = false;
        g_paused = false;
    }
}

bool MidiPlayer_IsPlaying(void)
{
    return g_playing && !g_paused;
}

bool MidiPlayer_IsPaused(void)
{
    return g_playing && g_paused;
}

// ---------------------------------------------------------------------------
// PCM rendering
// ---------------------------------------------------------------------------
// BASS decode streams don't pull themselves — the game loop must call
// Render every frame to advance playback.  When the stream ends (GetData
// returns (DWORD)-1) we either loop back or stop.
// ---------------------------------------------------------------------------

void MidiPlayer_Render(short *buffer, int num_samples, int)
{
    if (!g_stream || !g_playing || g_paused) {
        memset(buffer, 0, (size_t)num_samples * 2 * sizeof(short));
        return;
    }

    DWORD len = (DWORD)num_samples * 2 * (DWORD)sizeof(short);
    char *pos = (char *)buffer;
    DWORD remain = len;

    while (remain > 0) {
        DWORD got = BASS_ChannelGetData(g_stream, pos, remain);
        if (got == (DWORD)-1) {
            if (g_loop_on) {
                BASS_ChannelSetPosition(g_stream, g_loop_start,
                    BASS_POS_BYTE | BASS_MIDI_DECAYSEEK);
            } else {
                g_playing = false;
                break;
            }
        } else {
            pos += got;
            remain -= got;
        }
    }

    if (remain > 0)
        memset(pos, 0, remain);
}

const char *MidiPlayer_GetFilename(void)
{
    return g_loaded ? g_filename : NULL;
}
