// Copyright (c) 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/ohos_components_mapping.h"

#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <sys/stat.h>

#include "base/values.h"
#include "gn/build_settings.h"
#include "gn/config.h"
#include "gn/functions.h"
#include "gn/ohos_components.h"
#include "gn/parse_tree.h"
#include "gn/settings.h"
#include "gn/substitution_writer.h"
#include "gn/target.h"
#include "gn/value.h"

OhosComponentMapping *OhosComponentMapping::instance_ = nullptr;

static bool StartWith(const std::string &str, const std::string prefix)
{
    return (str.rfind(prefix, 0) == 0);
}

const std::string OhosComponentMapping::MappingTargetAbsoluteDpes(const BuildSettings* build_settings,
    const std::string label, const std::string deps) const
{
    if (build_settings == nullptr) {
        return "";
    }

    const OhosComponent *component = build_settings->GetOhosComponent(label);
    if (component == nullptr) {
        return "";
    }
    if (StartWith(deps, component->path())) {
        return "";
    }

    const OhosComponent *deps_component = build_settings->GetOhosComponent(deps);
    if (deps_component == nullptr) {
        return "";
    }

    size_t pos = deps.find(":");
    if (pos <= 0) {
        return "";
    }
    return deps_component->getInnerApi(deps.substr(pos + 1));
}

const std::string OhosComponentMapping::MappingImportOther(const BuildSettings* build_settings,
    const std::string label, const std::string deps) const
{
    if (build_settings == nullptr) {
        return "";
    }

    const OhosComponent *component = build_settings->GetOhosComponent(label);
    if (component == nullptr) {
        return "";
    }
    if (StartWith(deps, component->path()) || StartWith(deps, "//build/") || StartWith(deps, "//out/")) {
        return "";
    }
    return "//build/indep_configs/gni" + deps.substr(1);
}
