// Copyright 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/innerapis_publicinfo_generator.h"

#include <fstream>
#include <filesystem>
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
#include "gn/ohos_components_checker.h"
#include "gn/parse_tree.h"
#include "gn/settings.h"
#include "gn/substitution_writer.h"
#include "gn/target.h"
#include "gn/value.h"

InnerApiPublicInfoGenerator *InnerApiPublicInfoGenerator::instance_ = nullptr;

static bool StartWith(const std::string &str, const std::string prefix)
{
    return (str.rfind(prefix, 0) == 0);
}

static bool IsFileExists(const std::string &path)
{
    if (access(path.c_str(), F_OK) == 0) {
        return true;
    }
    return false;
}

static std::string GetOutName(const Scope *scope, std::string targetName, const std::string type)
{
    std::string outputName = "";
    std::string extension = "";
    const Value *outputNameValue = scope->GetValue("output_name");
    const Value *extensionValue = scope->GetValue("output_extension");
    if (outputNameValue != nullptr) {
        outputName = outputNameValue->string_value();
    }
    if (extensionValue != nullptr) {
        extension = extensionValue->string_value();
    }

    if (outputName == "") {
        outputName = targetName;
    }
    if (type == "shared_library") {
        if (extension == "") {
            extension = ".z.so";
        } else {
            extension = "." + extension;
        }
        if (!StartWith(outputName, "lib")) {
            outputName = "lib" + outputName;
        }
    } else if (type == "static_library") {
        extension = ".a";
        if (!StartWith(outputName, "lib")) {
            outputName = "lib" + outputName;
        }
    } else if (type == "rust_library") {
        if (extension == "") {
            extension = ".dylib.so";
        } else {
            extension = "." + extension;
        }
        if (!StartWith(outputName, "lib")) {
            outputName = "lib" + outputName;
        }
    }
    return outputName + extension;
}

static bool TraverIncludeDirs(const OhosComponentChecker *checker, const Target *target, const Scope *scope,
    const std::string label, Err *err)
{
    const Value *includes = scope->GetValue("include_dirs");
    if (includes != nullptr) {
        const std::vector<Value> &includes_list = includes->list_value();
        for (size_t i = 0; i < includes_list.size(); i++) {
            SourceDir real_dir = scope->GetSourceDir().ResolveRelativeDir(includes_list[i], err,
                scope->settings()->build_settings()->root_path_utf8());
            if (!checker->CheckIncludesAbsoluteDepsOther(target, label, real_dir.value(), err)) {
                return false;
            }
        }
    }
    return true;
}

static bool CheckIncludes(const OhosComponentChecker *checker, const std::string dir, bool isPublic,
    const PublicConfigInfoParams &params)
{
    const Target *target = params.target;
    const std::string label = params.label;
    Err *err = params.err;
    if (isPublic) {
        if (checker != nullptr) {
            if (!checker->CheckInnerApiIncludesOverRange(target, label, dir, err)) {
                return false;
            }
        }
    }
    if (checker != nullptr) {
        if (!checker->CheckIncludesAbsoluteDepsOther(target, label, dir, err)) {
            return false;
        }
    }
    return true;
}

static std::string GetIncludeDirsInfo(const Config *config, const OhosComponentChecker *checker, bool isPublic,
    const PublicConfigInfoParams &params)
{
    std::string info = ",\n    \"include_dirs\": [\n      ";
    const std::vector<SourceDir> dirs = config->own_values().include_dirs();
    bool first = true;
    for (const SourceDir &dir : dirs) {
        if (!first) {
            info += ",\n      ";
        }
        first = false;
        info += "\"" + dir.value() + "\"";
        if (!CheckIncludes(checker, dir.value(), isPublic, params)) {
            return "";
        }
    }
    info += "\n    ]\n";
    return info;
}

static std::string GetPublicConfigInfo(const PublicConfigInfoParams &params, Scope *scope,
    const UniqueVector<LabelConfigPair> &configs, const OhosComponentChecker *checker, bool isPublic)
{
    const Target *target = params.target;
    const std::string label = params.label;
    Err *err = params.err;
    Scope::ItemVector *collector = scope->GetItemCollector();
    std::string info = "[{";
    bool firstConfig = true;
    for (const auto &config : configs) {
        if (!firstConfig) {
            info += ", {";
        }
        firstConfig = false;
        info += "\n    \"label\": \"" + config.label.GetUserVisibleName(false) + "\"";
        for (auto &item : *collector) {
            if (item->label() != config.label)
                continue;
            Config *as_config = item->AsConfig();
            if (!as_config) {
                continue;
            }
            PublicConfigInfoParams params = { target, label, err };
            info += GetIncludeDirsInfo(as_config, checker, isPublic, params);
        }
        info += "  }";
    }
    info += "]";
    return info;
}

std::string GetPublicConfigsInfo(const Target *target, const std::string &labelString, Scope *scope,
    const OhosComponentChecker *checker, Err *err)
{
    std::string info = "";
    const UniqueVector<LabelConfigPair> configs = target->public_configs();
    if (configs.size() > 0) {
        info += ",\n  \"public_configs\": ";
        PublicConfigInfoParams params = { target, labelString, err };
        std::string tmp = GetPublicConfigInfo(params, scope, configs, checker, true);
        if (tmp == "") {
            return "";
        }
        info += tmp;
    }
    return info;
}

