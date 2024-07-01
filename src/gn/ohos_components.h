// Copyright (c) 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_OHOS_COMPONENTS_H_
#define TOOLS_GN_OHOS_COMPONENTS_H_

#include <map>
#include <mutex>

#include "base/files/file_path.h"
#include "base/values.h"
#include "gn/err.h"
#include "gn/value.h"

class OhosComponent {
public:
    OhosComponent();
    OhosComponent(const char *name, const char *subsystem, const char *path);

    const std::string &name() const
    {
        return name_;
    }
    const std::string &subsystem() const
    {
        return subsystem_;
    }
    const std::string &path() const
    {
        return path_;
    }

    void addInnerApi(const std::string &name, const std::string &label);

    const std::string &getInnerApi(const std::string &innerapi) const;

    void addInnerApiVisibility(const std::string &name, const std::vector<base::Value> &list);

    const std::vector<std::string> getInnerApiVisibility(const std::string &label) const;

    bool isInnerApi(const std::string &label) const;

private:
    std::string name_;
    std::string subsystem_;
    std::string path_;

    // InnerApi name to label map
    std::map<std::string, std::string> innerapi_names_;

    // InnerApi label to name map
    std::map<std::string, std::string> innerapi_labels_;

    // InnerApi lable to visibility map
    std::map<std::string, std::vector<std::string>> innerapi_visibility_;

    OhosComponent &operator = (const OhosComponent &) = delete;
};

class OhosComponentsImpl;

class OhosComponents {
public:
    OhosComponents();

    bool LoadOhosComponents(const std::string &build_dir, const Value *enable,
        const Value *indep, const Value *product, Err *err);

    bool isOhosComponentsLoaded() const;

    static bool isOhosIndepCompilerEnable();
    bool GetExternalDepsLabel(const Value &external_dep, std::string &label,
        const Label& current_toolchain, int &whole_status, Err *err) const;
    bool GetPrivateDepsLabel(const Value &dep, std::string &label,
        const Label& current_toolchain, int &whole_status, Err *err) const;
    bool GetSubsystemName(const Value &part_name, std::string &label, Err *err) const;

    const OhosComponent *GetComponentByLabel(const std::string &label) const;

    void LoadOhosComponentsChecker(const std::string &build_dir, const Value *support, int checkType,
        unsigned int ruleSwitch);

    void LoadOhosComponentsMapping(const std::string& build_dir, const Value *support, const Value *independent);

private:
    OhosComponentsImpl *mgr = nullptr;

    OhosComponents &operator = (const OhosComponents &) = delete;
};

#endif // TOOLS_GN_OHOS_COMPONENTS_H_
