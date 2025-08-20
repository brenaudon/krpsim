/*!
 *  @file compute_GA.cpp
 *  @brief Implementation of the genetic algorithm for krpsim resource management simulation
 *
 *  This file implements the genetic algorithm used to find a near-optimal trace for the krpsim resource management simulation.
 *  It includes the necessary headers and defines the `solve_with_ga` function, which performs the simulation based on the provided configuration.
 *  The function returns a vector of `TraceEntry` objects, each representing a launch event
 *  with a cycle and the name of the process that starts at that cycle.
 */

#include "genetic_algo.hpp"

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>


/**
 * @brief Parameters for the genetic algorithm.
 *
 * This struct contains parameters that control the behavior of the genetic algorithm,
 * such as the maximum number of iterations, population size, mutation rate, and weights for the fitness function.
 */
struct GeneticParameters {
    int maxIter = 1000;         ///< Maximum number of iterations for the genetic algorithm
    int populationSize = 100;   ///< Size of the population in the genetic algorithm
    int maxCycles = 50000;      ///< Maximum number of cycles to run the simulation
    double mutationRate = 10.0;  ///< Percentage (0-100) of mutation in the genetic algorithm
    double score_alpha = 1.0;   ///< Weight for the target stock in the fitness function
    double score_beta = 0.1;    ///< Weight for the other stocks in the fitness function
    double score_decay = 0.7;   ///< Decay factor for the other
};


/**
 * @brief Function to delete processes that produce items with stocks exceeding configured limits.
 *
 * This function iterates through the runnable list of processes and removes those
 * that produce stocks exceeding the configured limits, based on the candidate's current stock state.
 *
 * @param runnable_list The list of process IDs that are currently runnable.
 * @param is_runnable A vector indicating whether each process is runnable.
 * @param cfg The configuration containing the maximum stock limits.
 * @param candidate The current candidate containing stock information.
 */
void delete_high_stock_processes(std::vector<int>& runnable_list,
                                 std::vector<bool>& is_runnable,
                                 const Config& cfg,
                                 const Candidate& candidate)
{
    if (cfg.maxStocks.limiting_item.empty())
        return;
    if (runnable_list.empty() || (runnable_list.size() == 1 && runnable_list[0] == -1)) {
        return; // nothing to do
    }

    const int item_count = static_cast<int>(cfg.item_to_id.size());
    const bool factors_mode = (cfg.maxStocks.limiting_initial_stock == -1);
    std::vector<bool> over;
    over.assign(item_count, false);

    // limiting stock for factor caps
    const int limiting_stock =
        (cfg.maxStocks.limiting_initial_stock != -1)
            ? cfg.maxStocks.limiting_initial_stock
            : (!cfg.maxStocks.limiting_item.empty()
               ? candidate.stocks_by_id[cfg.item_to_id.at(cfg.maxStocks.limiting_item)]
               : 0);

    // mark overfull items
    for (int i = 0; i < item_count; ++i) {
        const int current_stock = candidate.stocks_by_id[i];
        const int stock_cap = cfg.maxStocks.abs_cap_by_id[i]; // -1 => no cap
        const double stock_factor = cfg.maxStocks.factor_by_id[i]; // -1 => no cap

        bool too_much = false;
        if (!factors_mode && stock_cap >= 0 && current_stock > stock_cap)
            too_much = true;
        if (factors_mode && stock_factor >= 0.0 && current_stock > limiting_stock * stock_factor)
            too_much = true;
        over[i] = too_much;
    }

    // helper lambda to check if a process can be dropped
    auto drop = [&](int pid) {
        if (pid < 0)
            return false; // keep wait sentinel
        const Process& p = cfg.processes[pid];
        if (p.results_by_id.empty())
            return false;
        for (auto [id, _] : p.results_by_id)
            if (!over[id])
                return false;
        return true;
    };

    // drop processes for which all results are overfull
    int non_wait_runnable = -1;
    for (auto it = runnable_list.begin(); it != runnable_list.end();) {
        if (*it < 0) {
            ++it; // skip wait sentinel
            continue;
        }
        if (non_wait_runnable == -1 && is_runnable[*it]) {
            non_wait_runnable = *it; // remember the first runnable process
        }
        if (drop(*it)) {
            is_runnable[*it] = false; // mark as not runnable
            it = runnable_list.erase(it); // remove from runnable list
        } else {
            ++it; // keep in runnable list
        }
    }

    // if we have no running processes and no runnable processes, we can add back the first non-wait runnable process
    if (candidate.running.empty() && non_wait_runnable != -1 && (runnable_list.empty() || (runnable_list.size() == 1 && runnable_list[0] == -1))) {
        if (!is_runnable[non_wait_runnable]) {
            is_runnable[non_wait_runnable] = true; // mark as runnable
            runnable_list.push_back(non_wait_runnable);
        }
    }

}


