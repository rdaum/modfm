#pragma once

#include <portaudio.h>
#include <portmidi.h>
#include <porttime.h>

#include <mutex>
#include <vector>

#include "envgen.h"
#include "oscillator.h"

class GUI;

class Generator {
 public:
  explicit Generator(int sample_frequency);

  void Perform(const GeneratorPatch &patch, std::complex<float> *out_buffer,
               float base_freq, unsigned long frames_per_buffer);

  void NoteOn(const GeneratorPatch &patch, PmTimestamp ts, uint8_t velocity,
              uint8_t note);

  void NoteOff(const GeneratorPatch &patch, uint8_t note);

 private:
  const int sample_frequency_;
  EnvelopeGenerator e_a_;
  EnvelopeGenerator e_k_;
  Oscillator o_;
};

class Player {
 public:
  Player(Patch *gennum, int num_voices, int sample_frequency);

  int Perform(const void *in_buffer, void *out_buffer,
              unsigned long frames_per_buffer,
              const PaStreamCallbackTimeInfo *time_info,
              PaStreamCallbackFlags status_flags);

  void NoteOn(PmTimestamp ts, uint8_t velocity, uint8_t note);

  void NoteOff(uint8_t note);

 private:
  struct Voice {
    std::vector<std::unique_ptr<Generator>> generators_;
    bool on;
    int32_t on_time;
    uint8_t note;
    float velocity;
    float base_freq;
  };
  Voice *NewVoice();
  Voice *VoiceFor(uint8_t note);

  std::mutex voices_mutex_;

  Patch *patch_;
  const int num_voices_ = 8;
  const int sample_frequency_;
  // Track free voices.
  // Could probably structure this as a ring buffer in order of note-on instead
  // of using timestamps.
  std::vector<Voice> voices_;
};
