// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/precise/precise_config.h"
#include <iostream>
#include <functional>
#include <map>
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/strings/string_util.h"
#include "gn/precise/precise_log.h"

namespace precise {

ConfigManager::ConfigManager() {
}

ConfigManager::~ConfigManager() {
}

bool ConfigManager::ReadFile(const std::string& path, std::string& content) {
    if (!base::ReadFileToString(base::FilePath(path), &content)) {
        return false;
    }
    return true;
}

void ConfigManager::LoadHFileList(const base::Value& list) {
    for (const base::Value& value : list.GetList()) {
        config_.modifyHFileList.push_back(value.GetString());
    }
}

void ConfigManager::LoadCFileList(const base::Value& list) {
    for (const base::Value& value : list.GetList()) {
        config_.modifyCFileList.push_back(value.GetString());
    }
}

void ConfigManager::LoadGnFileList(const base::Value& list) {
    for (const base::Value& value : list.GetList()) {
        config_.modifyGnFileList.push_back(value.GetString());
    }
}

void ConfigManager::LoadGnModuleList(const base::Value& list) {
    for (const base::Value& value : list.GetList()) {
        config_.modifyGnModuleList.push_back(value.GetString());
    }
}

void ConfigManager::LoadOtherFileList(const base::Value& list) {
    for (const base::Value& value : list.GetList()) {
        config_.modifyOtherFileList.push_back(value.GetString());
    }
}

void ConfigManager::LoadHFileDepth(const base::Value& depth) {
    config_.hFileDepth = depth.GetInt();
}

void ConfigManager::LoadCFileDepth(const base::Value& depth) {
    config_.cFileDepth = depth.GetInt();
}

void ConfigManager::LoadGnFileDepth(const base::Value& depth) {
    config_.gnFileDepth = depth.GetInt();
}

void ConfigManager::LoadGnModuleDepth(const base::Value& depth) {
    config_.gnModuleDepth = depth.GetInt();
}

void ConfigManager::LoadOtherFileDepth(const base::Value& depth) {
    config_.otherFileDepth = depth.GetInt();
}

void ConfigManager::LoadIgnoreList(const base::Value& list) {
    for (const base::Value& value : list.GetList()) {
        config_.ignoreList.push_back(value.GetString());
    }
}

void ConfigManager::LoadMaxRangeList(const base::Value& list) {
    for (const base::Value& value : list.GetList()) {
        config_.maxRangeList.push_back(value.GetString());
    }
}

void ConfigManager::LoadModifyFilesPath(const base::Value& value) {
    config_.modifyFilesPath = value.GetString();
}

void ConfigManager::LoadGnModificationsPath(const base::Value& value) {
    config_.gnModificationsPath = value.GetString();
}

void ConfigManager::LoadPreciseResultPath(const base::Value& value) {
    config_.preciseResultPath = value.GetString();
}

void ConfigManager::LoadPreciseLogPath(const base::Value& value) {
    config_.preciseLogPath = value.GetString();
}

void ConfigManager::LoadPreciseLogLevel(const base::Value& value) {
    config_.preciseLogLevel = value.GetString();
}

void ConfigManager::LoadTestOnly(const base::Value& value) {
    config_.testOnly = value.GetBool();
}

void ConfigManager::LoadTargetTypeList(const base::Value& list) {
    for (const base::Value& value : list.GetList()) {
        config_.targetTypeList.push_back(value.GetString());
    }
}

void ConfigManager::LoadIncludeParentTargets(const base::Value& list) {
    for (const base::Value& value : list.GetList()) {
        config_.includeParentTargets.insert(value.GetString());
    }
}

void ConfigManager::LoadExcludeParentTargets(const base::Value& list) {
    for (const base::Value& value : list.GetList()) {
        config_.excludeParentTargets.insert(value.GetString());
    }
}

void ConfigManager::LoadHeaderCheckerMaxDepth(const base::Value& value) {
    config_.headerCheckerMaxDepth = value.GetInt();
}

void ConfigManager::LoadEnableHeaderChecker(const base::Value& value) {
    config_.enableHeaderChecker = value.GetBool();
}

void ConfigManager::LoadHeaderCheckerMaxFileCount(const base::Value& value) {
    config_.headerCheckerMaxFileCount = value.GetInt();
}

bool ConfigManager::LoadConfig(const std::string& configPath) {
    std::string configContent;
    if (!ReadFile(configPath, configContent)) {
        LogMessage("ERROR", "Load precise config failed.");
        return false;
    }

    std::unique_ptr<base::Value> config = base::JSONReader::ReadAndReturnError(
        configContent, base::JSONParserOptions::JSON_PARSE_RFC,
        nullptr, nullptr, nullptr, nullptr);

    if (!config) {
        LogMessage("ERROR", "Read precise config json failed.");
        return false;
    }

    const base::DictionaryValue* configDict;
    if (!config->GetAsDictionary(&configDict)) {
        LogMessage("ERROR", "Get precise config dictionary failed.");
        return false;
    }

    std::map<std::string, std::function<void(const base::Value&)>> configMap = {
        {"h_file_depth", [this](const base::Value& v) { LoadHFileDepth(v); }},
        {"c_file_depth", [this](const base::Value& v) { LoadCFileDepth(v); }},
        {"gn_file_depth", [this](const base::Value& v) { LoadGnFileDepth(v); }},
        {"gn_module_depth", [this](const base::Value& v) { LoadGnModuleDepth(v); }},
        {"other_file_depth", [this](const base::Value& v) { LoadOtherFileDepth(v); }},
        {"test_only", [this](const base::Value& v) { LoadTestOnly(v); }},
        {"target_type_list", [this](const base::Value& v) { LoadTargetTypeList(v); }},
        {"ignore_list", [this](const base::Value& v) { LoadIgnoreList(v); }},
        {"max_range_list", [this](const base::Value& v) { LoadMaxRangeList(v); }},
        {"modify_files_path", [this](const base::Value& v) { LoadModifyFilesPath(v); }},
        {"gn_modifications_path", [this](const base::Value& v) { LoadGnModificationsPath(v); }},
        {"precise_result_path", [this](const base::Value& v) { LoadPreciseResultPath(v); }},
        {"precise_log_path", [this](const base::Value& v) { LoadPreciseLogPath(v); }},
        {"precise_log_level", [this](const base::Value& v) { LoadPreciseLogLevel(v); }},
        {"include_parent_targets", [this](const base::Value& v) { LoadIncludeParentTargets(v); }},
        {"exclude_parent_targets", [this](const base::Value& v) { LoadExcludeParentTargets(v); }},
        {"header_checker_max_depth", [this](const base::Value& v) { LoadHeaderCheckerMaxDepth(v); }},
        {"enable_header_checker", [this](const base::Value& v) { LoadEnableHeaderChecker(v); }},
        {"header_checker_max_file_count", [this](const base::Value& v) { LoadHeaderCheckerMaxFileCount(v); }}
    };

    for (auto kv : configDict->DictItems()) {
        auto iter = configMap.find(kv.first);
        if (iter != configMap.end()) {
            iter->second(kv.second);
        }
    }

    config_.preciseConfig = configPath;
    return true;
}

bool ConfigManager::LoadModifyList(const std::string& modifyFilesPath) {
    std::string modifyListContent;
    if (!ReadFile(modifyFilesPath, modifyListContent)) {
        LogMessage("ERROR", "Load modify file list failed.");
        return false;
    }

    std::unique_ptr<base::Value> modifyList = base::JSONReader::ReadAndReturnError(
        modifyListContent, base::JSONParserOptions::JSON_PARSE_RFC,
        nullptr, nullptr, nullptr, nullptr);

    if (!modifyList) {
        LogMessage("ERROR", "Read modify file json failed.");
        return false;
    }

    const base::DictionaryValue* modifyListDict;
    if (!modifyList->GetAsDictionary(&modifyListDict)) {
        LogMessage("ERROR", "Get modify file dictionary failed.");
        return false;
    }

    std::map<std::string, std::function<void(const base::Value&)>> modifyMap = {
        {"h_file", [this](const base::Value& v) { LoadHFileList(v); }},
        {"c_file", [this](const base::Value& v) { LoadCFileList(v); }},
        {"gn_file", [this](const base::Value& v) { LoadGnFileList(v); }},
        {"gn_module", [this](const base::Value& v) { LoadGnModuleList(v); }},
        {"other_file", [this](const base::Value& v) { LoadOtherFileList(v); }},
    };

    for (auto kv : modifyListDict->DictItems()) {
        auto iter = modifyMap.find(kv.first);
        if (iter != modifyMap.end()) {
            iter->second(kv.second);
        }
    }

    config_.modifyFilesPath = modifyFilesPath;

    return true;
}

void ConfigManager::PrintConfigInfo() const {
    std::cout << "========== Precise Build Configuration ==========" << std::endl;
    std::cout << "Depth Settings:" << std::endl;
    std::cout << "  - h_file_depth: " << config_.hFileDepth << std::endl;
    std::cout << "  - c_file_depth: " << config_.cFileDepth << std::endl;
    std::cout << "  - gn_file_depth: " << config_.gnFileDepth << std::endl;
    std::cout << "  - gn_module_depth: " << config_.gnModuleDepth << std::endl;
    std::cout << "  - other_file_depth: " << config_.otherFileDepth << std::endl;
    std::cout << "Filter Settings:" << std::endl;
    std::cout << "  - test_only: " << (config_.testOnly ? "true" : "false") << std::endl;
    std::cout << "  - target_type_list: " << config_.targetTypeList.size() << " types" << std::endl;
    std::cout << "  - ignore_list: " << config_.ignoreList.size() << " items" << std::endl;
    std::cout << "  - max_range_list: " << config_.maxRangeList.size() << " items" << std::endl;
    std::cout << "  - include_parent_targets: " << config_.includeParentTargets.size() << " items" << std::endl;
    std::cout << "  - exclude_parent_targets: " << config_.excludeParentTargets.size() << " items" << std::endl;
    std::cout << "Modified Files:" << std::endl;
    std::cout << "  - Header files: " << config_.modifyHFileList.size() << std::endl;
    std::cout << "  - C/C++ files: " << config_.modifyCFileList.size() << std::endl;
    std::cout << "  - GN files: " << config_.modifyGnFileList.size() << std::endl;
    std::cout << "  - GN modules: " << config_.modifyGnModuleList.size() << std::endl;
    std::cout << "  - Other files: " << config_.modifyOtherFileList.size() << std::endl;
    std::cout << "  - Total modifications: " << (config_.modifyHFileList.size() + config_.modifyCFileList.size() +
        config_.modifyGnFileList.size() + config_.modifyGnModuleList.size() + config_.modifyOtherFileList.size()) << std::endl;
    std::cout << "Paths:" << std::endl;
    std::cout << "  - modify_files_path: " << config_.modifyFilesPath << std::endl;
    std::cout << "  - gn_modifications_path: " << config_.gnModificationsPath << std::endl;
    std::cout << "  - precise_result_path: " << config_.preciseResultPath << std::endl;
    std::cout << "  - precise_log_path: " << config_.preciseLogPath << std::endl;
    std::cout << "  - precise_log_level: " << config_.preciseLogLevel << std::endl;
    std::cout << "HeaderChecker Settings:" << std::endl;
    std::cout << "  - enable_header_checker: " << (config_.enableHeaderChecker ? "true" : "false") << std::endl;
    std::cout << "  - header_checker_max_depth: " << config_.headerCheckerMaxDepth << std::endl;
    std::cout << "  - header_checker_max_file_count: " << config_.headerCheckerMaxFileCount << std::endl;
    std::cout << "==================================================" << std::endl;
}

}  // namespace precise