// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <filesystem>
#include <fstream>
#include <iostream>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "gn/build_settings.h"
#include "gn/config.h"
#include "gn/filesystem_utils.h"
#include "gn/functions.h"
#include "gn/label_ptr.h"
#include "gn/parse_tree.h"
#include "gn/precise/precise.h"
#include "gn/settings.h"
#include "gn/substitution_writer.h"
#include "gn/target.h"
#include "gn/value.h"

namespace fs = std::filesystem;

PreciseManager* PreciseManager::instance_ = nullptr;
static int hFileDepth_ = INT_MAX;
static int cFileDepth_ = INT_MAX;
static int gnFileDepth_ = INT_MAX;
static int gnModuleDepth_ = INT_MAX;
static bool testOnly_ = false;
static std::string outDir_;
static std::string preciseConfig_;
static std::string modifyFilesPath_;
static std::string preciseResultPath_;
static std::string preciseLogPath_;
static std::vector<std::string> targetTypeList_;
static std::vector<std::string> modifyHFileList_;
static std::vector<std::string> modifyCFileList_;
static std::vector<std::string> modifyGnFileList_;
static std::vector<std::string> modifyGnModuleList_;
static std::vector<std::string> ignoreList_;
static std::vector<std::string> maxRangeList_;

static bool ReadFile(base::FilePath path, std::string& content)
{
    if (!base::ReadFileToString(path, &content)) {
        return false;
    }
    return true;
}

static void LoadHFileList(const base::Value& list)
{
    for (const base::Value& value : list.GetList()) {
        modifyHFileList_.push_back(value.GetString());
    }
}

static void LoadCFileList(const base::Value& list)
{
    for (const base::Value& value : list.GetList()) {
        modifyCFileList_.push_back(value.GetString());
    }
}

static void LoadGnFileList(const base::Value& list)
{
    for (const base::Value& value : list.GetList()) {
        modifyGnFileList_.push_back(value.GetString());
    }
}

static void LoadGnModuleList(const base::Value& list)
{
    for (const base::Value& value : list.GetList()) {
        modifyGnModuleList_.push_back(value.GetString());
    }
}

static void LoadHFileDepth(const base::Value& depth)
{
    hFileDepth_ = depth.GetInt();
}

static void LoadCFileDepth(const base::Value& depth)
{
    cFileDepth_ = depth.GetInt();
}

static void LoadGnFileDepth(const base::Value& depth)
{
    gnFileDepth_ = depth.GetInt();
}

static void LoadGnModuleDepth(const base::Value& depth)
{
    gnModuleDepth_ = depth.GetInt();
}

static void LoadIgnoreList(const base::Value& list)
{
    for (const base::Value& value : list.GetList()) {
        ignoreList_.push_back(value.GetString());
    }
}

static void LoadMaxRangeList(const base::Value& list)
{
    for (const base::Value& value : list.GetList()) {
        maxRangeList_.push_back(value.GetString());
    }
}

static void LoadModifyFilesPath(const base::Value& value)
{
    modifyFilesPath_ = value.GetString();
    std::cout << "Precise config modify files path : " << modifyFilesPath_ << std::endl;
}

static void LoadPreciseResultPath(const base::Value& value)
{
    preciseResultPath_ = value.GetString();
    std::cout << "Precise config result path : " << preciseResultPath_ << std::endl;
}

static void LoadPreciseLogPath(const base::Value& value)
{
    preciseLogPath_ = value.GetString();
    std::cout << "Precise config log path : " << preciseLogPath_ << std::endl;
}

static void LoadTestOnly(const base::Value& value)
{
    testOnly_ = value.GetBool();
    std::cout << "Precise config testonly : " << testOnly_ << std::endl;
}

static void LoadTargetTypeList(const base::Value& list)
{
    for (const base::Value& value : list.GetList()) {
        targetTypeList_.push_back(value.GetString());
    }
}

static std::map<std::string, std::function<void(const base::Value& value)>> modifyMap_ = {
    { "h_file", LoadHFileList },
    { "c_file", LoadCFileList },
    { "gn_file", LoadGnFileList },
    { "gn_module", LoadGnModuleList },
};

