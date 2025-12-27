# Precise 精确构建系统

## 概述

Precise 是一个用于 GN 构建系统的精确构建分析工具，它能够根据代码变更自动分析出需要重新编译的目标(target)，从而避免不必要的重新编译，提高构建效率。

## 核心功能

### 1. 智能依赖分析
Precise 通过分析以下文件的修改来确定受影响的构建目标：
- **C/C++ 源文件** (.c, .cc, .cpp 等)
- **头文件** (.h)
- **GN 构建文件** (.gn, .gni)
- **GN 模块** (target)
- **其他文件** (action target 中的 script, inputs, args 等)

### 2. 多层级深度控制
针对不同类型的文件修改，可以配置不同的依赖分析深度：
- `h_file_depth`: 头文件修改的依赖深度
- `c_file_depth`: C/C++ 源文件修改的依赖深度
- `gn_file_depth`: GN 构建文件修改的依赖深度
- `gn_module_depth`: GN 模块修改的依赖深度
- `other_file_depth`: 其他文件修改的依赖深度

### 3. 头文件依赖检查
Precise 包含一个智能的头文件检查器，能够：
- 检查源文件是否实际使用了修改的头文件
- 递归分析头文件的包含关系
- 缓存检查结果以提高性能
- 支持直接包含和间接包含的检测

## 配置说明

### 配置文件结构

Precise 使用 JSON 格式的配置文件，包含以下主要配置项：

#### 基础配置

```json
{
  // 修改文件列表路径
  "modify_files_path": "out/build_configs/modify_files.json",

  // GN 文件修改列表路径
  "gn_modifications_path": "out/build_configs/gn_modifications.json",

  // 精确构建结果输出路径
  "precise_result_path": "out/precise_targets.txt",

  // 日志文件路径
  "precise_log_path": "out/precise.log",

  // 日志级别 (DEBUG, INFO, WARN, ERROR, FATAL)
  "precise_log_level": "INFO"
}
```

#### 深度配置

```json
{
  // 头文件修改分析深度 (默认: INT_MAX)
  "h_file_depth": 10,

  // C/C++ 源文件修改分析深度 (默认: INT_MAX)
  "c_file_depth": 10,

  // GN 构建文件修改分析深度 (默认: INT_MAX)
  "gn_file_depth": 10,

  // GN 模块修改分析深度 (默认: INT_MAX)
  "gn_module_depth": 10,

  // 其他文件修改分析深度 (默认: INT_MAX)
  "other_file_depth": 10
}
```

#### 过滤配置

```json
{
  // 仅编译测试目标
  "test_only": false,

  // 目标类型列表 (仅编译指定类型的目标)
  "target_type_list": [
    "shared_library",
    "static_library",
    "executable"
  ],

  // 忽略列表 (忽略指定的目标)
  "ignore_list": [
    "//third_party/*",
    "//build/*"
  ],

  // 最大范围列表 (仅分析指定范围内的目标)
  "max_range_list": [
    "//foundation/*",
    "//utils/*"
  ]
}
```

#### 父目标过滤

```json
{
  // 包含父目标列表 (仅当依赖链经过这些目标时才编译)
  "include_parent_targets": [
    "//foundation/arkui/ace:ace",
    "//utils/native/base:base"
  ],

  // 排除父目标列表 (依赖链经过这些目标时不编译)
  "exclude_parent_targets": [
    "//third_party/openssl:openssl"
  ]
}
```

### 修改文件列表

修改文件列表是一个 JSON 文件，记录了所有修改过的文件：

```json
{
  "h_file": [
    "/path/to/modified/header.h",
    "/path/to/another/header.h"
  ],
  "c_file": [
    "/path/to/modified/source.cc",
    "/path/to/another/source.cpp"
  ],
  "gn_file": [
    "/path/to/modified/BUILD.gn",
    "/path/to/modified/config.gni"
  ],
  "gn_module": [
    "//path/to:module1",
    "//path/to:module2"
  ],
  "other_file": [
    "/path/to/script.py",
    "/path/to/config.json"
  ]
}
```

## 工作流程

### 1. 初始化阶段

```cpp
// 在 Setup.cc 中初始化
const Value* preciseEnable = build_settings_.build_args().GetArgOverride(
    "ohos_module_precise_build");
if (preciseEnable && preciseEnable->boolean_value()) {
    const Value* preciseConfig = build_settings_.build_args().GetArgOverride(
        "ohos_precise_config");
    PreciseManager::Init(build_dir, build_settings_.root_path().value(),
                        preciseConfig);
}
```

### 2. 分析阶段

Precise 按以下顺序检查每个 target：

