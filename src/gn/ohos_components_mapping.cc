// Copyright (c) 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/ohos_components_mapping.h"

#include <fstream>
#include <functional>
#include <iostream>
#include <sys/stat.h>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/values.h"
#include "gn/build_settings.h"
#include "gn/config.h"
#include "gn/filesystem_utils.h"
#include "gn/functions.h"
#include "gn/ohos_components.h"
#include "gn/parse_tree.h"
#include "gn/settings.h"
#include "gn/substitution_writer.h"
#include "gn/target.h"
#include "gn/value.h"

static const std::string MAPPING_FILE_PATH = "component_mapping.json";

OhosComponentMapping *OhosComponentMapping::instance_ = nullptr;

static std::map<std::string, std::string> gni_mapping_file_map_;

static bool StartWith(const std::string &str, const std::string prefix)
{
    return (str.rfind(prefix, 0) == 0);
}

static bool ReadBuildConfigFile(base::FilePath path, std::string &content)
{
    if (!base::ReadFileToString(path, &content)) {
        return false;
    }
    return true;
}

static void LoadGniMappingFileMap(const base::Value &value)
{
    for (auto info : value.DictItems()) {
        gni_mapping_file_map_[info.first] = info.second.GetString();
    }
}

static std::map<std::string, std::function<void(const base::Value &value)>> mapping_map_ = {
    { "gni_mapping_file", LoadGniMappingFileMap }
};

static void LoadMappingFile(const std::string &build_dir)
{
    std::string mappingContent;
    if (!ReadBuildConfigFile(base::FilePath(build_dir + "/build_configs/" + MAPPING_FILE_PATH), mappingContent)) {
        return;
    }
    const base::DictionaryValue *mapping_dict;
    std::unique_ptr<base::Value> mapping = base::JSONReader::ReadAndReturnError(mappingContent,
        base::JSONParserOptions::JSON_PARSE_RFC, nullptr, nullptr, nullptr, nullptr);
    if (!mapping) {
        return;
    }
    if (!mapping->GetAsDictionary(&mapping_dict)) {
        return;
    }
    for (const auto kv : mapping_dict->DictItems()) {
        auto iter = mapping_map_.find(kv.first);
        if (iter != mapping_map_.end()) {
            iter->second(kv.second);
        }
    }
    return;
}

OhosComponentMapping::OhosComponentMapping(const std::string &build_dir)
{
    build_dir_ = build_dir;
    LoadMappingFile(build_dir);
}

const std::string OhosComponentMapping::MappingTargetAbsoluteDpes(const BuildSettings* build_settings,
    const std::string label, const std::string deps) const
{
    if (build_settings == nullptr || StartWith(deps, "//build/") ||
        StartWith(deps, "//out/")  || StartWith(deps, "//prebuilts/")) {
        return "";
    }

    const OhosComponent *component = build_settings->GetOhosComponent(label);
    if (component == nullptr) {
        return "";
    }
    if (StartWith(deps, component->path())) {
        return "";
    }

    const OhosComponent *deps_component = build_settings->GetOhosComponent(deps);
    if (deps_component == nullptr) {
        return "";
    }

    size_t pos = deps.find(":");
    if (pos == std::string::npos) {
        return "";
    }
    return deps_component->getInnerApi(deps.substr(pos + 1));
}

const std::string OhosComponentMapping::MappingImportOther(const BuildSettings* build_settings,
    const std::string label, const std::string deps) const
{
    if (build_settings == nullptr || StartWith(deps, "//build/") ||
        StartWith(deps, "//out/")  || StartWith(deps, "//prebuilts/")) {
        return "";
    }
    const OhosComponent *component = build_settings->GetOhosComponent(label);
    if (component == nullptr) {
        return "";
    }
    if (StartWith(deps, component->path())) {
        return "";
    }
    return gni_mapping_file_map_[deps];
}
