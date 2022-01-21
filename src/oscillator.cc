#include "oscillator.h"

#include <cmath>

namespace {
constexpr std::complex<float> kCpi = M_PI;
}  // namespace

void Oscillator::Perform(size_t buffer_size,
                         uint16_t sample_rate,
                         std::complex<float> buffer[],
                         float base_freq,
                         const Patch &patch,
                         const float level_a[], const float level_k[]) {
  std::complex<float> c_sample_rate = sample_rate;
  std::complex<float> freq = base_freq * patch.C;
  std::complex<float> omega_c = 2.0f * kCpi * freq;
  std::complex<float> omega_m = 2.0f * kCpi * (patch.M * freq);
  std::complex<float> S = std::complex<float>(0, patch.S);

  for (size_t i = 0; i < buffer_size; i++) {
    std::complex<float> A = std::complex<float>(patch.A * level_a[i]);
    std::complex<float> K = std::complex<float>(0, patch.K * level_k[i]);
    x_++;
    std::complex<float> t = (x_ / c_sample_rate);
    std::complex<float> omega_ct = t * omega_c;
    std::complex<float> omega_mt = t * omega_m;

    // I have no idea what I'm doing, but it "sounds" right?
    // Based on formula in "EXTENSIONS" section of
    // "Theory and Practice of Modified Frequency
    // Modulation Synthesis"
    // VICTOR LAZZARINI AND JOSEPH TIMONEY
    // https://mural.maynoothuniversity.ie/4697/1/JAES_V58_6_PG459hirez.pdf
//    buffer[i] = patch.A * (std::exp(K * std::cos(omega_mt)) * std::cos(omega_ct));
    buffer[i] = (A * (std::exp(patch.R * K * std::cos(omega_mt))
        * std::cos(omega_ct + S * K * std::sin(omega_mt))));
  }
}
