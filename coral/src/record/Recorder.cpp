#include "Recorder.h"
#include <portaudio.h>
#include <sndfile.h>
#include <iostream>
#include <vector>
#include <algorithm>
#include <cctype>
#include <cmath>
#include "stderrSilencer.h"
#include "stdoutSilencer.h"
#include "Logger.h"

#define RECORD_SAMPLE_RATE 48000  // Use 48kHz for recording (what the mic supports)
#define OUTPUT_SAMPLE_RATE 16000  // Output 16kHz for Whisper
#define FRAMES_PER_BUFFER 512
#define NUM_CHANNELS 1

struct RecordCallbackData {
    std::vector<float>* samples;
    float amplification;
};

Recorder* Recorder::sInstance = nullptr;
static bool s_paInitialized = false;

bool Recorder::ensurePortAudioInitialized() {
    if (s_paInitialized) return true;
    PaError err = Pa_Initialize();
    if (err == paNoError) {
        s_paInitialized = true;
        return true;
    }
    std::cerr << "PortAudio init failed: " << Pa_GetErrorText(err) << std::endl;
    return false;
}

Recorder* Recorder::getInstance()
{
	if (sInstance) {
		return sInstance;
	}
	sInstance = new Recorder();
	return sInstance;
}

void Recorder::terminatePortAudio() {
    if (s_paInitialized) {
        Pa_Terminate();
        s_paInitialized = false;
    }
}

void Recorder::setAudioParams(float amplification, float noiseGateThreshold) {
    mAudioAmplification = std::max(0.5f, std::min(20.f, amplification));
    mNoiseGateThreshold = std::max(0.f, std::min(0.1f, noiseGateThreshold));
}

Recorder::~Recorder() {
    if (stream_) {
        Pa_StopStream(stream_);
        Pa_CloseStream(stream_);
        stream_ = nullptr;
    }
    terminatePortAudio();
}

static int recordCallback(const void* inputBuffer, void* /*outputBuffer*/,
                         unsigned long framesPerBuffer,
                         const PaStreamCallbackTimeInfo*,
                         PaStreamCallbackFlags,
                         void* userData)
{
    RecordCallbackData* data = (RecordCallbackData*)userData;
    std::vector<float>* recordedSamples = data->samples;
    float amp = data->amplification;
    const float* in = (const float*)inputBuffer;
    if (in != nullptr) {
        for (unsigned long i = 0; i < framesPerBuffer; i++) {
            float amplified = in[i] * amp;
            amplified = std::max(-1.0f, std::min(1.0f, amplified));
            recordedSamples->push_back(amplified);
        }
    }
    return paContinue;
}

// Simple linear interpolation resampling from 48kHz to 16kHz
std::vector<float> resample48kTo16k(const std::vector<float>& input) {
    std::vector<float> output;
    output.reserve(input.size() / 3); // 48kHz to 16kHz is 3:1 ratio
    
    for (size_t i = 0; i < input.size(); i += 3) {
        if (i < input.size()) {
            output.push_back(input[i]);
        }
    }
    
    return output;
}

