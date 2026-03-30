// Copyright (c) 2026 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "gn/functions.h"
#include "gn/parse_tree.h"
#include "gn/scope.h"
#include "gn/source_dir.h"
#include "gn/test_with_scope.h"
#include "gn/value.h"
#include "util/test/test.h"

namespace {
Value RunGlobFiles(Scope* scope,
                   const std::string& path,
                   bool allow_empty = false) {
  Err err;
  std::vector<Value> args;
  args.push_back(Value(nullptr, path));
  if (allow_empty) {
    args.push_back(Value(nullptr, allow_empty));
  }

  FunctionCallNode function_call;
  Value result = functions::RunGlobFiles(scope, &function_call, args, &err);
  return result;
}
}  // namespace

TEST(GlobFilesTest, NormalScan) {
  TestWithScope setup;
  setup.scope()->set_source_dir(SourceDir("//some-dir/"));

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  setup.build_settings()->SetRootPath(temp_dir.GetPath());

  base::FilePath dir_path = temp_dir.GetPath().AppendASCII("some-dir");
  base::CreateDirectory(dir_path);

  base::WriteFile(dir_path.AppendASCII("file1.cc"), "foo", 3);
  base::WriteFile(dir_path.AppendASCII("file2.h"), "bar", 3);

  Value result = RunGlobFiles(setup.scope(), "//some-dir");
  EXPECT_TRUE(result.type() == Value::LIST);
  EXPECT_EQ(2u, result.list_value().size());

  EXPECT_EQ("//some-dir/file1.cc", result.list_value()[0].string_value());
  EXPECT_EQ("//some-dir/file2.h", result.list_value()[1].string_value());
}

TEST(GlobFilesTest, RecursiveScan) {
  TestWithScope setup;
  setup.scope()->set_source_dir(SourceDir("//some-dir/"));

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  setup.build_settings()->SetRootPath(temp_dir.GetPath());

  base::FilePath dir_path = temp_dir.GetPath().AppendASCII("some-dir");
  base::CreateDirectory(dir_path);
  base::WriteFile(dir_path.AppendASCII("root.cc"), "foo", 3);

  base::FilePath subdir_path = dir_path.AppendASCII("subdir");
  base::CreateDirectory(subdir_path);
  base::WriteFile(subdir_path.AppendASCII("sub.cc"), "bar", 3);

  base::FilePath nested_path = subdir_path.AppendASCII("nested");
  base::CreateDirectory(nested_path);
  base::WriteFile(nested_path.AppendASCII("nested.cc"), "baz", 3);

  Value result = RunGlobFiles(setup.scope(), "//some-dir");
  EXPECT_TRUE(result.type() == Value::LIST);
  EXPECT_EQ(3u, result.list_value().size());

  EXPECT_EQ("//some-dir/root.cc", result.list_value()[0].string_value());
  EXPECT_EQ("//some-dir/subdir/nested/nested.cc",
            result.list_value()[1].string_value());
  EXPECT_EQ("//some-dir/subdir/sub.cc", result.list_value()[2].string_value());
}

TEST(GlobFilesTest, EmptyDirectory) {
  TestWithScope setup;
  setup.scope()->set_source_dir(SourceDir("//some-dir/"));

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  setup.build_settings()->SetRootPath(temp_dir.GetPath());

  base::FilePath dir_path = temp_dir.GetPath().AppendASCII("some-dir");
  base::CreateDirectory(dir_path);

  {
    Err err;
    FunctionCallNode function_call;
    std::vector<Value> args;
    args.push_back(Value(nullptr, "//some-dir"));
    Value result =
        functions::RunGlobFiles(setup.scope(), &function_call, args, &err);
    EXPECT_TRUE(result.type() == Value::NONE);
    EXPECT_TRUE(err.has_error());
  }

  {
    Value result = RunGlobFiles(setup.scope(), "//some-dir", true);
    EXPECT_TRUE(result.type() == Value::LIST);
    EXPECT_EQ(0u, result.list_value().size());
  }
}