/**
 * @brief Function to apply a process to the candidate.
 *
 * @param candidate The candidate to modify.
 * @param cfg The configuration containing the processes.
 * @param proc_id The ID of the process to apply, or -1 to wait for the next running process.
 * @param missing A vector tracking how many required items each process is missing.
 * @param runnable A vector of process IDs that are currently runnable.
 * @param is_runnable A vector indicating whether each process is runnable.
 */
void apply_process(Candidate &candidate, const Config& cfg, int proc_id, std::vector<int>& missing, std::vector<int>& runnable, std::vector<bool>& is_runnable) {
    // Helpers
    auto add_runnable = [&](int pid) {
        if (!is_runnable[pid] && missing[pid] == 0) {
            is_runnable[pid] = true; // mark as runnable
            runnable.push_back(pid);
        }
    };
    auto remove_runnable = [&](int pid) {
        if (is_runnable[pid]) {
            is_runnable[pid] = false;
            runnable.erase(std::remove(runnable.begin(), runnable.end(), pid), runnable.end());
        }
    };
    auto on_stock_increase = [&](int item_id, int old_val, int new_val) {
        if (new_val <= old_val) return;
        for (auto [pid, need_q] : cfg.needers_by_item[item_id]) {
            if (old_val < need_q && new_val >= need_q) {
                if (--missing[pid] == 0)
                    add_runnable(pid);
            }
        }
    };
    auto on_stock_decrease = [&](int item_id, int old_val, int new_val) {
        if (new_val >= old_val) return;
        for (auto [pid, need_q] : cfg.needers_by_item[item_id]) {
            if (old_val >= need_q && new_val < need_q) {
                if (missing[pid]++ == 0)
                    remove_runnable(pid);
            }
        }
    };

    if (proc_id == -1) {
        // If -1 is selected, just wait for the next running process to finish
        if (!candidate.running.empty()) {
            candidate.cycle = candidate.running.top().finish;
            while (!candidate.running.empty() && candidate.running.top().finish <= candidate.cycle) {
                const int pid = candidate.running.top().id;
                candidate.running.pop();
                for(const auto &[id, qty] : cfg.processes[pid].results_by_id) {
                    const int before = candidate.stocks_by_id[id];
                    candidate.stocks_by_id[id] += qty;
                    on_stock_increase(id, before, candidate.stocks_by_id[id]);
                }
            }
        }
        return;
    }
    const Process &proc = cfg.processes[proc_id];

    // Launch the process
    candidate.running.emplace(candidate.cycle + proc.delay, proc_id);
    for (auto [id, qty] : proc.needs_by_id) {
        const int before = candidate.stocks_by_id[id];
        candidate.stocks_by_id[id] -= qty;
        on_stock_decrease(id, before, candidate.stocks_by_id[id]);
    }
    candidate.trace.push_back({candidate.cycle, proc_id});

}


/**
 * @brief Function to generate a child candidate from two parents.
 *
 * @param cfg The configuration containing the processes and initial stocks.
 * @param params The genetic parameters for the algorithm.
 * @param parent1 The first parent candidate.
 * @param parent2 The second parent candidate.
 * @return A new child candidate generated from the parents.
 */
