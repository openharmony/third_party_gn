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
#include "gn/ohos_components_mapping.h"

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

static std::string GetPath(const char *path)
{
    std::string process_path;
    if (strncmp(path, "//", PATH_PREFIX_LEN) == 0) {
        process_path = std::string(path);
    } else {
        process_path = "//" + std::string(path);
    }
    return process_path;
}

OhosComponent::OhosComponent(const char *name, const char *subsystem, const char *path,
    const std::vector<std::string> &modulePath, bool special_parts_switch)
{
    name_ = std::string(name);
    subsystem_ = std::string(subsystem);
    path_ = GetPath(path);
    special_parts_switch_ = special_parts_switch;

    auto result = std::find(modulePath.begin(), modulePath.end(), std::string(path));
    if (result == modulePath.end()) {
        module_path_.push_back(path_);
    } else {
        for (const auto &module_path : modulePath) {
            module_path_.push_back(GetPath(module_path.c_str()));
        }
    }
}

void OhosComponent::addInnerApi(const std::string &name, const std::string &label)
{
    std::string la = label;
    size_t pos = label.find(":");
    if (pos != std::string::npos) {
        if ((label[pos - 1]) == '/') { // some are like this : "//components/foo/libfoo/:libfoo"
            unsigned long indexToRemove = pos - 1;
            if (indexToRemove >= 0 && indexToRemove <= la.length()) {
                la.erase(la.begin() + indexToRemove);
            }
        }
    }
    innerapi_names_[name] = la;
    innerapi_labels_[la] = name;
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
    return base::ReadFileToString(base::FilePath(path), &content);
}

static bool GetComponentPath(std::string &content)
{
    std::string whiteListPath = "out/products_ext/component_path_whitelist.json";
    return base::ReadFileToString(base::FilePath(whiteListPath), &content);
}

static std::vector<std::string> GetModulePath(const std::string &path, base::ListValue *list)
{
    std::vector<std::string> module_path;
    for (const base::Value &value : list->GetList()) {
        const base::Value *component_path = value.FindKey("component_path");
        if (!component_path) {
            continue;
        }
        if (component_path->GetString() == path) {
            for (const base::Value &item : value.FindKey("module_path")->GetList()) {
                module_path.push_back(item.GetString());
            }
            return module_path;
        }
    }
    return module_path;
}

