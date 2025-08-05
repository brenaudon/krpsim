#ifndef COMPUTE_GA_HPP
#define COMPUTE_GA_HPP

#include "krpsim.hpp"     // Config, Process, Item  (+ <vector>/<string>)
#include <vector>
#include <string>
#include <unordered_map>
#include <queue>
#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <chrono>


///< @brief Represents a launch event in the simulation, containing a cycle and the ID of the process that starts at that cycle.
struct TraceEntry {
    long cycle; ///< launch time (cycle)
    int procId; ///< ID of the process that starts at that cycle
};


///< @brief Running process in the simulation
struct RunningProcess {
    int finish; ///< finish time of the process
    int id;     ///< process ID

    bool operator>(const RunningProcess& o) const noexcept { return finish > o.finish; } ///< comparison operator for priority queue
};


///< @brief Running process queue in the simulation
using RunPQ = std::priority_queue<RunningProcess, std::vector<RunningProcess>, std::greater<RunningProcess>>;


///< @brief Candidate in the genetic algorithm
struct Candidate {
    int                                     cycle{};    ///< current cycle in the simulation, initially 0
    std::unordered_map<std::string, int>    stocks;     ///< current stock of items, keyed by item name
    RunPQ                                   running;    ///< running processes in the simulation, ordered by finish time
    std::vector<TraceEntry>                 trace;      ///< trace of launch events leading to this node
};


/*!
 * @brief Genetic‑algorithm search for a near‑optimal krpsim trace.
 * @param cfg           Parsed configuration
 * @param timeBudgetMs  Wall‑clock budget granted by the grader (argv[2] in subject)
 * @return Vector of launch events sorted by increasing cycle, ready to print
 */
Candidate solve_with_ga(const Config &cfg, long timeBudgetMs);

#endif
