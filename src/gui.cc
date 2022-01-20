#include "gui.h"

#include <cmath>
#include <thread>
#include <mutex>
#include <complex>

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

#include <kiss_fftr.h>

#include "envgen.h"

namespace {
constexpr size_t kAnalysisBufferSize = 512;
} // namespace

GUI::~GUI() { Close(); }

void glfw_error_callback(int error, const char *description) {
  LOG(ERROR) << "Glfw Error: " << error << " (" << description << ")";
}

void window_close_callback(GLFWwindow *window) {
  LOG(ERROR) << "Close";
}

void GUI::Start(int x, int y) {
  gui_thread_ = std::thread([this, x, y] {
    {
      std::lock_guard<std::mutex> lock(gui_mutex_);

      glfwSetErrorCallback(glfw_error_callback);
      CHECK(glfwInit());

      window_ = glfwCreateWindow(999, 800, "modfm", nullptr, nullptr);
      CHECK(window_);
      glfwSetWindowPos(window_, x, y);
      glfwMakeContextCurrent(window_);
      glfwSwapInterval(1); // Enable vsync
//      glfwSetWindowUserPointer(window_, system_);
      glfwSetWindowCloseCallback(window_, window_close_callback);

      // Setup Dear ImGui context
      IMGUI_CHECKVERSION();
      ImGui::CreateContext();
      io_ = &ImGui::GetIO();

      // io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable
      // Keyboard Controls io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
      // // Enable Gamepad Controls

      // Setup Dear ImGui style
      ImGui::StyleColorsDark();
      // ImGui::StyleColorsClassic();

      // Setup Platform/Renderer bindings
      ImGui_ImplGlfw_InitForOpenGL(window_, true);
      ImGui_ImplOpenGL2_Init();
    }
    running_ = true;
    while (running_ && !glfwWindowShouldClose(window_)) {
      auto refresh_interval =
          std::chrono::steady_clock::now() + std::chrono::milliseconds(16);
      Render();
      std::this_thread::sleep_until(refresh_interval);
    }
  });
}

void GUI::Close() {
  running_ = false;
  if (window_) {
    // Cleanup
    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window_);
  }
}

void GUI::Stop() {
  running_ = false;
  Close();
}

static void convert_to_freq(kiss_fft_cpx *cout, int n) {
  const float NC = n / 2.0 + 1;
  while (n-- > 0) {
    cout->r /= NC;
    cout->i /= NC;
    cout++;
  }
}

