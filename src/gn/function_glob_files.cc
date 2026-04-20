// Copyright (c) 2026 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "gn/build_settings.h"
#include "gn/err.h"
#include "gn/filesystem_utils.h"
#include "gn/functions.h"
#include "gn/parse_tree.h"
#include "gn/scope.h"
#include "gn/settings.h"
#include "gn/source_dir.h"
#include "gn/source_file.h"
#include "gn/value.h"

namespace functions {

namespace {

bool ParseArguments(const std::vector<Value>& args,
                    const FunctionCallNode* function,
                    const Value& root_value,
                    bool& allow_empty,
                    Err* err) {
  if (args.size() < 1 || args.size() > 2) {
    *err = Err(function->function(), "Expecting 1 or 2 arguments.");
    return false;
  }

  if (!root_value.VerifyTypeIs(Value::STRING, err)) {
    return false;
  }

  if (root_value.string_value().empty()) {
    *err = Err(function->function(), "Empty path.",
               "glob_files: The root path cannot be empty.");
    return false;
  }

  allow_empty = false;
  if (args.size() == 2) {
    const Value& allow_empty_value = args[1];
    if (!allow_empty_value.VerifyTypeIs(Value::BOOLEAN, err)) {
      return false;
    }
    allow_empty = allow_empty_value.boolean_value();
  }
  return true;
}

// Helper function to check if a path points to a file and resolve it.
// Returns true if it's a file, false if it's a directory or doesn't exist.
bool ResolveAndCheckFile(Scope* scope,
                         const Value& root_value,
                         const std::string& source_root_path,
                         base::FilePath* system_path,
                         Err* err) {
  const std::string& input_string = root_value.string_value();
  // Paths ending with '/' are valid directories; skip ResolveRelativeFile
  // which would treat them as errors since it expects a file path.
  if (!input_string.empty() && input_string.back() == '/') {
    return false;
  }

  const SourceDir& cur_dir = scope->GetSourceDir();
  SourceFile resolved_file =
      cur_dir.ResolveRelativeFile(root_value, err, source_root_path);

  if (err->has_error()) {
    return false;
  }

  *system_path =
      scope->settings()->build_settings()->GetFullPath(resolved_file);
  return base::PathExists(*system_path) && !base::DirectoryExists(*system_path);
}

// Helper function to validate directory path
bool ValidateDirectoryPath(Scope* scope,
                           const FunctionCallNode* function,
                           const Value& root_value,
                           const std::string& source_root_path,
                           SourceDir* resolved_dir,
                           base::FilePath* system_path,
                           Err* err) {
  const SourceDir& cur_dir = scope->GetSourceDir();
  *resolved_dir = cur_dir.ResolveRelativeDir(root_value, err, source_root_path);
  if (err->has_error()) {
    return false;
  }

  if (resolved_dir->is_system_absolute()) {
    *err =
        Err(function->function(), "Invalid directory path.",
            "glob_files: The root directory cannot be a system-absolute path. "
            "Use a source-absolute path (starting with \"//\") or a path "
            "relative to the current file.");
    return false;
  }

  const SourceDir& build_dir = scope->settings()->build_settings()->build_dir();
  if (IsStringInOutputDir(build_dir, resolved_dir->value())) {
    *err =
        Err(function->function(), "Cannot scan output directory.",
            "glob_files: The root directory cannot be inside the build output "
            "directory (" +
                build_dir.value() +
                ". This function should only "
                "be used for source files, not generated files.");
    return false;
  }

  *system_path =
      scope->settings()->build_settings()->GetFullPath(*resolved_dir);
  return true;
}

// Helper function to return a single file as a list
Value ReturnSingleFile(const FunctionCallNode* function,
                       const base::FilePath& system_path,
                       const std::string& source_root_path,
                       Err* err) {
  std::string file_path_utf8 = FilePathToUTF8(system_path);
  std::string source_relative;
  if (MakeAbsolutePathRelativeIfPossible(source_root_path, file_path_utf8,
                                         &source_relative)) {
    Value result(function, Value::LIST);
    result.list_value().push_back(Value(function, source_relative));
    return result;
  } else {
    *err = Err(function->function(), "Invalid file path.",
               "glob_files: The specified file path could not be made "
               "relative to the source root.");
    return Value();
  }
}

// Helper function to collect all files in directory
std::vector<std::string> CollectFilesInDirectory(
    const base::FilePath& system_path,
    const std::string& source_root_path,
    int max_results,
    const FunctionCallNode* function,
    Err* err) {
  std::vector<std::string> results;
  base::FileEnumerator enumerator(system_path, true,
                                  base::FileEnumerator::FILES);
  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    if (static_cast<int>(results.size()) >= max_results) {
      *err = Err(function->function(), "Too many files found.",
                 "glob_files:  The number of files found exceeds the global "
                 "limit (glob_max_results = " +
                     std::to_string(max_results) + ").");
      return {};
    }
    std::string file_path_utf8 = FilePathToUTF8(path);

    std::string source_relative;
    if (MakeAbsolutePathRelativeIfPossible(source_root_path, file_path_utf8,
                                           &source_relative)) {
      results.push_back(source_relative);
    }
  }
  return results;
}

// Helper function to build final result list
Value BuildResultList(const FunctionCallNode* function,
                      const std::vector<std::string>& results) {
  Value result(function, Value::LIST);
  for (const auto& file : results) {
    result.list_value().push_back(Value(function, file));
  }
  return result;
}

bool HandleEmptyDirectory(const FunctionCallNode* function,
                          bool allow_empty,
                          const std::vector<std::string>& results,
                          Err* err) {
  if (!allow_empty && results.empty()) {
    *err = Err(function->function(), "Empty directory",
               "glob_files: The directory is empty and allow_empty is false.");
    return false;
  }

  if (allow_empty && results.empty()) {
    Err warning(function->function(), "Empty directory.",
                "glob_files: The directory is empty, returning an empty list.");
    warning.PrintNonfatalToStdout();
  }
  return true;
}

}  // namespace

