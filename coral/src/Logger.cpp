#include "Logger.h"
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <cstdarg>
#include <algorithm>
#include <vector>
#include <iomanip>
#include <ctime>
#include <filesystem>

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

// Constructor: Does not open any file. Only sets defaults.
Logger::Logger() : _isInitialized(false), 
                   _logToConsole(true), 
                   _currentLevel(2) 
{}

Logger::~Logger() 
{
    if (_logFile.is_open()) {
        _logFile.close();
    }
}

void Logger::init(const std::string& filename, int debugLevel) {
    std::lock_guard<std::mutex> lock(_mtx);
    if (_isInitialized) {
        return; // Prevent re-initialization
    }

    _logFilePath = filename;
    if (debugLevel >= 0 && debugLevel <= 5) {
        _currentLevel = debugLevel;
    }

    try {
        // Ensure the directory for the log file exists
        std::filesystem::path logPath(_logFilePath);
        if (logPath.has_parent_path()) {
            std::filesystem::create_directories(logPath.parent_path());
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Fatal: Failed to create directory for log file '" << _logFilePath
                  << "'. Check permissions. Reason: " << e.what() << std::endl;
        _logFilePath = "kurali.log"; // Fallback to default
        std::cerr << "Falling back to log file: " << _logFilePath << std::endl;
    }

    _logFile.open(_logFilePath, std::ios_base::app);
    if (!_logFile.is_open()) {
        std::cerr << "Failed to open log file: " << _logFilePath << std::endl;
    }

    _isInitialized = true;

    // Flush any messages that were logged before initialization
    for (const auto& msg_pair : _preInitMessages) {
        logInternal(msg_pair.first, msg_pair.second);
    }
    _preInitMessages.clear();
}

void Logger::log(LogLevel level, const std::string& msg) {
    std::lock_guard<std::mutex> lock(_mtx);
    if (!_isInitialized) {
        // Buffer messages until initialized
        _preInitMessages.emplace_back(level, msg);
        // As a fallback, also print to console
        if (_logToConsole) {
            std::cout << "[" << levelToString(level) << "] " << msg << std::endl;
        }
        return;
    }

    if (static_cast<int>(level) <= _currentLevel) {
        logInternal(level, msg);
    }
}

void Logger::logf(LogLevel level, const char* fmt, ...) {
    if (static_cast<int>(level) > _currentLevel) {
        return;
    }
    std::vector<char> buffer(1024);
    va_list args;
    va_start(args, fmt);
    int needed = vsnprintf(buffer.data(), buffer.size(), fmt, args);
    va_end(args);
    if (needed >= static_cast<int>(buffer.size())) {
        buffer.resize(needed + 1);
        va_start(args, fmt);
        vsnprintf(buffer.data(), buffer.size(), fmt, args);
        va_end(args);
    }
    // Route through the main log function to respect initialization state
    log(level, std::string(buffer.data()));
}

void Logger::setLogToConsole(bool enable) {
    std::lock_guard<std::mutex> lock(_mtx);
    _logToConsole = enable;
}

void Logger::setDebugLevel(int level) {
    std::lock_guard<std::mutex> lock(_mtx);
    if (level >= 0 && level <= 5) {
        _currentLevel = level;
    }
}

// No longer needs a lock, as public methods are now locking.
void Logger::logInternal(LogLevel level, const std::string& msg) {
    checkRotate();
    std::ostringstream oss;
    std::time_t t = std::time(nullptr);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
    oss << "[" << buf << "] [" << levelToString(level) << "] " << msg;
    if (msg.empty() || msg.back() != '\n') {
        oss << std::endl;
    }
    std::string out = oss.str();
    if (_logFile.is_open()) {
        _logFile << out;
        _logFile.flush();
    }
    if (_logToConsole) {
        std::cout << out;
        std::cout.flush();
    }
}

void Logger::checkRotate() {
    if (_logFile.is_open()) {
        if (_logFile.tellp() > static_cast<std::streampos>(MAX_LOG_SIZE)) {
            _logFile.close();
            for (int i = MAX_LOG_BACKUPS - 1; i > 0; --i) {
                std::string oldPath = _logFilePath + "." + std::to_string(i);
                std::string newPath = _logFilePath + "." + std::to_string(i + 1);
                std::rename(oldPath.c_str(), newPath.c_str());
            }
            std::rename(_logFilePath.c_str(), (_logFilePath + ".1").c_str());
            _logFile.open(_logFilePath, std::ios_base::app);
        }
    }
}

std::string Logger::levelToString(LogLevel level) {
    switch (level) {
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::WARN: return "WARN";
        case LogLevel::INFO: return "INFO";
        case LogLevel::DEBUG1: return "DEBUG1";
        case LogLevel::DEBUG2: return "DEBUG2";
        case LogLevel::DEBUG3: return "DEBUG3";
        default: return "UNKNOWN";
    }
}

// ... rest of the original code ...