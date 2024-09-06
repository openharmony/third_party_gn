// Copyright 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/innerapis_publicinfo_generator.h"

#include <fstream>
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

static std::map<std::string, std::map<std::string, std::vector<std::string>>> external_public_configs_;

static std::string build_out_;

static bool StartWith(const std::string &str, const std::string &prefix)
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

static std::string GetOutName(const Scope *scope, const std::string &target_name, const std::string &type)
{
    std::string output_name = "";
    std::string extension = "";
    const Value *name_value = scope->GetValue("output_name");
    const Value *extension_value = scope->GetValue("output_extension");
    if (name_value != nullptr) {
        output_name = name_value->string_value();
    }
    if (extension_value != nullptr) {
        extension = extension_value->string_value();
    }

    if (output_name == "") {
        output_name = target_name;
    }
    if (type == "shared_library") {
        if (extension == "") {
            extension = ".z.so";
        } else {
            extension = "." + extension;
        }
        if (!StartWith(output_name, "lib")) {
            output_name = "lib" + output_name;
        }
    } else if (type == "static_library") {
        extension = ".a";
        if (!StartWith(output_name, "lib")) {
            output_name = "lib" + output_name;
        }
    } else if (type == "rust_library") {
        if (extension == "") {
            extension = ".dylib.so";
        } else {
            extension = "." + extension;
        }
        if (!StartWith(output_name, "lib")) {
            output_name = "lib" + output_name;
        }
    }
    return output_name + extension;
}

static bool TraverIncludeDirs(const OhosComponentChecker *checker, const Target *target, const Scope *scope,
    const std::string &label, Err *err)
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

