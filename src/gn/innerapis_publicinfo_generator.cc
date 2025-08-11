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
#include "gn/standard_out.h"

InnerApiPublicInfoGenerator *InnerApiPublicInfoGenerator::instance_ = nullptr;

static bool StartWith(const std::string &str, const std::string &prefix)
{
    return (str.rfind(prefix, 0) == 0);
}

static std::string GetOutName(const Target *target, const std::string &module, const std::string &type, RustValues::CrateType crateType)
{
    std::string output = target->output_name();
    std::string extension = target->output_extension();
    if(extension != ""){
      extension = "." + extension;
    }else{
      if (crateType == RustValues::CRATE_AUTO){
        if (type == "shared_library") {
          extension = ".z.so";
        } else if (type == "static_library") {
          extension = ".a";
        }
      }else{
        if(crateType == RustValues::CRATE_RLIB){
          extension = ".rlib";
        }else if(crateType == RustValues::CRATE_DYLIB || crateType == RustValues::CRATE_PROC_MACRO){
          extension = ".dylib.so";
        } else if(crateType == RustValues::CRATE_STATICLIB){
          extension = ".a";
        } else if(crateType == RustValues::CRATE_CDYLIB){
          extension = ".z.so";
        }
      }
    }
    if (output  == "") {
      output  = module;
    }
    if (!StartWith(output , "lib") && crateType != RustValues::CRATE_BIN && type != "executable"){
      output  = "lib" + output ;
    }
    return output  + extension;
}

static std::string GetRustCrateInfo(const Target* target, const Label& toolchain_label)
{
    std::string info = "";
    if (target->has_rust_values()) {
        info += ",\n  \"rust_crate_name\": \"" + target->rust_values().crate_name() + "\",";
        info += "\n  \"rust_crate_type\": \"" + RustValues::GetCrateTypeStr(RustValues::InferredCrateType(target)) + "\"";
        auto private_deps = target->private_deps();
        if (private_deps.size() > 0) {
            info += ",\n  \"rust_deps\": [\n    ";
            bool first = true;
            for (const auto& dep : private_deps) {
                std::string dep_str = dep.label.GetUserVisibleName(toolchain_label);
                if (dep_str.find("__check") != std::string::npos ||
                    dep_str.find("__info") != std::string::npos ||
                    dep_str.find("__notice") != std::string::npos ||
                    dep_str.find("__collect") != std::string::npos) {
                    continue;
                }
                if (!first) {
                    info += ",\n    ";
                }
                first = false;
                info += "\"" + dep_str + "\"";
            }
            info += "\n  ]";
        }
    }
    return info;
}

static bool TraverIncludeDirs(const Target *target, const OhosComponentChecker *checker,
    const std::string &label, Err *err)
{
    if (checker == nullptr) {
        return true;
    }

    std::vector<SourceDir> dirs = target->include_dirs();
    for (const SourceDir &dir : dirs) {
        if (!checker->CheckIncludesAbsoluteDepsOther(target, label, dir.value(), err)) {
            return false;
        }
    }
    return true;
}