bool OhosComponentsImpl::LoadComponentInfo(const std::string &components_content, bool special_parts_switch,
    std::string &err_msg_out)
{
    const base::DictionaryValue *components_dict;
    std::unique_ptr<base::Value> components_value = base::JSONReader::ReadAndReturnError(components_content,
        base::JSONParserOptions::JSON_PARSE_RFC, nullptr, &err_msg_out, nullptr, nullptr);
    if (!components_value) {
        return false;
    }
    if (!components_value->GetAsDictionary(&components_dict)) {
        return false;
    }

    std::string components_path_content;
    base::ListValue *components_path_list;
    bool is_read_path = false;
    std::unique_ptr<base::Value> components_path_value;
    if (special_parts_switch && GetComponentPath(components_path_content)) {
        components_path_value = base::JSONReader::ReadAndReturnError(components_path_content,
            base::JSONParserOptions::JSON_PARSE_RFC, nullptr, &err_msg_out, nullptr, nullptr);
        if (components_path_value) {
            is_read_path = components_path_value->GetAsList(&components_path_list);
        }
    }

    for (const auto com : components_dict->DictItems()) {
        const base::Value *subsystem = com.second.FindKey("subsystem");
        const base::Value *path = com.second.FindKey("path");
        if (!subsystem || !path) {
            continue;
        }

        std::vector<std::string> module_path;
        if (special_parts_switch && is_read_path) {
            module_path = GetModulePath(path-> GetString(), components_path_list);
        }

        components_[com.first] =
            new OhosComponent(com.first.c_str(), subsystem->GetString().c_str(), path->GetString().c_str(),
            module_path, special_parts_switch);
        const base::Value *innerapis = com.second.FindKey("innerapis");
        if (!innerapis) {
            continue;
        }
        LoadInnerApi(com.first, innerapis->GetList());
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
    const struct OhosComponentTree *previous = pathTree;
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
            if (current->child != nullptr && previous->component != nullptr &&
                previous->component->specialPartsSwitch()) {
                    return previous->component;
                } else {
                    break;
                }
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

        // Save previous part target
        if (child->component != nullptr) {
            previous = child;
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
    std::vector<std::string> module_path = component->modulePath();
    struct OhosComponentTree *origin = current;
    for (const auto &part_path : module_path) {
        size_t len;
        const char *path = part_path.c_str() + PATH_PREFIX_LEN;
        const char *sep;
        current = origin;
    
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
}

void OhosComponentsImpl::setupComponentsTree()
{
    pathTree = new struct OhosComponentTree("//", nullptr);

    std::map<std::string, OhosComponent *>::iterator it;
    for (it = components_.begin(); it != components_.end(); it++) {
        addComponentToTree(pathTree, it->second);
    }
}

void OhosComponentsImpl::LoadInnerApi(const std::string &component_name, const std::vector<base::Value> &innerapis)
{
    OhosComponent *component = (OhosComponent *)GetComponentByName(component_name);
    if (!component) {
        return;
    }
    for (const base::Value &kv : innerapis) {
        const base::Value *label = kv.FindKey("label");
        const base::Value *name = kv.FindKey("name");

        if (!label || !name) {
            continue;
        }
        component->addInnerApi(name->GetString(), label->GetString());
        const base::Value *visibility = kv.FindKey("visibility");
        if (!visibility) {
            continue;
        }
        component->addInnerApiVisibility(name->GetString(), visibility->GetList());
    }
}

void OhosComponentsImpl::LoadOverrideMap(const std::string &override_map)
{
    const base::DictionaryValue *override_dict;
    std::unique_ptr<base::Value> override_value = base::JSONReader::ReadAndReturnError(override_map,
        base::JSONParserOptions::JSON_PARSE_RFC, nullptr, nullptr, nullptr, nullptr);
    if (!override_value) {
        return;
    }
    if (!override_value->GetAsDictionary(&override_dict)) {
        return;
    }

    for (const auto com : override_dict->DictItems()) {
        override_map_[com.first] = com.second.GetString();
    }
    return;
}

void OhosComponentsImpl::LoadToolchain(const Value *product)
{
    if (!product) {
        return;
    }
    std::string path = "out/preloader/" + product->string_value() + "/build_config.json";
    std::string content;
    if (!base::ReadFileToString(base::FilePath(path), &content)) {
        return;
    }

    const base::DictionaryValue *content_dict;
    std::unique_ptr<base::Value> content_value = base::JSONReader::ReadAndReturnError(content,
        base::JSONParserOptions::JSON_PARSE_RFC, nullptr, nullptr, nullptr, nullptr);
    if (!content_value) {
        return;
    }
    if (!content_value->GetAsDictionary(&content_dict)) {
        return;
    }

    for (const auto com : content_dict->DictItems()) {
        if (com.first == "product_toolchain_label") {
            toolchain_ = com.second.GetString();
            break;
        }
    }
    return;
}

bool OhosComponentsImpl::LoadOhosComponents(const std::string &build_dir, const Value *enable,
    const Value *indep, const Value *product, bool special_parts_switch, Err *err)
{
    const char *components_file = "parts_info/components.json";
    std::string components_content;
    if (!ReadBuildConfigFile(build_dir, components_file, components_content)) {
        *err = Err(*enable, "Your .gn file has enabled \"ohos_components_support\", but "
            "OpenHarmony build config file (" +
            std::string(components_file) + ") does not exists.\n");
        return false;
    }

    std::string override_map;
    if (ReadBuildConfigFile(build_dir, "component_override_map.json", override_map)) {
        LoadOverrideMap(override_map);
    }

    std::string err_msg_out;
    if (!LoadComponentInfo(components_content, special_parts_switch, err_msg_out)) {
        *err = Err(*enable, "Your .gn file has enabled \"ohos_components_support\", but "
            "OpenHarmony build config file parsing failed:\n" +
            err_msg_out + "\n");
        return false;
    }
    if (indep && indep->boolean_value()) {
        is_indep_compiler_enable_ = true;
    }
    LoadToolchain(product);
    return true;
}

const OhosComponent *OhosComponentsImpl::GetComponentByName(const std::string &component_name) const
{
    if (auto res = components_.find(component_name); res != components_.end()) {
        return res->second;
    }
    return nullptr;
}

static size_t GetWholeArchiveFlag(std::string str_val, int &whole_status)
{
    size_t sep_whole = str_val.find("(--whole-archive)");
    if (sep_whole != std::string::npos) {
        whole_status = 1;
    } else {
        sep_whole = str_val.find("(--no-whole-archive)");
        if (sep_whole != std::string::npos) {
            whole_status = 0;
        } else {
            whole_status = -1;
        }
    }
    return sep_whole;
}

bool OhosComponentsImpl::GetPrivateDepsLabel(const Value &dep, std::string &label,
    const Label& current_toolchain, int &whole_status, Err *err) const
{
    std::string str_val = dep.string_value();
    size_t sep_whole = GetWholeArchiveFlag(str_val, whole_status);

    if (sep_whole != std::string::npos) {
        label = str_val.substr(0, sep_whole);
    } else {
        label = str_val;
    }
    std::string current_toolchain_str = current_toolchain.GetUserVisibleName(false);
    size_t tool_sep = label.find("(");
    if (tool_sep == std::string::npos && GetTargetToolchain() != current_toolchain_str) {
        label += "(" + current_toolchain_str + ")";
    }
    if (label == EMPTY_INNERAPI) {
        *err = Err(dep,
            "Deps label: (" + dep.string_value() + ") format error.");
        return false;
    }
    return true;
}

bool OhosComponentsImpl::GetExternalDepsLabel(const Value &external_dep, std::string &label,
    const Label& current_toolchain, int &whole_status, Err *err) const
{
    std::string str_val = external_dep.string_value();
    size_t sep = str_val.find(":");
    if (sep == std::string::npos) {
        *err = Err(external_dep, "OHOS component external_deps format error: (" + external_dep.string_value() +
            "),"
            "it should be a string like \"component_name:innerapi_name\".");
        return false;
    }
    std::string component_name = str_val.substr(0, sep);
    for (const auto& pair : override_map_) {
        if (pair.first == component_name) {
            component_name = pair.second;
            break;
        }
    }
    const OhosComponent *component = GetComponentByName(component_name);
    if (component == nullptr) {
        *err = Err(external_dep, "OHOS component : (" + component_name + ") not found.");
        return false;
    }

    std::string innerapi_name;
    std::string tool_chain = "";
    size_t sep_whole = GetWholeArchiveFlag(str_val, whole_status);
    if (sep_whole != std::string::npos) {
        innerapi_name = str_val.substr(sep + 1, sep_whole - sep - 1);
    } else {
        innerapi_name = str_val.substr(sep + 1);
        size_t tool_sep = innerapi_name.find("(");
        if (tool_sep != std::string::npos) {
            tool_chain = innerapi_name.substr(tool_sep);
            innerapi_name = innerapi_name.substr(0, tool_sep);
        }
    }

    std::string current_toolchain_str = current_toolchain.GetUserVisibleName(false);
    if (tool_chain == "" && GetTargetToolchain() != current_toolchain_str) {
        tool_chain = "(" + current_toolchain_str + ")";
    }
    if (isOhosIndepCompilerEnable()) {
        label = component->getInnerApi(innerapi_name + tool_chain);
        if (label == EMPTY_INNERAPI) {
            label = component->getInnerApi(innerapi_name) + tool_chain;
        }
    } else {
        label = component->getInnerApi(innerapi_name) + tool_chain;
    }

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

bool OhosComponents::LoadOhosComponents(const std::string &build_dir,
    const Value *enable, const Value *indep, const Value *product, bool special_parts_switch, Err *err)
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

    if (!mgr->LoadOhosComponents(build_dir, enable, indep, product, special_parts_switch, err)) {
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

bool OhosComponents::GetExternalDepsLabel(const Value &external_dep, std::string &label,
    const Label& current_toolchain, int &whole_status, Err *err) const
{
    if (!mgr) {
        if (err) {
            *err = Err(external_dep, "You are compiling OpenHarmony components, but \n"
                "\"ohos_components_support\" is not enabled or build_configs files are invalid.");
        }
        return false;
    }
    return mgr->GetExternalDepsLabel(external_dep, label, current_toolchain, whole_status, err);
}

bool OhosComponents::GetPrivateDepsLabel(const Value &dep, std::string &label,
    const Label& current_toolchain, int &whole_status, Err *err) const
{
    if (!mgr) {
        if (err) {
            *err = Err(dep, "You are compiling OpenHarmony components, but \n"
                "\"ohos_components_support\" is not enabled or build_configs files are invalid.");
        }
        return false;
    }
    return mgr->GetPrivateDepsLabel(dep, label, current_toolchain, whole_status, err);
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

void OhosComponents::LoadOhosComponentsChecker(const std::string &build_dir, const Value *support, int checkType,
    unsigned int ruleSwitch)
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
    OhosComponentChecker::Init(build_dir, checkType, ruleSwitch);
    InnerApiPublicInfoGenerator::Init(build_dir, checkType);
    return;
}

void OhosComponents::LoadOhosComponentsMapping(const std::string& build_dir,
    const Value *support, const Value *independent)
{
    if (!support) {
        return;
    }
    if (!support->boolean_value()) {
        return;
    }

    if (!independent || !independent->boolean_value()) {
        return;
    }

    OhosComponentMapping::Init(build_dir);
    return;
}

const OhosComponent *OhosComponents::GetComponentByName(const std::string &component_name) {
    if (!mgr) {
        return nullptr;
    }
    return mgr->GetComponentByName(component_name);
}

bool OhosComponents::isOhosIndepCompilerEnable() {
    return mgr && mgr->isOhosIndepCompilerEnable();
}