static bool CheckIncludes(const OhosComponentChecker *checker, const std::string &dir,
    const PublicConfigInfoParams &params)
{
    const Target *target = params.target;
    const std::string label = params.label;
    Err *err = params.err;
    if (params.is_public) {
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

static bool CheckInExternalPublicConfigsMap(const std::string &label, bool is_public)
{
    if (!is_public) {
        return false;
    }

    if (external_public_configs_.find(label) != external_public_configs_.end() &&
        external_public_configs_[label].empty()) {
        return true;
    }
    return false;
}

static void SetExternalPublicConfigsKey(const std::string &label, bool is_public)
{
    if (!is_public) {
        return;
    }
    if (external_public_configs_.find(label) != external_public_configs_.end()) {
        return;
    }
    external_public_configs_[label];
    return;
}

static void SetExternalPublicConfigsValue(const std::string &label, const std::string &name, const std::string &value)
{
    external_public_configs_[label][name].push_back(value);
    return;
}

static std::string GetFlagsInfo(const Config *config, const std::string &label, bool needSetToConfigs)
{
    std::string info;

#define GET_FLAGS_INFO(flags)                                            \
    const std::vector<std::string> flags = config->own_values().flags(); \
    if (!flags.empty()) {                                                \
        info +=",\n    \"";                                              \
        info += #flags;                                                  \
        info += "\": [\n      ";                                         \
        bool first_##flags = true;                                       \
        for (const std::string &flag : flags) {                          \
            if (!first_##flags) {                                        \
                info += ",\n      ";                                     \
            }                                                            \
            first_##flags = false;                                       \
            info += "\"" + flag + "\"";                                  \
            if (needSetToConfigs) {                                      \
                SetExternalPublicConfigsValue(label, #flags, flag);      \
            }                                                            \
        }                                                                \
        info += "\n    ]";                                               \
    }

    GET_FLAGS_INFO(arflags)
    GET_FLAGS_INFO(asmflags)
    GET_FLAGS_INFO(cflags)
    GET_FLAGS_INFO(cflags_c)
    GET_FLAGS_INFO(cflags_cc)
    GET_FLAGS_INFO(cflags_objc)
    GET_FLAGS_INFO(cflags_objcc)
    GET_FLAGS_INFO(defines)
    GET_FLAGS_INFO(frameworks)
    GET_FLAGS_INFO(weak_frameworks)
    GET_FLAGS_INFO(ldflags)
    GET_FLAGS_INFO(rustflags)
    GET_FLAGS_INFO(rustenv)
    GET_FLAGS_INFO(swiftflags)
#undef GET_FLAGS_INFO

    info += "\n";
    return info;
}

static void WritePublicConfigs(const std::string &label, const std::string &info)
{
    int pos = label.find(":");
    std::string dir = build_out_ + "/" + "external_public_configs" + label.substr(1, pos - 1);
    std::string name = label.substr(pos + 1);

    base::FilePath path(dir);
    base::CreateDirectory(path);
    std::ofstream public_file;
    const std::string json_path = dir + "/" + name + ".json";
    if (IsFileExists(json_path)) {
        return;
    }
    public_file.open(json_path, std::ios::out);
    public_file << info;
    public_file.close();
    return;
}

static void GeneratedExternalPublicConfigs(const std::string &label)
{
    std::string info = "{";
    info += "\n    \"label\": \"" + label + "\",\n";

    bool first_outer = true;
    for (const auto& pair : external_public_configs_[label]) {
        if (!first_outer) {
            info += ",\n";
        }
        first_outer = false;

        info += "    \"" + pair.first + "\": [\n";
        bool first_inner = true;
        for (const auto& value : pair.second) {
            if (!first_inner) {
                info += ",\n";
            }
            first_inner = false;
            info += "        \"" + value + "\"";
        }
        info += "\n    ]";
    }
    info += "\n}";
    WritePublicConfigs(label, info);
    return;
}

static std::string GetIncludeDirsInfo(const Config *config, const OhosComponentChecker *checker,
    const std::string &label, const PublicConfigInfoParams &params, bool is_in_map)
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
        if (is_in_map) {
            SetExternalPublicConfigsValue(label, "include_dirs", dir.value());
        }
        if (!CheckIncludes(checker, dir.value(), params)) {
            return "";
        }
    }

    info += "\n    ]";
    return info;
}

static std::string GetPublicConfigInfo(const PublicConfigInfoParams &params, Scope *scope,
    const UniqueVector<LabelConfigPair> &configs, const OhosComponentChecker *checker)
{
    Scope::ItemVector *collector = scope->GetItemCollector();
    std::string info = "[{";
    bool first = true;
    for (const auto &config : configs) {
        if (!first) {
            info += ", {";
        }
        first = false;
        bool found = false;

        std::string label = config.label.GetUserVisibleName(false);
        info += "\n    \"label\": \"" + label + "\"";
        for (auto &item : *collector) {
            if (item->label().GetUserVisibleName(false) != label) {
                continue;
            }

            Config *as_config = item->AsConfig();
            if (!as_config) {
                continue;
            }
            found = true;
            bool is_in_map = CheckInExternalPublicConfigsMap(label, params.is_public);
            info += GetIncludeDirsInfo(as_config, checker, label, params, is_in_map);
            info += GetFlagsInfo(as_config, label, is_in_map);
            if (is_in_map) {
                GeneratedExternalPublicConfigs(label);
            }
        }
        info += "  }";
        if (!found) {
            SetExternalPublicConfigsKey(label, params.is_public);
        }
    }
    info += "]";
    return info;
}

static std::string GetPublicConfigsInfo(const Target *target, const std::string &label, Scope *scope,
    const OhosComponentChecker *checker, Err *err)
{
    std::string info = "";
    const UniqueVector<LabelConfigPair> configs = target->public_configs();
    if (configs.size() > 0) {
        info += ",\n  \"public_configs\": ";
        PublicConfigInfoParams params = { target, label, err, true };
        std::string tmp = GetPublicConfigInfo(params, scope, configs, checker);
        if (tmp == "") {
            return "";
        }
        info += tmp;
    }
    return info;
}

static std::string GetAllDependentConfigsInfo(const Target *target, const std::string &label, Scope *scope,
    const OhosComponentChecker *checker, Err *err)
{
    std::string info = "";
    const UniqueVector<LabelConfigPair> all_configs = target->all_dependent_configs();
    if (all_configs.size() > 0) {
        info += ",\n  \"all_dependent_configs\": ";
        PublicConfigInfoParams params = { target, label, err, true };
        std::string tmp = GetPublicConfigInfo(params, scope, all_configs, checker);
        if (tmp == "") {
            return "";
        }
        info += tmp;
        if (checker != nullptr) {
            if (!checker->CheckAllDepsConfigs(target, label, err)) {
                return "";
            }
        }
    }
    return info;
}

static std::string GetPrivateConfigsInfo(const Target *target, const std::string &label, Scope *scope,
    const OhosComponentChecker *checker, Err *err)
{
    std::string info = "";
    const UniqueVector<LabelConfigPair> private_configs = target->configs();
    if (private_configs.size() > 0) {
        PublicConfigInfoParams params = { target, label, err, false };
        std::string tmp = GetPublicConfigInfo(params, scope, private_configs, checker);
        if (tmp == "") {
            return "";
        }
    }
    return info;
}


static std::string GetPublicHeadersInfo(const Target *target)
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

static std::string GetPublicDepsInfo(const Target *target, const std::string &label,
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
            if (!checker->CheckInnerApiPublicDepsInner(target, label, dep_str, err)) {
                return "";
            }
        }
        info += "\n  ]";
    }
    return info;
}

