#include "player.h"

#include <glog/logging.h>

#include <cmath>
#include <execution>
#include <memory>
#include <mutex>

#include "oscillator.h"

namespace {
float NoteToFreq(int note) {
  float a = 440;  // frequency of A (coomon value is 440Hz)
  return (a / 32.0f) * std::pow(2, ((note - 9.0f) / 12.0f));
}
}  // namespace

Player::Player(Patch *patch, int num_voices, int sample_frequency)
    : patch_(patch),
      num_voices_(num_voices),
      sample_frequency_(sample_frequency) {
  for (int i = 0; i < num_voices; i++) {
    Voice v;
    for (auto &g : patch->generators) {
      v.generators_.emplace_back(
          std::make_unique<Generator>(&g, sample_frequency));
    }
    voices_.push_back(std::move(v));
  }

  patch_->RmGeneratorSignal.connect([this](int signum) {
    std::lock_guard<std::mutex> l(voices_mutex_);
    for (auto &voice : voices_) {
      voice.generators_.erase(voice.generators_.begin() + signum);
    }
  });

  patch_->AddGeneratorSignal.connect([this](GeneratorPatch *g_patch) {
    std::lock_guard<std::mutex> l(voices_mutex_);
    for (auto &voice : voices_) {
      voice.generators_.push_back(
          std::make_unique<Generator>(g_patch, sample_frequency_));
    }
  });
}

int Player::Perform(const void *in_buffer, void *out_buffer,
                    unsigned long frames_per_buffer,
                    const PaStreamCallbackTimeInfo *time_info,
                    PaStreamCallbackFlags status_flags) {
  memset(out_buffer, 0, frames_per_buffer * sizeof(float));

  std::mutex buffer_mutex;
  std::vector<std::unique_ptr<std::complex<float>[]>> mix_buffers;

  {
    std::lock_guard<std::mutex> player_lock(voices_mutex_);

    std::for_each(
        std::execution::par_unseq, voices_.begin(), voices_.end(),
        [frames_per_buffer, this, &mix_buffers, &buffer_mutex](Voice &voice) {
          // Probably faster without the branch in a loop...
          if (!voice.on) return;
          for (int g_num = 0; g_num < voice.generators_.size(); g_num++) {
            auto &g = voice.generators_[g_num];

            auto mix_buffer =
                std::make_unique<std::complex<float>[]>(frames_per_buffer);
            g->Perform(patch_->generators[g_num], mix_buffer.get(),
                       voice.base_freq, frames_per_buffer);
            {
              std::lock_guard<std::mutex> buffer_lock(buffer_mutex);
              mix_buffers.push_back(std::move(mix_buffer));
            }
          }
        });
  }

  // Mix down.
  if (!mix_buffers.empty()) {
    std::complex<float> mix_buffer[frames_per_buffer];
    for (auto &v_buffer : mix_buffers) {
      if (v_buffer)
        for (int i = 0; i < frames_per_buffer; i++) {
          mix_buffer[i] += v_buffer.get()[i];
        }
    }

    auto *f_buffer = (float *)out_buffer;
    for (int i = 0; i < frames_per_buffer; i++) {
      f_buffer[i] = mix_buffer[i].real();
    }
  }
  return paContinue;
}

void Player::NoteOn(PmTimestamp ts, uint8_t velocity, uint8_t note) {
  std::lock_guard<std::mutex> player_lock(voices_mutex_);

  // A note with no velocity is not a note at all.
  if (!velocity) return;

  float base_freq = NoteToFreq(note);
  float vel = (float)velocity / 80;

  // Check if we already have this note on, if so ignore.
  if (VoiceFor(note) != nullptr) return;

  Voice *v = NewVoice();
  CHECK(v) << "No voice available";
  v->note = note;
  v->on = true;
  v->on_time = ts;
  v->base_freq = base_freq;
  v->velocity = vel;
  for (int g_num = 0; g_num < v->generators_.size(); g_num++) {
    auto &g = v->generators_[g_num];
    auto &gp = patch_->generators[g_num];
    g->NoteOn(gp, ts, velocity, note);
  }
  // TODO legato, portamento, etc.
}

void Player::NoteOff(uint8_t note) {
  std::lock_guard<std::mutex> player_lock(voices_mutex_);

  // Find the oscillator playing this and turn it off.
  for (auto &v : voices_) {
    if (v.on && v.note == note) {
      v.on = false;
      for (int g_num = 0; g_num < v.generators_.size(); g_num++) {
        auto &g = v.generators_[g_num];
        auto &gp = patch_->generators[g_num];
        g->NoteOff(gp, note);
      }
      return;
    }
  }

  // Didn't find it?  Likely because the voice was already stolen.
  LOG(ERROR) << "STOLEN / UNKNOWN NOTE? " << std::hex << note;
}

Player::Voice *Player::NewVoice() {
  // Look for a free voice and grab it.
  for (auto &v : voices_) {
    if (!v.on) {
      return &v;
    }
  }

  // No free voice? Find the one with the lowest timestamp and steal it.
  PmTimestamp least_ts = INT_MAX;
  int stolen_voice_num = -1;
  for (int i = 0; i < num_voices_; i++) {
    if (voices_[i].on_time < least_ts) {
      stolen_voice_num = i;
      least_ts = voices_[i].on_time;
    }
  }
  if (stolen_voice_num == -1) return nullptr;

  Voice &voice = voices_[stolen_voice_num];
  return &voice;
}

Player::Voice *Player::VoiceFor(uint8_t note) {
  // Check if we already have this note on, if so ignore.
  for (auto &v : voices_) {
    if (v.note == note && v.on) return &v;
  }
  return nullptr;
}

Generator::Generator(const GeneratorPatch *patch, int sample_frequency)
    : sample_frequency_(sample_frequency),
      e_a_(sample_frequency),
      e_k_(sample_frequency) {}

void Generator::Perform(const GeneratorPatch &patch,
                        std::complex<float> *out_buffer, float base_freq,
                        unsigned long frames_per_buffer) {
  float env_levels_a[frames_per_buffer];
  float env_levels_k[frames_per_buffer];

  for (int i = 0; i < frames_per_buffer; i++) {
    env_levels_a[i] = e_a_.NextSample(patch.A_ENV);
    env_levels_k[i] = e_k_.NextSample(patch.K_ENV);
  }
  o_.Perform(frames_per_buffer, sample_frequency_, out_buffer, base_freq, patch,
             env_levels_a, env_levels_k);
}

void Generator::NoteOn(const GeneratorPatch &patch, PmTimestamp ts,
                       uint8_t velocity, uint8_t note) {
  e_a_.EnterStage(EnvelopeGenerator::ENVELOPE_STAGE_ATTACK, patch.A_ENV);
  e_k_.EnterStage(EnvelopeGenerator::ENVELOPE_STAGE_ATTACK, patch.K_ENV);
}

void Generator::NoteOff(const GeneratorPatch &patch, uint8_t note) {
  e_a_.EnterStage(EnvelopeGenerator::ENVELOPE_STAGE_RELEASE, patch.A_ENV);
  e_k_.EnterStage(EnvelopeGenerator::ENVELOPE_STAGE_RELEASE, patch.K_ENV);
}
