#ifndef RECORDER_H
#define RECORDER_H

#include <string>
#include <portaudio.h>
#include <vector>

class Recorder
{
public:
	void startRecording();
	void stopRecording(const std::string &filename);
	static Recorder * getInstance();
	void setAudioParams(float amplification, float noiseGateThreshold);
	/** Call at app shutdown to release PortAudio. */
	static void terminatePortAudio();
	virtual ~Recorder();
private:
	static bool ensurePortAudioInitialized();
	Recorder(){}
	Recorder(const Recorder&) = delete;
	Recorder& operator=(const Recorder&) = delete;

	static Recorder * sInstance;
	PaStream* stream_ = nullptr;
	std::vector<float> mRecordedSamplesVec;
	float mAudioAmplification = 2.5f;
	float mNoiseGateThreshold = 0.001f;
};

#endif