static bool CheckIncludes(const Target *target, const OhosComponentChecker *checker,
    const std::string &dir, Err *err, bool isPublic)
{
    std::string label = target->label().GetUserVisibleName(false);
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

static std::string ReplaceDoubleQuotes(const std::string &input) {
    std::string result;
    for (char c : input) {
        if (c == '"') {
            result += "\\\"";
        } else {
            result += c;
        }
    }
    return result;
}

static std::string GetSingleFlagInfo(const std::string &name, const std::vector<std::string> &flags)
{
    std::string info;
    if (!flags.empty()) {
        info +=",\n    \"";
        info += name;
        info += "\": [\n      ";
        bool first = true;
        for (const std::string &flag : flags) {
            if (!first) {
                info += ",\n      ";
            }
            first = false;
            std::string str = ReplaceDoubleQuotes(flag);
            info += "\"" + str + "\"";
        }
        info += "\n    ]";
    }
    return info;
}

static std::string GetFlagsInfo(const Config *config)
{
    std::string info;
    info += GetSingleFlagInfo("arflags", config->own_values().arflags());
    info += GetSingleFlagInfo("asmflags", config->own_values().asmflags());
    info += GetSingleFlagInfo("cflags", config->own_values().cflags());
    info += GetSingleFlagInfo("cflags_c", config->own_values().cflags_c());
    info += GetSingleFlagInfo("cflags_cc", config->own_values().cflags_cc());
    info += GetSingleFlagInfo("cflags_objc", config->own_values().cflags_objc());
    info += GetSingleFlagInfo("cflags_objcc", config->own_values().cflags_objcc());
    info += GetSingleFlagInfo("defines", config->own_values().defines());
    info += GetSingleFlagInfo("frameworks", config->own_values().frameworks());
    info += GetSingleFlagInfo("weak_frameworks", config->own_values().weak_frameworks());
    info += GetSingleFlagInfo("ldflags", config->own_values().ldflags());
    info += GetSingleFlagInfo("rustflags", config->own_values().rustflags());
    info += GetSingleFlagInfo("rustenv", config->own_values().rustenv());
    info += GetSingleFlagInfo("swiftflags", config->own_values().swiftflags());
    info += "\n";
    return info;
}

static bool TraverLibDirs(const Target *target, const OhosComponentChecker *checker,
    const std::vector<SourceDir> &dirs, Err *err)
{
    if (checker == nullptr) {
        return true;
    }
    std::string label = target->label().GetUserVisibleName(false);
    for (const SourceDir &dir : dirs) {
        if (!checker->CheckLibDir(target, label, dir.value(), err)) {
            return false;
        }
    }
    return true;
}

static std::string GetIncludeDirsInfo(const Target *target, const Config *config,
    const OhosComponentChecker *checker, Err *err, bool isPublic)
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
        if (!CheckIncludes(target, checker, dir.value(), err, isPublic)) {
            return "";
        }
    }

    info += "\n    ]";
    return info;
}

static std::string GetConfigInfo(const Target *target, const UniqueVector<LabelConfigPair> &configs,
    const OhosComponentChecker *checker, Err *err, bool isPublic)
{
    std::string info = "[{";
    bool first = true;
    for (const auto &config : configs) {
        const std::vector<SourceDir>lib_dirs = config.ptr->own_values().lib_dirs();
        if (!TraverLibDirs(target, checker, lib_dirs, err)) {
            return "";
        }
        std::string label = config.label.GetUserVisibleName(false);
        if (!first) {
            info += ", {";
        }
        first = false;
        info += "\n    \"label\": \"" + label + "\"";
        info += GetIncludeDirsInfo(target, config.ptr, checker, err, isPublic);
        info += GetFlagsInfo(config.ptr);
        info += "  }";
    }
    info += "]";
    return info;
}

static std::string GetPublicConfigsInfo(const Target *target, const OhosComponentChecker *checker, Err *err)
{
    std::string info = "";
    const UniqueVector<LabelConfigPair> configs = target->own_public_configs();
    if (configs.size() > 0) {
        info += ",\n  \"public_configs\": ";
        std::string tmp = GetConfigInfo(target, configs, checker, err, true);
        if (tmp == "") {
            return "";
        }
        info += tmp;
    }
    return info;
}

static std::string GetAllDependentConfigsInfo(const Target *target, const OhosComponentChecker *checker, Err *err)
{
    std::string info = "";
    const UniqueVector<LabelConfigPair> all_configs = target->own_all_dependent_configs();
    if (all_configs.size() > 0) {
        info += ",\n  \"all_dependent_configs\": ";
        std::string tmp = GetConfigInfo(target, all_configs, checker, err, true);
        if (tmp == "") {
            return "";
        }
        info += tmp;
        if (checker != nullptr) {
            if (!checker->CheckAllDepsConfigs(target, target->label().GetUserVisibleName(false), err)) {
                return "";
            }
        }
    }
    return info;
}