Candidate generate_child(const Config &cfg, const GeneticParameters &params, std::optional<Candidate> parent1 = std::nullopt, std::optional<Candidate> parent2 = std::nullopt) {
    Candidate child;
    child.cycle = 0;
    child.stocks_by_id.assign(cfg.item_to_id.size(), 0);
    for (auto& [name, qty] : cfg.initialStocks)
        child.stocks_by_id[cfg.item_to_id.at(name)] = qty;
    child.trace.clear();
    child.running = RunPQ();

    const int process_count = static_cast<int>(cfg.processes.size());
    std::vector<int> missing;
    std::vector<int> runnable;
    std::vector<bool> is_runnable; // keep track of processes in runnable list

    missing.assign(process_count, 0);
    is_runnable.assign(process_count, false);
    runnable.clear();
    runnable.reserve(process_count + 1); // +1 for the special -1 ID for waiting next running processes

    for (int pid = 0; pid < process_count; ++pid) {
        const auto& proc = cfg.processes[pid];
        for (auto [id, qty] : proc.needs_by_id)
            if (child.stocks_by_id[id] < qty)
                ++missing[pid];
        if (missing[pid] == 0) {
            runnable.push_back(pid);
            is_runnable[pid] = true; // mark as runnable
        }
    }
    runnable.push_back(-1); // wait
    delete_high_stock_processes(runnable, is_runnable, cfg, child);

    int i = 0;

    int parent1_size = 0;
    int parent2_size = 0;
    if (parent1.has_value()) {
        parent1_size = static_cast<int>(parent1.value().trace.size());
    }
    if (parent2.has_value()) {
        parent2_size = static_cast<int>(parent2.value().trace.size());
    }

    while (child.cycle < params.maxCycles) {
        if ((runnable.empty() || (runnable.size() == 1 && runnable[0] == -1)) && child.running.empty()) {
            break; // No more runnable or running processes, exit the loop
        }

        int first_cycle_process = -1;
        for (auto it = runnable.begin(); it != runnable.end(); ) {
            if (*it != -1 && cfg.processes[*it].in_cycle == true) {
                if (first_cycle_process == -1)
                    first_cycle_process = *it;
                is_runnable[*it] = false;
                runnable.erase(it); // Remove processes that are in a cycle
            } else {
                ++it; // Only increment if we didn't erase an element
            }
        }
        if (first_cycle_process != -1 && (runnable.empty() || (runnable.size() == 1 && runnable[0] == -1 && child.running.empty()))) {
            runnable.push_back(first_cycle_process); // Re-add the first cycle process to the end of the runnable list
            is_runnable[first_cycle_process] = true; // Mark it as runnable again
        }

        int random_choice = rand() % 100; // Randomly choose between parent1 action, parent2 action and mutation

        // parent1.trace[i].procId in runnable_list
        if (i < parent1_size // Check if i is within bounds
            && is_runnable[parent1.value().trace[i].procId]
            && random_choice < 100 - params.mutationRate / 2) // check if we should use parent1
        {
            apply_process(child, cfg, parent1.value().trace[i].procId, missing, runnable, is_runnable);
        } else if (i < parent2_size
            && is_runnable[parent2.value().trace[i].procId]
            && !(random_choice > 100 - params.mutationRate / 2))
        {
            apply_process(child, cfg, parent2.value().trace[i].procId, missing, runnable, is_runnable);
        } else { // mutate means random choice in runnable processes. Mutate if random_choice is greater than 100 - mutationRate or if parent_1 and parent_2 process at i are not runnable
            int proc_id = runnable[rand() % runnable.size()];
            apply_process(child, cfg, proc_id, missing, runnable, is_runnable);
        }

        const int missing_size = static_cast<int>(missing.size());
        for (int j = 0; j < missing_size; ++j) {
            if (!is_runnable[j] && missing[j] == 0) {
                is_runnable[j] = true; // Mark as runnable if no more missing items
                runnable.push_back(j);
            }
        }
        delete_high_stock_processes(runnable, is_runnable, cfg, child);
        ++i;
    }
    return child;
}


/**
 * @brief Function to generate a random candidate.
 *
 * @param cfg The configuration containing the processes and initial stocks.
 * @param params The genetic parameters for the algorithm.
 * @return A new candidate with a fully random trace.
 */
Candidate generate_candidate(const Config &cfg, const GeneticParameters &params) {
    return generate_child(cfg, params);
}


