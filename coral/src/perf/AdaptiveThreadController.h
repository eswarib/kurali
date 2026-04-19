#ifndef ADAPTIVE_THREAD_CONTROLLER_H
#define ADAPTIVE_THREAD_CONTROLLER_H

#include <chrono>
#include <mutex>
#include <string>
#include <vector>

class AdaptiveThreadController
{
public:
    static AdaptiveThreadController& getInstance();

    void init();

    int getThreadCount() const;

    // Call after each whisper inference with the elapsed time.
    // Samples are collected in memory; the controller evaluates the trend
    // every _windowSize transcriptions and adjusts thread count accordingly.
    void reportInferenceLatency(long long whisper_ms, long long audio_duration_ms);

private:
    AdaptiveThreadController();
    ~AdaptiveThreadController() = default;
    AdaptiveThreadController(const AdaptiveThreadController&) = delete;
    AdaptiveThreadController& operator=(const AdaptiveThreadController&) = delete;

    void evaluate();
    void scaleDown();
    void scaleUp();
    void recalibrateWindowSize();

    // Persistence: ~/.coral/conf/thread_controller.json
    std::string statePath() const;
    void loadState();
    void saveState() const;

    mutable std::mutex _mutex;

    int _threadCount;
    int _minThreads;
    int _maxThreads;

    static constexpr int kDefaultThreadCount = 6;

    // Dynamic window size, auto-calibrated from daily throughput.
    int _windowSize;
    static constexpr int kMinWindowSize     = 10;
    static constexpr int kMaxWindowSize     = 500;
    static constexpr int kDefaultWindowSize = 10;
    // Target: evaluate ~50 times per day (daily_count / 50).
    static constexpr int kTargetEvaluationsPerDay = 50;

    std::vector<float> _rtfSamples;

    // Thresholds applied to the p75 (75th percentile) of the window.
    static constexpr float kLagThreshold    = 0.85f;
    static constexpr float kStableThreshold = 0.60f;

    static constexpr int kScaleDownStep = 2;
    static constexpr int kScaleUpStep   = 1;

    int _consecutiveStableWindows;
    static constexpr int kStableWindowsNeeded = 2;

    std::chrono::steady_clock::time_point _lastAdjustTime;
    static constexpr long long kMinAdjustIntervalMs = 15000;

    // Daily throughput tracking.
    long long _totalTranscriptionsToday;
    std::string _todayDate;

    // Persisted across restarts.
    long long _lastDailyCount;
};

#endif // ADAPTIVE_THREAD_CONTROLLER_H
