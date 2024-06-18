// Copyright (c) 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_OHOS_COMPONENTS_MGR_H_
#define TOOLS_GN_OHOS_COMPONENTS_MGR_H_

#include <map>
#include <mutex>

#include "base/files/file_path.h"
#include "base/values.h"
#include "gn/err.h"
#include "gn/value.h"

class OhosComponent;

struct OhosComponentTree {
    const char *dirName;
    struct OhosComponentTree *next;
    struct OhosComponentTree *child;
    const OhosComponent *component;

public:
    OhosComponentTree(const char *dirName, const OhosComponent *component = nullptr)
    {
        this->dirName = strdup(dirName);
        this->component = component;
        this->next = nullptr;
        this->child = nullptr;
    }

    OhosComponentTree(const char *dirName, size_t len, const OhosComponent *component = nullptr)
    {
        this->dirName = (char *)malloc(len + 1);
        if (this->dirName) {
            strncpy((char *)this->dirName, dirName, len);
            ((char *)this->dirName)[len] = '\0';
        }
        this->component = component;
        this->next = nullptr;
        this->child = nullptr;
    }

    ~OhosComponentTree()
    {
        if (!this->dirName) {
            free((void *)this->dirName);
        }
    }
};

class OhosComponentsImpl {
public:
    OhosComponentsImpl();

    bool isOhosIndepCompilerEnable() const {
        return is_indep_compiler_enable_;
    }

    std::string GetTargetToolchain() const {
        return toolchain_;
    }

    void LoadToolchain(const Value *product);

    bool LoadOhosComponents(const std::string &build_dir,
                            const Value *enable, const Value *indep, const Value *product, Err *err);

    bool GetExternalDepsLabel(const Value &external_dep, std::string &label,
        const Label& current_toolchain, int &whole_status, Err *err) const;
    bool GetPrivateDepsLabel(const Value &dep, std::string &label,
        const Label& current_toolchain, int &whole_status, Err *err) const;
    bool GetSubsystemName(const Value &component_name, std::string &subsystem_name, Err *err) const;

    const OhosComponent *GetComponentByName(const std::string &component_name) const;

private:
    bool ReadBuildConfigFile(const std::string &build_dir,
                             const char *subfile, std::string &content);

    std::map<std::string, OhosComponent *> components_;

    std::map<std::string, std::string> override_map_;

    bool is_indep_compiler_enable_ = false;

    std::string toolchain_;

    struct OhosComponentTree *pathTree = nullptr;
    void setupComponentsTree();
    const struct OhosComponentTree *findChildByPath(const struct OhosComponentTree *current,
                                                    const char *path, size_t len);
    void addComponentToTree(struct OhosComponentTree *current, OhosComponent *component);

    void LoadOverrideMap(const std::string &override_map);

    // For unittest
public:
    const OhosComponent *matchComponentByLabel(const char *label);

    bool LoadComponentInfo(const std::string &components_content, std::string &err_msg_out);

    void LoadInnerApi(const std::string &component_name, const std::vector<base::Value> &innerapis);
};

#endif // TOOLS_GN_OHOS_COMPONENTS_MGR_H_