std::string GetAllDependentConfigsInfo(const Target *target, const std::string &labelString, Scope *scope,
    const OhosComponentChecker *checker, Err *err)
{
    std::string info = "";
    const UniqueVector<LabelConfigPair> all_configs = target->all_dependent_configs();
    if (all_configs.size() > 0) {
        info += ",\n  \"all_dependent_configs\": ";
        PublicConfigInfoParams params = { target, labelString, err };
        std::string tmp = GetPublicConfigInfo(params, scope, all_configs, checker, true);
        if (tmp == "") {
            return "";
        }
        info += tmp;
        if (checker != nullptr) {
            if (!checker->CheckAllDepsConfigs(target, labelString, err)) {
                return "";
            }
        }
    }
    return info;
}

std::string GetPrivateConfigsInfo(const Target *target, const std::string &labelString, Scope *scope,
    const OhosComponentChecker *checker, Err *err)
{
    std::string info = "";
    const UniqueVector<LabelConfigPair> private_configs = target->configs();
    if (private_configs.size() > 0) {
        PublicConfigInfoParams params = { target, labelString, err };
        std::string tmp = GetPublicConfigInfo(params, scope, private_configs, checker, false);
        if (tmp == "") {
            return "";
        }
    }
    return info;
}


std::string GetPublicHeadersInfo(const Target *target)
{
    std::string info = "";
    const std::vector<SourceFile> &headers = target->public_headers();
    if (headers.size() > 0) {
        info += ",\n  \"public\": [\n    ";
        bool first = true;
        for (const auto &header : headers) {
            if (!first) {
                info += ",\n    ";
            }
            first = false;
            info += "\"" + header.value() + "\"";
        }
        info += "  ]";
    }
    return info;
}

std::string GetPublicDepsInfo(const Target *target, const std::string &labelString,
    const OhosComponentChecker *checker, Err *err)
{
    std::string info = "";
    const LabelTargetVector deps = target->public_deps();
    if (deps.size() > 0) {
        info += ",\n  \"public_deps\": [\n    ";
        bool first = true;
        for (const auto &dep : deps) {
            if (!first) {
                info += ",\n    ";
            }
            first = false;
            std::string dep_str = dep.label.GetUserVisibleName(false);
            info += "\"" + dep_str + "\"";
            if (checker == nullptr) {
                continue;
            }
            if (!checker->CheckInnerApiPublicDepsInner(target, labelString, dep_str, err)) {
                return "";
            }
        }
        info += "\n  ]";
    }
    return info;
}

std::string GetOutNameAndTypeInfo(const Scope *scope, const std::string &targetName, const std::string &type)
{
    std::string info = "";
    const std::string name = GetOutName(scope, targetName, type);
    info += ",\n  \"out_name\":\"" + name + "\"";
    info += ",\n  \"type\":\"" + type + "\"";
    return info;
}

std::string GetComponentInfo(const std::string &subsystem, const std::string &component, const std::string &path)
{
    std::string info = "";
    info += ",\n  \"subsystem\":\"" + subsystem + "\"";
    info += ",\n  \"component\":\"" + component + "\"";
    info += ",\n  \"path\":\"" + path + "\"";
    info += "\n}\n";
    return info;
}

void InnerApiPublicInfoGenerator::GeneratedInnerapiPublicInfo(Target *target, Label label, Scope *scope,
    const std::string type, Err *err)
{
    if (target == nullptr || (ignoreTest_ && target->testonly())) {
        return;
    }
    const OhosComponentChecker *checker = OhosComponentChecker::getInstance();

    std::string labelString = label.GetUserVisibleName(false);
    std::string info = "{\n";

    info += "  \"label\": \"" + labelString + "\"";
    info += GetPublicConfigsInfo(target, labelString, scope, checker, err);
    info += GetAllDependentConfigsInfo(target, labelString, scope, checker, err);
    info += GetPrivateConfigsInfo(target, labelString, scope, checker, err);

    if (checker != nullptr) {
        if (!TraverIncludeDirs(checker, target, scope, labelString, err)) {
            return;
        }
    }

    if (target->all_headers_public()) {
        info += ",\n  \"public\": [ \"*\" ]";
    } else {
        info += GetPublicHeadersInfo(target);
    }

    info += GetPublicDepsInfo(target, labelString, checker, err);

    const OhosComponent *component = target->ohos_component();
    if (target->testonly() || component == nullptr || !component->isInnerApi(labelString)) {
        return;
    }
    int pos = labelString.find(":");
    std::string targetName = labelString.substr(pos + 1, labelString.length() - 1);
    info += GetOutNameAndTypeInfo(scope, targetName, type);
    info += GetComponentInfo(component->subsystem(), component->name(), component->path());

    const std::string dir = build_dir_ + "/" + component->subsystem() + "/" + component->name() + "/publicinfo";
    base::FilePath path(dir);
    base::CreateDirectory(path);
    std::ofstream publicfile;
    const std::string json_path = dir + "/" + targetName + ".json";
    if (IsFileExists(json_path)) {
        return;
    }
    publicfile.open(json_path, std::ios::out);
    publicfile << info;
    publicfile.close();
    return;
}
