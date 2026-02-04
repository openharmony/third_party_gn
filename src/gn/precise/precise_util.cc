// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/precise/precise_util.h"
#include <regex>
#include <iostream>
#include <fstream>
#include <algorithm>
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/string_util.h"
#include "gn/precise/precise_log.h"
#include "gn/target.h"
#include "gn/config.h"
#include "gn/item.h"
#include "gn/settings.h"
#include "gn/source_dir.h"
#include "gn/source_file.h"
#include "gn/build_settings.h"
#include "gn/label_ptr.h"

namespace precise {

HeaderChecker::HeaderChecker(const PreciseConfig& config, const std::string& rootDir)
    : rootDir_(rootDir), log_level_(config.preciseLogLevel), config_(config) {
}

HeaderChecker::~HeaderChecker() {
}

void HeaderChecker::AddCache(const std::string& include_dir, const std::unordered_set<std::string>& files) {
    hfileIncludeDirsCache_[include_dir] = files;
    LogMessage("DEBUG", "AddCache: added " + std::to_string(files.size()) +
               " files for include_dir: " + include_dir);
}

const std::unordered_map<std::string, std::unordered_set<std::string>>& HeaderChecker::GetHfileIncludeDirsCache() const {
    return hfileIncludeDirsCache_;
}

std::unordered_map<std::string, std::unordered_set<std::string>>& HeaderChecker::GetHfileIncludeDirsCache() {
    return hfileIncludeDirsCache_;
}

bool HeaderChecker::ReadFile(const std::string& path, std::string& content) {
    if (!base::ReadFileToString(base::FilePath(path), &content)) {
        return false;
    }
    return true;
}

void HeaderChecker::ClearCaches() {
    hfileIncludeDirsCache_.clear();
    fileIncludesCache_.clear();
    headerDependencyCache_.clear();
}

std::vector<std::string> HeaderChecker::ExtractIncludePatterns(const std::string& content) {
    std::vector<std::string> includes;
    std::regex include_regex(R"(#include\s*[<"]([^>"]+)[>"])");
    std::sregex_iterator iter(content.begin(), content.end(), include_regex);
    std::sregex_iterator end;

    while (iter != end) {
        includes.push_back(iter->str(1));
        ++iter;
    }
    return includes;
}

std::vector<std::string> HeaderChecker::GetFileIncludes(const std::string& filePath) {
    auto cacheIt = fileIncludesCache_.find(filePath);
    std::vector<std::string> includes;

    if (cacheIt != fileIncludesCache_.end()) {
        includes = cacheIt->second;
        LogMessage("DEBUG", "GetFileIncludes: using cached includes for " +
                   filePath + " (" + std::to_string(includes.size()) + " includes)");
    } else {
        std::string content;
        if (ReadFile(filePath, content)) {
            includes = ExtractIncludePatterns(content);
            LogMessage("DEBUG", "GetFileIncludes: extracted " +
                       std::to_string(includes.size()) + " includes from " + filePath);
        } else {
            LogMessage("ERROR", "GetFileIncludes: failed to read file: " + filePath);
        }
        fileIncludesCache_[filePath] = includes;
    }

    return includes;
}

std::vector<std::string> HeaderChecker::GetAllIncludeDirs(const Item* item) {
    std::vector<std::string> all_dirs;
    if (item == nullptr || item->GetItemTypeName() != "target") {
        return all_dirs;
    }

    const Target* target = item->AsTarget();

    // Add direct include_dirs
    for (const SourceDir& dir : target->include_dirs()) {
        all_dirs.push_back(dir.value());
    }

    // Add public_configs' include_dirs
    for (const auto& config : target->public_configs()) {
        for (const SourceDir& dir : config.ptr->own_values().include_dirs()) {
            all_dirs.push_back(dir.value());
        }
    }

    // Add configs' include_dirs
    for (const auto& config : target->configs()) {
        for (const SourceDir& dir : config.ptr->own_values().include_dirs()) {
            all_dirs.push_back(dir.value());
        }
    }

    // Add all_dependent_configs' include_dirs
    for (const auto& config : target->all_dependent_configs()) {
        for (const SourceDir& dir : config.ptr->own_values().include_dirs()) {
            all_dirs.push_back(dir.value());
        }
    }

    return all_dirs;
}

std::string HeaderChecker::ResolveIncludePath(const std::string& include_name,
                                              const std::vector<std::string>& include_dirs,
                                              const std::string& rootPath) {
    // Check each include_dir
    for (const std::string& dir : include_dirs) {
        std::string path_of_include_dir = dir + include_name;

        if (base::starts_with(path_of_include_dir, "//")) {
            path_of_include_dir = rootPath + "/" + path_of_include_dir.substr(2);
        }

        if (base::PathExists(base::FilePath(path_of_include_dir))) {
            LogMessage("DEBUG", "Resolved IncludePath: " + path_of_include_dir);
            return path_of_include_dir;
        }
    }

    return "";
}

bool HeaderChecker::CheckHeaderDependencyRecursive(const std::string& header_path,
                                                    const std::vector<std::string>& include_dirs,
                                                    const std::string& cachedIncludeDir,
                                                    const std::string& modifiedHeader,
                                                    std::unordered_set<std::string>& visited,
                                                    const std::string& rootPath,
                                                    int currentDepth) {
    if (header_path.empty()) {
        LogMessage("WARN", "CheckHeaderDependencyRecursive: empty header_path");
        return false;
    }

    // 统一使用绝对路径作为缓存键和visited检查
    std::string absolutePath = ConvertToAbsolutePath(header_path);

    if (visited.find(absolutePath) != visited.end()) {
        LogMessage("DEBUG", "CheckHeaderDependencyRecursive: already visited: " + absolutePath);
        return false;
    }

    // Check cache: use simple file path as key
    auto depCacheIt = headerDependencyCache_.find(header_path);
    if (depCacheIt != headerDependencyCache_.end()) {
        LogMessage("DEBUG", "CheckHeaderDependencyRecursive: using cached result for " +
                   header_path + ": " + (depCacheIt->second ? "HAS_DEPENDENCY" : "NO_DEPENDENCY"));
        return depCacheIt->second;
    }

    // 检查递归深度限制（在检查完缓存和visited之后）
    // currentDepth 从 0 开始，所以当 currentDepth >= headerCheckerMaxDepth 时停止
    if (config_.headerCheckerMaxDepth > 0 && currentDepth >= config_.headerCheckerMaxDepth) {
        LogMessage("WARN", "CheckHeaderDependencyRecursive: reached max recursion depth " +
                   std::to_string(config_.headerCheckerMaxDepth) + " (current: " +
                   std::to_string(currentDepth) + ") at " + header_path);

        // 缓存达到深度限制的结果，将当前链路上的所有头文件都标记为未确认依赖
        for (const std::string& visited_file : visited) {
            headerDependencyCache_[visited_file] = false;
            LogMessage("DEBUG", "CheckHeaderDependencyRecursive: marking visited file as no confirmed dependency: " +
                       visited_file);
        }
        // 也标记当前文件
        headerDependencyCache_[header_path] = false;

        return false;
    }

    LogMessage("DEBUG", "CheckHeaderDependencyRecursive: analyzing header: " + header_path +
               " (depth: " + std::to_string(currentDepth) + ")");
    visited.insert(header_path);

    if (header_path == modifiedHeader.substr(2)) {
        LogMessage("INFO", "CheckHeaderDependencyRecursive: FOUND MATCH - header is modified: " + header_path);
        headerDependencyCache_[header_path] = true;
        return true;
    }

    // Recursively check other header files included by this header file
    std::vector<std::string> includes = GetFileIncludes(header_path);

    if (includes.empty()) {
        LogMessage("DEBUG", "CheckHeaderDependencyRecursive: no includes found in " + header_path);
    }

    std::string headerDir = header_path.substr(0, header_path.find_last_of("/"));

    for (const std::string& includeName : includes) {
        std::string candidatePath = cachedIncludeDir + includeName;
        if (candidatePath == modifiedHeader) {
            LogMessage("INFO", "CheckHeaderDependencyRecursive: FOUND MATCH - include is modified: " + candidatePath);
            headerDependencyCache_[header_path] = true;
            return true;
        }
        std::string includedPath = ResolveIncludePath(includeName, include_dirs, rootPath);
        LogMessage("DEBUG", "CheckHeaderDependencyRecursive: checking recursive include '" +
                   includeName + "' -> '" + includedPath + "'");

        if (CheckHeaderDependencyRecursive(includedPath, include_dirs, cachedIncludeDir, modifiedHeader, visited, rootPath, currentDepth + 1)) {
            LogMessage("INFO", "CheckHeaderDependencyRecursive: FOUND DEPENDENCY - " +
                       header_path + " -> " + includedPath + " -> modified header");
            headerDependencyCache_[header_path] = true;
            return true;
        }
    }

    LogMessage("DEBUG", "CheckHeaderDependencyRecursive: NO DEPENDENCY found for " + header_path);
    headerDependencyCache_[header_path] = false;
    return false;
}

std::string HeaderChecker::ConvertToAbsolutePath(const std::string& gn_path) {
    if (base::starts_with(gn_path, "//")) {
        // Remove the leading "//" and concatenate with the build directory's root path
        std::string relativePath = gn_path.substr(2);
        return rootDir_ + "/" + relativePath;
    }
    return gn_path;
}

bool HeaderChecker::IsIncludeDirInTarget(const std::string& include_dir,
                                         const std::vector<std::string>& target_include_dirs) {
    for (const std::string& targetIncludeDir : target_include_dirs) {
        if (include_dir == targetIncludeDir) {
            LogMessage("DEBUG", "IsIncludeDirInTarget: " + include_dir +
                       " found in target include_dir: " + targetIncludeDir);
            return true;
        }
    }
    return false;
}

bool HeaderChecker::CheckDirectInclude(const std::string& source_path,
                                       const std::string& include_dir,
                                       const std::string& modified_header) {
    // 检查缓存：使用 source_path + modified_header 作为键
    // 注意：不同 include_dir 可能会导致不同结果，但为了简化缓存，这里忽略 include_dir
    // 实际使用中，同一源文件对同一修改头文件的直接包含关系通常是确定的
    std::string cacheKey = source_path + "|" + modified_header;
    auto cacheIt = headerDependencyCache_.find(cacheKey);
    if (cacheIt != headerDependencyCache_.end()) {
        if (cacheIt->second) {
            LogMessage("DEBUG", "CheckDirectInclude: CACHED - " + source_path +
                       " directly includes " + modified_header);
            return true;
        } else {
            LogMessage("DEBUG", "CheckDirectInclude: CACHED - " + source_path +
                       " does not directly include " + modified_header);
            return false;
        }
    }

    std::vector<std::string> includes = GetFileIncludes(source_path);

    if (includes.empty()) {
        // Check if the file really has no includes or failed to read
        auto cacheIt = fileIncludesCache_.find(source_path);
        if (cacheIt != fileIncludesCache_.end()) {
            // File was read but has no includes - normal case
            return false;
        } else {
            // File read failed
            LogMessage("ERROR", "CheckDirectInclude: failed to read file: " + source_path);
            headerDependencyCache_[cacheKey] = false;
            return false;
        }
    }

    for (const std::string& includeName : includes) {
        std::string candidatePath = include_dir + includeName;

        LogMessage("DEBUG", "CheckDirectInclude: checking '" +
                   includeName + "' -> '" + candidatePath + "'");

        if (candidatePath == modified_header) {
            LogMessage("INFO", "CheckDirectInclude: DIRECT MATCH - found modified header: " + modified_header);
            headerDependencyCache_[cacheKey] = true;
            return true;
        }
    }

    headerDependencyCache_[cacheKey] = false;
    return false;
}

bool HeaderChecker::CheckRecursiveDependency(const std::vector<std::string>& includes,
                                             const std::vector<std::string>& include_dirs,
                                             const std::string& cached_include_dir,
                                             const std::string& modified_header,
                                             int currentDepth) {
    for (const std::string& includeName : includes) {
        std::string includedPath = ResolveIncludePath(includeName, include_dirs, rootDir_);

        // Check cache using simple file path as key
        auto depCacheIt = headerDependencyCache_.find(includedPath);
        if (depCacheIt != headerDependencyCache_.end()) {
            if (depCacheIt->second) {
                LogMessage("INFO", "CheckRecursiveDependency: CACHED DEPENDENCY - " +
                           includedPath + " depends on modified header");
                return true;
            } else {
                LogMessage("DEBUG", "CheckRecursiveDependency: cached no dependency for " +
                           includedPath + ", skipping");
                continue;
            }
        }

        LogMessage("DEBUG", "CheckRecursiveDependency: checking recursive dependency for: " +
                   includedPath + " (depth: " + std::to_string(currentDepth) + ")");

        std::unordered_set<std::string> visited;
        bool hasDependency = CheckHeaderDependencyRecursive(
            includedPath, include_dirs, cached_include_dir, modified_header, visited, rootDir_, currentDepth + 1);

        // Cache the result using simple file path as key
        headerDependencyCache_[includedPath] = hasDependency;

        if (hasDependency) {
            LogMessage("INFO", "CheckRecursiveDependency: RECURSIVE DEPENDENCY - " +
                       includedPath + " depends on modified header");
            return true;
        }
    }

    return false;
}

bool HeaderChecker::CheckSingleSourceFile(const std::string& source_path,
                                          const std::vector<std::string>& include_dirs) {
    LogMessage("INFO", "==========================================================");
    LogMessage("INFO", "CheckSingleSourceFile: processing source file: " + source_path);

    std::string sourceAbsolutePath = ConvertToAbsolutePath(source_path);
    LogMessage("DEBUG", "CheckSingleSourceFile: absolute path: " + sourceAbsolutePath);

    // Iterate through all cached include_dirs
    for (const auto& cachePair : hfileIncludeDirsCache_) {
        const std::string& cachedIncludeDir = cachePair.first;
        const std::unordered_set<std::string>& modifiedHeadersInDir = cachePair.second;

        if (modifiedHeadersInDir.empty()) {
            continue;
        }

        // Check if the cached include_dir is in the target's include_dirs
        if (!IsIncludeDirInTarget(cachedIncludeDir, include_dirs)) {
            continue;
        }

        // Check all modified header files in this include_dir
        for (const std::string& modifiedHeader : modifiedHeadersInDir) {
            LogMessage("DEBUG", "CheckSingleSourceFile: checking modified header: " + modifiedHeader);

            // Check direct include
            if (CheckDirectInclude(sourceAbsolutePath, cachedIncludeDir, modifiedHeader)) {
                return true;
            }

            // Get includes list for recursive check
            std::vector<std::string> includes = GetFileIncludes(sourceAbsolutePath);
            if (includes.empty()) {
                continue;
            }

            // Check indirect include
            if (CheckRecursiveDependency(includes, include_dirs, cachedIncludeDir, modifiedHeader, -1)) {
                return true;
            }
        }
    }

    LogMessage("INFO", "CheckSingleSourceFile: finished processing " + source_path);
    LogMessage("INFO", "==========================================================");

    return false;
}

bool HeaderChecker::CheckActuallyUsedHeaders(const Item* item) {
    if (item == nullptr) {
        return false;
    }

    // 检查是否启用了 HeaderChecker
    if (!config_.enableHeaderChecker) {
        LogMessage("INFO", "CheckActuallyUsedHeaders: HeaderChecker is disabled, returning true");
        return true;  // 如果禁用，假定所有 target 都需要编译
    }

    const Target* target = item->AsTarget();
    if (target == nullptr) {
        return false;
    }

    // Get all include_dirs for the target
    std::vector<std::string> include_dirs = GetAllIncludeDirs(item);
    const std::vector<SourceFile>& sources = target->sources();

    // Check each source file
    for (const SourceFile& sourceFile : sources) {
        if (CheckSingleSourceFile(sourceFile.value(), include_dirs)) {
            return true;
        }
    }

    return false;
}

}  // namespace precise