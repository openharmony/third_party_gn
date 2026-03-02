// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/string_util.h"
#include "gn/build_settings.h"
#include "gn/config.h"
#include "gn/filesystem_utils.h"
#include "gn/functions.h"
#include "gn/label_ptr.h"
#include "gn/parse_tree.h"
#include "gn/precise/precise.h"
#include "gn/precise/precise_log.h"
#include "gn/precise/precise_config.h"
#include "gn/precise/precise_util.h"
#include "gn/settings.h"
#include "gn/substitution_writer.h"
#include "gn/target.h"
#include "gn/value.h"
#include "gn/output_file.h"

PreciseManager* PreciseManager::instance_ = nullptr;
static std::unique_ptr<precise::ConfigManager> configManager_;
static std::unique_ptr<precise::HeaderChecker> headerChecker_;
static std::unordered_map<std::string, bool> filter_cache;


PreciseManager::PreciseManager(const std::string& outDir, const std::string& rootDir, const std::string& preciseConfig)
{
    std::cout << "Read precise config from " << preciseConfig << std::endl;
    outDir_ = outDir;
    rootDir_ = rootDir;

    // Initialize configuration manager
    configManager_ = std::make_unique<precise::ConfigManager>();
    if (!configManager_->LoadConfig(preciseConfig)) {
        std::cout << "Failed to load precise config" << std::endl;
        return;
    }

    // Store config pointer for efficient access
    config_ = &configManager_->GetConfig();

    // Load modified file list
    gnFileDepth_ = config_->gnFileDepth;  // Initialize gnFileDepth
    if (!config_->modifyFilesPath.empty()) {
        configManager_->LoadModifyList(config_->modifyFilesPath);
    }

    // Initialize header checker
    headerChecker_ = std::make_unique<precise::HeaderChecker>(*config_, rootDir_);
}

void PreciseManager::AddModule(std::string name, Node* node)
{
    moduleList_[name] = node;
}

bool PreciseManager::IsIgnore(const std::string& name)
{
    auto result = std::find(config_->ignoreList.begin(), config_->ignoreList.end(), name);
    if (result != config_->ignoreList.end()) {
        return true;
    }
    return false;
}

bool PreciseManager::IsInMaxRange(const std::string& name)
{
    if (config_->maxRangeList.empty()) {
        return true;
    }
    auto result = std::find(config_->maxRangeList.begin(), config_->maxRangeList.end(), name);
    if (result != config_->maxRangeList.end()) {
        return true;
    }
    return false;
}

bool PreciseManager::IsContainModifiedFiles(const std::string& file, bool isHFile)
{
    if(isHFile){
        // Check if the result for this include_dir has been calculated in the cache
        std::unordered_map<std::string, std::unordered_set<std::string>>& hfileIncludeDirsCache
        = headerChecker_->GetHfileIncludeDirsCache();
        auto cacheIt = hfileIncludeDirsCache.find(file);
        if (cacheIt != hfileIncludeDirsCache.end()) {
            return !cacheIt->second.empty();
        }
        // Not in cache, calculate the result
        std::unordered_set<std::string> matching_files;
        for (const std::string& h : config_->modifyHFileList) {
            if (IsFileInList(file, config_->modifyHFileList, true)) {
                matching_files.insert(h);
            }
        }
        // Store the result in the cache
        hfileIncludeDirsCache[file] = matching_files;
        return !matching_files.empty();
    }
    return IsFileInList(file, config_->modifyCFileList, false);
}

// Check if file is in the given list (supports prefix matching)
bool PreciseManager::IsFileInList(const std::string& file,
                                  const std::vector<std::string>& fileList,
                                  bool checkDirPrefix) {
    for (const std::string& modifyFile : fileList) {
        // Exact match
        if (file == modifyFile) {
            return true;
        }
        if (checkDirPrefix && base::starts_with(modifyFile, file)) {
            return true;
        }
    }
    return false;
}

bool PreciseManager::CheckGNFileModified(const Item* item)
{
    for (const std::string& gn : config_->modifyGnFileList) {
        SourceFile gnFile(gn);
        for (const auto& cur_file : item->build_dependency_files()) {
            if (cur_file == gnFile){
                const Target* target = item->AsTarget();
                bool include_toolchain = (target && !target->settings()->is_default());
                precise::LogMessage("INFO", "CheckGNFileModified: [" + gnFile.value() + "] -> [" + cur_file.value() + "]" +
                    " in " + item->label().GetUserVisibleName(include_toolchain));
                return true;
            }
        }
    }
    const Target* target = item->AsTarget();
    bool include_toolchain = (target && !target->settings()->is_default());
    precise::LogMessage("DEBUG", "CheckGNFileModified: Not found in [" + item->label().GetUserVisibleName(include_toolchain) + "]");
    return false;
}

