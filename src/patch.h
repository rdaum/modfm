#pragma once

struct Patch {
  float C, A, M, K, R, S;

  struct Envelope {
    float A_R; // attack rate
    float A_L; // attack peak level
    float D_R; // decay rate
    float S_L; // sustain level
    float R_R; // release rate
  };
  Envelope A_ENV{0.5, 1, 0.5, 0.7, 0.4};
  Envelope K_ENV{0.5, 1, 0.5, 0.7, 0.4};

  bool operator==(const Patch &patch) const {
    return C == patch.C && A == patch.A && M == patch.M && K == patch.K && R == patch.R
        && S == patch.S;
  }
};

