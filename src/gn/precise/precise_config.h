// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GN_PRECISE_PRECISE_CONFIG_H_
#define GN_PRECISE_PRECISE_CONFIG_H_

#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <climits>
#include <base/values.h>

namespace precise {

// Configuration structure containing all configuration items
struct PreciseConfig {
    int hFileDepth = INT_MAX;
    int cFileDepth = INT_MAX;
    int gnFileDepth = INT_MAX;
    int gnModuleDepth = INT_MAX;
    int otherFileDepth = INT_MAX;
    bool testOnly = false;
    std::string preciseConfig;
    std::string modifyFilesPath;
    std::string gnModificationsPath;  // Path to GN file modification list
    std::string preciseResultPath;
    std::string preciseLogPath;
    std::string preciseLogLevel = "INFO";
    std::vector<std::string> targetTypeList;
    std::vector<std::string> modifyHFileList;
    std::vector<std::string> modifyCFileList;
    std::vector<std::string> modifyGnFileList;
    std::vector<std::string> modifyGnModuleList;
    std::vector<std::string> modifyOtherFileList;
    std::vector<std::string> ignoreList;
    std::vector<std::string> maxRangeList;
    std::unordered_set<std::string> includeParentTargets;
    std::unordered_set<std::string> excludeParentTargets;
};

// Configuration manager class
class ConfigManager {
public:
    ConfigManager();
    ~ConfigManager();

    // Load configuration file
    bool LoadConfig(const std::string& configPath);

    // Load modification file list
    bool LoadModifyList(const std::string& modifyFilesPath);

    // Get configuration
    const PreciseConfig& GetConfig() const { return config_; }

private:
    PreciseConfig config_;

    // Private methods to load various configurations
    void LoadHFileList(const base::Value& list);
    void LoadCFileList(const base::Value& list);
    void LoadGnFileList(const base::Value& list);
    void LoadGnModuleList(const base::Value& list);
    void LoadOtherFileList(const base::Value& list);
    void LoadHFileDepth(const base::Value& depth);
    void LoadCFileDepth(const base::Value& depth);
    void LoadGnFileDepth(const base::Value& depth);
    void LoadGnModuleDepth(const base::Value& depth);
    void LoadOtherFileDepth(const base::Value& depth);
    void LoadIgnoreList(const base::Value& list);
    void LoadMaxRangeList(const base::Value& list);
    void LoadModifyFilesPath(const base::Value& value);
    void LoadGnModificationsPath(const base::Value& value);
    void LoadPreciseResultPath(const base::Value& value);
    void LoadPreciseLogPath(const base::Value& value);
    void LoadPreciseLogLevel(const base::Value& value);
    void LoadTestOnly(const base::Value& value);
    void LoadTargetTypeList(const base::Value& list);
    void LoadIncludeParentTargets(const base::Value& list);
    void LoadExcludeParentTargets(const base::Value& list);

    // Helper function to read file content
    bool ReadFile(const std::string& path, std::string& content);
};

}  // namespace precise

#endif  // GN_PRECISE_PRECISE_CONFIG_H_