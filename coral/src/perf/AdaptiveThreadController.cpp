#include "AdaptiveThreadController.h"
#include "Logger.h"
#include "Utils.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <numeric>
#include <sstream>
#include <string>

// Minimal JSON helpers — avoids pulling in json.hpp for 5 fields.
namespace {

std::string todayStr()
{
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    char buf[16];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d", std::localtime(&t));
    return buf;
}

std::string jsonGet(const std::string& json, const std::string& key)
{
    std::string needle = "\"" + key + "\"";
    auto pos = json.find(needle);
    if (pos == std::string::npos) return "";
    pos = json.find(':', pos + needle.size());
    if (pos == std::string::npos) return "";
    ++pos;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) ++pos;
    if (pos >= json.size()) return "";
    if (json[pos] == '"') {
        auto end = json.find('"', pos + 1);
        return (end != std::string::npos) ? json.substr(pos + 1, end - pos - 1) : "";
    }
    auto end = json.find_first_of(",}\n", pos);
    return json.substr(pos, end - pos);
}

} // namespace

AdaptiveThreadController& AdaptiveThreadController::getInstance()
{
    static AdaptiveThreadController instance;
    return instance;
}

AdaptiveThreadController::AdaptiveThreadController()
    : _threadCount(kDefaultThreadCount),
      _minThreads(2),
      _maxThreads(16),
      _windowSize(kDefaultWindowSize),
      _consecutiveStableWindows(0),
      _lastAdjustTime(std::chrono::steady_clock::now()),
      _totalTranscriptionsToday(0),
      _lastDailyCount(0)
{
    _rtfSamples.reserve(kDefaultWindowSize);
}

std::string AdaptiveThreadController::statePath() const
{
    std::string home = Utils::getHomeDir();
    if (home.empty()) return "";
    return home + "/.coral/conf/thread_controller.json";
}

void AdaptiveThreadController::loadState()
{
    std::string path = statePath();
    if (path.empty()) return;

    std::ifstream in(path);
    if (!in.is_open()) return;

    std::ostringstream ss;
    ss << in.rdbuf();
    std::string json = ss.str();

    std::string savedThreads   = jsonGet(json, "thread_count");
    std::string savedDate      = jsonGet(json, "date");
    std::string savedDaily     = jsonGet(json, "daily_count");
    std::string savedWindow    = jsonGet(json, "window_size");
    std::string savedLastDaily = jsonGet(json, "last_daily_count");

    if (!savedThreads.empty()) {
        int t = std::stoi(savedThreads);
        _threadCount = std::clamp(t, _minThreads, _maxThreads);
    }
    if (!savedWindow.empty()) {
        int w = std::stoi(savedWindow);
        _windowSize = std::clamp(w, kMinWindowSize, kMaxWindowSize);
    }
    if (!savedLastDaily.empty()) {
        _lastDailyCount = std::stoll(savedLastDaily);
    }

    _todayDate = todayStr();
    if (savedDate == _todayDate && !savedDaily.empty()) {
        _totalTranscriptionsToday = std::stoll(savedDaily);
    } else {
        // New day: carry forward yesterday's count for window calibration.
        if (!savedDaily.empty()) {
            _lastDailyCount = std::stoll(savedDaily);
        }
        _totalTranscriptionsToday = 0;
    }

    INFO("AdaptiveThreadController: loaded state — threads=" + std::to_string(_threadCount)
         + " window=" + std::to_string(_windowSize)
         + " today_count=" + std::to_string(_totalTranscriptionsToday)
         + " last_daily=" + std::to_string(_lastDailyCount));
}

void AdaptiveThreadController::saveState() const
{
    std::string path = statePath();
    if (path.empty()) return;

    try {
        std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    } catch (...) {
        return;
    }

    std::ofstream out(path, std::ios::trunc);
    if (!out.is_open()) return;

    out << "{\n"
        << "  \"thread_count\": " << _threadCount << ",\n"
        << "  \"date\": \"" << _todayDate << "\",\n"
        << "  \"daily_count\": " << _totalTranscriptionsToday << ",\n"
        << "  \"window_size\": " << _windowSize << ",\n"
        << "  \"last_daily_count\": " << _lastDailyCount << "\n"
        << "}\n";
}

void AdaptiveThreadController::init()
{
    std::lock_guard<std::mutex> lock(_mutex);

    _threadCount = kDefaultThreadCount;
    _minThreads = 2;
    _maxThreads = 16;
    _windowSize = kDefaultWindowSize;
    _todayDate = todayStr();
    _totalTranscriptionsToday = 0;
    _lastDailyCount = 0;

    loadState();

    _rtfSamples.clear();
    _rtfSamples.reserve(_windowSize);
    _consecutiveStableWindows = 0;
    _lastAdjustTime = std::chrono::steady_clock::now();

    INFO("AdaptiveThreadController: threads=" + std::to_string(_threadCount)
         + " range=[" + std::to_string(_minThreads) + "," + std::to_string(_maxThreads) + "]"
         + " window_size=" + std::to_string(_windowSize));
}