```
对于每个 target:
  ├─ 检查是否包含修改的 C/C++ 源文件
  │   └─ 如果包含，进行深度为 c_file_depth 的依赖分析
  │
  ├─ 检查是否使用修改的头文件
  │   ├─ 检查 include_dirs
  │   ├─ 检查 public_configs
  │   ├─ 检查 configs
  │   └─ 检查 all_dependent_configs
  │   └─ 如果使用，进行深度为 h_file_depth 的依赖分析
  │
  ├─ 检查是否引用修改的 GN 构建文件
  │   └─ 如果引用，进行深度为 gn_file_depth 的依赖分析
  │
  ├─ 检查是否是修改的 GN 模块
  │   └─ 如果是，进行深度为 gn_module_depth 的依赖分析
  │
  └─ 检查 action target 中的其他文件
      ├─ 检查 script
      ├─ 检查 inputs
      ├─ 检查 sources
      ├─ 检查 args 中的文件路径
      └─ 检查 depfile
      └─ 如果包含，进行深度为 other_file_depth 的依赖分析
```

### 3. 过滤阶段

对分析出的 target 进行过滤：

```cpp
// 应用父目标过滤
void ApplyTargetFilters(std::vector<std::string>& result,
                        std::vector<Module*>& module_list) {
    for (int i = result.size() - 1; i >= 0; --i) {
        ModuleCheckResult checkResult = CheckModulePath(module, {});

        // 排除在 exclude_parent_targets 中的目标
        if (checkResult.is_excluded) {
            删除该目标;
        }

        // 如果配置了 include_parent_targets，
        // 仅保留依赖链经过这些目标的项目
        if (!includeParentTargets.empty() && !checkResult.is_included) {
            删除该目标;
        }
    }
}
```

### 4. 输出阶段

将最终确定的 target 列表写入文件：

```
//foundation/arkui/ace:ace
//utils/native/base:base
//utils/native/util:util
...
```

## 核心类说明

### PreciseManager

精确构建管理器，负责整体流程控制。

**主要方法：**
- `Init()`: 初始化管理器
- `GeneratPreciseTargets()`: 生成精确构建目标列表
- `PreciseSearch()`: 递归搜索依赖目标
- `CheckSourceInTarget()`: 检查 target 是否包含修改的源文件
- `CheckIncludeInTarget()`: 检查 target 是否使用修改的头文件目录
- `CheckGNFileModified()`: 检查 target 是否依赖修改的 GN 文件
- `CheckFilesInActionTarget()`: 检查 action target 中的文件

### ConfigManager

配置管理器，负责加载和管理配置。

**主要方法：**
- `LoadConfig()`: 加载主配置文件
- `LoadModifyList()`: 加载修改文件列表
- `GetConfig()`: 获取配置对象

### HeaderChecker

头文件检查器，负责分析头文件依赖关系。

**主要方法：**
- `CheckActuallyUsedHeaders()`: 检查 target 是否实际使用修改的头文件
- `GetFileIncludes()`: 获取文件的 include 列表
- `ResolveIncludePath()`: 解析 include 路径
- `CheckHeaderDependencyRecursive()`: 递归检查头文件依赖
- `AddCache()`: 添加缓存
- `ClearCaches()`: 清理所有缓存

### LogManager

日志管理器，提供实时日志功能。

**主要方法：**
- `Initialize()`: 初始化日志系统
- `LogMessage()`: 记录日志消息
- `ShouldLog()`: 判断是否应该记录日志
- `Close()`: 关闭日志文件

**日志级别：**
- `DEBUG`: 调试信息
- `INFO`: 一般信息
- `WARN`: 警告信息
- `ERROR`: 错误信息
- `FATAL`: 致命错误

## 使用方法

### 1. 启用 Precise 构建

在 args.gn 中添加：

```gn
ohos_module_precise_build = true
ohos_precise_config = "//build/precise_config.json"
```

### 2. 配置文件示例

创建 `precise_config.json`:

```json
{
  "h_file_depth": 5,
  "c_file_depth": 5,
  "gn_file_depth": 3,
  "gn_module_depth": 5,
  "other_file_depth": 3,
  "test_only": false,
  "target_type_list": [
    "shared_library",
    "static_library",
    "executable",
    "source_set"
  ],
  "ignore_list": [
    "//third_party/*",
    "//prebuilts/*"
  ],
  "max_range_list": [
    "//foundation/*",
    "//utils/*",
    "//drivers/*"
  ],
  "modify_files_path": "out/build_configs/modify_files.json",
  "gn_modifications_path": "out/build_configs/gn_modifications.json",
  "precise_result_path": "out/precise_targets.txt",
  "precise_log_path": "out/precise.log",
  "precise_log_level": "INFO",
  "include_parent_targets": [],
  "exclude_parent_targets": []
}
```

### 3. 生成修改文件列表

创建 `modify_files.json`:

```json
{
  "h_file": [
    "/path/to/project/foundation/arkui/ace/frameworks/core/components/common_layout.h"
  ],
  "c_file": [
    "/path/to/project/foundation/arkui/ace/frameworks/core/components/common_layout.cc"
  ],
  "gn_file": [],
  "gn_module": [],
  "other_file": []
}
```

