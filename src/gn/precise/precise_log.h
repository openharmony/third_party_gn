// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GN_PRECISE_PRECISE_LOG_H_
#define GN_PRECISE_PRECISE_LOG_H_

#include <string>
#include <vector>
#include <fstream>
#include <memory>

namespace precise {

// Log manager class supporting real-time writing
class LogManager {
public:
    LogManager();
    ~LogManager();

    // Initialize log system
    void Initialize(const std::string& logPath, const std::string& logLevel);

    // Check if log level should be recorded
    bool ShouldLog(const std::string& level) const;

    // Record log message (real-time write)
    void LogMessage(const std::string& level, const std::string& message);

    // Close log file
    void Close();

private:
    std::string logPath_;
    std::string currentLogLevel_;
    std::unique_ptr<std::ofstream> logFile_;

    // Write log entry to file
    void WriteToFile(const std::string& logEntry);
};

// Global log manager
extern std::unique_ptr<LogManager> gLogManager;

// Initialize real-time logging system
void InitializeRealTimeLog(const std::string& logPath, const std::string& logLevel);

// Log message with specified level
void LogMessage(const std::string& level, const std::string& message);

}  // namespace precise

#endif  // GN_PRECISE_PRECISE_LOG_H_