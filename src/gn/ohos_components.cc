// Copyright (c) 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "gn/ohos_components.h"

#include <cstring>
#include <iostream>
#include <map>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/values.h"

#include "gn/err.h"
#include "gn/filesystem_utils.h"
#include "gn/innerapis_publicinfo_generator.h"
#include "gn/ohos_components_checker.h"
#include "gn/ohos_components_impl.h"

/**
 * Ohos Component API
 *
 * Each component belongs to one subsystem.
 * Each component has a source path.
 * Each component has zero or more innerapis
 */

static const std::string EMPTY_INNERAPI;

static const int PATH_PREFIX_LEN = 2;

OhosComponent::OhosComponent() = default;

OhosComponent::OhosComponent(const char *name, const char *subsystem, const char *path)
{
    name_ = std::string(name);
    subsystem_ = std::string(subsystem);
    if (strncmp(path, "//", PATH_PREFIX_LEN) == 0) {
        path_ = std::string(path);
    } else {
        path_ = "//" + std::string(path);
    }
}

void OhosComponent::addInnerApi(const std::string &name, const std::string &label)
{
    innerapi_names_[name] = label;
    innerapi_labels_[label] = name;
}


const std::string &OhosComponent::getInnerApi(const std::string &innerapi) const
{
    if (auto res = innerapi_names_.find(innerapi); res != innerapi_names_.end()) {
        return res->second;
    }
    return EMPTY_INNERAPI;
}

bool OhosComponent::isInnerApi(const std::string &label) const
{
    if (auto res = innerapi_labels_.find(label); res != innerapi_labels_.end()) {
        return true;
    }
    return false;
}

void OhosComponent::addInnerApiVisibility(const std::string &name, const std::vector<base::Value> &list)
{
    for (const base::Value &visibility : list) {
        innerapi_visibility_[innerapi_names_[name]].push_back(visibility.GetString());
    }
}

const std::vector<std::string> OhosComponent::getInnerApiVisibility(const std::string &label) const
{
    if (auto res = innerapi_visibility_.find(label); res != innerapi_visibility_.end()) {
        return res->second;
    }
    return {};
}

/**
 * Ohos Component Implimentation API
 */
OhosComponentsImpl::OhosComponentsImpl() = default;

bool OhosComponentsImpl::ReadBuildConfigFile(const std::string &build_dir, const char *subfile, std::string &content)
{
    std::string path = build_dir;
    path += "/build_configs/";
    path += subfile;
    if (!base::ReadFileToString(base::FilePath(path), &content)) {
        return false;
    }
    return true;
}

bool OhosComponentsImpl::LoadComponentSubsystemAndPaths(const std::string &paths, const std::string &override_map,
    const std::string &subsystems, std::string &err_msg_out)
{
    const base::DictionaryValue *paths_dict;

    std::unique_ptr<base::Value> paths_value = base::JSONReader::ReadAndReturnError(paths,
        base::JSONParserOptions::JSON_PARSE_RFC, nullptr, &err_msg_out, nullptr, nullptr);
    if (!paths_value) {
        return false;
    }
    if (!paths_value->GetAsDictionary(&paths_dict)) {
        return false;
    }

    const base::DictionaryValue *subsystems_dict;
    std::unique_ptr<base::Value> subsystems_value = base::JSONReader::ReadAndReturnError(subsystems,
        base::JSONParserOptions::JSON_PARSE_RFC, nullptr, &err_msg_out, nullptr, nullptr);
    if (!subsystems_value) {
        return false;
    }
    if (!subsystems_value->GetAsDictionary(&subsystems_dict)) {
        return false;
    }

    const base::Value *subsystem;
    for (const auto com : paths_dict->DictItems()) {
        subsystem = subsystems_dict->FindKey(com.first);
        if (!subsystem) {
            continue;
        }
        components_[com.first] =
            new OhosComponent(com.first.c_str(), subsystem->GetString().c_str(), com.second.GetString().c_str());
    }
    setupComponentsTree();
    return true;
}

const struct OhosComponentTree *OhosComponentsImpl::findChildByPath(const struct OhosComponentTree *current,
    const char *path, size_t len)
{
    if (current->child == nullptr) {
        return nullptr;
    }
    const struct OhosComponentTree *item = current->child;
    while (item != nullptr) {
        if (strncmp(item->dirName, path, len) == 0) {
            // Exactly matching
            if (item->dirName[len] == '\0') {
                return item;
            }
        }
        item = item->next;
    }

    return nullptr;
}