TEST(GlobFilesTest, FilterOutputDir) {
  TestWithScope setup;
  setup.scope()->set_source_dir(SourceDir("//some-dir/"));
  setup.build_settings()->SetBuildDir(SourceDir("//out/Default/"));

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  setup.build_settings()->SetRootPath(temp_dir.GetPath());

  base::FilePath dir_path = temp_dir.GetPath().AppendASCII("some-dir");
  base::CreateDirectory(dir_path);
  base::WriteFile(dir_path.AppendASCII("source.cc"), "foo", 3);

  base::FilePath out_path = temp_dir.GetPath().AppendASCII("out");
  base::CreateDirectory(out_path);
  base::FilePath default_path = out_path.AppendASCII("Default");
  base::CreateDirectory(default_path);
  base::FilePath some_dir_in_out = default_path.AppendASCII("some-dir");
  base::CreateDirectory(some_dir_in_out);
  base::WriteFile(some_dir_in_out.AppendASCII("output.o"), "bar", 3);

  Value result = RunGlobFiles(setup.scope(), "//some-dir");
  EXPECT_TRUE(result.type() == Value::LIST);
  EXPECT_EQ(1u, result.list_value().size());
  EXPECT_EQ("//some-dir/source.cc", result.list_value()[0].string_value());
}

TEST(GlobFilesTest, InvalidArguments) {
  TestWithScope setup;
  FunctionCallNode function_call;

  {
    Err err;
    std::vector<Value> args;
    functions::RunGlobFiles(setup.scope(), &function_call, args, &err);
    EXPECT_TRUE(err.has_error());
  }

  {
    Err err;
    std::vector<Value> args;
    args.push_back(Value(nullptr, "a"));
    args.push_back(Value(nullptr, "b"));
    args.push_back(Value(nullptr, "c"));
    functions::RunGlobFiles(setup.scope(), &function_call, args, &err);
    EXPECT_TRUE(err.has_error());
  }

  {
    Err err;
    std::vector<Value> args;
    args.push_back(Value(nullptr, Value::LIST));
    functions::RunGlobFiles(setup.scope(), &function_call, args, &err);
    EXPECT_TRUE(err.has_error());
  }

  {
    Err err;
    std::vector<Value> args;
    args.push_back(Value(nullptr, "a"));
    args.push_back(Value(nullptr, "b"));
    functions::RunGlobFiles(setup.scope(), &function_call, args, &err);
    EXPECT_TRUE(err.has_error());
  }

  {
    Err err;
    std::vector<Value> args;
    args.push_back(Value(nullptr, ""));
    functions::RunGlobFiles(setup.scope(), &function_call, args, &err);
    EXPECT_TRUE(err.has_error());
  }
}

TEST(GlobFilesTest, SortAndDeduplicate) {
  TestWithScope setup;
  setup.scope()->set_source_dir(SourceDir("//some-dir/"));

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  setup.build_settings()->SetRootPath(temp_dir.GetPath());

  base::FilePath dir_path = temp_dir.GetPath().AppendASCII("some-dir");
  base::CreateDirectory(dir_path);
  base::WriteFile(dir_path.AppendASCII("b.cc"), "b", 1);
  base::WriteFile(dir_path.AppendASCII("a.cc"), "a", 1);
  base::WriteFile(dir_path.AppendASCII("c.cc"), "c", 1);

  Value result = RunGlobFiles(setup.scope(), "//some-dir");
  EXPECT_TRUE(result.type() == Value::LIST);
  EXPECT_EQ(3u, result.list_value().size());
  EXPECT_EQ("//some-dir/a.cc", result.list_value()[0].string_value());
  EXPECT_EQ("//some-dir/b.cc", result.list_value()[1].string_value());
  EXPECT_EQ("//some-dir/c.cc", result.list_value()[2].string_value());
}

TEST(GlobFilesTest, MaxResultsLimit) {
  TestWithScope setup;
  setup.scope()->set_source_dir(SourceDir("//some-dir/"));
  setup.build_settings()->set_glob_max_results(2);

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  setup.build_settings()->SetRootPath(temp_dir.GetPath());

  base::FilePath dir_path = temp_dir.GetPath().AppendASCII("some-dir");
  base::CreateDirectory(dir_path);
  base::WriteFile(dir_path.AppendASCII("file1.cc"), "1", 1);
  base::WriteFile(dir_path.AppendASCII("file2.cc"), "2", 1);
  base::WriteFile(dir_path.AppendASCII("file3.cc"), "3", 1);

  Err err;
  FunctionCallNode function_call;
  std::vector<Value> args;
  args.push_back(Value(nullptr, "//some-dir"));
  Value result =
      functions::RunGlobFiles(setup.scope(), &function_call, args, &err);
  EXPECT_TRUE(result.type() == Value::NONE);
  EXPECT_TRUE(err.has_error());
}

