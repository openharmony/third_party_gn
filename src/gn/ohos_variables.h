// Copyright 2024 The Open Harmony Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_OHOS_TARGET_VARIABLES_H_
#define TOOLS_GN_OHOS_TARGET_VARIABLES_H_

#include "gn/variables.h"

namespace variables {

// OpenHarmony target vars -----------------------------------------------------

extern const char kCopyLinkableFile[];
extern const char kCopyLinkableFile_HelpShort[];
extern const char kCopyLinkableFile_Help[];

extern const char kExternalDeps[];
extern const char kExternalDeps_HelpShort[];
extern const char kExternalDeps_Help[];

extern const char kPublicExternalDeps[];
extern const char kPublicExternalDeps_HelpShort[];
extern const char kPublicExternalDeps_Help[];
}  // namespace variables

#endif  // TOOLS_GN_SWIFT_TARGET_VARIABLES_H_