const char kGlobFiles[] = "glob_files";
const char kGlobFiles_HelpShort[] =
    "glob_files: Recursively find all files in a directory or return a single "
    "file.";
const char kGlobFiles_Help[] =
    R"(glob_files: Recursively find all files in a directory or return a single file.

  glob_files(root, allow_empty = false)

  Finds all files in the given directory and its subdirectories. If the path
  points to a file instead of a directory, return that single file.

  arguments:
    root: The directory to scan or the file to return. Must be a source-absolute
          path (e.g., "//foo/bar") or a path relative to the current file.

    allow_empty: If false (default), the function will fail if the directory is
                empty or contains no matching files. If true, an empty list will
                be returned and a warning will be issued. This parameter has no
                effect when a file is specified.

  Returns: A list of source-absolute file paths (e.g., ["//foo/bar/file.cc"]).
          If a single file is specified, returns a list containing just that file.

  Notes:
    - Only source files are returned. Files in the output directory are filtered out.
    - The root path can be a source-absolute path (starting with "//") or
      a path relative to the current file. system-absolute path are not allowed.
    - The root path cannot be inside the build output directory.
    - The results are sorted and deduplicated to ensure consistency across
      different system and file systems.
    - The number of files returned is limited by the global setting
      glob_max_results (default 5000) to prevent performance issues.
      This can be configured in the .gn file in your project root:
        glob_max_results = 10000
      This is a global limit that cannot be overridden in BUILD.gn files.
    - This function should only be used for build inputs. Do not use it to
      generated outputs.
    - The results can be further filtered using filter_include() and filter_exclude().
)";

Value RunGlobFiles(Scope* scope,
                   const FunctionCallNode* function,
                   const std::vector<Value>& args,
                   Err* err) {
  const Value& root_value = args[0];
  bool allow_empty;

  if (!ParseArguments(args, function, root_value, allow_empty, err)) {
    return Value();
  }

  const std::string& source_root_path =
      scope->settings()->build_settings()->root_path_utf8();

  base::FilePath system_path;
  bool is_file_path = ResolveAndCheckFile(scope, root_value, source_root_path,
                                          &system_path, err);

  if (is_file_path) {
    return ReturnSingleFile(function, system_path, source_root_path, err);
  }

  SourceDir resolved_dir;
  if (!ValidateDirectoryPath(scope, function, root_value, source_root_path,
                             &resolved_dir, &system_path, err)) {
    return Value();
  }

  if (!base::DirectoryExists(system_path)) {
    *err = Err(function->function(), "Path not found.",
               "glob_files: The specified path does not exist.");
    return Value();
  }

  int max_results = scope->settings()->build_settings()->glob_max_results();
  std::vector<std::string> results = CollectFilesInDirectory(
      system_path, source_root_path, max_results, function, err);

  if (err->has_error()) {
    return Value();
  }

  if (!HandleEmptyDirectory(function, allow_empty, results, err)) {
    return Value();
  }

  std::sort(results.begin(), results.end());
  results.erase(std::unique(results.begin(), results.end()), results.end());
  return BuildResultList(function, results);
}

}  // namespace functions
