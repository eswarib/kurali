#pragma once
#ifndef RECORDER_THREAD_H
#define RECORDER_THREAD_H

#include <thread>
#include <atomic>
#include <memory>
#include <string>
#include "Config.h"
#include "concurrentQueue.h"
#include "AudioEvent.h"
#include "RecorderEvent.h"

class RecorderThread
{
public:
	RecorderThread(const Config& config,
				   ConcurrentQueue<std::shared_ptr<AudioEvent>>& audioQueue,
				   ConcurrentQueue<std::shared_ptr<RecorderEvent>>& recorderEventQueue);
	~RecorderThread();
	void start();
	void stop();
private:
	void run();
	void doStopRecording();
	std::thread mThread;
	std::atomic<bool> mRunning;
	const Config& mConfig;
	ConcurrentQueue<std::shared_ptr<AudioEvent>>& mAudioQueue;
	ConcurrentQueue<std::shared_ptr<RecorderEvent>>& mRecorderEventQueue;
	// Recording state: Idle <-> Recording. Source: trigger (hold or double-tap) or cmd (hold only).
	bool mIsRecording{false};
	std::string mActiveTriggerKey; // which key started recording (trigger or cmd)
	enum class RecordingSource { None, Trigger, Cmd };
	RecordingSource mRecordingSource{RecordingSource::None};
};

#endif // RECORDER_THREAD_H

