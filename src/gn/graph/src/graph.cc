#include "gn/graph/include/graph.h"

#include <utility>

#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/strings/string_util.h"
#include "gn/config.h"
#include "gn/filesystem_utils.h"
#include "gn/target.h"

Graph* Graph::instance_ = nullptr;

namespace GraphHelper {

bool Filter(const std::string label) {
  if (base::ends_with(label, "__check")
    || base::ends_with(label, "__collect")
    || base::ends_with(label, "__notice")
    || base::ends_with(label, "_info_install_info")
    || base::ends_with(label, "_resource_copy")
    || base::ends_with(label, "__metadata")
    || base::ends_with(label, "__js_assets")
    || base::ends_with(label, "__compile_profile")) {
      return false;
  }
  return true;
}

// Helper to convert a vector of LabelConfigPair to a vector of strings
std::vector<std::string> LabelConfigVectorToStringVector(
    const UniqueVector<LabelConfigPair>& configs) {
  std::vector<std::string> vec;
  for (const auto& pair : configs) {
    vec.push_back(pair.label.GetUserVisibleName(false));
  }
  return vec;
}

// Helper to convert a vector to a base::Value
base::Value ToBaseValue(const std::vector<std::string>& vector) {
  base::ListValue res;
  for (const auto& v : vector)
    res.GetList().emplace_back(base::Value(v));
  return std::move(res);
}

// In order to reduce the size of the generated file, 
// if the value corresponding to the key is empty, it will not be set to the list.
void SetKey(const std::string key, base::Value& dict, const std::vector<std::string>& vector) {
  if (vector.empty()) {
    return;
  }
  dict.SetKey(key, ToBaseValue(vector));
}

std::string GetType(const Item* item) {
  std::string type;
  if (item->GetItemTypeName() == "target") {
    std::string tmp(Target::GetStringForOutputType(item->AsTarget()->output_type()));
    type += tmp;
  } else {
    type += item->GetItemTypeName();
  }
  return type;
}

std::vector<std::string> GetSources(const Item* item) {
  std::vector<std::string> sources;
  if (item->GetItemTypeName() == "target") {
    for (const auto& source : item->AsTarget()->sources()) {
      sources.push_back(source.value());
    }
  }
  return sources;
}

std::vector<std::string> GetIncludeDirs(const Item* item) {
  std::vector<SourceDir> dirs;
  if (item->GetItemTypeName() == "target") {
      dirs = item->AsTarget()->include_dirs();
  } else if (item->GetItemTypeName() == "config") {
      dirs = item->AsConfig()->own_values().include_dirs();
  }

  std::vector<std::string> include_dirs;
  for (const auto& dir : dirs) {
    include_dirs.push_back(dir.value());
  }
  return include_dirs;
}

std::vector<std::string> GetDefines(const Item* item) {
  std::vector<std::string> results;
  if (item->GetItemTypeName() == "target") {
      results = item->AsTarget()->config_values().defines();
  } else if (item->GetItemTypeName() == "config") {
      results = item->AsConfig()->resolved_values().defines();
  }
  return results;
}

std::vector<std::string> GetCflags(const Item* item) {
  std::vector<std::string> results;
  if (item->GetItemTypeName() == "target") {
      results = item->AsTarget()->config_values().cflags();
  } else if (item->GetItemTypeName() == "config") {
      results = item->AsConfig()->resolved_values().cflags();
  }
  return results;
}

std::vector<std::string> GetCflagsC(const Item* item) {
  std::vector<std::string> results;
  if (item->GetItemTypeName() == "target") {
      results = item->AsTarget()->config_values().cflags_c();
  } else if (item->GetItemTypeName() == "config") {
      results = item->AsConfig()->resolved_values().cflags_c();
  }
  return results;
}

std::vector<std::string> GetCflagsCC(const Item* item) {
  std::vector<std::string> results;
  if (item->GetItemTypeName() == "target") {
      results = item->AsTarget()->config_values().cflags_cc();
  } else if (item->GetItemTypeName() == "config") {
      results = item->AsConfig()->resolved_values().cflags_cc();
  }
  return results;
}

std::vector<std::string> GetLdflags(const Item* item) {
  std::vector<std::string> results;
  if (item->GetItemTypeName() == "target") {
      results = item->AsTarget()->config_values().ldflags();
  } else if (item->GetItemTypeName() == "config") {
      results = item->AsConfig()->resolved_values().ldflags();
  }
  return results;
}

std::vector<std::string> GetPublicHeaders(const Target* target) {
  std::vector<std::string> public_headers;
  for (const auto& dir : target->public_headers()) {
    public_headers.push_back(dir.value());
  }
  return public_headers;
}

std::vector<std::string> GetPublicDeps(const Target* target) {
  std::vector<std::string> public_deps;
  for (const auto& dep : target->public_deps()) {
    public_deps.push_back(dep.ptr->label().GetUserVisibleName(false));
  }
  return public_deps;
}

std::vector<std::string> GetPrivateDeps(const Target* target) {
  std::vector<std::string> private_deps;
  for (const auto& dep : target->private_deps()) {
    private_deps.push_back(dep.ptr->label().GetUserVisibleName(false));
  }
  return private_deps;
}

// List of direct configs that this target, excluding the indirect config passed by.
std::vector<std::string> GetDirectConfigs(const Item* item) {
  if (item->GetItemTypeName() == "target") {
      return LabelConfigVectorToStringVector(item->AsTarget()->own_configs());
  } else if (item->GetItemTypeName() == "config") {
      return LabelConfigVectorToStringVector(item->AsConfig()->configs());
  }
  return std::vector<std::string>();
}

// List of direct public_configs that this target, excluding the indirect config passed by.
std::vector<std::string> GetDirectPublicConfigs(const Target* target) {
  return LabelConfigVectorToStringVector(target->own_public_configs());
}

// List of direct all_dependent_configs that this target, excluding the indirect config passed by.
std::vector<std::string> GetDirectAllDependentConfigs(const Target* target) {
  return LabelConfigVectorToStringVector(target->own_all_dependent_configs());
}

// Helper to get diff configs.
UniqueVector<LabelConfigPair> GetDiffConfigs(
  UniqueVector<LabelConfigPair> configs, UniqueVector<LabelConfigPair> ext_configs) {
  std::vector<LabelConfigPair> sorted_configs(configs.begin(), configs.end());
  std::vector<LabelConfigPair> sorted_public_configs(ext_configs.begin(), ext_configs.end());
  auto comparator = [](const LabelConfigPair& a, const LabelConfigPair& b) {
    return a.label < b.label;
  };
  std::sort(sorted_configs.begin(), sorted_configs.end(), comparator);
  std::sort(sorted_public_configs.begin(), sorted_public_configs.end(), comparator);

  std::vector<LabelConfigPair> private_configs_vec;
  std::set_difference(sorted_configs.begin(), sorted_configs.end(),
                      sorted_public_configs.begin(),
                      sorted_public_configs.end(),
                      std::back_inserter(private_configs_vec), comparator);
  UniqueVector<LabelConfigPair> result;
  for (const auto& pair : private_configs_vec) {
    result.push_back(pair);
  }
  return result;
}

// Only get the configs passed by.
std::vector<std::string> GetIndirectConfigs(const Target* target) {
  return LabelConfigVectorToStringVector(GetDiffConfigs(target->own_configs(), target->configs()));
}

// Only get the public_configs passed by.
std::vector<std::string> GetIndirectPublicConfigs(const Target* target) {
  return LabelConfigVectorToStringVector(GetDiffConfigs(target->own_public_configs(), target->public_configs()));
}

// Only get the all_dependent_configs passed by.
std::vector<std::string> GetIndirectAllDependentConfigs(const Target* target) {
  return LabelConfigVectorToStringVector(target->own_all_dependent_configs());
}

}  // namespace GraphHelper

