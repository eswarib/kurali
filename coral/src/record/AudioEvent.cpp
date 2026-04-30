#include "AudioEvent.h"

AudioEvent::AudioEvent(const std::string& fileName, const std::string& triggerKey)
    : mAudioFileName(fileName), mTriggerKey(triggerKey)
{}

AudioEvent::~AudioEvent() = default;
