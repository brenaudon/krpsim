/*!
 *  @file genetic_algo.hpp
 *  @brief Header file for the genetic algorithm used in the krpsim simulation
 *
 * This file defines the structures and functions for implementing a genetic algorithm
 * to find a near-optimal trace for the krpsim resource management simulation.
 * It includes the definition of the Candidate structure, which represents a state in the simulation,
 * as well as the solve_with_ga function that performs the genetic algorithm search.
 * The function returns a Candidate object containing the best trace found during the search.
 */

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
#include <optional>


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
    RunningProcess() = default;
    RunningProcess(int f, int i) : finish(f), id(i) {}
};


///< @brief Running process queue in the simulation
using RunPQ = std::priority_queue<RunningProcess, std::vector<RunningProcess>, std::greater<RunningProcess>>;


///< @brief Candidate in the genetic algorithm
struct Candidate {
    int                     cycle{};        ///< current cycle in the simulation, initially 0
    std::vector<int>        stocks_by_id;   ///< current stock of items, indexed by item ID
    RunPQ                   running;        ///< running processes in the simulation, ordered by finish time
    std::vector<TraceEntry> trace;          ///< trace of launch events leading to this node
};


/*!
 * @brief Genetic‑algorithm search for a near‑optimal krpsim trace.
 * @param cfg           Parsed configuration
 * @param timeBudgetMs  Wall‑clock budget granted by the grader (argv[2] in subject)
 * @return Vector of launch events sorted by increasing cycle, ready to print
 */
Candidate solve_with_ga(const Config &cfg, long timeBudgetMs);

#endif