void Recorder::startRecording()
{
    DEBUG(3, "Recorder::startRecording --> Entering");
    stderrSilencer silencer;
    stdoutSilencer stdoutSilencer;
    if (!ensurePortAudioInitialized()) return;
    mRecordedSamplesVec.clear();

    // Find microphone input device - try different strategies
    int numDevices = Pa_GetDeviceCount();
    PaDeviceIndex inputDevice = paNoDevice;
    
    DEBUG(3, "Available audio devices:");
    for (int i = 0; i < numDevices; i++) {
        const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(i);
        if (deviceInfo->maxInputChannels > 0) {
            DEBUG(3, "  Device " + std::to_string(i) + ": " + deviceInfo->name 
                      + " (Input channels: " + std::to_string(deviceInfo->maxInputChannels) + ")");
        }
    }
    
    // Try to find the best microphone input
    PaDeviceIndex pulseIdx = paNoDevice;
    PaDeviceIndex defaultIdx = paNoDevice;
    PaDeviceIndex sysdefaultIdx = paNoDevice;
    PaDeviceIndex firstInputIdx = paNoDevice;
    PaDeviceIndex micNameIdx = paNoDevice;

    for (int i = 0; i < numDevices; i++) {
        const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(i);
        if (deviceInfo->maxInputChannels > 0) {
            const char* deviceName = deviceInfo->name;
            std::string nameLower(deviceName);
            std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);

            if (firstInputIdx == paNoDevice) firstInputIdx = i;
            if (nameLower == std::string("pulse") || nameLower.find("pulse") != std::string::npos) pulseIdx = i;
            if (nameLower == std::string("default") || nameLower.find("default") != std::string::npos) defaultIdx = i;
            if (nameLower.find("sysdefault") != std::string::npos) sysdefaultIdx = i;
            if (nameLower.find("mic") != std::string::npos || nameLower.find("microphone") != std::string::npos) micNameIdx = i;
        }
    }

    if (pulseIdx != paNoDevice) {
        inputDevice = pulseIdx;
        DEBUG(3, std::string("Selected input device (pulse): ") + Pa_GetDeviceInfo(inputDevice)->name);
    } else if (defaultIdx != paNoDevice) {
        inputDevice = defaultIdx;
        DEBUG(3, std::string("Selected input device (default): ") + Pa_GetDeviceInfo(inputDevice)->name);
    } else if (sysdefaultIdx != paNoDevice) {
        inputDevice = sysdefaultIdx;
        DEBUG(3, std::string("Selected input device (sysdefault): ") + Pa_GetDeviceInfo(inputDevice)->name);
    } else if (micNameIdx != paNoDevice) {
        inputDevice = micNameIdx;
        DEBUG(3, std::string("Selected input device (mic name match): ") + Pa_GetDeviceInfo(inputDevice)->name);
    } else if (firstInputIdx != paNoDevice) {
        inputDevice = firstInputIdx;
        DEBUG(3, std::string("Selected input device (first input-capable): ") + Pa_GetDeviceInfo(inputDevice)->name);
    }

    // If no microphone found, fall back to PortAudio default input device
    if (inputDevice == paNoDevice) {
        inputDevice = Pa_GetDefaultInputDevice();
        DEBUG(3, "No microphone detected, using PortAudio default input device");
    } else {
        const PaDeviceInfo* selectedDevice = Pa_GetDeviceInfo(inputDevice);
        DEBUG(3, std::string("Using microphone: ") + selectedDevice->name);
    }
     
    // Set up input parameters
    PaStreamParameters inputParameters;
    inputParameters.device = inputDevice;
    inputParameters.channelCount = 1;
    inputParameters.sampleFormat = paFloat32;
    inputParameters.suggestedLatency = Pa_GetDeviceInfo(inputDevice)->defaultLowInputLatency;
    inputParameters.hostApiSpecificStreamInfo = nullptr;
    
    static RecordCallbackData cbData;
    cbData.samples = &mRecordedSamplesVec;
    cbData.amplification = mAudioAmplification;
    PaError err = Pa_OpenStream(&stream_, &inputParameters, nullptr,
                               RECORD_SAMPLE_RATE, FRAMES_PER_BUFFER, paClipOff, recordCallback, &cbData);
    
    if (err != paNoError) {
        std::cerr << "Error opening audio stream: " << Pa_GetErrorText(err) << std::endl;
        return;
    }
    
    err = Pa_StartStream(stream_);
    if (err != paNoError) {
        std::cerr << "Error starting audio stream: " << Pa_GetErrorText(err) << std::endl;
        Pa_CloseStream(stream_);
        stream_ = nullptr;
        return;
    }
}

void Recorder::stopRecording(const std::string& filename)
{
    if (!stream_) return;
    Pa_StopStream(stream_);
    Pa_CloseStream(stream_);
    stream_ = nullptr;

    // Use the filename as provided by the caller (RecorderThread)
    std::string outFilename = filename;

    // Apply noise gate: zero out samples below threshold
    if (mNoiseGateThreshold > 0.f) {
        for (float& s : mRecordedSamplesVec) {
            if (std::fabs(s) < mNoiseGateThreshold) s = 0.f;
        }
    }

    // Resample from 48kHz to 16kHz
    std::vector<float> resampledSamples = resample48kTo16k(mRecordedSamplesVec);

    SF_INFO sfinfo;
    sfinfo.channels = 1;
    sfinfo.samplerate = OUTPUT_SAMPLE_RATE;  // Use 16kHz for output
    sfinfo.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;

    SNDFILE* outfile = sf_open(outFilename.c_str(), SFM_WRITE, &sfinfo);
    if (!outfile) {
        std::cerr << "Error opening audio file for writing.\n";
        return;
    }

    std::vector<short> intData(resampledSamples.size());
    for (size_t i = 0; i < resampledSamples.size(); ++i) {
        intData[i] = static_cast<short>(resampledSamples[i] * 32767);
    }

    sf_write_short(outfile, intData.data(), intData.size());
    sf_close(outfile);
}
