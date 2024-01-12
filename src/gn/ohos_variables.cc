// Copyright 2024 The Open Harmony Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/ohos_variables.h"

namespace variables {

// OpenHarmony target vars -----------------------------------------------------

const char kCopyLinkableFile[] = "copy_linkable_file";
const char kCopyLinkableFile_HelpShort[] =
    "copy_linkable_file: [boolean] Copy a linkable static or shared library.";
const char kCopyLinkableFile_Help[] =
    R"(copy_linkable_file: [boolean] Copy a linkable static or shared library.

  A copy target normally doesn't export configs for targets that depend on it.
  In some cases, a copy target is a linkable target, which means it is a static
  or shared library with some public_configs to be exported.

  In this case, we can set copy_linkable_file to export configs.

Example

  copy("foo") {
    copy_linkable_file = true
    public_configs = [ ":exported_header_files" ]
  }

  executable("bar") {
    # Target bar will get public_configs of foo target
    deps = [ "foo" ]
  }
)";

const char kExternalDeps[] = "external_deps";
const char kExternalDeps_HelpShort[] =
    "external_deps: [label list] Declare external dependencies for OpenHarmony component.";
const char kExternalDeps_Help[] =
    R"(external_deps: Declare external dependencies for OpenHarmony component.

  External dependencies are like private dependencies (see "gn help deps") but
  expressed in the form of: component_name:innerapi_name.
    * component and innerapi are defined by OpenHarmony bundle.json.
  With external_deps, deps can be added without absolute path.

  This variable is enabled by setting "ohos_components_support" in .gn file (see "gn help dotfile").

Example

  # This target "a" can include libinitapi from "init" component
  executable("a") {
    deps = [ ":b" ]
    external_deps = [ "init:libinitapi" ]
  }
)";

const char kPublicExternalDeps[] = "public_external_deps";
const char kPublicExternalDeps_HelpShort[] =
    "public_external_deps: [label list] Declare public external dependencies for OpenHarmony component.";
const char kPublicExternalDeps_Help[] =
    R"(public_external_deps: Declare public external dependencies for OpenHarmony component.

  Public external dependencies (public_external_deps) are like external_deps but additionally express that
  the current target exposes the listed external_deps as part of its public API.

  This variable is enabled by setting "ohos_components_support" in .gn file (see "gn help dotfile").

Example

  # Target "a" will include libinitapi from "init" component by deps "b":
  executable("a") {
    deps = [ ":b" ]
  }

  shared_library("b") {
    deps = [ ":b" ]
    public_external_deps = [ "init:libinitapi" ]
  }
)";

}  // namespace variables
