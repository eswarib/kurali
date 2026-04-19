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
    std::string defaultLogPath = home + "/.coral/logs/coral.log";
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
    recordWindowSeconds = j.value("recordWindowSeconds", 15);
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

    // 1) Config path used as-is (absolute / relative / tilde-expanded).
    std::string expandedModelPath = Utils::expandTilde(configuredPath);
    if (!expandedModelPath.empty() && fs::exists(expandedModelPath))
    {
        return expandedModelPath;
    }

    // 2) Build the ordered list of dirs to search for models. Same set is used
    //    for the bare-filename lookup below and for the implicit fallback scan.
    //    Order matters: ~/.coral/models (where the postinst symlinks land) is
    //    checked first so user customizations win over the system bundle.
    std::vector<fs::path> dirs;
    std::string homeDir = Utils::getHomeDir();
    if (!homeDir.empty()) {
        dirs.push_back(fs::path(homeDir) / ".coral/models");
    }
    #if defined(_WIN32)
    char exePathWin[MAX_PATH];
    DWORD got = GetModuleFileNameA(NULL, exePathWin, MAX_PATH);
    if (got > 0 && got < MAX_PATH) {
        fs::path exeDir = fs::path(exePathWin).parent_path();
        dirs.push_back(exeDir / "model");
        dirs.push_back(exeDir / "models");
        dirs.push_back(exeDir / "resources" / "models");
    }
    if (const char* appData = std::getenv("APPDATA");      appData && *appData)      dirs.push_back(fs::path(appData)      / "Coral" / "models");
    if (const char* localAppData = std::getenv("LOCALAPPDATA"); localAppData && *localAppData) dirs.push_back(fs::path(localAppData) / "Coral" / "models");
    #else
    if (const char* appdir = std::getenv("APPDIR"); appdir && *appdir) {
        dirs.push_back(fs::path(appdir) / "usr/share/coral/models");
    }
    dirs.push_back(fs::path("/usr/share/coral/models"));
    char exePath[4096];
    ssize_t len = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
    if (len != -1) {
        exePath[len] = '\0';
        fs::path exeDir = fs::path(exePath).parent_path();
        dirs.push_back(exeDir / "../share/coral/models");
        dirs.push_back(exeDir / "models");
    }
    #endif

    // 2a) If the configured value is a bare filename (no path separators), look
    //     it up in each known dir. This is what the Electron UI saves when the
    //     user picks a bundled/installed model from the dropdown.
    auto isBareFilename = [](const std::string& s) {
        if (s.empty()) return false;
        if (s.find('/') != std::string::npos) return false;
    #if defined(_WIN32)
        if (s.find('\\') != std::string::npos) return false;
    #endif
        return true;
    };
    if (isBareFilename(configuredPath)) {
        for (const auto& d : dirs) {
            fs::path p = d / configuredPath;
            if (fs::exists(p)) {
                INFO("Resolved whisper model from configured filename: [" + p.string() + "]");
                return p.string();
            }
        }
        WARN("Configured whisperModelPath '" + configuredPath +
             "' not found in any known model dir; falling back to implicit search");
    }

    // 3) Implicit fallback: try the known shipped model names in each dir.
    //    Order: q8 (preferred default — fastest) -> small -> base.
    std::vector<std::string> candidates;
    auto addCandidatesInDir = [&](const fs::path& dir){
        candidates.push_back((dir / WhisperModelNameSmallEnQ8).string());
        candidates.push_back((dir / WhisperModelNameSmallEn).string());
        candidates.push_back((dir / WhisperModelNameBaseEn).string());
    };
    for (const auto& d : dirs) addCandidatesInDir(d);

    for (const auto& c : candidates)
    {
        if (fs::exists(c))
        {
            INFO("Resolved whisper model path: [" + c + "]");
            return c;
        }
    }

    // Last-resort fallback path for the error log: where we *expect* the
    // default model to be on a working install. Prefer q8 (the shipped default).
    #if defined(_WIN32)
    std::string fallback;
    if (const char* appDataFB = std::getenv("APPDATA"); appDataFB && *appDataFB) {
        fallback = (fs::path(appDataFB) / "Coral" / "models" / WhisperModelNameSmallEnQ8).string();
    }
    if (fallback.empty() || !fs::exists(fallback)) {
        char exePathFB[MAX_PATH];
        DWORD gotFB = GetModuleFileNameA(NULL, exePathFB, MAX_PATH);
        if (gotFB > 0 && gotFB < MAX_PATH) {
            fs::path exeDir = fs::path(exePathFB).parent_path();
            fallback = (exeDir / "models" / WhisperModelNameSmallEnQ8).string();
        }
    }
    #else
    const char* appdirFB = std::getenv("APPDIR");
    std::string fallback = (appdirFB && *appdirFB)
        ? (fs::path(appdirFB) / "usr/share/coral/models" / WhisperModelNameSmallEnQ8).string()
        : (fs::path("/usr/share/coral/models") / WhisperModelNameSmallEnQ8).string();
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
    std::string userConfigDir = home + "/.coral";
    std::string userConfigPath = userConfigDir + "/conf/config.json";

    if (!fs::exists(userConfigPath)) {
        fs::create_directories(fs::path(userConfigPath).parent_path());

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
