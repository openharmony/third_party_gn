// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/copy_target_generator.h"

#include "gn/build_settings.h"
#include "gn/filesystem_utils.h"
#include "gn/ohos_variables.h"
#include "gn/parse_tree.h"
#include "gn/scope.h"
#include "gn/value.h"

CopyTargetGenerator::CopyTargetGenerator(Target* target,
                                         Scope* scope,
                                         const FunctionCallNode* function_call,
                                         Err* err)
    : TargetGenerator(target, scope, function_call, err) {}

CopyTargetGenerator::~CopyTargetGenerator() = default;

bool CopyTargetGenerator::FillCopyLinkableFile()
{
    const Value* value = scope_->GetValue(variables::kCopyLinkableFile, true);
    if (!value)
        return true;
    if (!value->VerifyTypeIs(Value::BOOLEAN, err_))
        return false;
    target_->set_copy_linkable_file(value->boolean_value());
  return true;
}

bool CopyTargetGenerator::FillCopyRustTargetInfo() {
  const Value* rustCrateType =
      scope_->GetValue(variables::kCopyRustCrateType, true);
  const Value* rustCrateName =
      scope_->GetValue(variables::kCopyRustCrateName, true);

  if (!rustCrateName || !rustCrateType) {
    return true;
  }

  if (!(rustCrateType->VerifyTypeIs(Value::STRING, err_) &&
        rustCrateName->VerifyTypeIs(Value::STRING, err_)))
    return false;

  target_->rust_values().crate_name() =
      std::move(rustCrateName->string_value());

  if (rustCrateType->string_value() == "bin") {
    target_->rust_values().set_crate_type(RustValues::CRATE_BIN);
    return true;
  }
  if (rustCrateType->string_value() == "cdylib") {
    target_->rust_values().set_crate_type(RustValues::CRATE_CDYLIB);
    return true;
  }
  if (rustCrateType->string_value() == "dylib") {
    target_->rust_values().set_crate_type(RustValues::CRATE_DYLIB);
    return true;
  }
  if (rustCrateType->string_value() == "proc-macro") {
    target_->rust_values().set_crate_type(RustValues::CRATE_PROC_MACRO);
    return true;
  }
  if (rustCrateType->string_value() == "rlib") {
    target_->rust_values().set_crate_type(RustValues::CRATE_RLIB);
    return true;
  }
  if (rustCrateType->string_value() == "staticlib") {
    target_->rust_values().set_crate_type(RustValues::CRATE_STATICLIB);
    return true;
  }

  *err_ =
      Err(rustCrateType->origin(),
          "Inadmissible crate type \"" + rustCrateType->string_value() + "\".");
  return false;
}

void CopyTargetGenerator::DoRun() {
  target_->set_output_type(Target::COPY_FILES);

  if (!FillSources())
    return;
  if (!FillOutputs(true))
    return;

  if (target_->sources().empty()) {
    *err_ = Err(
        function_call_, "Empty sources for copy command.",
        "You have to specify at least one file to copy in the \"sources\".");
    return;
  }
  if (target_->action_values().outputs().list().size() != 1) {
    *err_ = Err(
        function_call_, "Copy command must have exactly one output.",
        "You must specify exactly one value in the \"outputs\" array for the "
        "destination of the copy\n(see \"gn help copy\"). If there are "
        "multiple sources to copy, use source expansion\n(see \"gn help "
        "source_expansion\").");
    return;
  }
  
  if (!FillCopyLinkableFile())
    return;

  if (!FillCopyRustTargetInfo())
    return;
}
