// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRECISE_H_
#define PRECISE_H_

#include <climits>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "base/values.h"
#include "gn/build_settings.h"
#include "gn/config.h"
#include "gn/filesystem_utils.h"
#include "gn/functions.h"
#include "gn/graph/include/module.h"
#include "gn/graph/include/node.h"
#include "gn/item.h"
#include "gn/label_ptr.h"
#include "gn/parse_tree.h"
#include "gn/settings.h"
#include "gn/substitution_writer.h"
#include "gn/target.h"
#include "gn/precise/precise_config.h"
#include "gn/value.h"
struct ModuleCheckResult
{
    bool is_included;
    bool is_excluded;
    std::vector<std::string> cache_list;

    ModuleCheckResult() : is_included(false), is_excluded(false) {}
};

class PreciseManager {
public:
    static void Init(const std::string& buildDir, const std::string& rootDir, const Value* preciseConfig)
    {
        if (instance_ != nullptr) {
            return;
        }
        if (!preciseConfig || preciseConfig->string_value().empty()) {
            std::cout << "precise config null." << std::endl;
            return;
        }
        instance_ = new PreciseManager(buildDir, rootDir, preciseConfig->string_value());
    }

    static PreciseManager* GetInstance()
    {
        return instance_;
    }

    Node *GetModule(const std::string& name);
    void AddModule(std::string name, Node* node);
    void GeneratPreciseTargets();

private:
    static PreciseManager* instance_;
    std::map<std::string, Node*> moduleList_;
    std::string outDir_;
    std::string rootDir_;
    int gnFileDepth_;  // GN file modification depth
    const precise::PreciseConfig* config_;  // Store Config pointer
    bool CheckIncludeInConfig(const Config* config);
    bool CheckIncludeInTarget(const Item* item);
    bool CheckSourceInTarget(const Item* item);
    bool CheckFilesInActionTarget(const Item* item);
    bool CheckConfigInfo(const UniqueVector<LabelConfigPair>& configs);
    bool CheckPrivateConfigs(const Item* item);
    bool CheckPublicConfigs(const Item* item);
    bool CheckAllDepConfigs(const Item* item);
    bool CheckModuleMatch(const std::string& label);
    bool FilterType(const Item* item, const bool inRecursive);
    bool IsIgnore(const std::string& name);
    bool IsInMaxRange(const std::string& name);
    bool IsContainModifiedFiles(const std::string& file, bool isHFile);
    bool IsFileInList(const std::string& file, const std::vector<std::string>& fileList, bool checkDirPrefix);
    bool CheckSubstitutionPatternInList(const Target* target,
                                       const SubstitutionPattern& pattern,
                                       const std::vector<SourceFile>& sources,
                                       const std::vector<std::string>& fileList);
    bool CheckGNFileModified(const Item* item);
    bool IsFirstRecord(const std::vector<std::string>& result, const std::string& name);
    bool CheckActuallyUsedHeaders(const Item* item);
    bool IsTargetTypeMatch(const Item* item);
    bool IsTestOnlyMatch(const Item* item);
    void PreciseSearch(const Node* node, std::vector<std::string>& result, std::vector<Module*>& module_list,
         bool forGn, int depth, int maxDepth, bool isHeader);
    void WriteFile(const std::string& path, const std::string& info);
    void WritePreciseTargets(const std::vector<std::string>& result);
    void WritePreciseNinjaFile(const std::vector<Module*>& module_list);
    ModuleCheckResult CheckModulePath(Module* module, const std::vector<std::string>& cache_list);
    void ApplyTargetFilters(std::vector<std::pair<Module*, int>>& modules_with_type);  // 过滤带类型标记的模块列表
    PreciseManager() {}
    PreciseManager(const std::string& outDir, const std::string& rootDir, const std::string& preciseConfig);
    PreciseManager &operator = (const PreciseManager &) = delete;
};

#endif // PRECISE_H_