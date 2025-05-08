// Copyright (c) 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/ohos_components_checker.h"

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

static const std::string SCAN_RESULT_PATH = "scan_out";
static const std::string WHITELIST_PATH = "component_compilation_whitelist.json";
static const int BASE_BINARY = 1;
static std::vector<std::string> all_deps_config_;
static std::vector<std::string> includes_over_range_;
static std::map<std::string, std::vector<std::string>> innerapi_public_deps_inner_;
static std::vector<std::string> innerapi_not_lib_;
static std::vector<std::string> innerapi_not_declare_;
static std::map<std::string, std::vector<std::string>> includes_absolute_deps_other_;
static std::map<std::string, std::vector<std::string>> target_absolute_deps_other_;
static std::map<std::string, std::vector<std::string>> import_other_;
static std::map<std::string, std::vector<std::string>> deps_not_lib_;
static std::map<std::string, std::vector<std::string>> fuzzy_match_;

OhosComponentChecker *OhosComponentChecker::instance_ = nullptr;

static std::string& Trim(std::string &str)
{
    if (str.empty()) {
        return str;
    }
    str.erase(0, str.find_first_not_of(" \t\r\n"));
    str.erase(str.find_last_not_of(" \t\r\n") + 1);
    return str;
}

static bool StartWith(const std::string &str, const std::string &prefix)
{
    return (str.rfind(prefix, 0) == 0);
}

static void CreateScanOutDir(const std::string &dir)
{
    base::FilePath path(dir);
    base::CreateDirectory(path);
    return;
}

static void RemoveScanOutDir(const std::string& dir)
{
    if (access(dir.c_str(), F_OK) == -1) {
        return;
    }
    base::FilePath path(dir);
    base::DeleteFile(path, true);
    return;
}

static bool ReadBuildConfigFile(base::FilePath path, std::string &content)
{
    if (!base::ReadFileToString(path, &content)) {
        return false;
    }
    return true;
}

static void LoadAllDepsConfigWhitelist(const base::Value &list)
{
    for (const base::Value &value : list.GetList()) {
        all_deps_config_.push_back(value.GetString());
    }
}

static void LoadIncludesOverRangeWhitelist(const base::Value &list)
{
    for (const base::Value &value : list.GetList()) {
        includes_over_range_.push_back(value.GetString());
    }
}

static void LoadInnerApiPublicDepsInnerWhitelist(const base::Value &value)
{
    for (auto info : value.DictItems()) {
        for (const base::Value &value_tmp : info.second.GetList()) {
            innerapi_public_deps_inner_[info.first].push_back(value_tmp.GetString());
        }
    }
}

static void LoadInnerApiNotLibWhitelist(const base::Value &list)
{
    for (const base::Value &value : list.GetList()) {
        innerapi_not_lib_.push_back(value.GetString());
    }
}

static void LoadInnerApiNotDeclareWhitelist(const base::Value &list)
{
    for (const base::Value &value : list.GetList()) {
        innerapi_not_declare_.push_back(value.GetString());
    }
}

static void LoadIncludesAbsoluteDepsOtherWhitelist(const base::Value &value)
{
    for (auto info : value.DictItems()) {
        for (const base::Value &value_tmp : info.second.GetList()) {
            includes_absolute_deps_other_[info.first].push_back(value_tmp.GetString());
        }
    }
}

static void LoadAbsoluteDepsOtherWhitelist(const base::Value &value)
{
    for (auto info : value.DictItems()) {
        for (const base::Value &value_tmp : info.second.GetList()) {
            target_absolute_deps_other_[info.first].push_back(value_tmp.GetString());
        }
    }
}

static void LoadImportOtherWhitelist(const base::Value &value)
{
    for (auto info : value.DictItems()) {
        for (const base::Value &value_tmp : info.second.GetList()) {
            import_other_[info.first].push_back(value_tmp.GetString());
        }
    }
}

