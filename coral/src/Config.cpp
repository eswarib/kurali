#include "Config.h"
#include <filesystem>
#include <cstdlib>  // for getenv
#include <fstream>
#include <cstdlib>
#if !defined(_WIN32)
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif
#include <vector>
#include "Logger.h"
#include "Utils.h"
#if defined(_WIN32)
#include <windows.h>
#endif

namespace fs = std::filesystem;

const std::string Config::WhisperModelNameSmallEnQ8 = "ggml-small.en-q8_0.bin";
const std::string Config::WhisperModelNameSmallEn = "ggml-small.en.bin";
const std::string Config::WhisperModelNameBaseEn = "ggml-base.en.bin";

Config::Config(const std::string& filePath) : mFilePath(filePath)
{
    loadFromFile();
}

void Config::reload()
{
    std::unique_lock lk(mMutex);
    loadFromFile();
    INFO("Configuration reloaded from: " + mFilePath);
}

void Config::loadFromFile()
{
    std::ifstream file(mFilePath);
    if (!file)
    {
        throw std::runtime_error("Could not open config file: " + mFilePath);
    }

    nlohmann::json j;
    file >> j;
    silenceTimeoutSeconds = j.value("silenceTimeoutSeconds", DefaultSilenceTimeoutSeconds);
    INFO("silenceTimeoutSeconds:  [" + std::to_string(silenceTimeoutSeconds) + "]");
    audioSampleRate = j.value("audioSampleRate", DefaultAudioSampleRate);
    INFO("audioSampleRate:  [" + std::to_string(audioSampleRate) + "]");
    audioChannels = j.value("audioChannels", DefaultAudioChannels);
    INFO("audioChannels:  [" + std::to_string(audioChannels) + "]");
    triggerKey = j.value("triggerKey", DefaultTriggerKey);
    INFO("triggerKey:  [" + triggerKey + "]");
    whisperModelPath = j.value("whisperModelPath", DefaultWhisperModelPath);
    INFO("whisperModelPath:  [" + whisperModelPath + "]");
    debugLevel = j.value("debugLevel", 0);
    INFO("debugLevel:  [" + std::to_string(debugLevel) + "]");

    std::string home = Utils::getHomeDir();
    if (home.empty()) {
        throw std::runtime_error("Could not determine home directory (HOME or USERPROFILE)");
    }
    std::string defaultLogPath = home + "/.kurali/logs/kurali.log";
    logFilePath = j.value("logFilePath", defaultLogPath);

    logFilePath = Utils::expandTilde(logFilePath);

    try
    {
        std::filesystem::path logDir = std::filesystem::path(logFilePath).parent_path();
        if (!std::filesystem::exists(logDir))
        {
            std::filesystem::create_directories(logDir);
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Warning: Could not create log directory: " << e.what() << std::endl;
    }

    INFO("logFilePath:  [" + logFilePath + "]");
    logToConsole = j.value("logToConsole", true);
    INFO("logToConsole:  [" + std::string(logToConsole ? "true" : "false") + "]");
    cmdTriggerKey = j.value("cmdTriggerKey", std::string());
    INFO("cmdTriggerKey:  [" + cmdTriggerKey + "]");
    whisperLanguage = j.value("whisperLanguage", std::string("en"));
    INFO("whisperLanguage:  [" + whisperLanguage + "]");
    triggerMode = j.value("triggerMode", std::string("pushToTalk"));
    if (triggerMode != "pushToTalk" && triggerMode != "continuous") triggerMode = "pushToTalk";
    INFO("triggerMode:  [" + triggerMode + "]");
    doubleTapWindowMs = static_cast<uint32_t>(j.value("doubleTapWindowMs", 300));
    INFO("doubleTapWindowMs:  [" + std::to_string(doubleTapWindowMs) + "]");
    recordWindowSeconds = j.value("recordWindowSeconds", 60);
    INFO("recordWindowSeconds:  [" + std::to_string(recordWindowSeconds) + "]");
    saveAudioToFolder = j.value("saveAudioToFolder", std::string());
    saveAudioToFolder = Utils::expandTilde(saveAudioToFolder);
    INFO("saveAudioToFolder:  [" + (saveAudioToFolder.empty() ? "(empty, delete after transcribe)" : saveAudioToFolder) + "]");
    // audioAmplification and noiseGateThreshold: not read from JSON, use defaults (2.5, 0.001)
}


std::string Config::getWhisperModelPath() const {
    namespace fs = std::filesystem;

    // Snapshot the configured path under shared lock
    std::string configuredPath;
    {
        std::shared_lock lk(mMutex);
        configuredPath = whisperModelPath;
    }

    // 1) Config path (with tilde expansion)
    std::string expandedModelPath = Utils::expandTilde(configuredPath);
    if (!expandedModelPath.empty() && fs::exists(expandedModelPath))
    {
        return expandedModelPath;
    }

    // 2) Build a set of candidate directories and filenames
    std::vector<std::string> candidates;
    auto addCandidatesInDir = [&](const fs::path& dir){
        candidates.push_back((dir / WhisperModelNameBaseEn).string());
        candidates.push_back((dir / WhisperModelNameSmallEn).string());
        candidates.push_back((dir / WhisperModelNameSmallEnQ8).string());
    };

    #if defined(_WIN32)
    char exePathWin[MAX_PATH];
    DWORD got = GetModuleFileNameA(NULL, exePathWin, MAX_PATH);
    if (got > 0 && got < MAX_PATH)
    {
        fs::path exeDir = fs::path(exePathWin).parent_path();
        addCandidatesInDir(exeDir / "model");
        addCandidatesInDir(exeDir / "models");
        addCandidatesInDir(exeDir / "resources" / "models");
    }

    const char* appData = std::getenv("APPDATA");
    if (appData && *appData)
    {
        addCandidatesInDir(fs::path(appData) / "Kurali" / "models");
        addCandidatesInDir(fs::path(appData) / "Coral" / "models");
    }
    const char* localAppData = std::getenv("LOCALAPPDATA");
    if (localAppData && *localAppData)
    {
        addCandidatesInDir(fs::path(localAppData) / "Kurali" / "models");
        addCandidatesInDir(fs::path(localAppData) / "Coral" / "models");
    }
    #else
    const char* appdir = std::getenv("APPDIR");
    if (appdir && *appdir)
    {
        addCandidatesInDir(fs::path(appdir) / "usr/share/coral/models");
    }

    addCandidatesInDir(fs::path("/usr/share/coral/models"));

    char exePath[4096];
    ssize_t len = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
    if (len != -1) {
        exePath[len] = '\0';
        fs::path exeDir = fs::path(exePath).parent_path();
        addCandidatesInDir(exeDir / "../share/coral/models");
        addCandidatesInDir(exeDir / "models");
    }
    #endif

    std::string homeDir = Utils::getHomeDir();
    if (!homeDir.empty())
    {
        addCandidatesInDir(fs::path(homeDir) / ".kurali/models");
        addCandidatesInDir(fs::path(homeDir) / ".coral/models");
    }

    for (const auto& c : candidates)
    {
        if (fs::exists(c))
        {
            INFO("Resolved whisper model path: [" + c + "]");
            return c;
        }
    }

    #if defined(_WIN32)
    std::string fallback;
    const char* appDataFB = std::getenv("APPDATA");
    if (appDataFB && *appDataFB)
    {
        fallback = (fs::path(appDataFB) / "Kurali" / "models" / WhisperModelNameBaseEn).string();
        if (fallback.empty() || !fs::exists(fallback)) {
            fallback = (fs::path(appDataFB) / "Coral" / "models" / WhisperModelNameBaseEn).string();
        }
    }
    if (fallback.empty() || !fs::exists(fallback))
    {
        char exePathFB[MAX_PATH];
        DWORD gotFB = GetModuleFileNameA(NULL, exePathFB, MAX_PATH);
        if (gotFB > 0 && gotFB < MAX_PATH)
        {
            fs::path exeDir = fs::path(exePathFB).parent_path();
            fallback = (exeDir / "models" / WhisperModelNameBaseEn).string();
        }
    }
    #else
    std::string fallback = (appdir && *appdir)
        ? (fs::path(appdir) / "usr/share/coral/models" / WhisperModelNameBaseEn).string()
        : (fs::path("/usr/share/coral/models") / WhisperModelNameBaseEn).string();
    #endif

    if (!fs::exists(fallback))
    {
        ERROR("Whisper model not found. Expected at: " + fallback);
    }
    else
    {
        INFO("Using fallback whisper model: [" + fallback + "]");
    }
    return fallback;
}


void Config::copyConfigFileOnFirstRun()
{
    std::string home = Utils::getHomeDir();
    if (home.empty()) {
        throw std::runtime_error("Could not determine home directory (HOME or USERPROFILE)");
    }
    std::string userConfigDir = home + "/.kurali";
    std::string userConfigPath = userConfigDir + "/conf/config.json";
    std::string legacyUserConfigPath = home + "/.coral/conf/config.json";
    std::string legacyFlatConfigPath = home + "/.coral/config.json";

    if (!fs::exists(userConfigPath)) {
        fs::create_directories(fs::path(userConfigPath).parent_path());

        if (fs::exists(legacyUserConfigPath)) {
            fs::copy_file(legacyUserConfigPath, userConfigPath, fs::copy_options::overwrite_existing);
#if !defined(_WIN32)
            uid_t uid = getuid();
            gid_t gid = getgid();
            chown(userConfigPath.c_str(), uid, gid);
#endif
            return;
        }
        if (fs::exists(legacyFlatConfigPath)) {
            fs::copy_file(legacyFlatConfigPath, userConfigPath, fs::copy_options::overwrite_existing);
#if !defined(_WIN32)
            uid_t uid = getuid();
            gid_t gid = getgid();
            chown(userConfigPath.c_str(), uid, gid);
#endif
            return;
        }

#if defined(_WIN32)
        char exePath[MAX_PATH];
        DWORD got = GetModuleFileNameA(NULL, exePath, MAX_PATH);
        std::string defaultConfigPath;
        if (got > 0 && got < MAX_PATH) {
            fs::path exeDir = fs::path(exePath).parent_path();
            fs::path candidates[] = {
                exeDir / "conf" / "config.json",
                exeDir / "config.json",
                exeDir.parent_path().parent_path() / "coral" / "conf" / "config.json",
            };
            for (const auto& p : candidates) {
                if (fs::exists(p)) {
                    defaultConfigPath = p.string();
                    break;
                }
            }
        }
        if (defaultConfigPath.empty()) {
            throw std::runtime_error("Default config not found. Place config in exeDir/conf/config.json or coral/conf/");
        }
        fs::copy_file(defaultConfigPath, userConfigPath, fs::copy_options::overwrite_existing);
#else
        const char* appdir = std::getenv("APPDIR");
        std::string defaultConfigPath = appdir
            ? std::string(appdir) + "/usr/share/coral/config/config.json"
            : "/usr/share/coral/config/config.json";

        fs::copy_file(defaultConfigPath, userConfigPath, fs::copy_options::overwrite_existing);

        uid_t uid = getuid();
        gid_t gid = getgid();
        chown(userConfigPath.c_str(), uid, gid);
#endif
    }
}
