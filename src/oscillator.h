#pragma once

#include <complex>
#include <cstddef>
#include <cstdint>

#include "patch.h"

class Oscillator {
 public:
  void Perform(size_t buffer_size, uint16_t sample_rate,
               std::complex<float> buffer[], float freq,
               const GeneratorPatch &patch, const float level_a[],
               const float level_k[]);

  void Reset() { x_ = 0.0f; }

 private:
  float x_ = 0.0f;
};