static void LoadDepsNotLibWhitelist(const base::Value &value)
{
    for (auto info : value.DictItems()) {
        for (const base::Value &value_tmp : info.second.GetList()) {
            deps_not_lib_[info.first].push_back(value_tmp.GetString());
        }
    }
}

static void LoadFuzzyMatchWhitelist(const base::Value &value)
{
    for (auto info : value.DictItems()) {
        for (const base::Value &value_tmp : info.second.GetList()) {
            fuzzy_match_[info.first].push_back(value_tmp.GetString());
        }
    }
}

static std::map<std::string, std::function<void(const base::Value &value)>> whitelist_map_ = {
    { "all_dependent_configs", LoadAllDepsConfigWhitelist },
    { "includes_over_range", LoadIncludesOverRangeWhitelist },
    { "innerapi_not_lib", LoadInnerApiNotLibWhitelist },
    { "innerapi_not_declare", LoadInnerApiNotDeclareWhitelist },
    { "innerapi_public_deps_inner", LoadInnerApiPublicDepsInnerWhitelist },
    { "includes_absolute_deps_other", LoadIncludesAbsoluteDepsOtherWhitelist },
    { "target_absolute_deps_other", LoadAbsoluteDepsOtherWhitelist },
    { "import_other", LoadImportOtherWhitelist },
    { "deps_not_lib", LoadDepsNotLibWhitelist },
    { "fuzzy_match", LoadFuzzyMatchWhitelist }
};

static void LoadWhitelist(const std::string &build_dir)
{
    std::string whitelistContent;
    if (!ReadBuildConfigFile(base::FilePath(build_dir + "/" + WHITELIST_PATH), whitelistContent)) {
        if (!ReadBuildConfigFile(base::FilePath("out/products_ext/" + WHITELIST_PATH), whitelistContent)) {
            if (!ReadBuildConfigFile(base::FilePath("build/" + WHITELIST_PATH), whitelistContent)) {
                return;
            }
        }
    }
    const base::DictionaryValue *whitelist_dict;
    std::unique_ptr<base::Value> whitelist = base::JSONReader::ReadAndReturnError(whitelistContent,
        base::JSONParserOptions::JSON_PARSE_RFC, nullptr, nullptr, nullptr, nullptr);
    if (!whitelist) {
        return;
    }
    if (!whitelist->GetAsDictionary(&whitelist_dict)) {
        return;
    }

    for (const auto kv : whitelist_dict->DictItems()) {
        auto iter = whitelist_map_.find(kv.first);
        if (iter != whitelist_map_.end()) {
            iter->second(kv.second);
        }
    }
    return;
}

static bool IsIntercept(unsigned int switchValue, int shiftLeftNum)
{
    if ((switchValue & (BASE_BINARY << (shiftLeftNum - 1))) != 0) {
        return true;
    }
    return false;
}

bool OhosComponentChecker::InterceptAllDepsConfig(const Target *target, const std::string &label, Err *err) const
{
    if (!IsIntercept(ruleSwitch_, ALL_DEPS_CONFIG_BINARY)) {
        return true;
    }

    auto result = std::find(all_deps_config_.begin(), all_deps_config_.end(), label);
    if (result != all_deps_config_.end()) {
        return true;
    }

    *err = Err(target->defined_from(), "all_dependent_configs not allowed.",
        "The item " + label + " does not allow all_dependent_configs.");
    return false;
}

bool OhosComponentChecker::InterceptIncludesOverRange(const Target *target, const std::string &label,
    const std::string &dir, Err *err) const
{
    if (!IsIntercept(ruleSwitch_, INCLUDE_OVER_RANGE_BINARY)) {
        return true;
    }

    auto result = std::find(includes_over_range_.begin(), includes_over_range_.end(), label);
    if (result != includes_over_range_.end()) {
        return true;
    }
    *err = Err(target->defined_from(), "Header file range is too large.",
        "The item " + label + " header : " + dir + " range is too large.");
    return false;
}

