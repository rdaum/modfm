#include <GLFW/glfw3.h>
#include <gflags/gflags.h>
#include <glog/logging.h>

#include "gui.h"
#include "midi.h"
#include "oscillator.h"
#include "player.h"

namespace {
constexpr int kSampleFrequency = 44100;
}  // namespace

DEFINE_int32(midi, 0, "MIDI device to use for input. If not set, use default.");
DEFINE_string(device, "pulse", "Name of audio output device to use");

int pa_output_callback(const void *in_buffer, void *out_buffer,
                       unsigned long frames_per_buffer,
                       const PaStreamCallbackTimeInfo *time_info,
                       PaStreamCallbackFlags status_flags, void *user_data) {
  auto *player = (Player *)user_data;
  return player->Perform(in_buffer, out_buffer, frames_per_buffer, time_info,
                         status_flags);
}

int main(int argc, char *argv[]) {
  FLAGS_logtostderr = true;
  google::InitGoogleLogging(argv[0]);
  google::ParseCommandLineFlags(&argc, &argv, false);

  glfwInit();

  LOG(INFO) << "Good morning.";

  Patch patch{{GeneratorPatch::Default()}};

  LOG(INFO) << "Initializing PortAudio";
  PaError err = Pa_Initialize();
  CHECK(err == paNoError) << "PortAudio error: " << Pa_GetErrorText(err);

  PaDeviceIndex device = 0;
  for (int i = 0, end = Pa_GetDeviceCount(); i != end; ++i) {
    PaDeviceInfo const *info = Pa_GetDeviceInfo(i);
    if (!info) continue;
    std::string name = info->name;
    if (name == FLAGS_device) {
      device = i;
      break;
    }
  }
  CHECK(device != 0) << "Could not find device: " << FLAGS_device;

  PaStreamParameters audio_params;
  audio_params.device = device;
  audio_params.channelCount = 1;
  audio_params.sampleFormat = paFloat32;
  audio_params.suggestedLatency =
      Pa_GetDeviceInfo(audio_params.device)->defaultLowOutputLatency;
  audio_params.hostApiSpecificStreamInfo = nullptr;

  Player player(&patch, 8, kSampleFrequency);

  PaStream *stream;
  err = Pa_OpenStream(&stream, nullptr, &audio_params, kSampleFrequency, 512,
                      paClipOff, pa_output_callback, &player);
  CHECK_EQ(err, paNoError) << "PortAudio error: " << Pa_GetErrorText(err);

  // Set up the midi receiver and open the default device or what was passed in.
  MIDIReceiver midi_receiver;
  if (FLAGS_midi)
    CHECK(midi_receiver.OpenDevice(FLAGS_midi).ok())
        << "Unable to open MIDI device";
  else
    CHECK(midi_receiver.OpenDefaultDevice().ok())
        << "Unable to open MIDI device";

  // Wire in note on / off events to the player.
  midi_receiver.NoteOffSignal.connect(&Player::NoteOff, &player);
  midi_receiver.NoteOnSignal.connect(&Player::NoteOn, &player);

  GUI gui(&patch, &midi_receiver);
  gui.Start(50, 50);

  err = Pa_StartStream(stream);
  CHECK_EQ(err, paNoError) << "PortAudio error: " << Pa_GetErrorText(err);

  LOG(INFO) << "Started PortAudio on device #" << audio_params.device;

  /* start timer, ms accuracy */
  Pt_Start(1, nullptr, nullptr);

  CHECK(midi_receiver.Start().ok()) << "Unable to start MIDI device";
  while (gui.Running()) {
    Pa_Sleep(10000);
  }
  LOG(INFO) << "Closing...";
  CHECK(midi_receiver.Close().ok()) << "Unable to close MIDI device";
  CHECK(midi_receiver.Stop().ok()) << "Unable to stop MIDI device";

  CHECK_EQ(Pa_StopStream(stream), paNoError);
  CHECK_EQ(Pa_CloseStream(stream), paNoError);

  gui.Wait();

  LOG(INFO) << "Done.";

  return 0;
}
