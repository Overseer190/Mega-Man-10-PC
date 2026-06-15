#ifndef MIDI_PLAYER_H
#define MIDI_PLAYER_H

#include <stdbool.h>

// Initialise BASS + load a SoundFont (call once at startup).
bool MidiPlayer_Init(const char *sf2_path);

// Shut down BASS and free everything.
void MidiPlayer_Quit(void);

// Load a MIDI file for playback. Scans for loopStart/loopEnd markers.
bool MidiPlayer_Load(const char *mid_path);

// Unload the current MIDI file (does not shut down BASS).
void MidiPlayer_Free(void);

// Start playing from the beginning.  If loop is true, the renderer
// will seek back to loopStart (or 0) when the stream ends.
void MidiPlayer_Play(bool loop);

// Pause / resume (tracks state manually; BASS decode streams
// ignore ChannelPause at the API level).
void MidiPlayer_Pause(void);
void MidiPlayer_Resume(void);

// Stop and rewind to the beginning.
void MidiPlayer_Stop(void);

bool MidiPlayer_IsPlaying(void);
bool MidiPlayer_IsPaused(void);

// Fill a PCM buffer with decoded audio.  Must be called regularly
// when the game loop is running (BASS decode streams don't play
// without an explicit pull).
void MidiPlayer_Render(short *buffer, int num_samples, int sample_rate);

// Returns the path of the currently loaded MIDI file, or NULL.
const char *MidiPlayer_GetFilename(void);

#endif
