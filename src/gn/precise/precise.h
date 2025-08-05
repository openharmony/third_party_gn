// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRECISE_H_
#define PRECISE_H_

#include <climits>
#include <iostream>
#include <string>
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
#include "gn/value.h"

class PreciseManager {
public:
    static void Init(const std::string& buildDir, const Value* preciseConfig)
    {
        if (instance_ != nullptr) {
            return;
        }
        if (!preciseConfig || preciseConfig->string_value().empty()) {
            std::cout << "precise config null." << std::endl;
            return;
        }
        instance_ = new PreciseManager(buildDir, preciseConfig->string_value());
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
    bool CheckContains(const std::string& file, bool isHFile);
    bool CheckIncludeInConfig(const Config* config);
    bool CheckIncludeInTarget(const Item* item);
    bool CheckSourceInTarget(const Item* item);
    bool CheckConfigInfo(const UniqueVector<LabelConfigPair>& configs);
    bool CheckPrivateConfigs(const Item* item);
    bool CheckPublicConfigs(const Item* item);
    bool CheckAllDepConfigs(const Item* item);
    bool CheckModuleInGn(const std::string& label);
    bool CheckModuleMatch(const std::string& label);
    bool FilterType(const Item* item);
    bool IsDependent(const Node* node);
    bool IsIgnore(const std::string& name);
    bool IsInMaxRange(const std::string& name);
    bool IsContainModifiedFiles(const std::string& file, bool isHFile);
    bool IsFirstRecord(const std::vector<std::string>& result, const std::string& name);
    bool IsTargetTypeMatch(const Item* item);
    bool IsTestOnlyMatch(const Item* item);
    void EnsurePathExists(const std::string& filePath);
    void PreciseSearch(const Node* node, std::vector<std::string>& result, std::vector<std::string>& log,
        bool forGn, int depth, int maxDepth);
    void WriteFile(const std::string& path, const std::string& info);
    void WritePreciseTargets(const std::vector<std::string>& result, const std::vector<std::string>& log);
    PreciseManager() {}
    PreciseManager(const std::string& outDir, const std::string& preciseConfig);
    PreciseManager &operator = (const PreciseManager &) = delete;
};

#endif // PRECISE_H_