#include "gui.h"

#include <cmath>
#include <thread>
#include <mutex>
#include <complex>
#include <absl/strings/str_format.h>

#include <glog/logging.h>
#include <gflags/gflags.h>

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl2.h>
#include <imgui_plot.h>
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include <imgui_internal.h>

#include "oscillator.h"
#include "midi.h"

namespace {
constexpr size_t kAnalysisBufferSize = 256;
} // namespace

GUI::~GUI() {
  Close();
}

void glfw_error_callback(int error, const char *description) {
  LOG(ERROR) << "Glfw Error: " << error << " (" << description << ")";
}

void window_close_callback(GLFWwindow *window) {
  LOG(ERROR) << "Shutting down";
  auto *gui = static_cast<GUI *>(glfwGetWindowUserPointer(window));
  gui->Stop();
}

void GUI::Start(int x, int y) {
  gui_thread_ = std::thread([this, x, y] {
    {
      running_ = true;

      std::lock_guard<std::mutex> lock(gui_mutex_);

      glfwSetErrorCallback(glfw_error_callback);
      CHECK(glfwInit());

      window_ = glfwCreateWindow(999, 800, "modfm", nullptr, nullptr);
      CHECK(window_);
      glfwSetWindowPos(window_, x, y);
      glfwMakeContextCurrent(window_);
      glfwSwapInterval(1); // Enable vsync
      glfwSetWindowUserPointer(window_, this);
      glfwSetWindowCloseCallback(window_, window_close_callback);

      // Setup Dear ImGui context
      IMGUI_CHECKVERSION();
      ImGui::CreateContext();

      // Setup Dear ImGui style
      ImGui::StyleColorsDark();

      // Setup Platform/Renderer bindings
      ImGui_ImplGlfw_InitForOpenGL(window_, true);
      ImGui_ImplOpenGL2_Init();
    }
    while (running_ && !glfwWindowShouldClose(window_)) {
      auto refresh_interval =
          std::chrono::steady_clock::now() + std::chrono::milliseconds(16);
      Render();
      std::this_thread::sleep_until(refresh_interval);
    }
  });
}

void GUI::Close() {
  std::lock_guard<std::mutex> lock(gui_mutex_);

  running_ = false;
  if (window_) {
    // Cleanup
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window_);
  }
  window_ = nullptr;
}

void GUI::Stop() {
  std::lock_guard<std::mutex> lock(gui_mutex_);

  running_ = false;
}

