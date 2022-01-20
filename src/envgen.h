#pragma once

#include <cmath>
#include "patch.h"

class EnvelopeGenerator {
 public:
  EnvelopeGenerator() :
      minimum_level_(0.0001),
      stage_(ENVELOPE_STAGE_OFF),
      current_level_(minimum_level_),
      multiplier_(1.0),
      sample_rate_(44100.0),
      current_sample_index_(0),
      next_stage_sample_index_(0) {
  };

  enum EnvelopeStage {
    ENVELOPE_STAGE_OFF = 0,
    ENVELOPE_STAGE_ATTACK,
    ENVELOPE_STAGE_DECAY,
    ENVELOPE_STAGE_SUSTAIN,
    ENVELOPE_STAGE_RELEASE,
    kNumEnvelopeStages
  };
  void EnterStage(EnvelopeStage new_stage, const Patch::Envelope envelope);
  double NextSample(const Patch::Envelope &env);
  void SetSampleRate(double newSampleRate);
  inline EnvelopeStage Stage() const { return stage_; };
  const double minimum_level_;

 private:
  void CalculateMultiplier(double start_level, double end_level, unsigned long long length_in_samples);
  EnvelopeStage stage_;
  double current_level_;
  double multiplier_;
  double sample_rate_;
  unsigned long long current_sample_index_;
  unsigned long long next_stage_sample_index_;
};