TEST(GlobFilesTest, CannotOverrideMaxResultsInScope) {
  TestWithScope setup;
  setup.scope()->set_source_dir(SourceDir("//some-dir/"));
  // Set a global limit via build_settings
  setup.build_settings()->set_glob_max_results(2);

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  setup.build_settings()->SetRootPath(temp_dir.GetPath());

  base::FilePath dir_path = temp_dir.GetPath().AppendASCII("some-dir");
  base::CreateDirectory(dir_path);
  base::WriteFile(dir_path.AppendASCII("file1.cc"), "1", 1);
  base::WriteFile(dir_path.AppendASCII("file2.cc"), "2", 1);
  base::WriteFile(dir_path.AppendASCII("file3.cc"), "3", 1);


  // Try to override the limit by setting a scope variable
  setup.scope()->SetValue("glob_max_results", Value(nullptr, static_cast<int64_t>(10000)), nullptr);
  Err err;
  FunctionCallNode function_call;
  std::vector<Value> args;
  args.push_back(Value(nullptr, "//some-dir"));
  Value result =
      functions::RunGlobFiles(setup.scope(), &function_call, args, &err);
  EXPECT_TRUE(result.type() == Value::NONE);
  EXPECT_TRUE(err.has_error());
}

TEST(GlobFilesTest, RejectOutputDirectory) {
  TestWithScope setup;
  setup.scope()->set_source_dir(SourceDir("//some-dir/"));
  setup.build_settings()->SetBuildDir(SourceDir("//out/Default/"));

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  setup.build_settings()->SetRootPath(temp_dir.GetPath());

  base::FilePath out_path = temp_dir.GetPath().AppendASCII("out");
  base::CreateDirectory(out_path);
  base::FilePath default_path = out_path.AppendASCII("Default");
  base::CreateDirectory(default_path);
  base::WriteFile(default_path.AppendASCII("file.o"), "bar", 3);

  Err err;
  FunctionCallNode function_call;
  std::vector<Value> args;
  args.push_back(Value(nullptr, "//out/Default"));
  Value result =
      functions::RunGlobFiles(setup.scope(), &function_call, args, &err);
  EXPECT_TRUE(result.type() == Value::NONE);
  EXPECT_TRUE(err.has_error());
}

TEST(GlobFilesTest, RejectOutputSubDirectory) {
  TestWithScope setup;
  setup.scope()->set_source_dir(SourceDir("//some-dir/"));
  setup.build_settings()->SetBuildDir(SourceDir("//out/"));

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  setup.build_settings()->SetRootPath(temp_dir.GetPath());

  base::FilePath out_path = temp_dir.GetPath().AppendASCII("out");
  base::CreateDirectory(out_path);
  base::FilePath subdir_path = out_path.AppendASCII("subdir");
  base::CreateDirectory(subdir_path);
  base::WriteFile(subdir_path.AppendASCII("file.o"), "bar", 3);

  Err err;
  FunctionCallNode function_call;
  std::vector<Value> args;
  args.push_back(Value(nullptr, "//out/subdir"));
  Value result =
      functions::RunGlobFiles(setup.scope(), &function_call, args, &err);
  EXPECT_TRUE(result.type() == Value::NONE);
  EXPECT_TRUE(err.has_error());
}

TEST(GlobFilesTest, RejectOutSideSource) {
  TestWithScope setup;
  setup.scope()->set_source_dir(SourceDir("//some-dir/"));

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  setup.build_settings()->SetRootPath(temp_dir.GetPath());

  Err err;
  FunctionCallNode function_call;
  std::vector<Value> args;
  args.push_back(Value(nullptr, ".."));
  Value result =
      functions::RunGlobFiles(setup.scope(), &function_call, args, &err);
  EXPECT_TRUE(result.type() == Value::NONE);
  EXPECT_TRUE(err.has_error());
}