void GUI::Render() {
  glfwPollEvents();

  ImGui_ImplOpenGL2_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  ImGui::SetNextWindowPos({0, 0}, ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize({333, 800}, ImGuiCond_FirstUseEver);

  static bool midi_open = true;
  if (ImGui::Begin("MIDI", &midi_open)) {
    const auto *current_device = midi_receiver_->CurrentDeviceInfo();
    char current_device_name[80] = "None";
    if (current_device) {
      std::strncpy(current_device_name,
                   absl::StrFormat("%s (%d)", current_device->name, midi_receiver_->CurrentDeviceID()).c_str(),
                   80);
    }
    if (ImGui::BeginCombo("Input device", current_device_name)) {
      auto devices = midi_receiver_->ListDevices();
      for (const auto &device: devices) {
        bool is_selected;
        if (ImGui::Selectable(absl::StrFormat("%s (%d)", device.second->name, device.first).c_str(),
                              &is_selected)) {
          CHECK(midi_receiver_->Stop().ok());
          CHECK(midi_receiver_->Close().ok());
          CHECK(midi_receiver_->OpenDevice(device.first).ok()) << "Unable to open device: " << device.first;
          CHECK(midi_receiver_->Start().ok());
        }
        if (is_selected) ImGui::SetItemDefaultFocus();
      }
      ImGui::EndCombo();
    }
  }
  ImGui::End();

  if (ImGui::Begin("Patch", nullptr)) {
    int g_num = 0;
    auto &generators = patch_->generators;
    // TODO: re-enable once proper multi-generator patching is working
//    if (ImGui::Button("+")) {
//      generators.
//          push_back(std::move(GeneratorPatch::Default())
//      );
//    }

    unsigned long num_generators = generators.size();
    bool active[num_generators];
    memset(active,
           true, sizeof(active));
    for (
      auto &g
        : generators) {

      if (
          ImGui::CollapsingHeader(absl::StrFormat("Generator %d", g_num)
                                      .
                                          c_str(),
                                  &active[g_num],
                                  ImGuiTreeNodeFlags_DefaultOpen
          )) {
        ImGui::BeginTable(absl::StrFormat("table-gen-%d", g_num)
                              .
                                  c_str(),
                          2);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        if (ImGui::CollapsingHeader("Oscillator Parameters", nullptr)) {
          ImGui::SliderFloat("Carrier Ratio", &g.C, 0, 10);
          ImGui::SliderFloat("Amplitude", &g.A, 0, 1);
          ImGui::SliderFloat("Modulator Ratio", &g.M, 0, 10);
          ImGui::SliderFloat("Modulator Level", &g.K, 0, 10);
          ImGui::SliderFloat("R", &g.R, 0, 1);
          ImGui::SliderFloat("S", &g.S, -1, 1);
        }
        ImGui::Separator();
        EnvelopeEditor("Amplitude Envelope", &g.A_ENV);
        ImGui::Separator();
        EnvelopeEditor("Modulator Level Envelope", &g.K_ENV);
        ImGui::TableNextColumn();
        float x_data[kAnalysisBufferSize];
        std::complex<float> c_buf_data[kAnalysisBufferSize];
        float buf_data[kAnalysisBufferSize];

        Oscillator oscillator_;
        float e_c[kAnalysisBufferSize];
        for (
            int i = 0;
            i < kAnalysisBufferSize;
            i++) {
          e_c[i] = 1.0f;
        }
        oscillator_.
            Perform(kAnalysisBufferSize,
                    44100, c_buf_data, 440, g, e_c, e_c);
        for (
            int i = 0;
            i < kAnalysisBufferSize;
            i++) {
          buf_data[i] = c_buf_data[i].
              real();
        }

        PlotWave(kAnalysisBufferSize, x_data, buf_data
        );
        ImGui::EndTable();
      }
      g_num++;
    }
    while (num_generators--) {
      if (!active[num_generators]) {
        generators.
            erase(generators
                      .
                          begin()
                      + num_generators);
      }
    }
  }
  ImGui::End();
  ImGui::EndFrame();
  ImGui::Render();

  ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
  int display_w, display_h;
  glfwGetFramebufferSize(window_, &display_w, &display_h
  );
  glViewport(0, 0, display_w, display_h);
  glClearColor(clear_color
                   .x, clear_color.y, clear_color.z, clear_color.w);
  glClear(GL_COLOR_BUFFER_BIT);

// If you are using this code with non-legacy OpenGL header/contexts (which
// you should not, prefer using imgui_impl_opengl3.cpp!!), you may need to
// backup/reset/restore current shader using the commented lines below.
// GLint last_program;
// glGetIntegerv(GL_CURRENT_PROGRAM, &last_program);
// glUseProgram(0);
  ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
// glUseProgram(last_program);

  glfwMakeContextCurrent(window_);
  glfwSwapBuffers(window_);
}

// static
void GUI::EnvelopeEditor(const std::string &title, GeneratorPatch::Envelope *envelope) {
  if (ImGui::CollapsingHeader(title.c_str())) {
    ImGui::SliderFloat("Attack rate", &envelope->A_R, 0.0f, 10.0f, "%.3f");
    ImGui::SliderFloat("Decay rate", &envelope->D_R, 0.0f, 10.0f, "%.3f");
    ImGui::SliderFloat("Sustain level", &envelope->S_L, 0.0f, 1.0f, "%.3f");
    ImGui::SliderFloat("Release rate", &envelope->R_R, 0.0f, 10.0f, "%.3f");
  }
}

// static
void GUI::PlotWave(const size_t buf_size, const float *x_data, const float *y_data1) {
  ImGui::PlotConfig conf;
  const float *y_data[] = {y_data1};
  ImU32 colors[3] = {ImColor(0, 255, 0)};
  uint32_t selection_start = 0, selection_length = 0;

  conf.values.xs = x_data;
  conf.values.count = buf_size;
  conf.values.ys_list = y_data; // use ys_list to draw several lines simultaneously
  conf.values.ys_count = 1;
  conf.values.colors = colors;
  conf.scale.min = -1;
  conf.scale.max = 1;
  conf.tooltip.show = true;
  conf.grid_x.show = true;
  conf.grid_x.size = 128;
  conf.grid_x.subticks = 4;
  conf.grid_y.show = true;
  conf.grid_y.size = 0.5f;
  conf.grid_y.subticks = 5;
  conf.selection.show = false;
  conf.frame_size = ImVec2(buf_size, 75);
  ImGui::Plot("plot1", conf);
}

void GUI::Wait() {
  gui_thread_.join();
}

bool GUI::Running() {
  std::lock_guard<std::mutex> lock(gui_mutex_);

  return running_;
}
