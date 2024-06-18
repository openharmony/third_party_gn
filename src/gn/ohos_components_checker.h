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
        ALL
    };

    static void Init(const std::string &build_dir, int checkType, unsigned int ruleSwitch)
    {
        if (instance_ != nullptr) {
            return;
        }
        instance_ = new OhosComponentChecker(build_dir, checkType, ruleSwitch);
    }

    bool CheckAllDepsConfigs(const Target *target, const std::string label, Err *err) const;
    bool CheckInnerApiIncludesOverRange(const Target *target, const std::string label, const std::string dir,
        Err *err) const;
    bool CheckInnerApiPublicDepsInner(const Target *target, const std::string label, const std::string deps,
        Err *err) const;
    bool CheckInnerApiNotLib(const Item *item, const OhosComponent *component, const std::string label,
        const std::string deps, Err *err) const;
    bool CheckInnerApiNotDeclare(const Item *item, const OhosComponent *component, const std::string label,
        Err *err) const;
    bool CheckIncludesAbsoluteDepsOther(const Target *target, const std::string label, const std::string includes,
        Err *err) const;
    bool CheckInnerApiVisibilityDenied(const Item *item, const OhosComponent *component, const std::string label,
        const std::string deps, Err *err) const;
    bool CheckTargetAbsoluteDepsOther(const Item *item, const OhosComponent *component, const std::string label,
        const std::string deps, bool is_external_deps, Err *err) const;
    bool CheckImportOther(const FunctionCallNode* function, const BuildSettings* build_settings,
        const std::string label, const std::string deps, Err *err) const;

    static OhosComponentChecker *getInstance()
    {
        return instance_;
    }

private:
    int checkType_ = NONE;
    bool ignoreTest_ = true;
    unsigned int ruleSwitch_;
    std::string build_dir_;
    static OhosComponentChecker *instance_;
    bool InterceptAllDepsConfig(const Target *target, const std::string label, Err *err) const;
    bool InterceptIncludesOverRange(const Target *target, const std::string label, const std::string dir,
        Err *err) const;
    bool InterceptInnerApiPublicDepsInner(const Target *target, const std::string label, const std::string deps,
        Err *err) const;
    bool InterceptInnerApiNotLib(const Item *item, const std::string label, Err *err) const;
    bool InterceptDepsNotLib(const Item *item, const std::string label, const std::string deps, Err *err) const;
    bool InterceptInnerApiNotDeclare(const Item *item, const std::string label, Err *err) const;
    bool InterceptIncludesAbsoluteDepsOther(const Target *target, const std::string label, const std::string includes,
        Err *err) const;
    bool InterceptInnerApiVisibilityDenied(const Item *item, const std::string from_label, const std::string to_label,
        Err *err) const;
    bool InterceptTargetAbsoluteDepsOther(const Item *item, const std::string label, const std::string deps,
        Err *err) const;
    bool InterceptImportOther(const FunctionCallNode* function, const std::string label, const std::string deps,
        Err *err) const;
    void GenerateScanList(const std::string path, const std::string subsystem, const std::string component,
        const std::string label, const std::string deps) const;
    OhosComponentChecker() {}
    OhosComponentChecker(const std::string &build_dir, int checkType, unsigned int ruleSwitch);
    OhosComponentChecker &operator = (const OhosComponentChecker &) = delete;
};

#endif // OHOS_COMPONENTS_CHECKER_H_
