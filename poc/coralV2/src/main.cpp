#include "Config.h"
#include "concurrentQueue.h"
#include "AudioEvent.h"
#include "TranscriberThread.h"
#include "InjectorThread.h"
#include "RecorderThread.h"
#include "KeyDetector.h"
#include "TextInjector.h"
#include <memory>
#include <iostream>
#include "Logger.h"
#include <filesystem>
#include <cstdlib>

int main(int argc, char* argv[]) {
    try {
        std::string configPath;
        if (argc > 1) {
            configPath = argv[1];
        } else {
            const char* home = std::getenv("HOME");
            if (home) {
                configPath = std::string(home) + "/.kurali/conf/config.json";
            } else {
                std::cerr << "Could not determine home directory for default config path." << std::endl;
                return 1;
            }
        }
        if (!std::filesystem::exists(configPath)) {
            std::cerr << "Config file not found: " << configPath << std::endl;
            return 1;
        }
        INFO("Reading configuration from: " + configPath);
        Config config(configPath);
        Logger::getInstance().setLogFile(config.logFilePath);
        Logger::getInstance().setLogLevel(config.debugLevel);
        INFO("Kurali is up and running");
        ConcurrentQueue<std::shared_ptr<AudioEvent>> audioEventQueue;
        ConcurrentQueue<std::shared_ptr<TextEvent>> textEventQueue;
        ConcurrentQueue<std::shared_ptr<TextEvent>> cmdTextEventQueue;
        KeyDetector keyDetector;

        RecorderThread recorderThread(config, audioEventQueue, keyDetector);
        TranscriberThread transcriberThread(config, audioEventQueue, textEventQueue, cmdTextEventQueue);
        InjectorThread injectorThread(config, textEventQueue, cmdTextEventQueue);

        recorderThread.start();
        transcriberThread.start();
        injectorThread.start();

        // Main thread waits indefinitely until externally interrupted
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        recorderThread.stop();
        transcriberThread.stop();
        injectorThread.stop();
        INFO("Kurali shutting down.");
    } catch (const std::exception& ex) {
        ERROR(std::string("model file is not found. Shutting down the application: ") + ex.what());
        return 1;
    }
    return 0;
}

