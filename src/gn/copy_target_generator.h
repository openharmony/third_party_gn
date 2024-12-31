// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_COPY_TARGET_GENERATOR_H_
#define TOOLS_GN_COPY_TARGET_GENERATOR_H_

#include "gn/target_generator.h"
#include "gn/ohos_variables.h"

// Populates a Target with the values from a copy rule.
class CopyTargetGenerator : public TargetGenerator {
 public:
  CopyTargetGenerator(Target* target,
                      Scope* scope,
                      const FunctionCallNode* function_call,
                      Err* err);
  ~CopyTargetGenerator() override;

 protected:
  void DoRun() override;

 private:
  CopyTargetGenerator(const CopyTargetGenerator&) = delete;
  CopyTargetGenerator& operator=(const CopyTargetGenerator&) = delete;

  bool FillCopyLinkableFile();
  bool FillCopyRustTargetInfo();
};

#endif  // TOOLS_GN_COPY_TARGET_GENERATOR_H_
