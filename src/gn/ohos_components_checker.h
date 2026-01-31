// Copyright (c) 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_COMPONENTS_CHECKER_H_
#define OHOS_COMPONENTS_CHECKER_H_

#include "gn/build_settings.h"
#include "gn/config.h"
#include "gn/functions.h"
#include "gn/parse_tree.h"
#include "gn/settings.h"
#include "gn/substitution_writer.h"
#include "gn/target.h"
#include "gn/value.h"

class OhosComponentChecker {
public:
    enum CheckType {
        NONE = 0,
        SCAN_IGNORE_TEST,
        SCAN_ALL,
        INTERCEPT_IGNORE_TEST,
        INTERCEPT_ALL
    };
    enum BinaryLeftShift {
        UNKNOWN = 0,
        ALL_DEPS_CONFIG_BINARY,
        INCLUDE_OVER_RANGE_BINARY,
        INNERAPI_PUBLIC_DEPS_INNER_BINARY,
        INNERAPI_NOT_LIB_BINARY,
        DEPS_NOT_LIB_BINARY,
        INNERAPI_NOT_DECLARE_BINARY,
        INCLUDES_ABSOLUTE_DEPS_OTHER_BINARY,
        TARGET_ABSOLUTE_DEPS_OTHER_BINARY,
        IMPORT_OTHER_BINARY,
        INNERAPI_VISIBILITY_DENIED,
        PUBLIC_DEPS_BINARY,
        LIB_DIRS_BINARY,
        DEPS_COMPONENT_NOT_DECLARE,
        EXTERNAL_DEPS_INNER,
        ALL
    };

    static void Init(const std::string &build_dir, int checkType, unsigned int ruleSwitch, bool whitelistDebug = false)
    {
        if (instance_ != nullptr) {
            return;
        }
        instance_ = new OhosComponentChecker(build_dir, checkType, ruleSwitch, whitelistDebug);
    }

    // 在构建完成后调用此方法来写入拦截列表（如果启用了白名单调试模式）
    static void WriteInterceptedListIfNeeded()
    {
        if (instance_ != nullptr && instance_->whitelistDebug_) {
            instance_->WriteInterceptedListToFile();
        }
    }

    bool CheckAllDepsConfigs(const Target *target, const std::string &label, Err *err) const;
    bool CheckInnerApiIncludesOverRange(const Target *target, const std::string &label, const std::string &dir,
        Err *err) const;
    bool CheckInnerApiPublicDepsInner(const Target *target, const std::string &label, const std::string &deps,
        Err *err) const;
    bool CheckPublicDeps(const Target *target, const std::string &label, const std::string &deps,
        Err *err) const;
    bool CheckLibDir(const Target *target, const std::string &label, const std::string &dir, Err *err) const;
    bool CheckInnerApiNotLib(const Item *item, const OhosComponent *component, const std::string &label,
        const std::string &deps, Err *err) const;
    bool CheckInnerApiNotDeclare(const Item *item, const OhosComponent *component, const std::string &label,
        Err *err) const;
    bool CheckIncludesAbsoluteDepsOther(const Target *target, const std::string &label, const std::string &includes,
        Err *err) const;
    bool CheckInnerApiVisibilityDenied(const Item *item, const OhosComponent *component, const std::string &label,
        const std::string &deps, Err *err) const;
    bool CheckTargetAbsoluteDepsOther(const Item *item, const OhosComponent *component, const std::string &label,
        const std::string &deps, bool is_external_deps, Err *err) const;
    bool CheckImportOther(const FunctionCallNode *function, const BuildSettings *build_settings,
        const std::string &label, const std::string &deps, Err *err) const;
    bool CheckExternalDepsInner(const Item *item, const OhosComponent *component, const std::string &label,
                                const std::string &deps, bool is_external_deps, Err *err) const;
    bool CheckDepsComponentNotDeclare(const Item *from, const Item *to, const std::string &label,
                                const std::string &deps, bool is_external_deps, Err *err) const;

    static OhosComponentChecker *getInstance()
    {
        return instance_;
    }

    // 检查指定的 label 和 deps 组合是否在 public_deps 白名单中
    static bool IsPublicDepsWhitelisted(const std::string& label, const std::string& deps);

    // 设置白名单调试模式
    void SetWhitelistDebug(bool enable) { whitelistDebug_ = enable; }
    bool IsWhitelistDebug() const { return whitelistDebug_; }

    // 添加拦截项到列表中（用于白名单调试模式）
    void AddToInterceptedList(const std::string &category, const std::string &label, const std::string &value = "") const;

private:
    int checkType_ = NONE;
    bool ignoreTest_ = true;
    bool whitelistDebug_ = false;  // 白名单调试模式: 打印但不中断，并且自动收集拦截清单
    unsigned int ruleSwitch_;
    std::string build_dir_;
    static OhosComponentChecker *instance_;

    // 收集被拦截的目标（用于生成拦截清单）
    mutable std::map<std::string, std::vector<std::string>> interceptedList_;  // 简单列表类型的拦截项
    mutable std::map<std::string, std::map<std::string, std::vector<std::string>>> interceptedDict_;  // 字典类型的拦截项
    bool InterceptAllDepsConfig(const Target *target, const std::string &label, Err *err) const;
    bool InterceptIncludesOverRange(const Target *target, const std::string &label, const std::string &dir,
        Err *err) const;
    bool InterceptInnerApiPublicDepsInner(const Target *target, const std::string &label, const std::string &deps,
        Err *err) const;
    bool InterceptPublicDeps(const Target *target, const std::string &label, const std::string &deps,
        const OhosComponent *from_component, Err *err) const;
    bool InterceptLibDir(const Target *target, const std::string &label, const std::string &dir, Err *err) const;
    bool InterceptInnerApiNotLib(const Item *item, const std::string &label, Err *err) const;
    bool InterceptDepsNotLib(const Item *item, const std::string &label, const std::string &deps, Err *err) const;
    bool InterceptInnerApiNotDeclare(const Item *item, const std::string &label, Err *err) const;
    bool InterceptIncludesAbsoluteDepsOther(const Target *target, const std::string &label, const std::string &includes,
        Err *err) const;
    bool InterceptInnerApiVisibilityDenied(const Item *item, const std::string &from_label, const std::string &to_label,
        Err *err) const;
    bool InterceptTargetAbsoluteDepsOther(const Item *item, const std::string &label, const std::string &deps,
        Err *err) const;
    bool InterceptImportOther(const FunctionCallNode* function, const std::string &label, const std::string &deps,
        Err *err) const;
    bool InterceptDepsComponentNotDeclare(const Item *item, const std::string &label, const std::string &from_name,
                                          const std::string &to_name, const std::string &path, Err *err) const;
    bool InterceptExternalDepsInner(const Item *item, const std::string &from_label, const std::string &to_label,
                                    Err *err) const;
    void GenerateScanList(const std::string &path, const std::string &subsystem, const std::string &component,
        const std::string &label, const std::string &deps) const;
    void WriteInterceptedListToFile() const;
    OhosComponentChecker() {}
    OhosComponentChecker(const std::string &build_dir, int checkType, unsigned int ruleSwitch, bool whitelistDebug = false);
    OhosComponentChecker &operator = (const OhosComponentChecker &) = delete;
};

#endif // OHOS_COMPONENTS_CHECKER_H_
