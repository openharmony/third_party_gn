// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_TARGET_GENERATOR_H_
#define TOOLS_GN_TARGET_GENERATOR_H_

#include <string>
#include <vector>

#include "gn/label_ptr.h"
#include "gn/unique_vector.h"

class BuildSettings;
class Err;
class FunctionCallNode;
class Scope;
class SubstitutionPattern;
class Value;

// Fills the variables in a Target object from a Scope (the result of a script
// execution). Target-type-specific derivations of this class will be used
// for each different type of function call. This class implements the common
// behavior.
class TargetGenerator {
 public:
  TargetGenerator(Target* target,
                  Scope* scope,
                  const FunctionCallNode* function_call,
                  Err* err);
  virtual ~TargetGenerator();

  void Run();

  // The function call is the parse tree node that invoked the target.
  // err() will be set on failure.
  static void GenerateTarget(Scope* scope,
                             const FunctionCallNode* function_call,
                             const std::vector<Value>& args,
                             const std::string& output_type,
                             Err* err);

 protected:
  // Derived classes implement this to do type-specific generation.
  virtual void DoRun() = 0;

  const BuildSettings* GetBuildSettings() const;

  virtual bool FillSources();
  bool FillIncludes();
  bool FillPublic();
  bool FillConfigs();
  bool FillOwnConfigs();
  bool FillOutputs(bool allow_substitutions);
  bool FillCheckIncludes();
  bool FillOutputExtension();

  // Rrturns true if the given pattern will expand to a file in the output
  // directory. If not, returns false and sets the error, blaming the given
  // Value.
  bool EnsureSubstitutionIsInOutputDir(const SubstitutionPattern& pattern,
                                       const Value& original_value);

  Target* target_;
  Scope* scope_;
  const FunctionCallNode* function_call_;
  Err* err_;

 private:
  bool FillDependentConfigs();  // Includes all types of dependent configs.
  bool FillData();
  bool FillDependencies();  // Includes data dependencies.
  bool FillMetadata();
  bool FillTestonly();
  bool FillAssertNoDeps();
  bool FillWriteRuntimeDeps();
  void FillCheckFlag();

  // Reads configs/deps from the given var name, and uses the given setting on
  // the target to save them.
  bool FillGenericConfigs(const char* var_name,
                          UniqueVector<LabelConfigPair>* dest);
  bool FillGenericDeps(const char* var_name, LabelTargetVector* dest);

  bool FillOhosComponentDeps(const char* var_name, LabelTargetVector* dest,
    LabelTargetVector* whole_dest, LabelTargetVector* no_whole_dest);
  bool FillGenericDepsWithWholeArchive(const char* var_name, LabelTargetVector* dest,
    LabelTargetVector* whole_dest, LabelTargetVector* no_whole_dest);

  TargetGenerator(const TargetGenerator&) = delete;
  TargetGenerator& operator=(const TargetGenerator&) = delete;
};

#endif  // TOOLS_GN_TARGET_GENERATOR_H_