bool PreciseManager::CheckActuallyUsedHeaders(const Item* item)
{
    // 逃生机制：检查修改的头文件数量是否超过配置的最大值
    if (config_->headerCheckerMaxFileCount > 0) {
        size_t totalHeaderFiles = config_->modifyHFileList.size();
        if (totalHeaderFiles > static_cast<size_t>(config_->headerCheckerMaxFileCount)) {
            precise::LogMessage("WARN", "CheckActuallyUsedHeaders: Modified header files count (" +
                std::to_string(totalHeaderFiles) + ") exceeds maximum (" +
                std::to_string(config_->headerCheckerMaxFileCount) + "), skipping HeaderChecker");
            return true;  // 超过限制，假定所有 target 都需要编译
        }
    }

    if (headerChecker_) {
        return headerChecker_->CheckActuallyUsedHeaders(item);
    }
    return false;
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

// Helper function: determine if a string is likely a file path
static bool IsLikelyFilePath(const std::string& str) {
    // Empty string is not a file path
    if (str.empty()) {
        return false;
    }

    // Check if it starts with '-' (usually command line options, not files)
    if (str[0] == '-') {
        return false;
    }

    // Check if it contains path separators or looks like a file path
    if (str.find('/') != std::string::npos || str.find('\\') != std::string::npos) {
        return true;
    }

    // Check if it has a file extension (contains a dot and doesn't start with a dot)
    size_t dot_pos = str.find('.');
    if (dot_pos != std::string::npos && dot_pos > 0) {
        return true;
    }

    // Check for common GN build variable substitution patterns (these are not real file paths)
    if (str.find("{{") != std::string::npos) {
        return false;
    }

    return false;
}

// Check if SubstitutionPattern is in the given file list
// First check the raw pattern string, then try to expand the substitution pattern with sources
bool PreciseManager::CheckSubstitutionPatternInList(
    const Target* target,
    const SubstitutionPattern& pattern,
    const std::vector<SourceFile>& sources,
    const std::vector<std::string>& fileList) {

    // First check the raw pattern string
    std::string pattern_str = pattern.AsString();
    if (IsLikelyFilePath(pattern_str)) {
        if (IsFileInList(pattern_str, fileList, false)) {
            return true;
        }
    }

    // If it contains substitution patterns, try to expand with each source
    if (!sources.empty() && target->settings()) {
        for (const SourceFile& source : sources) {
            std::string expanded =
                SubstitutionWriter::ApplyPatternToSourceAsString(
                    target, target->settings(), pattern, source);
            if (IsLikelyFilePath(expanded)) {
                if (IsFileInList(expanded, fileList, false)) {
                    return true;
                }
            }
        }
    }

    return false;
}

bool PreciseManager::CheckFilesInActionTarget(const Item* item)
{
    if (item == nullptr || item->GetItemTypeName() != "target") {
        return false;
    }

    const Target* target = item->AsTarget();
    bool include_toolchain = !target->settings()->is_default();
    std::string label = item->label().GetUserVisibleName(include_toolchain);

    precise::LogMessage("DEBUG", "CheckFilesInActionTarget: checking target " + label);

    if(target->output_type() != Target::ACTION &&
        target->output_type() != Target::ACTION_FOREACH &&
        target->output_type() != Target::COPY_FILES) {
        precise::LogMessage("DEBUG", "CheckFilesInActionTarget: target type mismatch for " + label);
        return false;
    }

    // Check script
    const SourceFile& script = target->action_values().script();
    if (!script.is_null()) {
        precise::LogMessage("DEBUG", "CheckFilesInActionTarget: checking script " + script.value() + " for " + label);
        if (IsFileInList(script.value(), config_->modifyOtherFileList, false)) {
            precise::LogMessage("INFO", "CheckFilesInActionTarget: FOUND in script - " + script.value() + " for " + label);
            return true;
        }
    }

    // Check inputs
    const std::vector<SourceFile>& inputs = target->config_values().inputs();
    precise::LogMessage("DEBUG", "CheckFilesInActionTarget: checking " + std::to_string(inputs.size()) + " inputs for " + label);
    for (const SourceFile& input : inputs) {
        if (IsFileInList(input.value(), config_->modifyOtherFileList, false)) {
            precise::LogMessage("INFO", "CheckFilesInActionTarget: FOUND in inputs - " + input.value() + " for " + label);
            return true;
        }
    }

    // Check sources
    const std::vector<SourceFile>& sources = target->sources();
    precise::LogMessage("DEBUG", "CheckFilesInActionTarget: checking " + std::to_string(sources.size()) + " sources for " + label);
    for (const SourceFile& source : sources) {
        if (IsFileInList(source.value(), config_->modifyOtherFileList, false)) {
            precise::LogMessage("INFO", "CheckFilesInActionTarget: FOUND in sources - " + source.value() + " for " + label);
            return true;
        }
    }

    // Check file paths in args
    const SubstitutionList& args = target->action_values().args();
    precise::LogMessage("DEBUG", "CheckFilesInActionTarget: checking " + std::to_string(args.list().size()) + " args for " + label);
    for (const SubstitutionPattern& arg : args.list()) {
        if (CheckSubstitutionPatternInList(target, arg, sources, config_->modifyOtherFileList)) {
            precise::LogMessage("INFO", "CheckFilesInActionTarget: FOUND in args for " + label);
            return true;
        }
    }

    // Check depfile
    if (target->action_values().has_depfile()) {
        precise::LogMessage("DEBUG", "CheckFilesInActionTarget: checking depfile for " + label);
        const SubstitutionPattern& depfile = target->action_values().depfile();
        if (CheckSubstitutionPatternInList(target, depfile, sources, config_->modifyOtherFileList)) {
            precise::LogMessage("INFO", "CheckFilesInActionTarget: FOUND in depfile for " + label);
            return true;
        }
    }

    precise::LogMessage("DEBUG", "CheckFilesInActionTarget: NO MATCH found for " + label);
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

void PreciseManager::WriteFile(const std::string& path, const std::string& info)
{
    std::string outFile = outDir_ + "/" + path;
    base::FilePath dirPath = base::FilePath(outDir_);
    if (!base::PathExists(dirPath)) {
        base::CreateDirectory(dirPath);
    }
    std::ofstream fileFd;
    fileFd.open(outFile, std::ios::out);
    fileFd << info;
    fileFd.close();
}

bool PreciseManager::FilterType(const Item* item, const bool inRecursive)
{
    if (item == nullptr || item->GetItemTypeName() != "target") {
        return false;
    }

    const Target* target = item->AsTarget();
    Target::OutputType type = target->output_type();

    // Use a list to define allowed target types
    static std::set<Target::OutputType> allowed_types = {
        Target::SHARED_LIBRARY,
        Target::STATIC_LIBRARY,
        Target::RUST_LIBRARY,
        Target::SOURCE_SET,
        Target::RUST_PROC_MACRO
    };

    if (!inRecursive) {
      // In non-recursive mode, also include action targets
      allowed_types.insert(Target::ACTION);
      allowed_types.insert(Target::ACTION_FOREACH);
      allowed_types.insert(Target::GROUP);
      allowed_types.insert(Target::COPY_FILES);
      allowed_types.insert(Target::EXECUTABLE);
    }

    if (allowed_types.find(type) == allowed_types.end()) {
        return false;
    }

    std::string name = item->label().GetUserVisibleName(false);
    if (base::ends_with(name, "__check")
        || base::ends_with(name, "__collect")
        || base::ends_with(name, "__notice")
        || base::ends_with(name, "_info_install_info")
        || base::ends_with(name, "__compile_resources")
        || base::ends_with(name, "__metadata")
        || base::ends_with(name, "__js_assets")
        || base::ends_with(name, "_info")
        ) {
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

    if (std::find(config_->targetTypeList.begin(), config_->targetTypeList.end(), type) != config_->targetTypeList.end()) {
        return true;
    }

    return false;
}

bool PreciseManager::IsTestOnlyMatch(const Item* item)
{
    if (item == nullptr || (config_->testOnly && !item->testonly())) {
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

void PreciseManager::PreciseSearch(const Node* node, std::vector<std::string>& result,
    std::vector<Module*>& module_list, bool forGn, int depth, int maxDepth, bool isHeader)
{
    Module* module = (Module* )node;
    const Item* item = module->GetItem();
    const Target* target = item->AsTarget();
    bool include_toolchain = (target && !target->settings()->is_default());
    std::string name = item->label().GetUserVisibleName(include_toolchain);
    precise::LogMessage("INFO", "Check:" + name);

    if (depth >= maxDepth) {
        precise::LogMessage("WARN", "Over Depth:" + name);
        return;
    }

    // 在深度搜索的每一层都进行父目标过滤检查
    // 确保整个依赖链上的每个模块都符合过滤规则
    ModuleCheckResult checkResult = CheckModulePath(module, {});
    if (checkResult.is_excluded) {
        precise::LogMessage("DEBUG", "Module excluded by parent filter:" + name + " (depth:" + std::to_string(depth) + ")");
        return;
    }
    if (!config_->includeParentTargets.empty() && !checkResult.is_included) {
        precise::LogMessage("DEBUG", "Module not in include parent list:" + name + " (depth:" + std::to_string(depth) + ")");
        return;
    }

    if (!FilterType(item, depth != 0)) {
        precise::LogMessage("DEBUG", "FilterType false:" + name);
        return;
    }

    if (isHeader && depth == 0 && !CheckActuallyUsedHeaders(item) ) {
        precise::LogMessage("DEBUG", "SourcesIncludeModifiedHeaders false:" + name);
        return;
    }

    if (IsTargetTypeMatch(item) && IsTestOnlyMatch(item) && IsFirstRecord(result, name)
        && !IsIgnore(name) && IsInMaxRange(name)) {
        precise::LogMessage("INFO", "OK:" + name);
        result.push_back(name);
        module_list.push_back(module);
        return;
    }

    for (Node* parent : node->GetFromList()) {
        Module* moduleParent = (Module* )parent;
        const Item* itemParent = moduleParent->GetItem();
        const Target* targetParent = itemParent->AsTarget();
        bool include_toolchain_parent = (targetParent && !targetParent->settings()->is_default());
        std::string nameParent = itemParent->label().GetUserVisibleName(include_toolchain_parent);
        precise::LogMessage("INFO", "Check Parent:" + nameParent + "->" + name);
        PreciseSearch(parent, result, module_list, forGn, depth + 1, maxDepth, isHeader);
    }
}


bool PreciseManager::CheckModuleMatch(const std::string& label)
{
    for (const std::string& modify : config_->modifyGnModuleList) {
        if (label == modify) {
            return true;
        }
    }
    return false;
}

void PreciseManager::WritePreciseTargets(const std::vector<std::string>& result)
{
    std::string resultInfo = "";
    for(size_t i = 0; i < result.size(); ++i) {
        resultInfo += result[i];
        resultInfo += " ";
        resultInfo += "\n";
    }
    WriteFile(config_->preciseResultPath, resultInfo);
}

void PreciseManager::WritePreciseNinjaFile(const std::vector<Module*>& module_list)
{
    std::cout << "Writing precise targets file..." << std::endl;

    // Collect all targets and their output files
    std::vector<std::string> target_outputs;
    std::vector<std::string> target_names;

    if (module_list.empty()) {
        std::cout << "No targets to add to precise build." << std::endl;
        std::cout << "Writing empty precise target to prevent ninja build failure." << std::endl;
    } else {
        for (const Module* module : module_list) {
            if (module == nullptr) {
                continue;
            }

            const Item* item = module->GetItem();
            if (item == nullptr || item->GetItemTypeName() != "target") {
                continue;
            }

            const Target* target = item->AsTarget();
            if (target == nullptr) {
                continue;
            }

            // Get the target's output file
            OutputFile output = target->dependency_output_file();
            target_outputs.push_back(output.value());

            // Get label with toolchain info for non-default toolchains
            bool include_toolchain = !target->settings()->is_default();
            std::string label = item->label().GetUserVisibleName(include_toolchain);
            target_names.push_back(label);
            precise::LogMessage("INFO", "Adding target to precise build: " + label + " -> " + output.value());
        }
    }

    if (target_outputs.empty()) {
        std::cout << "No valid targets found for precise build." << std::endl;
        std::cout << "Creating phony precise target with no dependencies." << std::endl;
    }

    // Write to precise_targets.txt file
    // This file will be read by NinjaBuildWriter to generate the precise phony target
    std::string precise_file_content = "# Auto-generated precise build targets\n";
    precise_file_content += "# This file contains the list of targets for precise compilation\n";
    precise_file_content += "# Each line is an output file path relative to build directory\n\n";

    if (target_outputs.empty()) {
        // Write a phony target marker to prevent ninja build failure
        precise_file_content += "# No targets identified for precise build\n";
        precise_file_content += "# This is a placeholder to allow 'ninja precise' to succeed\n";
    } else {
        for (const std::string& output : target_outputs) {
            precise_file_content += output + "\n";
        }
    }

    std::string precise_targets_path = "precise_targets.txt";
    WriteFile(precise_targets_path, precise_file_content);

    std::cout << "Precise targets file written to: " << outDir_ + "/" + precise_targets_path << std::endl;
    std::cout << "Total targets: " << target_names.size() << std::endl;
    std::cout << "The 'precise' target will be automatically added to build.ninja" << std::endl;

    precise::LogMessage("INFO", "Precise targets file written with " + std::to_string(target_names.size()) + " targets");
}

ModuleCheckResult PreciseManager::CheckModulePath(Module* module, const std::vector<std::string>& cache_list = {})
{
    ModuleCheckResult result;
    result.cache_list = cache_list;

    if (module == nullptr) {
        result.cache_list.clear();
        return result;
    }

    std::string label = module->GetItem()->label().GetUserVisibleName(false);

    if (std::find(result.cache_list.begin(), result.cache_list.end(), label) != result.cache_list.end()) {
        result.cache_list.clear();
        return result;
    }

    if (config_->excludeParentTargets.find(label) != config_->excludeParentTargets.end()) {
        result.is_excluded = true;
        return result;
    }

    if (config_->includeParentTargets.find(label) != config_->includeParentTargets.end() || filter_cache[label]) {
        result.is_included = true;
        filter_cache[label] = true;
        if (!result.cache_list.empty()) {
            for (auto &cached_label : result.cache_list) {
                filter_cache[cached_label] = true;
            }
        }
        return result;
    }

    result.cache_list.push_back(label);
    std::vector<Node*> from_list = module->GetFromList();
    if (from_list.empty()) {
        return result;
    }

    for (Node* parent : module->GetFromList()) {
        Module* moduleParent = (Module* )parent;
        ModuleCheckResult parent_result = CheckModulePath(moduleParent, result.cache_list);
        
        result.is_included = parent_result.is_included;
        result.is_excluded = parent_result.is_excluded;
        if (!result.is_excluded && result.is_included) {
            filter_cache[label] = true;
            for (auto &cached_label : result.cache_list) {
                    filter_cache[cached_label] = true;
            }
            return result;
        } 
        
    }
    
    result.cache_list.clear();
    filter_cache[label] = false;
    return result;
}

void PreciseManager::ApplyTargetFilters(std::vector<std::pair<Module*, int>>& modules_with_type)
{
    for (int i = modules_with_type.size() - 1; i >= 0; --i)
    {
        Module* module = modules_with_type[i].first;
        const Item* item = module->GetItem();
        const Target* target = item->AsTarget();
        bool include_toolchain = (target && !target->settings()->is_default());

        std::string label = item->label().GetUserVisibleName(false);
        std::string label_with_toolchain = item->label().GetUserVisibleName(include_toolchain);

        ModuleCheckResult checkResult = CheckModulePath(module, {});
        bool should_keep = true;

        if (checkResult.is_excluded) {
            should_keep = false;
            std::cout <<
            "Delete target in exclude parent targets:" << label_with_toolchain << "(index:" << i << ")" << std::endl;
        }
        else if (!config_->includeParentTargets.empty() && !checkResult.is_included) {
            should_keep = false;
            std::cout <<
            "Delete target not from include parent targets:" << label_with_toolchain << "(index:" << i << ")" << std::endl;
        }

        if (!should_keep) {
            modules_with_type.erase(modules_with_type.begin() + i);
        }
    }
}

void PreciseManager::GeneratPreciseTargets()
{
    std::cout << "GeneratPreciseTargets Begin." << std::endl;
    std::vector<std::string> result;
    std::vector<Module*> module_list;

    // Initialize real-time log system
    precise::InitializeRealTimeLog(outDir_ + "/" + config_->preciseLogPath, config_->preciseLogLevel);

    // 打印全部配置信息
    configManager_->PrintConfigInfo();

    // 阶段1: 收集所有初步匹配的模块（不进行深度搜索）
    std::cout << "Phase 1: Collecting candidate modules..." << std::endl;
    std::vector<Module*> candidate_modules;
    std::vector<std::pair<Module*, int>> modules_with_type;  // Module + 类型标记

    size_t total_modules = moduleList_.size();
    size_t processed_modules = 0;

    for (const auto& pair : moduleList_) {
        Module* module = (Module* )pair.second;
        const Item* item = module->GetItem();
        const Target* target = item->AsTarget();
        bool include_toolchain = (target && !target->settings()->is_default());
        std::string label = item->label().GetUserVisibleName(false);
        std::string label_with_toolchain = item->label().GetUserVisibleName(include_toolchain);

        if (!FilterType(item, false)) {
            processed_modules++;
            continue;
        }

        // 收集匹配的模块（标记类型但不进行深度搜索）
        // 0: C/C++, 1: Action, 2: Header, 3: GN File, 4: GN Module
        if (!config_->modifyCFileList.empty() && CheckSourceInTarget(item)) {
            modules_with_type.push_back({module, 0});
        } else if (!config_->modifyOtherFileList.empty() && CheckFilesInActionTarget(item)) {
            modules_with_type.push_back({module, 1});
        } else if (!config_->modifyHFileList.empty() && (CheckIncludeInTarget(item) || CheckPrivateConfigs(item)
            || CheckPublicConfigs(item) || CheckAllDepConfigs(item))) {
            modules_with_type.push_back({module, 2});
        } else if (!config_->modifyGnFileList.empty() && CheckGNFileModified(item)) {
            modules_with_type.push_back({module, 3});
        } else if (!config_->modifyGnModuleList.empty() && CheckModuleMatch(label)) {
            modules_with_type.push_back({module, 4});
        }

        processed_modules++;
        if (processed_modules % 100 == 0 || processed_modules == total_modules) {
            std::cout << "[" << processed_modules << "/" << total_modules << "] ("
                      << (processed_modules * 100 / total_modules) << "%)" << std::endl;
        }
    }

    std::cout << "Found " << modules_with_type.size() << " candidate modules" << std::endl;

    // 阶段2: 应用过滤器并生成缓存
    std::cout << "Phase 2: Applying filters and building cache..." << std::endl;
    ApplyTargetFilters(modules_with_type);
    std::cout << "After filtering: " << modules_with_type.size() << " modules remain" << std::endl;

    // 阶段3: 深度搜索（使用 Phase2 生成的缓存）
    std::cout << "Phase 3: Performing deep search with cached filters..." << std::endl;
    size_t filtered_count = modules_with_type.size();
    size_t processed_count = 0;

    for (const auto& pair : modules_with_type) {
        Module* module = pair.first;
        int type = pair.second;
        const Item* item = module->GetItem();
        const Target* target = item->AsTarget();
        bool include_toolchain = (target && !target->settings()->is_default());
        std::string label_with_toolchain = item->label().GetUserVisibleName(include_toolchain);

        // 根据类型进行深度搜索
        switch (type) {
            case 0: // C/C++
                precise::LogMessage("INFO", "Deep search C:" + label_with_toolchain);
                PreciseSearch(module, result, module_list, false, 0, config_->cFileDepth, false);
                break;
            case 1: // Action
                precise::LogMessage("INFO", "Deep search Action:" + label_with_toolchain);
                PreciseSearch(module, result, module_list, false, 0, config_->otherFileDepth, false);
                break;
            case 2: // Header
                precise::LogMessage("INFO", "Deep search H:" + label_with_toolchain);
                PreciseSearch(module, result, module_list, false, 0, config_->hFileDepth, true);
                break;
            case 3: // GN File
                precise::LogMessage("INFO", "Deep search GN File:" + label_with_toolchain);
                PreciseSearch(module, result, module_list, true, 0, gnFileDepth_, false);
                break;
            case 4: // GN Module
                precise::LogMessage("INFO", "Deep search Module:" + label_with_toolchain);
                PreciseSearch(module, result, module_list, true, 0, config_->gnModuleDepth, false);
                break;
        }

        // 更新进度
        processed_count++;
        if (processed_count % 10 == 0 || processed_count == filtered_count) {
            std::cout << "[" << processed_count << "/" << filtered_count << "]" << std::endl;
        }
    }

    std::cout << "Final module target count: " << result.size() << std::endl;

    WritePreciseTargets(result);
    WritePreciseNinjaFile(module_list);

    // Clean up log system
    if (precise::gLogManager) {
        precise::gLogManager->Close();
        precise::gLogManager.reset();
    }
}
