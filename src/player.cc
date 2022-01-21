#include "player.h"

#include <cmath>
#include <memory>
#include <execution>
#include <mutex>

#include <glog/logging.h>

#include "oscillator.h"

namespace {
float NoteToFreq(int note) {
  float a = 440; //frequency of A (coomon value is 440Hz)
  return (a / 32.0f) * std::pow(2, ((note - 9.0f) / 12.0f));
}
} // namespace

Player::Player(const Patch &patch, int num_voices, int sample_frequency)
    : patch_(patch), num_voices_(num_voices), sample_frequency_(sample_frequency) {
  for (int i = 0; i < num_voices; i++) {
    voices_.push_back(Voice{EnvelopeGenerator(sample_frequency), EnvelopeGenerator(sample_frequency), false /* on */});
  }
}

int Player::Perform(const void *in_buffer,
                    void *out_buffer,
                    unsigned long frames_per_buffer,
                    const PaStreamCallbackTimeInfo *time_info,
                    PaStreamCallbackFlags status_flags) {

  memset(out_buffer, 0, frames_per_buffer * sizeof(float));

  std::mutex buffer_mutex;
  std::vector<std::unique_ptr<std::complex<float>[]>> mix_buffers;

  {
    std::lock_guard<std::mutex> player_lock(voices_mutex_);

    std::for_each(std::execution::par_unseq,
                  voices_.begin(),
                  voices_.end(),
                  [frames_per_buffer, this, &mix_buffers, &buffer_mutex](Voice &voice) {
                    // Probably faster without the branch in a loop...
                    if (!voice.on) return;
                    auto mix_buffer = std::make_unique<std::complex<float>[]>(frames_per_buffer);
                    float env_levels_a[frames_per_buffer];
                    float env_levels_k[frames_per_buffer];

                    for (int i = 0; i < frames_per_buffer; i++) {
                      env_levels_a[i] = voice.e_a.NextSample(patch_.A_ENV);
                      env_levels_k[i] = voice.e_k.NextSample(patch_.K_ENV);
                    }
                    voice.o.Perform(frames_per_buffer, sample_frequency_, mix_buffer.get(), voice.base_freq, patch_,
                                    env_levels_a, env_levels_k);
                    {
                      std::lock_guard<std::mutex> buffer_lock(buffer_mutex);
                      mix_buffers.push_back(std::move(mix_buffer));
                    }
                  });
  }

  // Mix down.
  std::complex<float> mix_buffer[frames_per_buffer];
  for (auto &v_buffer: mix_buffers) {
    if (v_buffer)
      for (int i = 0; i < frames_per_buffer; i++) {
        mix_buffer[i] += v_buffer.get()[i];
      }
  }

  auto *f_buffer = (float *) out_buffer;
  for (int i = 0; i < frames_per_buffer; i++) {
    f_buffer[i] = mix_buffer[i].real();
  }
  return paContinue;
}

void Player::NoteOn(PmTimestamp ts, uint8_t velocity, uint8_t note) {
  std::lock_guard<std::mutex> player_lock(voices_mutex_);

  // A note with no velocity is not a note at all.
  if (!velocity) return;

  float base_freq = NoteToFreq(note);
  float vel = (float) velocity / 80;

  // Check if we already have this note on, if so ignore.
  for (const auto &o: voices_) {
    if (o.note == note && o.on) return;
  }

  // Look for a free voice and grab it.
  for (auto &v: voices_) {
    if (!v.on) {
      v.note = note;
      v.on = true;
      v.on_time = ts;
      v.base_freq = base_freq;
      v.velocity = vel;
      v.e_a.EnterStage(EnvelopeGenerator::ENVELOPE_STAGE_ATTACK, patch_.A_ENV);
      v.e_k.EnterStage(EnvelopeGenerator::ENVELOPE_STAGE_ATTACK, patch_.K_ENV);
      return;
    }
  }

  // No free voice? Find the one with the lowest timestamp and steal it.
  PmTimestamp least_ts = ts;
  int stolen_voice_num = 0;
  for (int i = 0; i < num_voices_; i++) {
    if (voices_[i].on_time < least_ts) {
      stolen_voice_num = i;
      least_ts = voices_[i].on_time;
    }
  }
  Voice &voice = voices_[stolen_voice_num];
  voice.on_time = ts;
  voice.on = true;
  voice.note = note;
  voice.base_freq = base_freq;
  voice.velocity = vel;
  voice.e_a.EnterStage(EnvelopeGenerator::ENVELOPE_STAGE_ATTACK, patch_.A_ENV);
  voice.e_k.EnterStage(EnvelopeGenerator::ENVELOPE_STAGE_ATTACK, patch_.A_ENV);

  // TODO legato, portamento, etc.
}

void Player::NoteOff(uint8_t note) {
  std::lock_guard<std::mutex> player_lock(voices_mutex_);

  // Find the oscillator playing this and turn it off.
  for (auto &v: voices_) {
    if (v.on && v.note == note) {
      v.on = false;
      v.e_a.EnterStage(EnvelopeGenerator::ENVELOPE_STAGE_RELEASE, patch_.A_ENV);
      v.e_k.EnterStage(EnvelopeGenerator::ENVELOPE_STAGE_RELEASE, patch_.K_ENV);
      return;
    }
  }

  // Didn't find it?  Likely because the voice was already stolen.
  LOG(ERROR) << "STOLEN NOTE: " << std::hex << note;
}