Graph::JsonNodeBuilder::JsonNodeBuilder(const Module& info) : info_(info) {}

base::Value Graph::JsonNodeBuilder::BuildModules() {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetKey("id", base::Value(info_.GetItem()->label().name()));
  dict.SetKey("label", base::Value(info_.GetName()));
  dict.SetKey("type", base::Value(GraphHelper::GetType(info_.GetItem())));
  dict.SetKey("path", base::Value(info_.GetPath()));
  GraphHelper::SetKey("sources", dict, GraphHelper::GetSources(info_.GetItem()));
  GraphHelper::SetKey("include_dirs", dict, GraphHelper::GetIncludeDirs(info_.GetItem()));
  GraphHelper::SetKey("defines", dict, GraphHelper::GetDefines(info_.GetItem()));
  GraphHelper::SetKey("cflags", dict, GraphHelper::GetCflags(info_.GetItem()));
  GraphHelper::SetKey("cflags_c", dict, GraphHelper::GetCflagsC(info_.GetItem()));
  GraphHelper::SetKey("cflags_cc", dict, GraphHelper::GetCflagsCC(info_.GetItem()));
  GraphHelper::SetKey("ldflags", dict, GraphHelper::GetLdflags(info_.GetItem()));
  GraphHelper::SetKey("direct_configs", dict, GraphHelper::GetDirectConfigs(info_.GetItem()));

  if (info_.GetItem()->GetItemTypeName() == "target") {
    GraphHelper::SetKey("public_headers", dict, GraphHelper::GetPublicHeaders(info_.GetItem()->AsTarget()));
    GraphHelper::SetKey("private_deps", dict, GraphHelper::GetPrivateDeps(info_.GetItem()->AsTarget()));
    GraphHelper::SetKey("public_deps", dict, GraphHelper::GetPublicDeps(info_.GetItem()->AsTarget()));
    GraphHelper::SetKey("direct_public_configs", dict, GraphHelper::GetDirectPublicConfigs(info_.GetItem()->AsTarget()));
    GraphHelper::SetKey("direct_all_dependent_configs", dict, GraphHelper::GetDirectAllDependentConfigs(info_.GetItem()->AsTarget()));
    GraphHelper::SetKey("indirect_configs", dict, GraphHelper::GetIndirectConfigs(info_.GetItem()->AsTarget()));
    GraphHelper::SetKey("indirect_public_configs", dict, GraphHelper::GetIndirectPublicConfigs(info_.GetItem()->AsTarget()));
    GraphHelper::SetKey("indirect_all_dependent_configs", dict, GraphHelper::GetIndirectAllDependentConfigs(info_.GetItem()->AsTarget()));
  }
  return dict;
}

void Graph::DumpGraphToJsonFile(const std::vector<Module>& modules,
                                const base::FilePath& output_path) {
  base::Value module_list(base::Value::Type::LIST);
  for (const auto& module : modules) {
    Graph::JsonNodeBuilder builder(module);
    module_list.GetList().push_back(builder.BuildModules());
  }

  base::Value root(base::Value::Type::DICTIONARY);
  root.SetKey("modules", std::move(module_list));

  std::string output;
  base::JSONWriter::Write(root, &output);
  base::WriteFile(output_path, output.data(), static_cast<unsigned int>(output.size()));
}

void Graph::GenGraph(const std::vector<const Item*> items)
{
  std::vector<Module> modules;
  for (const Item *item : items) {
    std::string label = item->label().GetUserVisibleName(false);
    if (!GraphHelper::Filter(label)) {
      continue;
    }
    Module module(label, label, item);
    modules.push_back(module);
  }
  DumpGraphToJsonFile(modules, base::FilePath(out_dir_ + "/" + "graph.json"));
}