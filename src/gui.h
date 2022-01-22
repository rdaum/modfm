#pragma once

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl2.h>
#include <thread>
#include <mutex>
#include <atomic>

#include "patch.h"

class MIDIReceiver;
class GUI {
 public:
  GUI(Patch *patch, MIDIReceiver *midi_receiver) : patch_(patch), midi_receiver_(midi_receiver) {}
  ~GUI();

  void Start(int x, int y);
  void Stop();
  bool Running();

  void Wait();

 private:
  void Render();
  void Close();
  static void PlotWave(size_t buf_size, const float *x_data, const float *y_data1) ;
  static void EnvelopeEditor(const std::string &title, GeneratorPatch::Envelope *envelope) ;

  Patch *patch_;
  MIDIReceiver *midi_receiver_;

  std::mutex gui_mutex_;
  std::thread gui_thread_;

  std::atomic_bool running_;
  GLFWwindow *window_ = nullptr;
};
