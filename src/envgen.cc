#include "envgen.h"

// originally based on http://www.martin-finke.de/blog/articles/audio-plugins-011-envelopes/
// TODO: too much branching in loops, need to be able to set levels (not just rates), more stages, velocity & aftertouch

float EnvelopeGenerator::NextSample(const GeneratorPatch::Envelope &env) {
  if (stage_ != ENVELOPE_STAGE_OFF &&
      stage_ != ENVELOPE_STAGE_SUSTAIN) {
    if (current_sample_index_ == next_stage_sample_index_) {
      auto new_stage = static_cast<EnvelopeStage>(
          (stage_ + 1) % kNumEnvelopeStages
      );
      EnterStage(new_stage, env);
    }
    current_level_ *= multiplier_;
    current_sample_index_++;
  }
  return current_level_;
}

void EnvelopeGenerator::CalculateMultiplier(float start_level,
                                            float end_level,
                                            unsigned long long length_in_samples) {
  multiplier_ = 1.0f + (std::log(end_level) - std::log(start_level)) / ((float)length_in_samples);
}

void EnvelopeGenerator::EnterStage(EnvelopeStage new_stage, const GeneratorPatch::Envelope &envelope) {
  const float stage_values[]{
      0.0f, envelope.A_R, envelope.D_R, envelope.S_L, envelope.R_R
  };
  stage_ = new_stage;
  current_sample_index_ = 0;
  if (stage_ == ENVELOPE_STAGE_OFF ||
      stage_ == ENVELOPE_STAGE_SUSTAIN) {
    next_stage_sample_index_ = 0;
  } else {
    next_stage_sample_index_ = stage_values[stage_] * sample_rate_;
  }
  switch (new_stage) {
    case ENVELOPE_STAGE_OFF:
      current_level_ = 0.0f;
      multiplier_ = 1.0f;
      break;
    case ENVELOPE_STAGE_ATTACK:
      current_level_ = minimum_level_;
      CalculateMultiplier(current_level_,
                          1.0f,
                          next_stage_sample_index_);
      break;
    case ENVELOPE_STAGE_DECAY:
      current_level_ = 1.0f;
      CalculateMultiplier(current_level_,
                          std::fmax(stage_values[ENVELOPE_STAGE_SUSTAIN], minimum_level_),
                          next_stage_sample_index_);
      break;
    case ENVELOPE_STAGE_SUSTAIN:
      current_level_ = stage_values[ENVELOPE_STAGE_SUSTAIN];
      multiplier_ = 1.0f;
      break;
    case ENVELOPE_STAGE_RELEASE:
      // We could go from ATTACK/DECAY to RELEASE,
      // so we're not changing currentLevel here.
      CalculateMultiplier(current_level_,
                          minimum_level_,
                          next_stage_sample_index_);
      break;
    default:
      break;
  }
}

void EnvelopeGenerator::SetSampleRate(float newSampleRate) {
  sample_rate_ = newSampleRate;
}


