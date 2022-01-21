#pragma once

#include <portaudio.h>
#include <portmidi.h>
#include <porttime.h>

#include <vector>
#include <mutex>

#include "oscillator.h"
#include "envgen.h"

class GUI;

class Player {
 public:
  Player(const Patch &patch, int num_voices, int sample_frequency);

  int Perform(const void *in_buffer, void *out_buffer, unsigned long frames_per_buffer,
              const PaStreamCallbackTimeInfo *time_info,
              PaStreamCallbackFlags status_flags);

  void NoteOn(PmTimestamp ts, uint8_t velocity, uint8_t note);

  void NoteOff(uint8_t note);

 private:
  std::mutex voices_mutex_;

  const Patch &patch_;
  const int num_voices_ = 8;
  const int sample_frequency_;
  // Track free voices.
  struct Voice {
    EnvelopeGenerator e_a;
    EnvelopeGenerator e_k;
    bool on;
    int32_t on_time;
    uint8_t note;
    float velocity;
    float base_freq;
    Oscillator o;
  };
  // Could probably structure this as a ring buffer in order of note-on instead of using timestamps.
  std::vector<Voice> voices_;
};
