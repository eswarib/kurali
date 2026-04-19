#ifndef _AUDIO_EVENT_H_
#define _AUDIO_EVENT_H_
#include<string>
class AudioEvent
{
    public:
        AudioEvent(const std::string& fileName, const std::string& triggerKey);
	virtual ~AudioEvent();
        const std::string& getFileName() const
        {
            return mAudioFileName;
        }
        const std::string& getTriggerKey() const
        {
            return mTriggerKey;
        }
    private:
	std::string mAudioFileName;
        std::string mTriggerKey;
        AudioEvent();
        AudioEvent(AudioEvent& );
	AudioEvent& operator=(AudioEvent& );
};

#endif