/**
 * @brief Function to score a candidate based on the configuration and genetic parameters.
 *
 * @param candidate The candidate to score.
 * @param cfg The configuration containing the optimization keys and distance map.
 * @param params The genetic parameters for scoring.
 * @return An integer score for the candidate.
 */
int score_candidate(const Candidate &candidate, const Config &cfg, const GeneticParameters &params) {

    std::string target;

    if (cfg.optimizeKeys.size() == 1 && cfg.optimizeKeys[0] == "time") {
        if (candidate.cycle == 0) {
            return 100000; // No running processes, return max score
        }
        return 100000 / candidate.cycle;
    }

    for (const auto& key : cfg.optimizeKeys) {
        if (key != "time") {
            target = key;
            break; // We only need one target for scoring
        }
    }

    const int inf = 1000000; // Arbitrary large value for unreachable stocks
    const double targetQty = candidate.stocks_by_id[cfg.item_to_id.at(target)];

    double interm = 0.0;
    for (size_t i = 0; i < candidate.stocks_by_id.size(); ++i) {
        std::string s = cfg.id_to_item[i];
        const int qty = candidate.stocks_by_id[i];
        if (s == target || qty <= 0) continue;
        auto it = cfg.dist.find(s);

        if (it == cfg.dist.end() || it->second >= inf) continue; // unreachable → no credit
        const double w = std::pow(params.score_decay, it->second);
        interm += w * qty;
    }
    return params.score_alpha * targetQty + params.score_beta * interm;
}


Candidate solve_with_ga(const Config &cfg, long timeBudgetMs){
    GeneticParameters params;
    Candidate best_candidate;
    best_candidate.stocks_by_id.assign(cfg.item_to_id.size(), 0);
    for (auto& [name, qty] : cfg.initialStocks)
        best_candidate.stocks_by_id[cfg.item_to_id.at(name)] = qty;

    // get start time
    auto start_time = std::chrono::steady_clock::now();

    srand(start_time.time_since_epoch().count());

    std::vector<Candidate> candidates;
    for (int i = 0; i < params.populationSize; ++i) {
        auto current_time = std::chrono::steady_clock::now();
        auto elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time).count();
        if (elapsed_time > timeBudgetMs) {
            break;
        }
        //std::cout << "Generating candidate " << i + 1 << " of " << params.populationSize << std::endl;
        candidates.push_back(generate_candidate(cfg, params));
    }

    for (int i = 0; i < params.maxIter; ++i) {
        //std::cout << "Iteration " << i + 1 << " of " << params.maxIter << std::endl;
        // check if we reached the time budget
        auto current_time = std::chrono::steady_clock::now();
        auto elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time).count();
        if (elapsed_time > timeBudgetMs) {
            break;
        }

        // Sort candidates by score
        std::sort(candidates.begin(), candidates.end(), [&](const Candidate &a, const Candidate &b) {
            const int score_a = score_candidate(a, cfg, params);
            const int score_b = score_candidate(b, cfg, params);
            if (score_a == score_b) {
                return a.cycle < b.cycle;
            }
            return score_a > score_b;
        });

        Candidate parent1 = candidates[0];
        Candidate parent2 = candidates[1];

        if (score_candidate(parent1, cfg, params) > score_candidate(best_candidate, cfg, params)) {
            best_candidate = parent1; // Update the best candidate if we found a better one
        }

        candidates.clear();

        // Generate new candidates by crossing over the best ones
        size_t pop_size = static_cast<size_t>(params.populationSize);
        while (candidates.size() < pop_size / 2) {
            auto current_time_ = std::chrono::steady_clock::now();
            auto elapsed_time_ = std::chrono::duration_cast<std::chrono::milliseconds>(current_time_ - start_time).count();
            if (elapsed_time_ > timeBudgetMs) {
                break;
            }
            candidates.push_back(generate_child(cfg, params, parent1, parent2));
        }
        while (candidates.size() < pop_size) {
            auto current_time_ = std::chrono::steady_clock::now();
            auto elapsed_time_ = std::chrono::duration_cast<std::chrono::milliseconds>(current_time_ - start_time).count();
            if (elapsed_time_ > timeBudgetMs) {
                break;
            }
            candidates.push_back(generate_candidate(cfg, params)); // Fill the rest with random candidates
        }
    }

    return best_candidate;

}