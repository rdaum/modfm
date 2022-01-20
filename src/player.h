#pragma once

#include <portaudio.h>
#include <portmidi.h>
#include <porttime.h>

#include "oscillator.h"
#include "envgen.h"

namespace {
constexpr int kNumVoices = 8;
}  // namespace

class GUI;

class Player {
 public:
  explicit Player(const Patch &patch);

  int Perform(const void *in_buffer, void *out_buffer, unsigned long frames_per_buffer,
              const PaStreamCallbackTimeInfo *time_info,
              PaStreamCallbackFlags status_flags);

  void NoteOn(PmTimestamp ts, uint8_t velocity, uint8_t note);

  void NoteOff(uint8_t note);

 private:
  const Patch &patch_;

  // Track free voices.
  struct Voice {
    bool on;
    int32_t on_time;
    uint8_t note;
    float velocity;
    float base_freq;
    Oscillator o;
    EnvelopeGenerator e_a;
    EnvelopeGenerator e_k;
  };
  // Could probably structure this as a ring buffer in order of note-on instead of using timestamps.
  Voice voices_[8];
};