void GUI::Render() {
  glfwPollEvents();

  ImGui_ImplOpenGL2_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  ImGui::SetNextWindowPos({0, 0}, ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize({333, 800}, ImGuiCond_FirstUseEver);

  ImGui::Begin("Base Parameters", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

  Patch old = *patch_;
  ImGui::SliderFloat("Carrier Ratio", &patch_->C, 0, 10);
  ImGui::SliderFloat("Amplitude", &patch_->A, 0, 1);
  ImGui::SliderFloat("Modulator Ratio", &patch_->M, 0, 10);
  ImGui::SliderFloat("Modulator Level", &patch_->K, 0, 10);
  ImGui::SliderFloat("R", &patch_->R, 0, 1);
  ImGui::SliderFloat("S", &patch_->S, -1, 1);
  ImGui::End();

  EnvelopeEditor("Amplitude Envelope", &patch_->A_ENV);
  EnvelopeEditor("Modulator Level Envelope", &patch_->K_ENV);

  static float x_data[kAnalysisBufferSize];
  static std::complex<float> c_buf_data[kAnalysisBufferSize];
  static float buf_data[kAnalysisBufferSize];
  static bool first = true;
  if (*patch_ != old || first) {
    first = false;
    oscillator_.Reset();
    float e_c[kAnalysisBufferSize];
    for (int i = 0; i < kAnalysisBufferSize; i++) {
      e_c[i] = 1.0f;
    }
    oscillator_.Perform(kAnalysisBufferSize, 44100, c_buf_data, 440, *patch_, e_c, e_c);
    for (int i = 0; i < kAnalysisBufferSize; i++) {
      buf_data[i] = c_buf_data[i].real();
    }
  }

  ImGui::Begin("Wave", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
  PlotWave(kAnalysisBufferSize, x_data, buf_data);
  ImGui::End();

  if (ImGui::Begin("FFT", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    static float fft_o[kAnalysisBufferSize];
    static float fft_x[kAnalysisBufferSize];
    if (*patch_ != old || first) {
      first = false;
    }
    kiss_fft_cfg cfg = kiss_fft_alloc(kAnalysisBufferSize, false, nullptr, 0);
    kiss_fft_cpx cx_in[kAnalysisBufferSize];
    for (int i = 0; i < kAnalysisBufferSize; i++) {
      cx_in[i].r = c_buf_data[i].real();
      cx_in[i].i = c_buf_data[i].imag();
    }
    kiss_fft_cpx cx_out[kAnalysisBufferSize];
    kiss_fft(cfg, cx_in, cx_out);
    convert_to_freq(cx_out, kAnalysisBufferSize);
    for (size_t i = 0; i < kAnalysisBufferSize; ++i) {
      fft_o[i] = cx_out[i].r;
      fft_x[i] = i;
    }
    kiss_fft_free(cfg);
    PlotFFT(kAnalysisBufferSize, fft_x, fft_o);
    ImGui::End();
  }

  ImGui::EndFrame();
  ImGui::Render();

  ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
  int display_w, display_h;
  glfwGetFramebufferSize(window_, &display_w, &display_h);
  glViewport(0, 0, display_w, display_h);
  glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
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

void GUI::EnvelopeEditor(const std::string &title, Patch::Envelope *envelope) const {
  ImGui::Begin(title.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize);

  ImGui::SliderFloat("Attack rate", &envelope->A_R, 0.0f, 1.0f, "%.3f");
  ImGui::SliderFloat("Decay rate", &envelope->D_R, 0.0f, 1.0f, "%.3f");
  ImGui::SliderFloat("Sustain level", &envelope->S_L, 0.0f, 1.0f, "%.3f");
  ImGui::SliderFloat("Release rate", &envelope->R_R, 0.0f, 1.0f, "%.3f");

  ImGui::End();
}

void GUI::PlotFFT(const size_t size, const float *x_data, const float *y_data1) const {
  ImGui::PlotConfig conf;
  const float *y_data[] = {y_data1};
  ImU32 colors[3] = {ImColor(0, 255, 0)};

  conf.values.xs = x_data;
  conf.values.count = size;
  conf.values.ys_list = y_data; // use ys_list to draw several lines simultaneously
  conf.values.ys_count = 1;
  conf.values.colors = colors;
  conf.scale.min = -0.5;
  conf.scale.max = 1;
  conf.tooltip.show = true;
  conf.grid_x.show = true;
  conf.grid_x.size = 32;
  conf.grid_x.subticks = 8;
  conf.grid_y.show = true;
  conf.grid_y.size = 0.5f;
  conf.grid_y.subticks = 5;
  conf.selection.show = false;
  conf.frame_size = ImVec2(size, 200);
  ImGui::Plot("plot1", conf);
}

void GUI::PlotWave(const size_t buf_size, const float *x_data, const float *y_data1) const {
  ImGui::PlotConfig conf;
  const float *y_data[] = {y_data1};
  ImU32 colors[3] = {ImColor(0, 255, 0), ImColor(255, 0, 0), ImColor(0, 0, 255)};
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
  conf.selection.show = true;
  conf.selection.start = &selection_start;
  conf.selection.length = &selection_length;
  conf.frame_size = ImVec2(buf_size, 200);
  ImGui::Plot("plot1", conf);

  // Draw second plot with the selection
// reset previous values
  conf.values.ys_list = nullptr;
  conf.selection.show = false;
  // set new ones
  conf.values.ys = y_data1;
  conf.values.offset = selection_start;
  conf.values.count = selection_length;
  conf.line_thickness = 2.f;
  ImGui::Plot("plot2", conf);
}