bool OhosComponentChecker::InterceptInnerApiPublicDepsInner(const Target *target, const std::string &label,
    const std::string &deps, Err *err) const
{
    if (!IsIntercept(ruleSwitch_, INNERAPI_PUBLIC_DEPS_INNER_BINARY)) {
        return true;
    }

    if (auto res = innerapi_public_deps_inner_.find(label); res != innerapi_public_deps_inner_.end()) {
        std::string deps_str(deps);
        auto res_second = std::find(res->second.begin(), res->second.end(), Trim(deps_str));
        if (res_second != res->second.end()) {
            return true;
        }
    }
    *err = Err(target->defined_from(), "InnerApi not allow the use of public_deps dependent internal modules.",
        "The item " + label + " not allow the use of public_deps dependent internal modules : " + deps);
    return false;
}

bool OhosComponentChecker::InterceptInnerApiNotLib(const Item *item, const std::string &label, Err *err) const
{
    if (!IsIntercept(ruleSwitch_, INNERAPI_NOT_LIB_BINARY)) {
        return true;
    }

    auto result = std::find(innerapi_not_lib_.begin(), innerapi_not_lib_.end(), label);
    if (result != innerapi_not_lib_.end()) {
        return true;
    }
    *err =
        Err(item->defined_from(), "InnerApi is not a library type.", "The item " + label + " is not a library type.");
    return false;
}

bool OhosComponentChecker::InterceptDepsNotLib(const Item *item, const std::string &label,
    const std::string &deps, Err *err) const
{
    if (!IsIntercept(ruleSwitch_, DEPS_NOT_LIB_BINARY)) {
        return true;
    }

    if (auto res = fuzzy_match_.find("deps_not_lib"); res != fuzzy_match_.end()) {
        std::string deps_str(deps);
        for (auto res_second : res->second) {
            if (StartWith(Trim(deps_str), res_second)) {
                return true;
            }
        }
    }

    if (auto res = deps_not_lib_.find(label); res != deps_not_lib_.end()) {
        std::string deps_str(deps);
        auto res_second = std::find(res->second.begin(), res->second.end(), Trim(deps_str));
        if (res_second != res->second.end()) {
            return true;
        }
    }
    *err = Err(item->defined_from(), "Depend a non-lib target.",
        "The item " + label + " cannot depend on a non-lib target " + deps);
    return false;
}

bool OhosComponentChecker::InterceptInnerApiNotDeclare(const Item *item, const std::string &label, Err *err) const
{
    if (!IsIntercept(ruleSwitch_, INNERAPI_NOT_DECLARE_BINARY)) {
        return true;
    }

    auto result = std::find(innerapi_not_declare_.begin(), innerapi_not_declare_.end(), label);
    if (result != innerapi_not_declare_.end()) {
        return true;
    }
    *err = Err(item->defined_from(), "InnerApi is not defined in bundle.json.",
        "The item " + label + " is not defined in bundle.json.");
    return false;
}

bool OhosComponentChecker::InterceptIncludesAbsoluteDepsOther(const Target *target, const std::string &label,
    const std::string &includes, Err *err) const
{
    if (!IsIntercept(ruleSwitch_, INCLUDES_ABSOLUTE_DEPS_OTHER_BINARY)) {
        return true;
    }

    if (auto res = fuzzy_match_.find("deps_includes_absolute"); res != fuzzy_match_.end()) {
        std::string includes_str(includes);
        for (auto res_second : res->second) {
            if (StartWith(Trim(includes_str), res_second)) {
                return true;
            }
        }
    }

    if (auto res = includes_absolute_deps_other_.find(label); res != includes_absolute_deps_other_.end()) {
        std::string includes_str(includes);
        auto res_second = std::find(res->second.begin(), res->second.end(), Trim(includes_str));
        if (res_second != res->second.end()) {
            return true;
        }
    }
    *err = Err(target->defined_from(), "Do not directly use header files of other components.",
        "The item " + label + " do not directly use header files : " + includes + " of other components." +
        "\n"
        "Please use 'external_deps/public_external_deps' dependent module.");
    return false;
}