const OhosComponent *OhosComponentsImpl::matchComponentByLabel(const char *label)
{
    const struct OhosComponentTree *child;
    const struct OhosComponentTree *current = pathTree;

    if (!label) {
        return nullptr;
    }
    // Skip leading //
    if (strncmp(label, "//", PATH_PREFIX_LEN) == 0) {
        label += PATH_PREFIX_LEN;
    }

    size_t len;
    const char *sep;
    while (label[0] != '\0') {
        // Get next path seperator
        sep = strchr(label, '/');
        if (sep) {
            len = sep - label;
        } else {
            // Check if it is a label with target name
            sep = strchr(label, ':');
            if (sep) {
                len = sep - label;
            } else {
                len = strlen(label);
            }
        }

        // Match with children
        child = findChildByPath(current, label, len);
        if (child == nullptr) {
            break;
        }

        // No children, return current matched item
        if (child->child == nullptr) {
            return child->component;
        }

        label += len;
        // Finish matching if target name started
        if (label[0] == ':') {
            return child->component;
        }

        // Match with child again
        current = child;

        // Skip leading seperator
        if (label[0] == '/') {
            label += 1;
        }
    }

    return nullptr;
}

void OhosComponentsImpl::addComponentToTree(struct OhosComponentTree *current, OhosComponent *component)
{
    size_t len;
    const char *path = component->path().c_str() + PATH_PREFIX_LEN;
    const char *sep;

    while (path[0] != '\0') {
        sep = strchr(path, '/');
        if (sep) {
            len = sep - path;
        } else {
            len = strlen(path);
        }

        // Check if node already exists
        struct OhosComponentTree *child = (struct OhosComponentTree *)findChildByPath(current, path, len);
        if (!child) {
            // Add intermediate node
            child = new struct OhosComponentTree(path, len, nullptr);
            child->next = current->child;
            current->child = child;
        }

        // End of path detected, setup component pointer
        path = path + len;
        if (path[0] == '\0') {
            child->component = component;
            break;
        }

        // Continue to add next part
        path += 1;
        current = child;
    }
}

void OhosComponentsImpl::setupComponentsTree()
{
    pathTree = new struct OhosComponentTree("//", nullptr);

    std::map<std::string, OhosComponent *>::iterator it;
    for (it = components_.begin(); it != components_.end(); it++) {
        addComponentToTree(pathTree, it->second);
    }
}

void OhosComponentsImpl::LoadInnerApi(const base::DictionaryValue *innerapis)
{
    OhosComponent *component;
    for (const auto kv : innerapis->DictItems()) {
        for (const auto inner : kv.second.DictItems()) {
            component = (OhosComponent *)GetComponentByName(kv.first);
            if (!component) {
                break;
            }
            for (const auto info : inner.second.DictItems()) {
                if (info.first == "label") {
                    component->addInnerApi(inner.first, info.second.GetString());
                    break;
                }
            }
            for (const auto info : inner.second.DictItems()) {
                if (info.first == "visibility") {
                    component->addInnerApiVisibility(inner.first, info.second.GetList());
                    break;
                }
            }
        }
    }
}

bool OhosComponentsImpl::LoadOhosInnerApis_(const std::string innerapi_content, std::string &err_msg_out)
{
    const base::DictionaryValue *innerapis_dict;

    std::unique_ptr<base::Value> innerapis = base::JSONReader::ReadAndReturnError(innerapi_content,
        base::JSONParserOptions::JSON_PARSE_RFC, nullptr, &err_msg_out, nullptr, nullptr);
    if (!innerapis) {
        return false;
    }
    if (!innerapis->GetAsDictionary(&innerapis_dict)) {
        return false;
    }
    LoadInnerApi(innerapis_dict);
    return true;
}

bool OhosComponentsImpl::LoadOhosComponents(const std::string &build_dir, const Value *enable, Err *err)
{
    const char *paths_file = "parts_info/parts_path_info.json";
    std::string paths_content;
    if (!ReadBuildConfigFile(build_dir, paths_file, paths_content)) {
        *err = Err(*enable, "Your .gn file has enabled \"ohos_components_support\", but "
            "OpenHarmony build config file (" +
            std::string(paths_file) + ") does not exists.\n");
        return false;
    }
    const char *subsystems_file = "parts_info/part_subsystem.json";
    std::string subsystems_content;
    if (!ReadBuildConfigFile(build_dir, subsystems_file, subsystems_content)) {
        *err = Err(*enable, "Your .gn file has enabled \"ohos_components_support\", but "
            "OpenHarmony build config file (" +
            std::string(subsystems_file) + ") does not exists.\n");
        return false;
    }
    const char *innerapis_file = "parts_info/inner_kits_info.json";
    std::string innerapis_content;
    if (!ReadBuildConfigFile(build_dir, innerapis_file, innerapis_content)) {
        *err = Err(*enable, "Your .gn file has enabled \"ohos_components_support\", but "
            "OpenHarmony build config file (" +
            std::string(innerapis_file) + ") does not exists.\n");
        return false;
    }
    const char *override_file = "component_override_map.json";
    std::string override_map;
    if (!ReadBuildConfigFile(build_dir, override_file, override_map)) {
        override_map = EMPTY_INNERAPI;
    }
    std::string err_msg_out;
    if (!LoadComponentSubsystemAndPaths(paths_content, override_map, subsystems_content, err_msg_out)) {
        *err = Err(*enable, "Your .gn file has enabled \"ohos_components_support\", but "
            "OpenHarmony build config file parsing failed:\n" +
            err_msg_out + "\n");
        return false;
    }
    if (!LoadOhosInnerApis_(innerapis_content, err_msg_out)) {
        *err = Err(*enable, "Your .gn file has enabled \"ohos_components_support\", but "
            "OpenHarmony build config file " +
            std::string(innerapis_file) + " parsing failed:\n" + err_msg_out + "\n");
        return false;
    }
    return true;
}

