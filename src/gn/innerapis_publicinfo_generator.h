// Copyright 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INNERAPIS_PUBLICINFO_GENERATOR_H_
#define INNERAPIS_PUBLICINFO_GENERATOR_H_

#include <iostream>

#include "gn/build_settings.h"
#include "gn/config.h"
#include "gn/functions.h"
#include "gn/ohos_components_checker.h"
#include "gn/parse_tree.h"
#include "gn/settings.h"
#include "gn/substitution_writer.h"
#include "gn/target.h"
#include "gn/value.h"

struct PublicConfigInfoParams {
    const Target *target;
    std::string label;
    Err *err;
    bool is_public;
};

class InnerApiPublicInfoGenerator {
public:
    void GeneratedInnerapiPublicInfo(const Target *target, const Label &label,
        Scope *scope, const std::string &type, Err *err);

    static InnerApiPublicInfoGenerator *getInstance()
    {
        return instance_;
    }

    static void Init(const std::string &build_dir, int checkType)
    {
        if (instance_ != nullptr) {
            return;
        }
        instance_ = new InnerApiPublicInfoGenerator(build_dir, checkType);
    }

private:
    bool ignoreTest_ = true;
    std::string build_dir_;
    int checkType_ = OhosComponentChecker::CheckType::NONE;
    static InnerApiPublicInfoGenerator *instance_;
    InnerApiPublicInfoGenerator(const std::string &build_dir, int checkType)
    {
        checkType_ = checkType;
        build_dir_ = build_dir;
        if (checkType == OhosComponentChecker::CheckType::SCAN_ALL ||
            checkType == OhosComponentChecker::CheckType::INTERCEPT_ALL) {
            ignoreTest_ = false;
        }
    }
    InnerApiPublicInfoGenerator() {}
    InnerApiPublicInfoGenerator &operator = (const InnerApiPublicInfoGenerator &) = delete;
};

#endif // INNERAPIS_PUBLICINFO_GENERATOR_H_
