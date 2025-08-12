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
 * @brief Function to apply stock changes based on items.
 *
 * @param stocks_by_id The current stock of items, indexed by item ID.
 * @param items_by_id A vector of pairs, where each pair contains an item ID and its quantity.
 * @param sign The sign to apply to the quantities (positive for adding, negative for removing).
 */
static void apply_items(std::vector<int>& stocks_by_id, const std::vector<std::pair<int,int>>& items_by_id, const int sign){
    for(const auto &[id, qty] : items_by_id)
        stocks_by_id[id] += sign * qty;
}


/**
 * @brief Function to realise finished processes in a node based on the current cycle value.
 *
 * @param c The candidate containing the running processes and current cycle.
 * @param procs The vector of processes, used to apply results when a process finishes.
 */
static void realise_finishes(Candidate &c, const std::vector<Process>& procs) {
    while (!c.running.empty() && c.running.top().finish <= c.cycle) {
        const int id = c.running.top().id;
        c.running.pop();
        apply_items(c.stocks_by_id, procs[id].results_by_id, +1);
    }
}


/**
 * @brief Function to check if all needs of a process are satisfied by the current stock.
 *
 * @param stocks_by_id The current stock of items, indexed by item ID.
 * @param proc The process to check, containing its needs.
 */
static bool needs_satisfied(const std::vector<int>& stocks_by_id, const Process& proc) {
    for(auto [id, qty] : proc.needs_by_id) {
        if(stocks_by_id[id] < qty)
            return false;
    }
    return true;
}


void delete_high_stock_processes(std::vector<int>& runnable_list,
                                 const Config& cfg,
                                 const std::vector<int>& stocks_by_id)
{
    const int N = static_cast<int>(cfg.item_to_id.size());
    const bool factors_mode = (cfg.maxStocks.limiting_initial_stock == -1);
    static thread_local std::vector<uint8_t> over;   // reused scratch
    over.assign(N, 0);

    // limiting stock for factor caps
    const int limiting_stock =
        (cfg.maxStocks.limiting_initial_stock != -1)
            ? cfg.maxStocks.limiting_initial_stock
            : (!cfg.maxStocks.limiting_item.empty()
               ? stocks_by_id[cfg.item_to_id.at(cfg.maxStocks.limiting_item)]
               : 0);

    // mark overfull items
    for (int i = 0; i < N; ++i) {
        const int   cur   = stocks_by_id[i];
        const int   cap   = cfg.maxStocks.abs_cap_by_id[i];     // -1 => no cap
        const double fac  = cfg.maxStocks.factor_by_id[i];      // -1 => no cap

        bool too_much = false;
        if (!factors_mode && cap >= 0 && cur > cap)
            too_much = true;
        if (factors_mode && fac >= 0.0 && cur > limiting_stock * fac)
            too_much = true;
        over[i] = too_much ? 1 : 0;
    }

    // erase_if: drop processes whose *all* results are overfull
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

    runnable_list.erase(std::remove_if(runnable_list.begin(), runnable_list.end(), drop),runnable_list.end());
}



/**
 * @brief Function to apply a process to the candidate.
 *
 * @param candidate The candidate to modify.
 * @param cfg The configuration containing the processes.
 * @param proc_id The ID of the process to apply, or -1 to wait for the next running process.
 */
