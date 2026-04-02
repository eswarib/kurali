#include "TranscriberThread.h"
#include "Transcriber.h"
#include "Logger.h"
#include "TextUtils.h"
#include <iostream>
#include <cstdio>
#include <filesystem>

TranscriberThread::TranscriberThread(const Config& config,
                                     ConcurrentQueue<std::shared_ptr<AudioEvent>>& audioQueue,
                                     ConcurrentQueue<std::shared_ptr<TextEvent>>& textQueue)
    : _config(config), _audioQueue(audioQueue), _textQueue(textQueue), _running(false) {}

TranscriberThread::~TranscriberThread() {
    stop();
}

void TranscriberThread::start() {
    _running = true;
    _thread = std::thread(&TranscriberThread::run, this);
}

void TranscriberThread::stop() {
    _running = false;
    if (_thread.joinable()) {
        _thread.join();
    }
}

void TranscriberThread::run() 
{
    INFO("TranscriberThread started, waiting for audio events...");
    try {
        while (_running) 
        {
            INFO("TranscriberThread is going to pop from recorder");
            auto audioEvent = _audioQueue.waitAndPop();
            DEBUG(3, "Transcriber picked up audio event: " + audioEvent->getFileName() + ", trigger key: " + audioEvent->getTriggerKey());
            const std::string& audioFile = audioEvent->getFileName();
            DEBUG(3, "Transcribing audio file: [" + audioFile + "] + whisper model: [" + _config.getWhisperModelPath() + "] + language: [" + _config.getWhisperLanguage() + "]");
            std::cout << "TRANSCRIBING_START" << std::endl;
            std::string text = Transcriber::getInstance()->transcribeAudio(audioFile, _config.getWhisperModelPath(), _config.getWhisperLanguage());
            if (audioEvent->getTriggerKey() == _config.getCmdTriggerKey())
            {
                text = TextUtils::trim(text);
                std::vector<std::string> specialSubstrings{"...", "?", "!"};
                text = TextUtils::removeSpecialSubstrings(text, specialSubstrings);
                TextUtils::toLower(text);
                text += "\n";
                DEBUG(3, "Pushing transcribed text to textEventQueue (cmdTriggerKey)");
            }
            else
            {
                DEBUG(3, "Pushing transcribed text to textEventQueue (triggerKey)");
            }

            if (TextUtils::shouldDiscardTranscript(text))
            {
                DEBUG(3, "Transcript discarded (junk filter), not injecting");
            }
            else
            {
                // Frontend pairs this with INJECTION_DONE to measure queue→inject latency
                std::cout << "INJECT_PENDING" << std::endl;
                _textQueue.push(std::make_shared<TextEvent>(text));
            }

            //transcription done the text inserted in queue, 
            // notify the frontend to stop the animation
            std::cout << "TRANSCRIBING_DONE" << std::endl;
            
            std::string saveFolder = _config.getSaveAudioToFolder();
            if (!saveFolder.empty())
            {
                try
                {
                    std::filesystem::create_directories(saveFolder);
                    std::filesystem::path src(audioFile);
                    std::filesystem::path dst = std::filesystem::path(saveFolder) / src.filename();
                    std::filesystem::copy_file(src, dst, std::filesystem::copy_options::overwrite_existing);
                    std::filesystem::remove(src);
                    DEBUG(3, "Saved audio file to: " + dst.string());
                }
                catch (const std::exception& e)
                {
                    WARN(std::string("Failed to save audio file to ") + saveFolder + ": " + e.what());
                    try { std::filesystem::remove(audioFile); } catch (...) {}
                }
            }
            else if (std::remove(audioFile.c_str()) == 0)
            {
                DEBUG(3, "Deleted audio file: " + audioFile);
            }
            else
            {
                WARN("Failed to delete audio file: " + audioFile);
            }
            
        }
    } catch (const std::exception& ex) {
        ERROR(std::string("Exception in TranscriberThread: ") + ex.what());
    }
}
