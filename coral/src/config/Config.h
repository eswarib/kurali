#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <fstream>
#include <stdexcept>
#include <cstdint>
#include <shared_mutex>
#include "json.hpp"

class Config
{
public:
    static constexpr int DefaultSilenceTimeoutSeconds = 300;
    static constexpr int DefaultAudioSampleRate = 16000;
    static constexpr int DefaultAudioChannels = 1;
    static constexpr const char* DefaultTriggerKey = "Fn";
    static constexpr const char* DefaultWhisperModelPath = "~/usr/share/coral/models/ggml-base.en.bin";
    static constexpr const char* WhisperModelURLSmallQ8 = "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-small.en-q8_0.bin";
    static const std::string WhisperModelNameSmallEnQ8;
    static const std::string WhisperModelNameSmallEn;
    static const std::string WhisperModelNameBaseEn;

    static void copyConfigFileOnFirstRun();
    static void downloadModelIfNotPresent();

    explicit Config(const std::string& filePath);

    /** Re-read the config file. Thread-safe (exclusive lock). */
    void reload();

    int getSilenceTimeoutSeconds() const { std::shared_lock lk(mMutex); return silenceTimeoutSeconds; }
    int getAudioSampleRate() const { std::shared_lock lk(mMutex); return audioSampleRate; }
    int getAudioChannels() const { std::shared_lock lk(mMutex); return audioChannels; }
    std::string getTriggerKey() const { std::shared_lock lk(mMutex); return triggerKey; }
    std::string getWhisperModelPath() const;
    int getDebugLevel() const { std::shared_lock lk(mMutex); return debugLevel; }
    std::string getLogFilePath() const { std::shared_lock lk(mMutex); return logFilePath; }
    bool getLogToConsole() const { std::shared_lock lk(mMutex); return logToConsole; }
    std::string getCmdTriggerKey() const { std::shared_lock lk(mMutex); return cmdTriggerKey; }
    std::string getWhisperLanguage() const { std::shared_lock lk(mMutex); return whisperLanguage; }
    std::string getTriggerMode() const { std::shared_lock lk(mMutex); return triggerMode; }
    uint32_t getDoubleTapWindowMs() const { std::shared_lock lk(mMutex); return doubleTapWindowMs; }
    int getRecordWindowSeconds() const { std::shared_lock lk(mMutex); return recordWindowSeconds; }
    std::string getSaveAudioToFolder() const { std::shared_lock lk(mMutex); return saveAudioToFolder; }
    float getAudioAmplification() const { std::shared_lock lk(mMutex); return audioAmplification; }
    float getNoiseGateThreshold() const { std::shared_lock lk(mMutex); return noiseGateThreshold; }

private:
    void loadFromFile();

    mutable std::shared_mutex mMutex;
    std::string mFilePath;
    int silenceTimeoutSeconds;
    int audioSampleRate;
    int audioChannels;
    std::string triggerKey;
    std::string whisperModelPath;
    int debugLevel;
    std::string logFilePath;
    bool logToConsole;
    std::string cmdTriggerKey;
    std::string whisperLanguage;
    std::string triggerMode;   // "pushToTalk" | "continuous"
    uint32_t doubleTapWindowMs{300};
    int recordWindowSeconds{15};
    std::string saveAudioToFolder;
    float audioAmplification{2.5f};
    float noiseGateThreshold{0.001f};
};

#endif // CONFIG_H
