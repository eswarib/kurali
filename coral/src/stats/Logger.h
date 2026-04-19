#pragma once
#include <string>
#include <fstream>
#include <mutex>
#include <memory>
#include <iostream>
#include <sstream>
#include <ctime>
#include <vector>

enum class LogLevel {
    ERROR = 0,
    WARN = 1,
    INFO = 2,
    DEBUG1 = 3,
    DEBUG2 = 4,
    DEBUG3 = 5
};

class Logger {
public:
    static Logger& getInstance();
    void init(const std::string& filename, int debugLevel);
    void setDebugLevel(int level);
    void log(LogLevel level, const std::string& msg);
    void logf(LogLevel level, const char* fmt, ...);
    void setLogToConsole(bool enable);
private:
    Logger();
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    void init(const std::string& logFilePath, int debugLevel, bool logToConsole);
    int _currentLevel = 2; // Default INFO
    std::ofstream _logFile;
    std::string _logFilePath;
    std::mutex _mtx;
    bool _logToConsole = true;
    bool _isInitialized = false;
    std::vector<std::pair<LogLevel, std::string>> _preInitMessages;

    void logInternal(LogLevel level, const std::string& msg);
    std::string levelToString(LogLevel level);
    void checkRotate();
    static constexpr size_t MAX_LOG_SIZE = 10 * 1024 * 1024; // 10MB
    static constexpr int MAX_LOG_BACKUPS = 3;
};

#define ERROR(msg)   Logger::getInstance().log(LogLevel::ERROR, msg)
#define WARN(msg)    Logger::getInstance().log(LogLevel::WARN, msg)
#define INFO(msg)    Logger::getInstance().log(LogLevel::INFO, msg)
#define DEBUG(level, msg) Logger::getInstance().log(static_cast<LogLevel>(2 + (level)), msg)