static std::map<std::string, std::function<void(const base::Value& value)>> configMap_ = {
    { "h_file_depth", LoadHFileDepth },
    { "c_file_depth", LoadCFileDepth },
    { "gn_file_depth", LoadGnFileDepth },
    { "gn_module_depth", LoadGnModuleDepth },
    { "test_only", LoadTestOnly},
    { "target_type_list", LoadTargetTypeList },
    { "ignore_list", LoadIgnoreList },
    { "max_range_list", LoadMaxRangeList },
    { "modify_files_path", LoadModifyFilesPath },
    { "precise_result_path", LoadPreciseResultPath },
    { "precise_log_path", LoadPreciseLogPath },
};

static void LoadModifyList()
{
    std::string modifyListContent;
    if (!ReadFile(base::FilePath(modifyFilesPath_), modifyListContent)) {
        std::cout << "Load modify file list failed." << std::endl;
        return;
    }
    const base::DictionaryValue* modifyListDict;
    std::unique_ptr<base::Value> modifyList = base::JSONReader::ReadAndReturnError(modifyListContent,
        base::JSONParserOptions::JSON_PARSE_RFC, nullptr, nullptr, nullptr, nullptr);
    if (!modifyList) {
        std::cout << "Read modify file json failed." << std::endl;
        return;
    }
    if (!modifyList->GetAsDictionary(&modifyListDict)) {
        std::cout << "Get modify file dictionary failed." << std::endl;
        return;
    }

    for (const auto kv : modifyListDict->DictItems()) {
        auto iter = modifyMap_.find(kv.first);
        if (iter != modifyMap_.end()) {
            iter->second(kv.second);
        }
    }
}

static void LoadPreciseConfig()
{
    std::string configContent;
    if (!ReadFile(base::FilePath(preciseConfig_), configContent)) {
        std::cout << "Load precise config failed." << std::endl;
        return;
    }
    const base::DictionaryValue* configDict;
    std::unique_ptr<base::Value> config = base::JSONReader::ReadAndReturnError(configContent,
        base::JSONParserOptions::JSON_PARSE_RFC, nullptr, nullptr, nullptr, nullptr);
    if (!config) {
        std::cout << "Read precise config json failed." << std::endl;
        return;
    }
    if (!config->GetAsDictionary(&configDict)) {
        std::cout << "Get precise config dictionary failed." << std::endl;
        return;
    }

    for (const auto kv : configDict->DictItems()) {
        auto iter = configMap_.find(kv.first);
        if (iter != configMap_.end()) {
            iter->second(kv.second);
        }
    }
}

PreciseManager::PreciseManager(const std::string& outDir, const std::string& preciseConfig)
{
    std::cout << "Read precise config from " << preciseConfig << std::endl;
    outDir_ = outDir;
    preciseConfig_ = preciseConfig;
    LoadPreciseConfig();
    LoadModifyList();
}

void PreciseManager::AddModule(std::string name, Node* node)
{
    moduleList_[name] = node;
}

bool PreciseManager::IsIgnore(const std::string& name)
{
    auto result = std::find(ignoreList_.begin(), ignoreList_.end(), name);
    if (result != ignoreList_.end()) {
        return true;
    }
    return false;
}

bool PreciseManager::IsInMaxRange(const std::string& name)
{
    if (maxRangeList_.empty()) {
        return true;
    }
    auto result = std::find(maxRangeList_.begin(), maxRangeList_.end(), name);
    if (result != maxRangeList_.end()) {
        return true;
    }
    return false;
}

bool PreciseManager::IsDependent(const Node* node)
{
    int size = node->GetFromList().size();
    if (size == 0) {
        return false;
    }
    if (size == 1) {
        const Item* item = ((Module* )(node->GetFromList()[0]))->GetItem();
        if(!FilterType(item)) {
            return false;
        }
    }
    return true;
}

bool PreciseManager::IsContainModifiedFiles(const std::string& file, bool isHFile)
{
    if (isHFile) {
        for (const std::string& h : modifyHFileList_) {
            if (base::starts_with(h, file)) {
                return true;
            }
        }
        return false;
    } else {
        for (const std::string& c : modifyCFileList_) {
            if (c == file) {
                return true;
            }
        }
        return false;
    }
}

Node* PreciseManager::GetModule(const std::string& name)
{
    return moduleList_[name];
}

bool PreciseManager::CheckIncludeInConfig(const Config* config)
{
    const std::vector<SourceDir> dirs = config->own_values().include_dirs();
    for (const SourceDir& dir : dirs) {
        if (IsContainModifiedFiles(dir.value(), true)) {
            return true;
        }
    }
    return false;
}

