/**
 * @file krpsim_verif.hpp
 * @brief Header file for verification functions in KRPSim.
 *
 * This file contains declarations for functions that verify the correctness of process launched in the trace.
 */

#ifndef KRPSIM_KRPSIM_VERIF_HPP
#define KRPSIM_KRPSIM_VERIF_HPP

#include <vector>
#include <queue>

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

#endif