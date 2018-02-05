#include <unordered_map>
#include <vector>

class Graph {
public:
  Graph(int num_nodes) : adjacency_list_(num_nodes) {
  }

  Graph(int num_nodes, std::initializer_list<std::pair<int, int>> edges)
      : Graph(num_nodes) {
    for (const auto &edge : edges) {
      if ((edge.first < 0) || (edge.first >= num_nodes) || (edge.second < 0) || (edge.second >= num_nodes)) {
        continue;
      }
      adjacency_list_[edge.first].push_back(edge.second);
    }
  }

  void addEdge(int from, int to) {
    int num_nodes = static_cast<int>(adjacency_list_.size());
    if ((from < 0) || (from >= num_nodes) || (to < 0) || (to >= num_nodes)) {
      return;
    }
    adjacency_list_[from].push_back(to);
  }

  Graph transpose() const {
    int num_nodes = static_cast<int>(adjacency_list_.size());
    Graph g(num_nodes);
    for (int from = 0; from < num_nodes; ++from) {
      const auto &adjacency = adjacency_list_[from];
      for (int to : adjacency) {
        g.addEdge(to, from);
      }
    }
    return g;
  }

  std::vector<std::vector<int>> getStronglyConnectedComponents() const {
    // Run DFS to get topological order.
    std::vector<int> order;
    dfs(&noop<>, &noop<int>, &noop<int, int>, [&order] (int node) -> bool {
      order.push_back(node);
      return true;
    });
    order = std::vector<int>(order.rbegin(), order.rend());

    // Run DFS again in topological order to get predecessor subgraph from transposed graph.
    Graph transposed = transpose();
    std::vector<int> predecessor(adjacency_list_.size(), -1);
    transposed.dfs(order, &noop<>, &noop<int>, [&predecessor] (int source, int neighbor) -> bool {
      predecessor[neighbor] = source;
      return true;
    });

    // Collect.
    std::unordered_map<int, std::vector<int>> strongly_connected_component_map;
    for (size_t i = 0; i < static_cast<int>(predecessor.size()); ++i) {
      int root = i;
      while (predecessor[root] != -1) {
        root = predecessor[root];
      }
      strongly_connected_component_map[root].push_back(i);
    }
    std::vector<std::vector<int>> strongly_connected_components;
    for (const auto &p : strongly_connected_component_map) {
      strongly_connected_components.emplace_back(std::move(p.second));
    }
    return strongly_connected_components;
  }

  bool isCyclic() const {
    return !dfs([] () -> bool { return false; });
  }

private:
  enum DfsState {
    DFS_STATE_INIT, DFS_STATE_ON_PATH, DFS_STATE_VISITED
  };

  enum DfsResult {
    DFS_RESULT_SUCCEEDED, DFS_RESULT_FAILED, DFS_RESULT_SKIPPED
  };

  template<class... Args>
  static bool noop(Args...) {
    return true;
  }

  template<
    class OnCycle = bool(*)(),
    class OnVisit = bool(*)(int),
    class OnVisiting = bool(*)(int, int),
    class OnVisited = bool(*)(int)>
  bool dfs(const std::vector<int> &order,
      OnCycle on_cycle = &noop<>,
      OnVisit on_visit = &noop<int>,
      OnVisiting on_visiting = &noop<int, int>,
      OnVisited on_visited = &noop<int>) const {
    std::vector<DfsState> state(adjacency_list_.size(), DFS_STATE_INIT);
    for (int node : order) {
      if (dfs_impl(node, state, on_cycle, on_visit, on_visiting, on_visited) == DFS_RESULT_FAILED) {
        return false;
      }
    }
    return true;
  }

  template<
    class OnCycle = bool(*)(),
    class OnVisit = bool(*)(int),
    class OnVisiting = bool(*)(int, int),
    class OnVisited = bool(*)(int)>
  bool dfs(
      OnCycle on_cycle = &noop<>,
      OnVisit on_visit = &noop<int>,
      OnVisiting on_visiting = &noop<int, int>,
      OnVisited on_visited = &noop<int>) const {
    std::vector<int> order(adjacency_list_.size(), -1);
    for (size_t i = 0; i < order.size(); ++i) {
      order[i] = static_cast<int>(i);
    }
    return dfs(order, on_cycle, on_visit, on_visiting, on_visited);
  }

  template<class OnCycle, class OnVisit, class OnVisiting, class OnVisited>
  DfsResult dfs_impl(int source, std::vector<DfsState> &state,
      OnCycle on_cycle,
      OnVisit on_visit,
      OnVisiting on_visiting,
      OnVisited on_visited) const {
    switch (state[source]) {
      case DFS_STATE_ON_PATH: return on_cycle() ? DFS_RESULT_SKIPPED : DFS_RESULT_FAILED;
      case DFS_STATE_VISITED: return DFS_RESULT_SKIPPED;
      default: break;
    }
    state[source] = DFS_STATE_ON_PATH;
    if (!on_visit(source)) {
      return DFS_RESULT_FAILED;
    }
    for (int neighbor : adjacency_list_[source]) {
      auto result = dfs_impl(neighbor, state, on_cycle, on_visit, on_visiting, on_visited);
      if (result == DFS_RESULT_FAILED) {
        return DFS_RESULT_FAILED;
      }
      if (result == DFS_RESULT_SKIPPED) {
        continue;
      }
      if (!on_visiting(source, neighbor)) {
        return DFS_RESULT_FAILED;
      }
    }
    state[source] = DFS_STATE_VISITED;
    return on_visited(source) ? DFS_RESULT_SUCCEEDED : DFS_RESULT_FAILED;
  }

  std::vector<std::vector<int>> adjacency_list_;
};

int main() {
  Graph g(9, {
    {0, 1}, {1, 2}, {1, 4}, {2, 0}, {2, 3}, {2, 5}, {3, 2},
    {4, 5}, {4, 6}, {5, 4}, {5, 6}, {5, 7},
    {6, 7}, {7, 8}, {8, 6}
  });
  auto scc = g.getStronglyConnectedComponents();
  auto is_cyclic = g.isCyclic();
  return 0;
}
