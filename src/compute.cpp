#include "compute.hpp"

#include <functional>
#include <queue>
#include <vector>

///< @brief Running process in the simulation
struct RunningProcess {
    int finish; ///< finish time of the process
    int id;     ///< process ID

    bool operator>(const RunningProcess& o) const noexcept { return finish > o.finish; } ///< comparison operator for priority queue
};

///< @brief Running process queue in the simulation
using RunPQ = std::priority_queue<RunningProcess, std::vector<RunningProcess>, std::greater<RunningProcess>>;

///< @brief Node in the beam search tree
struct Node {
    int                                     cycle{};    ///< current cycle in the simulation, initially 0
    std::unordered_map<std::string, int>    stocks;     ///< current stock of items, keyed by item name
    RunPQ                                   running;    ///< running processes in the simulation, ordered by finish time
    int                                     score{};    ///< score of the node, initially 0
    std::shared_ptr<Node>                   parent;     ///< parent node in the beam search tree, initially nullptr
};

/**
 * @brief Function to apply stock changes based on items.
 *
 * @param stock The stock map to modify, keyed by item name.
 * @param items The vector of items to apply, each with a name and quantity.
 * @param sign The sign to apply to the quantities (positive for adding, negative for removing).
 */
static void apply_items(std::unordered_map<std::string, int>& stock, const std::vector<Item>& items, int sign){
    for(const auto &i: items)
        stock[i.name] += sign * i.qty;
}

/**
 * @brief Function to realise finished processes in a node based on the current cycle value.
 *
 * @param n The node containing the running processes and current cycle.
 * @param procs The vector of processes, used to apply results when a process finishes.
 */
static void realise_finishes(Node &n, const std::vector<Process>& procs) {
    while (!n.running.empty() && n.running.top().finish <= n.cycle) {
        int id = n.running.top().id; n.running.pop();
        apply_items(n.stocks, procs[id].results, +1);
    }
}

/**
 * @brief Function to check if all needs of a process are satisfied by the current stock.
 *
 * @param stocks The current stock of items, keyed by item name.
 * @param proc The process to check, containing its needs.
 */
static bool needs_satisfied(const std::unordered_map<std::string, int>& stocks, const Process& proc){
    for(auto &it:proc.needs) {
        auto f=stocks.find(it.name);
        if(f==stocks.end()||f->second<it.qty)
            return false;
    }
    return true;
}

static int heuristic(const Node& n, const std::vector<Process>& procs){
    // Base = earliest finish among running procs, or current cycle if none
    int h = n.running.empty() ? n.cycle : n.running.top().finish;

    // Add the minimum delay of *runnable* processes (if any). This peeks one step ahead and
    // usually guides the beam toward branches where something can complete sooner.
    int bestDelay = INT32_MAX;
    for(size_t pid = 0; pid < procs.size(); ++pid){
        const Process &p = procs[pid];
        bool ok = true;
        for(const auto &need : p.needs){
            auto it = n.stocks.find(need.name);
            if(it == n.stocks.end() || it->second < need.qty){ ok = false; break; }
        }
        if(ok) bestDelay = std::min(bestDelay, p.delay);
    }
    if(bestDelay != INT32_MAX) h += bestDelay; // one optimistic step further

    return h;
}

///< @brief Comparator for nodes in the beam search, sorts by score in descending order
struct NodeCmp{ bool operator()(const Node& a,const Node& b) const { return a.score > b.score; } };

int beam_search(const Config &cfg, int beam_width, int max_iter) {
    Node root;
    root.stocks = cfg.initialStocks;
    std::vector<Node> beam{root};
    const auto &procs = cfg.processes;

    for(int it = 0; it < max_iter && !beam.empty(); ++it) {
        std::vector<Node> successors;
        for(const Node& stateRef : beam){
            Node state = stateRef;
            realise_finishes(state, procs);

            bool runnable=false;
            for(size_t id = 0; id < procs.size(); ++id) {
                if(needs_satisfied(state.stocks, procs[id])) {
                    runnable=true;
                    break;
                }
            }

            if(!runnable && state.running.empty())
                return state.cycle;

            // Launch runnable process
            for(size_t id = 0; id < procs.size(); ++id){
                const Process &p=procs[id];
                if(!needs_satisfied(state.stocks,p)) continue;
                Node child = state;
                apply_items(child.stocks, p.needs, -1);
                child.running.push({child.cycle + p.delay, static_cast<int>(id)});
                child.score = child.cycle + heuristic(child, procs);
                child.parent = std::make_shared<Node>(stateRef);
                successors.emplace_back(std::move(child));
            }

            // Child that just waits to next completion
            if(!state.running.empty()){
                Node wait=state;
                wait.cycle = state.running.top().finish;
                wait.score = wait.cycle + heuristic(wait, procs);
                wait.parent = std::make_shared<Node>(stateRef);
                successors.emplace_back(std::move(wait));
            }
        }

        if (successors.empty())
            break;

        std::sort(successors.begin(), successors.end(), NodeCmp{});
        if(static_cast<int>(successors.size()) > beam_width)
            successors.resize(beam_width);
        beam.swap(successors);
    }

    return -1;
}
