#pragma once

#include <mutex>
#include <sigslot/signal.hpp>
#include <vector>

struct GeneratorPatch {
  static GeneratorPatch Default() {
    return {1.0,
            0.5,
            3.0,
            4.0,
            1,
            0.0,
            GeneratorPatch::Envelope{0.025, 0.175, 0.25, 0.75},
            GeneratorPatch::Envelope{0.05, 0.33, 0.25, 0.5}};
  }
  float C, A, M, K, R, S;

  struct Envelope {
    float A_R;  // attack rate
    float A_L;  // attack peak level
    float D_R;  // decay rate
    float S_L;  // sustain level
    float R_R;  // release rate

    bool operator==(const Envelope &rhs) const {
      return A_R == rhs.A_R && A_L == rhs.A_L && D_R == rhs.D_R &&
             S_L == rhs.S_L && R_R == rhs.R_R;
    }
  };
  Envelope A_ENV;
  Envelope K_ENV;

  bool operator==(const GeneratorPatch &rhs) const {
    return C == rhs.C && A == rhs.A && M == rhs.M && K == rhs.K && R == rhs.R &&
           S == rhs.S && A_ENV == rhs.A_ENV && K_ENV == rhs.K_ENV;
  }
};

struct Patch {
  std::vector<GeneratorPatch> generators;

  sigslot::signal<GeneratorPatch *> AddGeneratorSignal;
  sigslot::signal<int> RmGeneratorSignal;

  GeneratorPatch *AddGenerator() {
    generators.push_back(GeneratorPatch::Default());
    GeneratorPatch *n_gp = &generators.back();
    AddGeneratorSignal(n_gp);
    return n_gp;
  }

  void RmGenerator(int index) {
    generators.erase(generators.begin() + index);
    RmGeneratorSignal(index);
  }

  bool operator==(const Patch &rhs) const {
    return generators == rhs.generators;
  }
};
