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
    std::cout << "Precise config modify files path : " << config_.modifyFilesPath << std::endl;
}

void ConfigManager::LoadGnModificationsPath(const base::Value& value) {
    config_.gnModificationsPath = value.GetString();
    std::cout << "Precise config gn modifications path : " << config_.gnModificationsPath << std::endl;
}

void ConfigManager::LoadPreciseResultPath(const base::Value& value) {
    config_.preciseResultPath = value.GetString();
    std::cout << "Precise config result path : " << config_.preciseResultPath << std::endl;
}

void ConfigManager::LoadPreciseLogPath(const base::Value& value) {
    config_.preciseLogPath = value.GetString();
    std::cout << "Precise config log path : " << config_.preciseLogPath << std::endl;
}

void ConfigManager::LoadPreciseLogLevel(const base::Value& value) {
    config_.preciseLogLevel = value.GetString();
    std::cout << "Precise config log level : " << config_.preciseLogLevel << std::endl;
}

void ConfigManager::LoadTestOnly(const base::Value& value) {
    config_.testOnly = value.GetBool();
    std::cout << "Precise config testonly : " << config_.testOnly << std::endl;
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

bool ConfigManager::LoadConfig(const std::string& configPath) {
    std::string configContent;
    if (!ReadFile(configPath, configContent)) {
        std::cout << "Load precise config failed." << std::endl;
        return false;
    }

    std::unique_ptr<base::Value> config = base::JSONReader::ReadAndReturnError(
        configContent, base::JSONParserOptions::JSON_PARSE_RFC,
        nullptr, nullptr, nullptr, nullptr);

    if (!config) {
        std::cout << "Read precise config json failed." << std::endl;
        return false;
    }

    const base::DictionaryValue* configDict;
    if (!config->GetAsDictionary(&configDict)) {
        std::cout << "Get precise config dictionary failed." << std::endl;
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
        {"exclude_parent_targets", [this](const base::Value& v) { LoadExcludeParentTargets(v); }}
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
        std::cout << "Load modify file list failed." << std::endl;
        return false;
    }

    std::unique_ptr<base::Value> modifyList = base::JSONReader::ReadAndReturnError(
        modifyListContent, base::JSONParserOptions::JSON_PARSE_RFC,
        nullptr, nullptr, nullptr, nullptr);

    if (!modifyList) {
        std::cout << "Read modify file json failed." << std::endl;
        return false;
    }

    const base::DictionaryValue* modifyListDict;
    if (!modifyList->GetAsDictionary(&modifyListDict)) {
        std::cout << "Get modify file dictionary failed." << std::endl;
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

}  // namespace precise