// Copyright (c) 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OHOS_COMPONENTS_MAPPING_H_
#define OHOS_COMPONENTS_MAPPING_H_

#include "gn/build_settings.h"
#include "gn/config.h"
#include "gn/functions.h"
#include "gn/parse_tree.h"
#include "gn/settings.h"
#include "gn/substitution_writer.h"
#include "gn/target.h"
#include "gn/value.h"

class OhosComponentMapping {
public:
    static void Init(const std::string& build_dir)
    {
        if (instance_ != nullptr) {
            return;
        }
        instance_ = new OhosComponentMapping(build_dir);
    }

    const std::string MappingTargetAbsoluteDpes(const BuildSettings *settings,
        const std::string &label, const std::string &deps) const;
    const std::string MappingImportOther(const BuildSettings *settings,
        const std::string &label, const std::string &deps) const;

    static OhosComponentMapping *getInstance()
    {
        return instance_;
    }

private:
    std::string build_dir_;
    static OhosComponentMapping *instance_;
    OhosComponentMapping() {}
    OhosComponentMapping(const std::string &build_dir);
    OhosComponentMapping &operator = (const OhosComponentMapping &) = delete;
};

#endif // OHOS_COMPONENTS_MAPPING_H_
