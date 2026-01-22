// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GN_PRECISE_PRECISE_UTIL_H_
#define GN_PRECISE_PRECISE_UTIL_H_

#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include "gn/precise/precise_config.h"

class Item;

namespace precise {

// Header file dependency checker utility class
class HeaderChecker {
public:
    HeaderChecker(const PreciseConfig& config, const std::string& rootDir);
    ~HeaderChecker();

    // Check if the target actually uses the modified header files
    bool CheckActuallyUsedHeaders(const Item* item);

    // Add header file directory cache
    void AddCache(const std::string& include_dir, const std::unordered_set<std::string>& files);

    // Get header file directory cache (const version)
    const std::unordered_map<std::string, std::unordered_set<std::string>>& GetHfileIncludeDirsCache() const;

    // Get header file directory cache (non-const version, for modification)
    std::unordered_map<std::string, std::unordered_set<std::string>>& GetHfileIncludeDirsCache();

    // Clear all caches
    void ClearCaches();

private:
    std::string rootDir_;
    std::vector<std::string> modify_h_files_;
    std::string log_level_;

    // Cache related
    std::unordered_map<std::string, std::unordered_set<std::string>> hfileIncludeDirsCache_;
    std::unordered_map<std::string, std::vector<std::string>> fileIncludesCache_;
    std::unordered_map<std::string, bool> headerDependencyCache_;

    // Extract include patterns from content
    std::vector<std::string> ExtractIncludePatterns(const std::string& content);

    // Get the list of includes for a file
    std::vector<std::string> GetFileIncludes(const std::string& filePath);

    // Read file content
    bool ReadFile(const std::string& path, std::string& content);

    // Resolve include path
    std::string ResolveIncludePath(const std::string& include_name,
                                  const std::vector<std::string>& include_dirs,
                                  const std::string& rootPath);

    // Recursively check header file dependencies
    bool CheckHeaderDependencyRecursive(const std::string& header_path,
                                        const std::vector<std::string>& include_dirs,
                                        const std::string& cachedIncludeDir,
                                        const std::string& modifiedHeader,
                                        std::unordered_set<std::string>& visited,
                                        const std::string& rootPath);

    // Get all include directories for the target
    std::vector<std::string> GetAllIncludeDirs(const Item* item);

    // Convert GN path to absolute path
    std::string ConvertToAbsolutePath(const std::string& gn_path);

    // Check if include_dir is in target's include_dirs
    bool IsIncludeDirInTarget(const std::string& include_dir,
                             const std::vector<std::string>& target_include_dirs);

    // Check if the source file directly includes the modified header file
    bool CheckDirectInclude(const std::string& source_path,
                           const std::string& include_dir,
                           const std::string& modified_header);

    // Check if the source file indirectly includes the modified header file
    bool CheckRecursiveDependency(const std::vector<std::string>& includes,
                                  const std::vector<std::string>& include_dirs,
                                  const std::string& cached_include_dir,
                                  const std::string& modified_header);

    // Check if a single source file uses the modified header files
    bool CheckSingleSourceFile(const std::string& source_path,
                               const std::vector<std::string>& include_dirs);
};

}  // namespace precise

#endif  // GN_PRECISE_PRECISE_UTIL_H_