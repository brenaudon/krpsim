#include "compute.hpp"

#include <functional>
#include <iostream>
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

static int heuristic(const Node &n, const std::vector<Process> &procs,
                     bool optimizeTime,
                     const std::vector<std::string> &rewardStocks,
                     const std::unordered_map<std::string,double> &weight,
                     double lambda) {
    if (optimizeTime) {
        int h = 0;
        if (!n.running.empty()) {
            h = std::max(0, n.running.top().finish - n.cycle);
        } else {
            int bestDelay = INT32_MAX;
            for (const Process &p : procs)
                if (needs_satisfied(n.stocks, p))
                    bestDelay = std::min(bestDelay, p.delay);
            if (bestDelay != INT32_MAX) h = bestDelay;
        }
        return h;
    }

    int reward = 0;
    for (const auto &k : rewardStocks) {
        auto it = n.stocks.find(k);
        if (it == n.stocks.end()) continue;
        auto wit = weight.find(k);
        double w = (wit == weight.end()) ? 1.0 : wit->second;
        reward += static_cast<int>(it->second * w);
    }
    return static_cast<int>(n.cycle * lambda - reward);
}

///< @brief Comparator for nodes in the beam search, sorts by score in descending order
struct NodeCmp{ bool operator()(const Node& a, const Node& b) const { return a.score < b.score; } };

int beam_search(const Config &cfg, int beam_width, int max_iter, double lambda) {
    const auto &procs = cfg.processes;

    lambda = 0.0;

    bool optimizeTime = (cfg.optimizeKeys.size()==1 && cfg.optimizeKeys[0]=="time");
    std::vector<std::string> rewardStocks;
    if(!optimizeTime) rewardStocks = cfg.optimizeKeys;

    Node root;
    root.stocks = cfg.initialStocks;
    root.score = heuristic(root, procs, optimizeTime, rewardStocks, cfg.weights, lambda);
    root.parent = nullptr; // root has no parent
    std::vector<Node> beam{root};


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

            if(!runnable && state.running.empty()) {
                if(optimizeTime) return state.cycle;  // makespan
                int stockScore=0;
                for(const auto &k: rewardStocks){ auto it=state.stocks.find(k); if(it!=state.stocks.end()) stockScore+=it->second; }
                std::cout << "Final stocks: " << stockScore << " (cycle=" << state.cycle << ")\n";
                return state.cycle; // returns cycles just for consistency
            }


            // Launch runnable process
            for(size_t id = 0; id < procs.size(); ++id){
                const Process &p=procs[id];
                if(!needs_satisfied(state.stocks,p)) continue;
                Node child = state;
                apply_items(child.stocks, p.needs, -1);
                child.running.push({child.cycle + p.delay, static_cast<int>(id)});
                child.score = child.cycle + heuristic(child, procs, optimizeTime, rewardStocks, cfg.weights, lambda);
                child.parent = std::make_shared<Node>(stateRef);
                successors.emplace_back(std::move(child));
            }

            // Child that just waits to next completion
            if(!state.running.empty()){
                Node wait=state;
                wait.cycle = state.running.top().finish;
                wait.score = wait.cycle + heuristic(wait, procs, optimizeTime, rewardStocks, cfg.weights, lambda);
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
