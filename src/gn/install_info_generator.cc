// Copyright 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/install_info_generator.h"

#include <fstream>
#include <iostream>
#include <unordered_map>
#include <vector>
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
#include "gn/variables.h"

InstallInfoGenerator *InstallInfoGenerator::instance_ = nullptr;
std::unordered_map<std::string, std::vector<std::string>> InstallInfoGenerator::labelInstallInfo_{};

namespace {
    bool StartWith(const std::string &str, const std::string prefix)
    {
        return (str.rfind(prefix, 0) == 0);
    }

    bool EndWith(const std::string &str, const std::string suffix)
    {
        auto pos = str.rfind(suffix);
        return (pos != std::string::npos)
            && (pos + suffix.size() == str.size());
    }

    bool IsFileExists(const std::string &path)
    {
        if (access(path.c_str(), F_OK) == 0) {
            return true;
        }
        return false;
    }

    std::string GetOutName(const Scope *scope, std::string targetName, const std::string type)
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

    std::string GetInstallImages(const std::vector<std::string>& install_images)
    {
        std::string info = "";
        if (!install_images.empty()) {
            info += ",\n  \"install_images\":[";
            bool first = true;
            for (const auto& item : install_images) {
                if (first) {
                    first = false;
                    info += "\n    \"" + item + "\"";
                } else {
                    info += ",\n    \"" + item + "\"";
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
}

void InstallInfoGenerator::GeneratedInstallInfo(Target *target, Label label,
    Scope *scope, const std::string type, Err *err)
{
    if (target == nullptr || (ignoreTest_ && target->testonly())) {
        return;
    }
    
    const OhosComponent *component = target->ohos_component();
    if (target->testonly() || component == nullptr) {
        return;
    }

    const Value* install_images_list = scope->GetValue("install_images");
    const std::string COLLECT_SUFFIX{"__collect"};
    std::string labelString = label.GetUserVisibleName(false);
    std::string labelCollect = labelString + COLLECT_SUFFIX;
    if (install_images_list != nullptr && EndWith(labelString, COLLECT_SUFFIX)) {
        if (labelInstallInfo_.find(labelString) == labelInstallInfo_.end()) {
            labelInstallInfo_[labelString] = {};
        }
        for (const auto& item : install_images_list->list_value()) {
            labelInstallInfo_[labelString].emplace_back(item.string_value());
        }
        return;
    }
    if (labelInstallInfo_.find(labelCollect) == labelInstallInfo_.end()) {
        return;
    }

    std::string info = "{\n  \"label\": \"" + labelString + "\"";
    info += GetInstallImages(labelInstallInfo_.at(labelCollect));
    int pos = labelString.find(":");
    std::string targetName = labelString.substr(pos + 1, labelString.length() - 1);
    info += GetOutNameAndTypeInfo(scope, targetName, type);
    info += GetComponentInfo(component->subsystem(), component->name(), component->path());

    const std::string dir = build_dir_ + "/" + component->subsystem() + "/" + component->name() + "/install_info";
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