### 4. 运行 GN 生成

```bash
gn gen out --args="ohos_module_precise_build=true ohos_precise_config=\"//build/precise_config.json\""
```

### 5. 查看结果

- **目标列表**: `out/precise_targets.txt`
- **详细日志**: `out/precise.log`

## 性能优化

### 1. 缓存机制

Precise 使用多层缓存来提高性能：

- **头文件目录缓存**: 缓存每个 include_dir 对应的修改头文件集合
- **文件 include 缓存**: 缓存已解析的文件的 include 列表
- **依赖关系缓存**: 缓存头文件对修改头文件的依赖关系

### 2. 建议配置

根据项目规模调整深度配置：

```
小型项目 (< 1000 targets):
  h_file_depth: 10
  c_file_depth: 10
  gn_file_depth: 5
  gn_module_depth: 10

中型项目 (1000-5000 targets):
  h_file_depth: 5
  c_file_depth: 5
  gn_file_depth: 3
  gn_module_depth: 5

大型项目 (> 5000 targets):
  h_file_depth: 3
  c_file_depth: 3
  gn_file_depth: 2
  gn_module_depth: 3
```

## 典型使用场景

### 场景 1: 修改公共头文件

```json
{
  "h_file": [
    "/path/utils/native/base/include/utils.h"
  ],
  "c_file": [],
  "gn_file": [],
  "gn_module": [],
  "other_file": []
}
```

**结果**: 编译所有直接或间接包含该头文件的 target（深度受 h_file_depth 控制）

### 场景 2: 修改构建配置

```json
{
  "h_file": [],
  "c_file": [],
  "gn_file": [
    "/path/foundation/BUILD.gn"
  ],
  "gn_module": [],
  "other_file": []
}
```

**结果**: 重新编译依赖该 BUILD.gn 文件的所有 target（深度受 gn_file_depth 控制）

### 场景 3: 修改特定模块

```json
{
  "h_file": [],
  "c_file": [],
  "gn_file": [],
  "gn_module": [
    "//foundation/arkui/ace:ace"
  ],
  "other_file": []
}
```

**结果**: 编译依赖该模块的所有 target（深度受 gn_module_depth 控制）

## 调试技巧

### 1. 启用详细日志

```json
{
  "precise_log_level": "DEBUG"
}
```

### 2. 查看日志信息

日志中包含以下关键信息：
- `Check:` - 正在检查的 target
- `Hit C:` - 匹配到 C/C++ 源文件修改
- `Hit H:` - 匹配到头文件修改
- `Hit GN File:` - 匹配到 GN 文件修改
- `Hit Module:` - 匹配到 GN 模块修改
- `Hit Action:` - 匹配到 Action target 文件修改
- `OK:` - 确定需要编译的 target
- `Over Depth:` - 超过深度限制

### 3. 常见问题

**问题 1**: 分析结果为空
- 检查 modify_files.json 是否正确配置
- 检查文件路径是否正确
- 启用 DEBUG 日志查看详细过程

**问题 2**: 分析结果过多
- 调整深度配置，降低依赖分析深度
- 检查是否正确配置 ignore_list
- 检查是否正确配置 max_range_list
- 使用 include_parent_targets 过滤

**问题 3**: 性能较慢
- 降低深度配置
- 检查日志级别是否为 DEBUG（调试时性能较慢）
- 查看是否有循环依赖

## 注意事项

1. **路径格式**: 所有路径使用绝对路径或 GN 源路径（//开头）
2. **深度设置**: 深度过大会导致分析时间长、结果集大；深度过小可能遗漏依赖
3. **缓存清理**: 修改配置后建议删除旧的缓存文件重新分析
4. **日志管理**: 日志文件会持续增长，建议定期清理
5. **性能考虑**: 大型项目建议使用较小的深度值，配合 include_parent_targets 精确控制

## 相关文件

- `src/gn/precise/precise.h` - Precise 主管理器
- `src/gn/precise/precise.cc` - Precise 实现
- `src/gn/precise/precise_config.h` - 配置管理
- `src/gn/precise/precise_config.cc` - 配置实现
- `src/gn/precise/precise_util.h` - 工具类
- `src/gn/precise/precise_util.cc` - 工具实现
- `src/gn/precise/precise_log.h` - 日志管理
- `src/gn/precise/precise_log.cc` - 日志实现
- `src/gn/setup.cc` - Precise 初始化入口

## 版本历史

- **v1.0** - 初始版本，支持基础文件类型分析
- **v1.1** - 添加头文件依赖检查功能
- **v1.2** - 添加父目标过滤功能
- **v1.3** - 添加实时日志系统
- **v1.4** - 添加性能优化和缓存机制