bool OhosComponentChecker::InterceptTargetAbsoluteDepsOther(const Item *item, const std::string &label,
    const std::string &deps, Err *err) const
{
    if (!IsIntercept(ruleSwitch_, TARGET_ABSOLUTE_DEPS_OTHER_BINARY)) {
        return true;
    }

    if (auto res = fuzzy_match_.find("deps_component_absolute"); res != fuzzy_match_.end()) {
        std::string deps_str(deps);
        for (auto res_second : res->second) {
            if (StartWith(Trim(deps_str), res_second)) {
                return true;
            }
        }
    }

    if (auto res = target_absolute_deps_other_.find(label); res != target_absolute_deps_other_.end()) {
        std::string deps_str(deps);
        auto res_second = std::find(res->second.begin(), res->second.end(), Trim(deps_str));
        if (res_second != res->second.end()) {
            return true;
        }
    }
    *err = Err(item->defined_from(), "Not allow use absolute dependent other component.",
        "The item " + label + " not allow use absolute dependent other component : " + deps +
        "\n"
        "Please use 'external_deps/public_external_deps'.");
    return false;
}

bool OhosComponentChecker::InterceptInnerApiVisibilityDenied(const Item *item, const std::string &from_label,
    const std::string &to_label, Err *err) const
{
    if (!IsIntercept(ruleSwitch_, INNERAPI_VISIBILITY_DENIED)) {
        return true;
    }

    *err = Err(item->defined_from(), "InnerApi visibility denied.",
        "The item " + from_label + " cannot dependent  " + to_label +
        "\n"
        "Please check 'visibility' field in 'bundle.json' of " +
        to_label);
    return false;
}

bool OhosComponentChecker::InterceptImportOther(const FunctionCallNode *function, const std::string &label,
    const std::string &deps, Err *err) const
{
    if (!IsIntercept(ruleSwitch_, IMPORT_OTHER_BINARY)) {
        return true;
    }

    if (auto res = fuzzy_match_.find("deps_gni"); res != fuzzy_match_.end()) {
        std::string deps_str(deps);
        for (auto res_second : res->second) {
            if (StartWith(Trim(deps_str), res_second)) {
                return true;
            }
        }
    }

    if (auto res = import_other_.find(label); res != import_other_.end()) {
        std::string deps_str(deps);
        auto res_second = std::find(res->second.begin(), res->second.end(), Trim(deps_str));
        if (res_second != res->second.end()) {
            return true;
        }
    }
    *err = Err(function->function(), "Not allow import other gni.",
        label + " not allow import other gni : " + deps);
    return false;
}

OhosComponentChecker::OhosComponentChecker(const std::string &build_dir, int checkType, unsigned int ruleSwitch)
{
    checkType_ = checkType;
    build_dir_ = build_dir;
    ruleSwitch_ = ruleSwitch;
    if (checkType_ == CheckType::INTERCEPT_IGNORE_TEST || checkType_ == CheckType::INTERCEPT_ALL) {
        LoadWhitelist(build_dir_);
    }
    if (checkType_ == CheckType::SCAN_ALL || checkType_ == CheckType::INTERCEPT_ALL) {
        ignoreTest_ = false;
    }
    RemoveScanOutDir(build_dir_ + "/" + SCAN_RESULT_PATH);
}

void OhosComponentChecker::GenerateScanList(const std::string &path, const std::string &subsystem,
    const std::string &component, const std::string &label, const std::string &deps) const
{
    CreateScanOutDir(build_dir_ + "/" + SCAN_RESULT_PATH);
    std::ofstream file;
    file.open(build_dir_ + "/" + SCAN_RESULT_PATH + "/" + path, std::ios::app);
    file << subsystem << " " << component << " " << label << " " << deps << "\n";
    file.close();
    return;
}

