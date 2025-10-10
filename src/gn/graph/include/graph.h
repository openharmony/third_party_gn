#ifndef GN_GRAPH_INCLUDE_GRAPH_H_
#define GN_GRAPH_INCLUDE_GRAPH_H_

#include <vector>

#include "base/files/file_path.h"
#include "base/values.h"
#include "gn/item.h"
#include "gn/label_ptr.h"
#include "gn/target.h"
#include "gn/unique_vector.h"
#include "gn/graph/include/module.h"

class Graph {
  public:
    static void Init(const std::string& out_dir) {
      if (instance_ != nullptr) {
        return;
      }
      instance_ = new Graph(out_dir);
    }

    static Graph* GetInstance() {
       return instance_;
    }

    void GenGraph(const std::vector<const Item*> items);

 private:
  class JsonNodeBuilder {
    public:
    explicit JsonNodeBuilder(const Module& info);
    base::Value BuildModules();

    private:
    const Module& info_;
  };
  static Graph* instance_;
  std::string out_dir_;
  void DumpGraphToJsonFile(const std::vector<Module>& modules,
                           const base::FilePath& output_path);
  Graph() {}
  Graph(const std::string& out_dir) { out_dir_ = out_dir; }
  Graph &operator = (const Graph &) = delete;
};

#endif  // GN_GRAPH_INCLUDE_GRAPH_H_
