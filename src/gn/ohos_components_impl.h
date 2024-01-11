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

    bool LoadOhosComponents(const std::string &build_dir, 
                            const Value *enable, Err *err);

    bool GetExternalDepsLabel(const Value &external_dep, std::string &label, Err *err) const;
    bool GetSubsystemName(const Value &component_name, std::string &subsystem_name, Err *err) const;

    const OhosComponent *GetComponentByName(const std::string &component_name) const;

private:
    bool ReadBuildConfigFile(const std::string &build_dir, 
                             const char *subfile, std::string &content);

    std::map<std::string, OhosComponent *> components_;

    struct OhosComponentTree *pathTree = nullptr;
    void setupComponentsTree();
    const struct OhosComponentTree *findChildByPath(const struct OhosComponentTree *current, 
        const char *path,
        size_t len);
    void addComponentToTree(struct OhosComponentTree *current, OhosComponent *component);

    // For unittest
public:
    const OhosComponent *matchComponentByLabel(const char *label);

    bool LoadComponentSubsystemAndPaths(const std::string &paths, 
                                        const std::string &override_map,
                                        const std::string &subsystems, 
                                        std::string &err_msg_out);

    bool LoadOhosInnerApis_(const std::string innerapi_content, std::string &err_msg_out);
};

#endif // TOOLS_GN_OHOS_COMPONENTS_MGR_H_