TEST(GlobFilesTest, RelativePath) {
  TestWithScope setup;
  setup.scope()->set_source_dir(SourceDir("//some-dir/"));

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  setup.build_settings()->SetRootPath(temp_dir.GetPath());

  base::FilePath dir_path = temp_dir.GetPath().AppendASCII("some-dir");
  base::CreateDirectory(dir_path);
  base::WriteFile(dir_path.AppendASCII("file.cc"), "foo", 3);

  base::FilePath subdir_path = dir_path.AppendASCII("subdir");
  base::CreateDirectory(subdir_path);
  base::WriteFile(subdir_path.AppendASCII("sub.cc"), "bar", 3);

  {
    Value result = RunGlobFiles(setup.scope(), ".");
    EXPECT_TRUE(result.type() == Value::LIST);
    EXPECT_EQ(2u, result.list_value().size());
    EXPECT_EQ("//some-dir/file.cc", result.list_value()[0].string_value());
    EXPECT_EQ("//some-dir/subdir/sub.cc",
              result.list_value()[1].string_value());
  }

  {
    Value result = RunGlobFiles(setup.scope(), "./subdir");
    EXPECT_TRUE(result.type() == Value::LIST);
    EXPECT_EQ(1u, result.list_value().size());
    EXPECT_EQ("//some-dir/subdir/sub.cc",
              result.list_value()[0].string_value());
  }
}

TEST(GlobFilesTest, RejectSystemAbsolutePath) {
  TestWithScope setup;
  setup.scope()->set_source_dir(SourceDir("//some-dir/"));

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  setup.build_settings()->SetRootPath(temp_dir.GetPath());

  // Test system absolute path "/tmp"
  {
    Err err;
    FunctionCallNode function_call;
    std::vector<Value> args;
    args.push_back(Value(nullptr, "/tmp"));
    Value result =
        functions::RunGlobFiles(setup.scope(), &function_call, args, &err);
    EXPECT_TRUE(result.type() == Value::NONE);
    EXPECT_TRUE(err.has_error());
  }

  // Test system absolute path "/home"
  {
    Err err;
    FunctionCallNode function_call;
    std::vector<Value> args;
    args.push_back(Value(nullptr, "/home"));
    Value result =
        functions::RunGlobFiles(setup.scope(), &function_call, args, &err);
    EXPECT_TRUE(result.type() == Value::NONE);
    EXPECT_TRUE(err.has_error());
  }
}

TEST(GlobFilesTest, DirectoryNotFound) {
  TestWithScope setup;
  setup.scope()->set_source_dir(SourceDir("//some-dir/"));

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  setup.build_settings()->SetRootPath(temp_dir.GetPath());

  // Test non-existent directory
  {
    Err err;
    FunctionCallNode function_call;
    std::vector<Value> args;
    args.push_back(Value(nullptr, "//non-existent-dir"));
    Value result =
        functions::RunGlobFiles(setup.scope(), &function_call, args, &err);
    EXPECT_TRUE(result.type() == Value::NONE);
    EXPECT_TRUE(err.has_error());
  }

  // Test non-existent relative path
  {
    Err err;
    FunctionCallNode function_call;
    std::vector<Value> args;
    args.push_back(Value(nullptr, "./non-existent-dir"));
    Value result =
        functions::RunGlobFiles(setup.scope(), &function_call, args, &err);
    EXPECT_TRUE(result.type() == Value::NONE);
    EXPECT_TRUE(err.has_error());
  }
}

TEST(GlobFilesTest, SingleFile) {
  TestWithScope setup;
  setup.scope()->set_source_dir(SourceDir("//some-dir/"));

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  setup.build_settings()->SetRootPath(temp_dir.GetPath());

  base::FilePath dir_path = temp_dir.GetPath().AppendASCII("some-dir");
  base::CreateDirectory(dir_path);
  base::WriteFile(dir_path.AppendASCII("file.cc"), "foo", 3);

  // Test with a single file path
  {
    Value result = RunGlobFiles(setup.scope(), "//some-dir/file.cc");
    EXPECT_TRUE(result.type() == Value::LIST);
    EXPECT_EQ(1u, result.list_value().size());
    EXPECT_EQ("//some-dir/file.cc", result.list_value()[0].string_value());
  }

  // Test with relative file path
  {
    Value result = RunGlobFiles(setup.scope(), "./file.cc");
    EXPECT_TRUE(result.type() == Value::LIST);
    EXPECT_EQ(1u, result.list_value().size());
    EXPECT_EQ("//some-dir/file.cc", result.list_value()[0].string_value());
  }
}