int AdaptiveThreadController::getThreadCount() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _threadCount;
}

void AdaptiveThreadController::reportInferenceLatency(long long whisper_ms, long long audio_duration_ms)
{
    if (audio_duration_ms <= 0) return;

    float rtf = static_cast<float>(whisper_ms) / static_cast<float>(audio_duration_ms);

    std::lock_guard<std::mutex> lock(_mutex);

    // Handle day rollover.
    std::string now = todayStr();
    if (now != _todayDate) {
        _lastDailyCount = _totalTranscriptionsToday;
        _totalTranscriptionsToday = 0;
        _todayDate = now;
        recalibrateWindowSize();
    }

    ++_totalTranscriptionsToday;
    _rtfSamples.push_back(rtf);

    DEBUG(3, "AdaptiveThreadController: sample " + std::to_string(_rtfSamples.size())
          + "/" + std::to_string(_windowSize)
          + " rtf=" + std::to_string(rtf)
          + " threads=" + std::to_string(_threadCount));

    if (static_cast<int>(_rtfSamples.size()) >= _windowSize) {
        evaluate();
        _rtfSamples.clear();
    }

    // Persist periodically (every windowSize samples).
    if (_totalTranscriptionsToday % _windowSize == 0) {
        saveState();
    }
}

void AdaptiveThreadController::recalibrateWindowSize()
{
    long long reference = _lastDailyCount;
    if (reference <= 0) return;

    int newWindow = static_cast<int>(reference / kTargetEvaluationsPerDay);
    newWindow = std::clamp(newWindow, kMinWindowSize, kMaxWindowSize);

    if (newWindow != _windowSize) {
        INFO("AdaptiveThreadController: recalibrated window_size " + std::to_string(_windowSize)
             + " -> " + std::to_string(newWindow)
             + " (yesterday_count=" + std::to_string(_lastDailyCount) + ")");
        _windowSize = newWindow;
        _rtfSamples.clear();
        _rtfSamples.reserve(_windowSize);
    }
}

void AdaptiveThreadController::evaluate()
{
    std::vector<float> sorted = _rtfSamples;
    std::sort(sorted.begin(), sorted.end());

    size_t p75idx = (sorted.size() * 3) / 4;
    float p75 = sorted[std::min(p75idx, sorted.size() - 1)];

    float mean = std::accumulate(sorted.begin(), sorted.end(), 0.0f)
                 / static_cast<float>(sorted.size());

    auto now = std::chrono::steady_clock::now();
    long long sinceLast = std::chrono::duration_cast<std::chrono::milliseconds>(
                              now - _lastAdjustTime).count();

    INFO("AdaptiveThreadController: window[" + std::to_string(sorted.size()) + "] — mean_rtf="
         + std::to_string(mean) + " p75_rtf=" + std::to_string(p75)
         + " threads=" + std::to_string(_threadCount)
         + " today_count=" + std::to_string(_totalTranscriptionsToday));

    if (sinceLast < kMinAdjustIntervalMs) {
        DEBUG(3, "AdaptiveThreadController: skipping adjustment, only "
              + std::to_string(sinceLast) + "ms since last change");
        return;
    }

    bool adjusted = false;

    if (p75 > kLagThreshold) {
        scaleDown();
        _consecutiveStableWindows = 0;
        _lastAdjustTime = now;
        adjusted = true;
    } else if (p75 < kStableThreshold) {
        ++_consecutiveStableWindows;
        if (_consecutiveStableWindows >= kStableWindowsNeeded) {
            scaleUp();
            _consecutiveStableWindows = 0;
            _lastAdjustTime = now;
            adjusted = true;
        } else {
            INFO("AdaptiveThreadController: stable window " + std::to_string(_consecutiveStableWindows)
                 + "/" + std::to_string(kStableWindowsNeeded) + ", waiting before scale-up");
        }
    } else {
        _consecutiveStableWindows = 0;
    }

    if (adjusted) {
        saveState();
    }
}

void AdaptiveThreadController::scaleDown()
{
    int prev = _threadCount;
    _threadCount = std::max(_minThreads, _threadCount - kScaleDownStep);
    if (_threadCount != prev) {
        INFO("AdaptiveThreadController: lag trend, threads " + std::to_string(prev)
             + " -> " + std::to_string(_threadCount));
    }
}

void AdaptiveThreadController::scaleUp()
{
    int prev = _threadCount;
    _threadCount = std::min(_maxThreads, _threadCount + kScaleUpStep);
    if (_threadCount != prev) {
        INFO("AdaptiveThreadController: stable trend, threads " + std::to_string(prev)
             + " -> " + std::to_string(_threadCount));
    }
}
