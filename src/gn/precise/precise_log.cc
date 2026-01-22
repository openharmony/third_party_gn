// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/precise/precise_log.h"
#include <map>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace precise {

// Global log manager instance
std::unique_ptr<LogManager> gLogManager;

LogManager::LogManager() {
}

LogManager::~LogManager() {
    Close();
}

void LogManager::Initialize(const std::string& logPath, const std::string& logLevel) {
    logPath_ = logPath;
    currentLogLevel_ = logLevel;

    // Open log file
    if (!logPath_.empty()) {
        logFile_ = std::make_unique<std::ofstream>(logPath_, std::ios::app);
        if (!logFile_->is_open()) {
            std::cerr << "Failed to open log file: " << logPath_ << std::endl;
        } else {
            // Write initialization marker
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            *logFile_ << "\n=== Log Session Started: " << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") << " ===" << std::endl;
            logFile_->flush();
        }
    }
}

bool LogManager::ShouldLog(const std::string& level) const {
    // Log level mapping: DEBUG=0, INFO=1, WARN=2, ERROR=3, FATAL=4
    std::map<std::string, int> logLevels = {
        {"DEBUG", 0},
        {"INFO", 1},
        {"WARN", 2},
        {"ERROR", 3},
        {"FATAL", 4}
    };

    auto currentIt = logLevels.find(currentLogLevel_);
    auto checkIt = logLevels.find(level);

    if (currentIt == logLevels.end() || checkIt == logLevels.end()) {
        return level == "INFO"; // Default to INFO and above
    }

    return checkIt->second >= currentIt->second;
}

void LogManager::WriteToFile(const std::string& logEntry) {
    if (logFile_ && logFile_->is_open()) {
        *logFile_ << logEntry << std::endl;
        logFile_->flush();  // Real-time flush to file
    }
}

void LogManager::LogMessage(const std::string& level, const std::string& message) {
    if (!ShouldLog(level)) {
        return;
    }

    // Generate timestamp
    auto currentTime = std::chrono::system_clock::now();
    auto currentTime_t = std::chrono::system_clock::to_time_t(currentTime);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        currentTime.time_since_epoch()) % 1000;

    std::ostringstream oss;
    oss << "[" << std::put_time(std::localtime(&currentTime_t), "%Y-%m-%d %H:%M:%S");
    oss << "." << std::setfill('0') << std::setw(3) << ms.count() << "]";
    oss << "[" << level << "] " << message;

    std::string logEntry = oss.str();

    // Write to file
    WriteToFile(logEntry);
}

void LogManager::Close() {
    if (logFile_ && logFile_->is_open()) {
        auto endTime = std::chrono::system_clock::now();
        auto endTime_t = std::chrono::system_clock::to_time_t(endTime);
        *logFile_ << "=== Log Session Ended: " << std::put_time(std::localtime(&endTime_t), "%Y-%m-%d %H:%M:%S") << " ===" << std::endl;
        logFile_->flush();
        logFile_->close();
        logFile_.reset();
    }
}

// Initialize real-time logging system
void InitializeRealTimeLog(const std::string& logPath, const std::string& logLevel) {
    if (!gLogManager) {
        gLogManager = std::make_unique<LogManager>();
    }
    gLogManager->Initialize(logPath, logLevel);
}

// Log message with specified level
void LogMessage(const std::string& level, const std::string& message) {
    if (gLogManager) {
        gLogManager->LogMessage(level, message);
    } else {
        std::cerr << "LogManager not initialized. Falling back to stderr: ["
                  << level << "] " << message << std::endl;
    }
}

}  // namespace precise