static void TraverPrivateConfigsInfo(const Target *target, const OhosComponentChecker *checker, Err *err)
{
    const UniqueVector<LabelConfigPair> private_configs = target->own_configs();
    if (private_configs.size() > 0) {
        (void)GetConfigInfo(target, private_configs, checker, err, false);
    }
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
            if (!checker->CheckPublicDeps(target, label, dep_str, err)) {
                return "";
            }
        }
        info += "\n  ]";
    }
    return info;
}

static std::string GetOutNameAndTypeInfo(const Target *target, const std::string &module)
{
    std::string type(Target::GetStringForOutputType(target->output_type()));
    std::string info = "";
    RustValues::CrateType crate_type = target->has_rust_values() ? target->rust_values().crate_type() : RustValues::CrateType::CRATE_AUTO;
    const std::string name = GetOutName(target, module, type, crate_type);
    const std::vector<OutputFile>& outputFiles = target->computed_outputs();
    if (outputFiles.size() > 0) {
      info += ",\n  \"outputs\": [\n    ";
      bool first = true;
      for (const auto& outputFile : outputFiles) {
        if (!first) {
          info += ",\n    ";
        }
        first = false;
        info += "\"" + outputFile.value() + "\"";
      }
      info += "\n  ]";
    }
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
    return info;
}

static std::string GetPublicInfo(const Target *target, const std::string &label,
    const OhosComponentChecker *checker, Err *err)
{
    std::string info = GetPublicConfigsInfo(target, checker, err);
    info += GetAllDependentConfigsInfo(target, checker, err);
    if (target->all_headers_public()) {
        info += ",\n  \"public\": [ \"*\" ]";
    } else {
        info += GetPublicHeadersInfo(target);
    }
    info += GetPublicDepsInfo(target, label, checker, err);
    info += "\n}\n";
    return info;
}

static std::string GetBaseInfo(const Target *target, const std::string &label,
    const std::string &module, const OhosComponent *component)
{
    std::string info = "{\n";
    info += "  \"label\": \"" + label + "\"";
    info += GetOutNameAndTypeInfo(target, module);
    info += GetRustCrateInfo(target, target->settings()->default_toolchain_label());
    if (component != nullptr) {
        info += GetComponentInfo(component->subsystem(), component->name(), component->path());
    }
    return info;
}

static void WritePublicInfo(const std::string &build, const std::string &module,
    const OhosComponent *component, const std::string &info)
{
    const std::string dir = build + "/" + component->subsystem() + "/" + component->name() + "/publicinfo";
    base::FilePath path(dir);
    base::CreateDirectory(path);
    std::ofstream file;
    const std::string json = dir + "/" + module + ".json";
    file.open(json, std::ios::out);
    file << info;
    file.close();
}

void InnerApiPublicInfoGenerator::DoGeneratedInnerapiPublicInfo(const Target *target,
    const OhosComponentChecker *checker, Err *err)
{
    if (target == nullptr || (ignoreTest_ && target->testonly())) {
        return;
    }
    std::string label = target->label().GetUserVisibleName(false);
    size_t pos = label.find(":");
    if (pos == std::string::npos) {
        return;
    }
    std::string module = label.substr(pos + 1, label.length() - 1);
    const OhosComponent *component = target->ohos_component();
    std::string info = GetBaseInfo(target, label, module, component);
    info += GetPublicInfo(target, label, checker, err);
    const std::vector<SourceDir> &lib_dirs = target->config_values().lib_dirs();
    if (!TraverLibDirs(target, checker, lib_dirs, err)) {
        return;
    }
    if (!TraverIncludeDirs(target, checker, label, err)) {
        return;
    }
    TraverPrivateConfigsInfo(target, checker, err);

    if (target->testonly() || component == nullptr || !component->isInnerApi(label)) {
        return;
    }

    WritePublicInfo(build_dir_, module, component, info);
    return;
}

bool InnerApiPublicInfoGenerator::GeneratedInnerapiPublicInfo(const std::vector<const Target*>& items, Err *err)
{
    const OhosComponentChecker *checker = OhosComponentChecker::getInstance();
    for (const Target *item : items) {
        if (item->ohos_component()) {
          DoGeneratedInnerapiPublicInfo(item, checker, err);
          if (err->has_error()) {
            return false;
          }
        }
    }
    return true;
}