const OhosComponent *OhosComponentsImpl::GetComponentByName(const std::string &component_name) const
{
    if (auto res = components_.find(component_name); res != components_.end()) {
        return res->second;
    }
    return nullptr;
}

bool OhosComponentsImpl::GetExternalDepsLabel(const Value &external_dep, std::string &label, Err *err) const
{
    std::string str_val = external_dep.string_value();
    size_t sep = str_val.find(":");
    if (sep <= 0) {
        *err = Err(external_dep, "OHOS component external_deps format error: (" + external_dep.string_value() +
            "),"
            "it should be a string like \"component_name:innerapi_name\".");
        return false;
    }
    std::string component_name = str_val.substr(0, sep);
    const OhosComponent *component = GetComponentByName(component_name);
    if (component == nullptr) {
        *err = Err(external_dep, "OHOS component : (" + component_name + ") not found.");
        return false;
    }
    std::string innerapi_name = str_val.substr(sep + 1);
    label = component->getInnerApi(innerapi_name);
    if (label == EMPTY_INNERAPI) {
        *err = Err(external_dep,
            "OHOS innerapi: (" + innerapi_name + ") not found for component (" + component_name + ").");
        return false;
    }
    return true;
}

bool OhosComponentsImpl::GetSubsystemName(const Value &component_name, std::string &subsystem_name, Err *err) const
{
    const OhosComponent *component = GetComponentByName(component_name.string_value());
    if (component == nullptr) {
        *err = Err(component_name, "OHOS component : (" + component_name.string_value() + ") not found.");
        return false;
    }

    subsystem_name = component->subsystem();
    return true;
}

/**
 * Ohos Components Public API
 */

OhosComponents::OhosComponents() = default;

bool OhosComponents::LoadOhosComponents(const std::string &build_dir, const Value *enable, Err *err)
{
    if (!enable) {
        // Not enabled
        return true;
    }
    if (!enable->VerifyTypeIs(Value::BOOLEAN, err)) {
        return false;
    }

    // Disabled
    if (!enable->boolean_value()) {
        return true;
    }

    mgr = new OhosComponentsImpl();

    if (!mgr->LoadOhosComponents(build_dir, enable, err)) {
        delete mgr;
        mgr = nullptr;
        return false;
    }

    return true;
}

bool OhosComponents::isOhosComponentsLoaded() const
{
    if (mgr == nullptr) {
        return false;
    } else {
        return true;
    }
}

bool OhosComponents::GetExternalDepsLabel(const Value &external_dep, std::string &label, Err *err) const
{
    if (!mgr) {
        if (err) {
            *err = Err(external_dep, "You are compiling OpenHarmony components, but \n"
                "\"ohos_components_support\" is not enabled or build_configs files are invalid.");
        }
        return false;
    }
    return mgr->GetExternalDepsLabel(external_dep, label, err);
}

bool OhosComponents::GetSubsystemName(const Value &part_name, std::string &label, Err *err) const
{
    if (!mgr) {
        if (err) {
            *err = Err(part_name, "You are compiling OpenHarmony components, but \n"
                "\"ohos_components_support\" is not enabled or build_configs files are invalid.");
        }
        return false;
    }
    return mgr->GetSubsystemName(part_name, label, err);
}

const OhosComponent *OhosComponents::GetComponentByLabel(const std::string &label) const
{
    if (!mgr) {
        return nullptr;
    }
    return mgr->matchComponentByLabel(label.c_str());
}

void OhosComponents::LoadOhosComponentsChecker(const std::string &build_dir, const Value *support, int checkType)
{
    if (!support) {
        return;
    }
    if (!support->boolean_value()) {
        return;
    }
    if (checkType > OhosComponentChecker::CheckType::INTERCEPT_ALL ||
        checkType <= OhosComponentChecker::CheckType::NONE) {
        InnerApiPublicInfoGenerator::Init(build_dir, 0);
        return;
    }
    OhosComponentChecker::Init(build_dir, checkType);
    InnerApiPublicInfoGenerator::Init(build_dir, checkType);
    return;
}
