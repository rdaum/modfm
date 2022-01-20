#pragma once

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl2.h>
#include <thread>
#include <mutex>
#include <atomic>

#include "oscillator.h"

class GUI {
 public:
  explicit GUI(Patch *patch) : patch_(patch) {}
  ~GUI();

  void Start(int x, int y);
  void Stop();
  bool Running();

  void Wait();

 private:
  void Render();
  void Close();
  void PlotWave(size_t buf_size, const float *x_data, const float *y_data1) const;
  void PlotFFT(size_t size, const float *x_data, const float *y_data1) const;
  void EnvelopeEditor(const std::string &title, Patch::Envelope *envelope) const;

  Oscillator oscillator_;
  Patch *patch_;

  std::mutex gui_mutex_;
  std::thread gui_thread_;

  std::atomic_bool running_;
  GLFWwindow *window_ = nullptr;
};
