#pragma once

#include <cmath>
#include "patch.h"

class EnvelopeGenerator {
 public:
  explicit EnvelopeGenerator(int sample_rate) :
      minimum_level_(0.0001),
      stage_(ENVELOPE_STAGE_OFF),
      current_level_(minimum_level_),
      multiplier_(1.0),
      sample_rate_(sample_rate),
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
  void EnterStage(EnvelopeStage new_stage, const GeneratorPatch::Envelope &envelope);
  float NextSample(const GeneratorPatch::Envelope &env);
  void SetSampleRate(float newSampleRate);
  inline EnvelopeStage Stage() const { return stage_; };
  const float minimum_level_;

 private:
  void CalculateMultiplier(float start_level, float end_level, unsigned long long length_in_samples);
  EnvelopeStage stage_;
  float current_level_;
  float multiplier_;
  float sample_rate_;
  unsigned long long current_sample_index_;
  unsigned long long next_stage_sample_index_;
};
