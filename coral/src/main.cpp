#include "Config.h"
#include "Utils.h"
#include "concurrentQueue.h"
#include "AudioEvent.h"
#include "RecorderEvent.h"
#include "Recorder.h"
#include "TranscriberThread.h"
#include "InjectorThread.h"
#include "RecorderThread.h"
#include "KeyDetector.h"
#include "DoubleTapKeyDetector.h"
#include "PushToTalkRecordEventThread.h"
#include "ContinuousRecordEventThread.h"
#include "PushToTalkCmdEventThread.h"
#include "ConfigReloadThread.h"
#include "TextInjector.h"
#include <memory>
#include <iostream>
#include "Logger.h"
#include <filesystem>
#include <cstdlib>
#include "version.h"
#include <iostream>
#if defined(_WIN32)
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#endif

// --- SIGUSR1 bridge: signal handler sets flag, ConfigReloadThread polls it ---
#if !defined(_WIN32)
static std::atomic<bool> g_configReloadRequested{false};

extern "C" void handleSIGUSR1(int)
{
    g_configReloadRequested.store(true, std::memory_order_release);
}
#endif

int main(int argc, char* argv[])
{
    std::cout << "Backend version: " << APP_VERSION
              << ", Build date: " << BUILD_DATE
              << std::endl;

#if !defined(_WIN32)
    // Install SIGUSR1 handler early, before Electron can send it
    struct sigaction sa{};
    sa.sa_handler = handleSIGUSR1;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGUSR1, &sa, nullptr);
#endif

    try {
        std::string configPath;
        if (argc > 1) {
            configPath = argv[1];
        }
        else
        {
            std::string home = Utils::getHomeDir();
            if (!home.empty()) {
                configPath = home + "/.kurali/conf/config.json";
            }
            else
            {
                std::cerr << "Could not determine home directory for default config path." << std::endl;
                return 1;
            }
        }
#if defined(_WIN32)
        {
            char exePathWin[4096];
            if (GetModuleFileNameA(NULL, exePathWin, sizeof(exePathWin)) > 0) {
                std::filesystem::path exeDir = std::filesystem::path(exePathWin).parent_path();
                std::filesystem::path exeConf = exeDir / "conf" / "config.json";
                if (std::filesystem::exists(exeConf)) {
                    configPath = exeConf.string();
                }
            }
        }
#endif
        if (!std::filesystem::exists(configPath)) {
            INFO("Config file does not exist at $HOME/.kurali, this may be the first run of the application, copying the default config file");
            Config::copyConfigFileOnFirstRun();
        }

        Config config(configPath);

        Logger::getInstance().init(config.getLogFilePath(), config.getDebugLevel());

        INFO("Reading configuration from: " + configPath);
        INFO("Kurali is up and running");
        ConcurrentQueue<std::shared_ptr<AudioEvent>> audioEventQueue;
        ConcurrentQueue<std::shared_ptr<TextEvent>> textEventQueue;
        ConcurrentQueue<std::shared_ptr<RecorderEvent>> recorderEventQueue;
        KeyDetector keyDetector;
        DoubleTapKeyDetector doubleTapKeyDetector(keyDetector, config.getDoubleTapWindowMs());
        doubleTapKeyDetector.setTriggerKey(config.getTriggerKey());
        doubleTapKeyDetector.setCmdTriggerKey(config.getCmdTriggerKey());

#if !defined(_WIN32)
        if (!keyDetector.hasEvdevAccess())
        {
            std::cout << "NEED_INPUT_GROUP" << std::endl;
            WARN("evdev not available — key detection may not work on Wayland. "
                 "Add user to 'input' group: sudo usermod -aG input $USER");
        }

        {
            int uinputFd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
            if (uinputFd < 0)
            {
                std::cout << "NEED_UINPUT_RULE" << std::endl;
                WARN("/dev/uinput not accessible — text injection via virtual keyboard unavailable. "
                     "A udev rule is needed: KERNEL==\"uinput\", GROUP=\"input\", MODE=\"0660\"");
            }
            else
            {
                close(uinputFd);
            }
        }
#endif

        RecorderThread recorderThread(config, audioEventQueue, recorderEventQueue);
        TranscriberThread transcriberThread(config, audioEventQueue, textEventQueue);
        InjectorThread injectorThread(config, textEventQueue);

        PushToTalkRecordEventThread pushToTalkRecordEventThread(config, keyDetector, recorderEventQueue);
        ContinuousRecordEventThread continuousRecordEventThread(config, keyDetector, doubleTapKeyDetector, recorderEventQueue);
        PushToTalkCmdEventThread pushToTalkCmdEventThread(config, keyDetector, recorderEventQueue);

#if !defined(_WIN32)
        ConfigReloadThread configReloadThread(config,
                                              pushToTalkRecordEventThread,
                                              continuousRecordEventThread,
                                              pushToTalkCmdEventThread,
                                              doubleTapKeyDetector,
                                              recorderEventQueue,
                                              g_configReloadRequested);
#endif

        recorderThread.start();
        bool isPushToTalk = (config.getTriggerMode() == "pushToTalk");
        if (isPushToTalk)
            pushToTalkRecordEventThread.start();
        else
            continuousRecordEventThread.start();
        if (!config.getCmdTriggerKey().empty())
            pushToTalkCmdEventThread.start();

        transcriberThread.start();
        injectorThread.start();

#if !defined(_WIN32)
        configReloadThread.start();
#endif

        std::cout << "BACKEND_READY" << std::endl;
        INFO("All threads started — backend is ready");

        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

#if !defined(_WIN32)
        configReloadThread.stop();
#endif
        pushToTalkRecordEventThread.stop();
        continuousRecordEventThread.stop();
        pushToTalkCmdEventThread.stop();
        recorderThread.stop();
        transcriberThread.stop();
        injectorThread.stop();
        Recorder::terminatePortAudio();
        INFO("Kurali shutting down.");
    } catch (const std::exception& ex) {
        ERROR(std::string("model file is not found. Shutting down the application: ") + ex.what());
        return 1;
    }
    return 0;
}
