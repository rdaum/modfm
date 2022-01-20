#include <glog/logging.h>
#include <gflags/gflags.h>
#include <GLFW/glfw3.h>

#include "gui.h"
#include "oscillator.h"
#include "player.h"

namespace {
constexpr int kSampleFrequency = 44100;
}  // namespace

DEFINE_bool(listmidi, false, "List available MIDI devices then exit");
DEFINE_int32(midi, 0, "MIDI device to use for input. If not set, use default.");

int pa_output_callback(const void *in_buffer, void *out_buffer, unsigned long frames_per_buffer,
                       const PaStreamCallbackTimeInfo *time_info,
                       PaStreamCallbackFlags status_flags, void *user_data) {
  auto *player = (Player *) user_data;
  return player->Perform(in_buffer, out_buffer, frames_per_buffer, time_info, status_flags);
}

int main(int argc, char *argv[]) {
  FLAGS_logtostderr = true;
  google::InitGoogleLogging(argv[0]);
  google::ParseCommandLineFlags(&argc, &argv, false);

  glfwInit();

  LOG(INFO) << "Good morning.";

  Patch patch{2.0, 0.8, 3.0, 0.5, 1, 0.0};

  LOG(INFO) << "Initializing PortAudio";
  PaError err = Pa_Initialize();
  CHECK(err == paNoError) << "PortAudio error: " << Pa_GetErrorText(err);

  PaDeviceIndex device = 0;
  for (int i = 0, end = Pa_GetDeviceCount(); i != end; ++i) {
    PaDeviceInfo const *info = Pa_GetDeviceInfo(i);
    if (!info) continue;
    std::string name = info->name;
    if (name == "pulse") {
      device = i;
    }
  }
  CHECK(device != 0) << "Could not find Pulse device";

  PaStreamParameters audio_params;
  audio_params.device = device;
  audio_params.channelCount = 1;
  audio_params.sampleFormat = paFloat32;
  audio_params.suggestedLatency = Pa_GetDeviceInfo(audio_params.device)->defaultLowOutputLatency;
  audio_params.hostApiSpecificStreamInfo = nullptr;

  Player player(patch, 8, kSampleFrequency);

  PaStream *stream;
  err = Pa_OpenStream(&stream, nullptr, &audio_params,
                      kSampleFrequency, 512, paClipOff, pa_output_callback,
                      &player);
  CHECK(err == paNoError) << "PortAudio error: " << Pa_GetErrorText(err);

  GUI gui(&patch);
  gui.Start(50, 50);

  err = Pa_StartStream(stream);
  CHECK(err == paNoError) << "PortAudio error: " << Pa_GetErrorText(err);

  if (FLAGS_listmidi) {
    for (int i = 0; i < Pm_CountDevices(); i++) {
      const auto *info = Pm_GetDeviceInfo(i);
      if (info->input) {
        LOG(INFO) << "Input device: #" << i << ": " << info->name
                  << (i == Pm_GetDefaultInputDeviceID() ? " (default)" : "");
      }
    }
    return 0;
  }

  PmDeviceID midi_device = Pm_GetDefaultInputDeviceID();
  if (FLAGS_midi != 0) {
    midi_device = FLAGS_midi;
  }
  CHECK(Pm_GetDeviceInfo(midi_device)) << "Invalid MIDI device: " << midi_device;

  PmStream *midi;

  /* start timer, ms accuracy */
  Pt_Start(1, nullptr, nullptr);

  auto midi_err = Pm_OpenInput(&midi, midi_device,
                               nullptr,
                               100 /* input buffer size */,
                               nullptr /* time proc */,
                               nullptr /* time info */
  );
  /* empty buffer before starting */
  PmEvent buffer[1];
  while (Pm_Poll(midi)) {
    Pm_Read(midi, buffer, 1);
  }

  while (gui.Running()) {
    auto length = Pm_Read(midi, buffer, 1);
    if (length) {
      PmMessage status = Pm_MessageStatus(buffer[0].message);
      PmMessage data1 = Pm_MessageData1(buffer[0].message);
      PmMessage data2 = Pm_MessageData2(buffer[0].message);
      auto event_masked = status & 0xf0;
      if (event_masked == 0x90) {
        auto channel = status & 0xf;
        auto note = data1 & 0x7f;
        auto velocity = data2 & 0x7f;
        player.NoteOn(buffer[0].timestamp, velocity, note);
      } else if (event_masked == 0x80) {
        auto channel = status & 0xf;
        auto note = data1 & 0x7f;
        player.NoteOff(note);
      }
    }
    Pa_Sleep(1);
  }
  Pm_Close(midi);
  Pa_CloseStream(stream);

  gui.Wait();

  return 0;
}