bool OhosComponentChecker::CheckAllDepsConfigs(const Target *target, const std::string &label, Err *err) const
{
    if (checkType_ <= CheckType::NONE || target == nullptr || (ignoreTest_ && target->testonly())) {
        return true;
    }

    const OhosComponent *component = target->ohos_component();
    if (component == nullptr) {
        return true;
    }
    if (checkType_ >= CheckType::INTERCEPT_IGNORE_TEST) {
        return InterceptAllDepsConfig(target, label, err);
    }
    GenerateScanList("all_dependent_configs.list", component->subsystem(), component->name(), label, "");
    return true;
}

bool OhosComponentChecker::CheckInnerApiIncludesOverRange(const Target *target, const std::string &label,
    const std::string &dir, Err *err) const
{
    if (checkType_ <= CheckType::NONE || target == nullptr || (ignoreTest_ && target->testonly())) {
        return true;
    }

    const OhosComponent *component = target->ohos_component();
    if (component == nullptr || (!component->isInnerApi(label) && !StartWith(label, "//third_party"))) {
        return true;
    }

    if (dir != "." && dir != "./" && dir != "../" && dir != component->path() && dir != component->path() + "/") {
        return true;
    }

    if (checkType_ >= CheckType::INTERCEPT_IGNORE_TEST) {
        return InterceptIncludesOverRange(target, label, dir, err);
    }
    GenerateScanList("includes_over_range.list", component->subsystem(), component->name(), label, dir);
    return true;
}

bool OhosComponentChecker::CheckInnerApiPublicDepsInner(const Target *target, const std::string &label,
    const std::string &deps, Err *err) const
{
    if (checkType_ <= CheckType::NONE || target == nullptr || (ignoreTest_ && target->testonly())) {
        return true;
    }

    const OhosComponent *component = target->ohos_component();
    if (component == nullptr || !component->isInnerApi(label)) {
        return true;
    }

    if (!StartWith(deps, component->path()) && StartWith(deps, "//")) {
        return true;
    }

    if (checkType_ >= CheckType::INTERCEPT_IGNORE_TEST) {
        return InterceptInnerApiPublicDepsInner(target, label, deps, err);
    }
    GenerateScanList("innerapi_public_deps_inner.list", component->subsystem(), component->name(), label, deps);
    return true;
}

bool OhosComponentChecker::CheckInnerApiNotLib(const Item *item, const OhosComponent *component,
    const std::string &label, const std::string &deps, Err *err) const
{
    if (checkType_ <= CheckType::NONE || item == nullptr || item->AsTarget() == nullptr ||
        (ignoreTest_ && item->testonly()) || component == nullptr) {
        return true;
    }

    Target::OutputType type = item->AsTarget()->output_type();
    if (type == Target::SHARED_LIBRARY || type == Target::STATIC_LIBRARY || type == Target::RUST_LIBRARY ||
        type == Target::EXECUTABLE || type == Target::COPY_FILES) {
        return true;
    }

    if (type == Target::GROUP && item->checkflag() == false) {
        return true;
    }

    if (checkType_ >= CheckType::INTERCEPT_IGNORE_TEST) {
        return InterceptDepsNotLib(item, label, deps, err) && InterceptInnerApiNotLib(item, deps, err);
    }

    std::string type_str(Target::GetStringForOutputType(type));
    GenerateScanList("innerapi_not_lib.list", component->subsystem(), component->name(), deps, type_str);
    GenerateScanList("deps_not_lib.list", component->subsystem(), component->name(), label, deps);
    return true;
}

bool OhosComponentChecker::CheckInnerApiNotDeclare(const Item *item, const OhosComponent *component,
    const std::string &label, Err *err) const
{
    if (checkType_ <= CheckType::NONE || component == nullptr || item == nullptr || (ignoreTest_ && item->testonly())) {
        return true;
    }

    if (component->isInnerApi(label)) {
        return true;
    }
    if (checkType_ >= CheckType::INTERCEPT_IGNORE_TEST) {
        return InterceptInnerApiNotDeclare(item, label, err);
    }
    GenerateScanList("innerapi_not_declare.list", component->subsystem(), component->name(), label, "");
    return true;
}

