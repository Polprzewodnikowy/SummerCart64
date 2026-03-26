#pragma once

#include <trx/core/vector.h>
#include <trx/game/shell/args.h>

#include <SDL2/SDL_events.h>

// Test replay: a module to record game playthroughs.
// ============================================================================

// Initialize test recorder for recording mode.
// @param path  Path to the recording to write to.
void TestRecorder_Open(const char *path, VECTOR *original_args);

// Close the recorder.
void TestRecorder_Close(void);

// Return whether the recording mode is currently active.
bool TestRecorder_IsOpened(void);

// Should be called at the start of each frame to handle skip logic.
void TestRecorder_BeginFrame(void);

// Record a single SDL_Event. Called for each event polled. Only essential
// events are recorded.
// @param event     Event to record
void TestRecorder_RecordEvent(const SDL_Event *event);

// Should be called after processing events each frame to update skip counters.
void TestRecorder_EndFrame(void);