static std::string GetOutNameAndTypeInfo(const Scope *scope, const std::string &target, const std::string &type)
{
    std::string info = "";
    const std::string name = GetOutName(scope, target, type);
    info += ",\n  \"out_name\":\"" + name + "\"";
    info += ",\n  \"type\":\"" + type + "\"";
    return info;
}

static std::string GetComponentInfo(const std::string &subsystem, const std::string &component, const std::string &path)
{
    std::string info = "";
    info += ",\n  \"subsystem\":\"" + subsystem + "\"";
    info += ",\n  \"component\":\"" + component + "\"";
    info += ",\n  \"path\":\"" + path + "\"";
    info += "\n}\n";
    return info;
}

void InnerApiPublicInfoGenerator::GeneratedInnerapiPublicInfo(const Target *target, const Label &label, Scope *scope,
    const std::string &type, Err *err)
{
    if (target == nullptr || (ignoreTest_ && target->testonly())) {
        return;
    }
    build_out_ = build_dir_;
    const OhosComponentChecker *checker = OhosComponentChecker::getInstance();

    std::string label_string = label.GetUserVisibleName(false);
    std::string info = "{\n";

    info += "  \"label\": \"" + label_string + "\"";
    info += GetPublicConfigsInfo(target, label_string, scope, checker, err);
    info += GetAllDependentConfigsInfo(target, label_string, scope, checker, err);
    info += GetPrivateConfigsInfo(target, label_string, scope, checker, err);

    if (checker != nullptr) {
        if (!TraverIncludeDirs(checker, target, scope, label_string, err)) {
            return;
        }
    }

    if (target->all_headers_public()) {
        info += ",\n  \"public\": [ \"*\" ]";
    } else {
        info += GetPublicHeadersInfo(target);
    }

    info += GetPublicDepsInfo(target, label_string, checker, err);

    const OhosComponent *component = target->ohos_component();
    if (target->testonly() || component == nullptr || !component->isInnerApi(label_string)) {
        return;
    }
    int pos = label_string.find(":");
    std::string target_name = label_string.substr(pos + 1, label_string.length() - 1);
    info += GetOutNameAndTypeInfo(scope, target_name, type);
    info += GetComponentInfo(component->subsystem(), component->name(), component->path());

    const std::string dir = build_dir_ + "/" + component->subsystem() + "/" + component->name() + "/publicinfo";
    base::FilePath path(dir);
    base::CreateDirectory(path);
    std::ofstream public_file;
    const std::string json_path = dir + "/" + target_name + ".json";
    if (IsFileExists(json_path)) {
        return;
    }
    public_file.open(json_path, std::ios::out);
    public_file << info;
    public_file.close();
    return;
}