bool OhosComponentChecker::CheckIncludesAbsoluteDepsOther(const Target *target, const std::string &label,
    const std::string &includes, Err *err) const
{
    if (checkType_ <= CheckType::NONE || target == nullptr || (ignoreTest_ && target->testonly())) {
        return true;
    }

    if (includes == "//" || !StartWith(includes, "//") || StartWith(includes, "//out/")
        || StartWith(includes, "////out/") || StartWith(includes, "//prebuilts/")) {
        return true;
    }

    const OhosComponent *component = target->ohos_component();
    if (component == nullptr) {
        return true;
    }

    if (StartWith(includes, component->path())) {
        return true;
    }

    if (checkType_ >= CheckType::INTERCEPT_IGNORE_TEST) {
        return InterceptIncludesAbsoluteDepsOther(target, label, includes, err);
    }
    GenerateScanList("includes_absolute_deps_other.list", component->subsystem(), component->name(), label, includes);
    return true;
}

bool OhosComponentChecker::CheckInnerApiVisibilityDenied(const Item *item, const OhosComponent *component,
    const std::string &label, const std::string &deps, Err *err) const
{
    if (checkType_ <= CheckType::NONE || component == nullptr || item == nullptr || (ignoreTest_ && item->testonly())) {
        return true;
    }

    if (!component->isInnerApi(deps)) {
        return true;
    }
    std::vector<std::string> visibility = component->getInnerApiVisibility(deps);
    if (visibility.empty()) {
        return true;
    }

    const OhosComponent *from_component = item->ohos_component();
    if (from_component == nullptr) {
        return true;
    }
    auto result = std::find(visibility.begin(), visibility.end(), from_component->name());
    if (result != visibility.end()) {
        return true;
    }

    if (checkType_ >= CheckType::INTERCEPT_IGNORE_TEST) {
        return InterceptInnerApiVisibilityDenied(item, label, deps, err);
    }
    GenerateScanList("innerkit_visibility_denied.list", from_component->subsystem(), from_component->name(), label,
        deps);
    return true;
}

bool OhosComponentChecker::CheckTargetAbsoluteDepsOther(const Item *item, const OhosComponent *component,
    const std::string &label, const std::string &deps, bool is_external_deps, Err *err) const
{
    if (checkType_ <= CheckType::NONE || component == nullptr || item == nullptr || (ignoreTest_ && item->testonly())) {
        return true;
    }

    if (is_external_deps) {
        return true;
    }

    if (checkType_ >= CheckType::INTERCEPT_IGNORE_TEST) {
        return InterceptTargetAbsoluteDepsOther(item, label, deps, err);
    }

    const OhosComponent *from_component = item->ohos_component();
    if (from_component == nullptr) {
        return true;
    }
    GenerateScanList("target_absolute_deps_other.list",
        from_component->subsystem(), from_component->name(), label, deps);
    return true;
}

bool OhosComponentChecker::CheckImportOther(const FunctionCallNode *function, const BuildSettings *build_settings,
    const std::string &label, const std::string &deps, Err *err) const
{
    if (checkType_ <= CheckType::NONE || function == nullptr || build_settings == nullptr) {
        return true;
    }
    const OhosComponent *component = build_settings->GetOhosComponent(label);
    if (component == nullptr) {
        return true;
    }
    if (StartWith(deps, component->path()) || StartWith(deps, "//build/") || StartWith(deps, "//out/")
        || StartWith(deps, "//prebuilts/")) {
        return true;
    }

    if (checkType_ >= CheckType::INTERCEPT_IGNORE_TEST) {
        return InterceptImportOther(function, label, deps, err);
    }
    GenerateScanList("import_other.list", component->subsystem(), component->name(), label, deps);
    return true;
}