void apply_process(Candidate &candidate, const Config& cfg, int proc_id) {
    if (proc_id == -1) {
        // If -1 is selected, just wait for the next running process to finish
        if (!candidate.running.empty()) {
            candidate.cycle = candidate.running.top().finish;
            realise_finishes(candidate, cfg.processes);
        }
        return;
    }
    const Process &proc = cfg.processes[proc_id];

    // Launch the process
    candidate.running.emplace(candidate.cycle + proc.delay, proc_id);
    apply_items(candidate.stocks_by_id, proc.needs_by_id, -1);
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

    int i = 0;
    // std::string target;
    // for (const auto& key : cfg.optimizeKeys) {
    //     if (key != "time") {
    //         target = key;
    //         break; // We only need one target for the child
    //     }
    // }

    // int longest_proc_delay = 0;
    // for (const auto& proc : cfg.processes) {
    //     auto it = std::find_if(proc.results.begin(), proc.results.end(),
    //                              [&target](const Item &item) { return item.name == target; });
    //     if (it != proc.results.end() && proc.delay > longest_proc_delay) {
    //         longest_proc_delay = proc.delay;
    //     }
    // }

    int parent1_size = 0;
    int parent2_size = 0;
    if (parent1.has_value()) {
        parent1_size = static_cast<int>(parent1.value().trace.size());
    }
    if (parent2.has_value()) {
        parent2_size = static_cast<int>(parent2.value().trace.size());
    }

    int processes_size = static_cast<int>(cfg.processes.size());
    std::vector<int> runnable_list;
    runnable_list.reserve(processes_size + 1); // +1 for the special -1 ID for waiting next running processes

    while (child.cycle < params.maxCycles) {
        // Check if there are any runnable processes
        runnable_list.clear();

        for (int id = 0; id < processes_size; ++id) {
            if (needs_satisfied(child.stocks_by_id, cfg.processes[id])) {
                runnable_list.push_back(id);
            }
        }

        if (!child.running.empty()) {
            runnable_list.push_back(-1); // Add a special ID for waiting next running processes
        }

        if (runnable_list.empty() && child.running.empty()) {
            break; // No more runnable processes, exit the loop
        }

        for (auto it = runnable_list.begin(); it != runnable_list.end(); ) {
            if (*it != -1 && cfg.processes[*it].in_cycle == true) {
                runnable_list.erase(it); // Remove processes that are in a cycle
            } else {
                ++it; // Only increment if we didn't erase an element
            }
        }

        delete_high_stock_processes(runnable_list, cfg, child.stocks_by_id);

        // if (child.cycle >= params.maxCycles - longest_proc_delay) {
        //     std::vector<int> runnable_list_copy = runnable_list;
        //     for (int proc_id : runnable_list_copy) {
        //         if (proc_id != -1) {
        //             auto it = std::find_if(cfg.processes[proc_id].results.begin(), cfg.processes[proc_id].results.end(),
        //                          [&target](const Item &item) { return item.name == target; });
        //             if (it == cfg.processes[proc_id].results.end()) {
        //                 runnable_list.erase(std::remove(runnable_list.begin(), runnable_list.end(), proc_id), runnable_list.end());
        //             }
        //         }
        //     }
        //     runnable_list.erase(std::remove(runnable_list.begin(), runnable_list.end(), -1), runnable_list.end());
        //     if (runnable_list.empty()) {
        //         runnable_list = runnable_list_copy;
        //     }
        // }

        int random_choice = rand() % 100; // Randomly choose between parent1, parent2 and mutation

        // find parent1.trace[i].procId in runnable_list
        if (i < parent1_size // Check if i is within bounds
            && std::find(runnable_list.begin(), runnable_list.end(), parent1.value().trace[i].procId) != runnable_list.end() // Check if parent1.trace[i].procId is runnable
            && random_choice < 100 - params.mutationRate / 2) // check if we should use parent1
        {
            apply_process(child, cfg, parent1.value().trace[i].procId);
        } else if (i < parent2_size
            && std::find(runnable_list.begin(), runnable_list.end(), parent2.value().trace[i].procId) != runnable_list.end()
            && !(random_choice > 100 - params.mutationRate / 2))
        {
            apply_process(child, cfg, parent2.value().trace[i].procId);
        } else { // mutate means random choice in runnable processes. Mutate if random_choice is greater than 100 - mutationRate or if parent_1 and parent_2 process at i are not runnable
            int proc_id = runnable_list[rand() % runnable_list.size()];
            apply_process(child, cfg, proc_id);
        }
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

    const int INF = 1000000; // Arbitrary large value for unreachable stocks
    const double targetQty = candidate.stocks_by_id[cfg.item_to_id.at(target)];

    double interm = 0.0;
    for (size_t i = 0; i < candidate.stocks_by_id.size(); ++i) {
        std::string s = cfg.id_to_item[i];
        int qty = candidate.stocks_by_id[i];
        if (s == target || qty <= 0) continue;
        auto it = cfg.dist.find(s);

        if (it == cfg.dist.end() || it->second >= INF) continue; // unreachable → no credit
        double w = std::pow(params.score_decay, it->second);
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
        std::cout << "Generating candidate " << i + 1 << " of " << params.populationSize << std::endl;
        candidates.push_back(generate_candidate(cfg, params));
    }

    for (int i = 0; i < params.maxIter; ++i) {
        // check if we reached the time budget
        auto current_time = std::chrono::steady_clock::now();
        auto elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time).count();
        if (elapsed_time > timeBudgetMs) {
            break;
        }

        // Sort candidates by score
        std::sort(candidates.begin(), candidates.end(), [&](const Candidate &a, const Candidate &b) {
            int score_a = score_candidate(a, cfg, params);
            int score_b = score_candidate(b, cfg, params);
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