bool PreciseManager::CheckIncludeInTarget(const Item* item)
{
    if (item == nullptr) {
        return false;
    }

    std::vector<SourceDir> dirs;
    if (item->GetItemTypeName() == "target") {
        dirs = item->AsTarget()->include_dirs();
    } else if (item->GetItemTypeName() == "config") {
        dirs = item->AsConfig()->own_values().include_dirs();
    } else {
        return false;
    }

    std::string name = item->label().GetUserVisibleName(false);
    for (const SourceDir& dir : dirs) {
        if (IsContainModifiedFiles(dir.value(), true)) {
            return true;
        }
    }
    return false;
}

bool PreciseManager::CheckSourceInTarget(const Item* item)
{
    if (item == nullptr || item->GetItemTypeName() != "target") {
        return false;
    }

    const std::vector<SourceFile>& dirs = item->AsTarget()->sources();
    for (const SourceFile& dir : dirs) {
        if (IsContainModifiedFiles(dir.value(), false)) {
            return true;
        }
    }
    return false;
}

bool PreciseManager::CheckConfigInfo(const UniqueVector<LabelConfigPair>& configs)
{
    for (const auto& config : configs) {
        std::string label = config.label.GetUserVisibleName(false);
        if (base::starts_with(label, "//build/config")) {
            continue;
        }
        if (CheckIncludeInConfig(config.ptr)) {
            return true;
        }
    }
    return false;
}

bool PreciseManager::CheckPrivateConfigs(const Item* item)
{
    if (item == nullptr || item->GetItemTypeName() != "target") {
        return false;
    }

    const UniqueVector<LabelConfigPair> configs = item->AsTarget()->configs();
    if (configs.size() > 0) {
        if (CheckConfigInfo(configs)) {
            return true;
        }
    }
    return false;
}

bool PreciseManager::CheckPublicConfigs(const Item* item)
{
    if (item == nullptr || item->GetItemTypeName() != "target") {
        return false;
    }

    const UniqueVector<LabelConfigPair> configs = item->AsTarget()->public_configs();
    if (configs.size() > 0) {
        if (CheckConfigInfo(configs)) {
            return true;
        }
    }
    return false;
}

bool PreciseManager::CheckAllDepConfigs(const Item* item)
{
    if (item == nullptr || item->GetItemTypeName() != "target") {
        return false;
    }

    const UniqueVector<LabelConfigPair> configs = item->AsTarget()->all_dependent_configs();
    if (configs.size() > 0) {
        if (CheckConfigInfo(configs)) {
            return true;
        }
    }
    return false;
}

void PreciseManager::EnsurePathExists(const std::string& filePath)
{
    fs::path path(filePath);
    auto dirPath = path.parent_path();
    if (!dirPath.empty() && !fs::exists(dirPath)) {
        fs::create_directories(dirPath);
    }
}

void PreciseManager::WriteFile(const std::string& path, const std::string& info)
{
    std::string outFile = outDir_ + "/" + path;
    EnsurePathExists(outFile);
    std::ofstream fileFd;
    fileFd.open(outFile, std::ios::out);
    fileFd << info;
    fileFd.close();
}

bool PreciseManager::FilterType(const Item* item)
{
    if (item == nullptr || item->GetItemTypeName() == "config") {
        return false;
    }

    std::string name = item->label().GetUserVisibleName(false);
    if (base::ends_with(name, "__check")
        || base::ends_with(name, "__collect")
        || base::ends_with(name, "__notice")
        || base::ends_with(name, "_info_install_info")
        || base::ends_with(name, "_resource_copy")) {
        return false;
    }

    return true;
}

bool PreciseManager::IsTargetTypeMatch(const Item* item)
{
    if (item == nullptr || item->GetItemTypeName() == "config") {
        return false;
    }

    std::string type;
    if (item->GetItemTypeName() == "target") {
        std::string tmp(Target::GetStringForOutputType(item->AsTarget()->output_type()));
        type += tmp;
    } else {
        type += item->GetItemTypeName();
    }

    if (std::find(targetTypeList_.begin(), targetTypeList_.end(), type) != targetTypeList_.end()) {
        return true;
    }

    return false;
}

bool PreciseManager::IsTestOnlyMatch(const Item* item)
{
    if (item == nullptr || (testOnly_ && !item->testonly())) {
        return false;
    }

    return true;
}

