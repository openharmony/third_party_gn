// Copyright (c) 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/ohos_components.h"
#include "gn/ohos_components_impl.h"

#include <iostream>
#include <stdint.h>
#include <memory>
#include <utility>

#include "gn/test_with_scope.h"
#include "util/test/test.h"

std::string pathLoadComponentSubsystemAndPaths = "{"
            "\"foo\": \"components/foo\","
            "\"bar\": \"components/bar\","
            "\"baz\": \"components/baz\""
        "}";

std::string subsystemLoadComponentSubsystemAndPaths = "{"
            "\"foo\": \"samples\","
            "\"bar\": \"samples\","
            "\"baz\": \"samples\""
        "}";

std::string strLoadOhosInnerApis = "{"
        "\"foo\": {"
          "\"libfoo\": {"
            "\"label\": \"//components/foo/interfaces/innerapis/libfoo:libfoo\""
          "}"
        "},"
        "\"bar\": {"
          "\"libbar\": {"
            "\"label\": \"//components/bar/interfaces/innerapis/libbar:libbar\""
          "}"
        "},"
        "\"baz\": {"
          "\"libbaz\": {"
            "\"label\": \"//components/baz/interfaces/innerapis/libbaz:libbaz\""
          "}"
        "}"
    "}";

TEST(OhosComponent, ComponentInnerApi) {
    OhosComponent com("foo", "samples", "components/foo");
    EXPECT_EQ("foo", com.name());
    EXPECT_EQ("samples", com.subsystem());
    EXPECT_EQ("//components/foo", com.path());

    // Add two innerapis
    const std::string fooLabel = "//components/foo/interfaces/innerapis/libfoo:libfoo";
    com.addInnerApi("libfoo", fooLabel.c_str());
    const std::string barLabel = "//components/bar/interfaces/innerapis/libbar:libbar";
    com.addInnerApi("libbar", barLabel.c_str());

    // Get first innerapi
    std::string result = com.getInnerApi("libfoo");
    EXPECT_EQ(fooLabel, result);

    // Get second innerapi api
    result = com.getInnerApi("libbar");
    EXPECT_EQ(barLabel, result);

    // Get non exist innerapi api
    result = com.getInnerApi("libnone");
    EXPECT_EQ(0, result.size());

    // Check valid innerapi label
    bool ret = com.isInnerApi("//components/bar/interfaces/innerapis/libbar:libbar");
    ASSERT_TRUE(ret);

    // Check invalid innerapi label
    ret = com.isInnerApi("//components/bar/interfaces/innerapis/libbar:libbar2");
    ASSERT_FALSE(ret);
}

TEST(OhosComponentsImpl, LoadComponentSubsystemAndPaths) {
    OhosComponentsImpl *mgr = new OhosComponentsImpl();
    std::string errStr;
    bool ret = mgr->LoadComponentSubsystemAndPaths(pathLoadComponentSubsystemAndPaths,
                                                   "",
                                                   subsystemLoadComponentSubsystemAndPaths,
                                                   errStr);
    ASSERT_TRUE(ret);

    ret = mgr->LoadOhosInnerApis_(strLoadOhosInnerApis, errStr);
    ASSERT_TRUE(ret);

    const OhosComponent *component;

    component = mgr->matchComponentByLabel("components/foo");
    EXPECT_EQ("foo", component->name());
    component = mgr->matchComponentByLabel("//components/foo");
    EXPECT_EQ("foo", component->name());
    component = mgr->matchComponentByLabel("//components/foo:libfoo");
    EXPECT_EQ("foo", component->name());
    component = mgr->matchComponentByLabel("//components/foo/test");
    EXPECT_EQ("foo", component->name());
    component = mgr->matchComponentByLabel("//components/foo/test:libfoo");
    EXPECT_EQ("foo", component->name());

    component = mgr->matchComponentByLabel("components/fo");
    EXPECT_EQ(nullptr, component);
    component = mgr->matchComponentByLabel("components/");
    EXPECT_EQ(nullptr, component);
    component = mgr->matchComponentByLabel("components");
    EXPECT_EQ(nullptr, component);

    Err err;
    Value external_dep(nullptr, "foo:libfoo");
    std::string label;
    ret = mgr->GetExternalDepsLabel(external_dep, label, &err);
    ASSERT_TRUE(ret);

    delete mgr;
}