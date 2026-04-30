#ifndef TRANSCRIBER_H
#define TRANSCRIBER_H
#include <string>
#include <vector>

struct whisper_context;

class Transcriber
{
public:
    std::string transcribeAudio(const std::string& wavFile, const std::string& whisperModelPath, const std::string& language = "en");
    static Transcriber* getInstance();
    virtual ~Transcriber();
private:
    static Transcriber* sInstance;
    Transcriber() = default;
    Transcriber(const Transcriber&) = delete;
    Transcriber& operator=(const Transcriber&) = delete;
    bool whisper_pcmf32_from_wav(const std::string& path, std::vector<float>& outPCM);

    // Load (or reload on path change) the whisper context. Returns true on success.
    bool ensureModel(const std::string& modelPath);

    whisper_context* _ctx = nullptr;
    std::string _loadedModelPath;

    // Reusable PCM buffer — avoids heap alloc/free on every transcription.
    // Only grows; vector::resize does not shrink capacity.
    std::vector<float> _pcmBuf;
};

#endif