bool PreciseManager::IsFirstRecord(const std::vector<std::string>& result, const std::string& name)
{
    if (std::find(result.begin(), result.end(), name) == result.end()) {
        return true;
    }
    return false;
}

void PreciseManager::PreciseSearch(const Node* node, std::vector<std::string>& result, std::vector<std::string>& log,
    bool forGn, int depth, int maxDepth)
{
    Module* module = (Module* )node;
    const Item* item = module->GetItem();
    std::string name = item->label().GetUserVisibleName(false);
    log.push_back("Check:" + name);

    if (depth >= maxDepth) {
        log.push_back("Over Depth:" + name);
        return;
    }

    if (!FilterType(item)) {
        log.push_back("FilterType false:" + name);
        return;
    }

    if (IsTargetTypeMatch(item) && IsTestOnlyMatch(item) && IsFirstRecord(result, name)
        && !IsIgnore(name) && IsInMaxRange(name)) {
        log.push_back("OK:" + name);
        result.push_back(name);
        return;
    }

    for (Node* parent : node->GetFromList()) {
        Module* moduleParent = (Module* )parent;
        const Item* itemParent = moduleParent->GetItem();
        std::string nameParent = itemParent->label().GetUserVisibleName(false);
        log.push_back("Check Parent:" + nameParent + "->" + name);
        PreciseSearch(parent, result, log, forGn, depth + 1, maxDepth);
    }
}

bool PreciseManager::CheckModuleInGn(const std::string& label)
{
    for (const std::string& gn : modifyGnFileList_) {
        size_t pos = label.find(":");
        if (pos == std::string::npos) {
            return false;
        }
        std::string labelPrefix = label.substr(0, pos);

        size_t posGn = gn.find("BUILD.gn");
        if (posGn == std::string::npos) {
            return false;
        }
        std::string filePrefix = gn.substr(0, posGn - 1);
        if (labelPrefix == filePrefix) {
            return true;
        }
    }
    return false;
}

bool PreciseManager::CheckModuleMatch(const std::string& label)
{
    for (const std::string& modify : modifyGnModuleList_) {
        if (label == modify) {
            return true;
        }
    }
    return false;
}

void PreciseManager::WritePreciseTargets(const std::vector<std::string>& result, const std::vector<std::string>& log)
{
    std::string logInfo = "";
    for(size_t i = 0; i < log.size(); ++i) {
        logInfo += log[i];
        logInfo += " ";
        logInfo += "\n";
    }
    WriteFile(preciseLogPath_, logInfo);

    std::string resultInfo = "";
    for(size_t i = 0; i < result.size(); ++i) {
        resultInfo += result[i];
        resultInfo += " ";
        resultInfo += "\n";
    }
    WriteFile(preciseResultPath_, resultInfo);
}

void PreciseManager::GeneratPreciseTargets()
{
    std::cout << "GeneratPreciseTargets Begain." << std::endl;
    std::vector<std::string> result;
    std::vector<std::string> log;
    log.push_back("Init Precise depth:" + std::to_string(hFileDepth_) + " " + std::to_string(cFileDepth_)
        + " " + std::to_string(gnFileDepth_) + " " + std::to_string(gnModuleDepth_));

    for (const auto& pair : moduleList_) {
        Module* module = (Module* )pair.second;
        const Item* item = module->GetItem();
        std::string label = item->label().GetUserVisibleName(false);
        if (!FilterType(item)) {
            continue;
        }

        if (CheckSourceInTarget(item)) {
            log.push_back("Hit C:");
            PreciseSearch(pair.second, result, log, false, 0, cFileDepth_);
        } else if (CheckIncludeInTarget(item) || CheckPrivateConfigs(item)
            || CheckPublicConfigs(item) || CheckAllDepConfigs(item)) {
            log.push_back("Hit H:");
            PreciseSearch(pair.second, result, log, false, 0, hFileDepth_);
        } else if (CheckModuleInGn(label)) {
            log.push_back("Hit GN:");
            PreciseSearch(pair.second, result, log, true, 0, gnFileDepth_);
        } else if (CheckModuleMatch(label)) {
            log.push_back("Hit Module:");
            PreciseSearch(pair.second, result, log, true, 0, gnModuleDepth_);
        }
    }
    WritePreciseTargets(result, log);
}