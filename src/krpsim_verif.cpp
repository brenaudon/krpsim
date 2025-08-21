/*!
 *  @file krpsim_verif.cpp
 *  @brief Main entry point for the krpsim_verif executable.
 *
 *  This file serves as the main entry point for the krpsim_verif executable, which
 *  parses a configuration file and the result file and runs the verification (check if process can be run
 *  when it is launch in the trace).
 */

#include "krpsim.hpp"
#include "parsing.hpp"
#include "krpsim_verif.hpp"

/**
 * @brief Resolves finished processes and updates stocks.
 *
 * This function checks the running processes and if any have finished by the current cycle,
 * it updates the stocks based on the results of those processes.
 * It pops the finished processes from the priority queue.
 *
 * @param cycle The current cycle in the simulation.
 * @param running_processes The priority queue of currently running processes.
 * @param stocks The current stocks of items, updated with results from finished processes.
 * @param cfg The configuration containing the processes and their results.
 */
void resolve_finished_processes(int cycle, RunPQ &running_processes,
                                std::unordered_map<std::string, int> &stocks,
                                const Config &cfg) {
    while (!running_processes.empty() && running_processes.top().finish <= cycle) {
        const RunningProcess &rp = running_processes.top();
        const Process &proc = cfg.processes[rp.id];
        for (const auto &result : proc.results) {
            stocks[result.name] += result.qty;
        }
        running_processes.pop();
    }
}


/**
 * @brief Main function for the krpsim_verif executable.
 *
 * This function reads the configuration file and the trace file, verifies the trace,
 * and prints the final stocks and cycle count.
 *
 * @param argc The number of command line arguments.
 * @param argv The command line arguments.
 * @return EXIT_SUCCESS on success, EXIT_FAILURE on error.
 */
int main(int argc, char **argv) {
    try {
        if (argc != 3) {
            std::cerr << "Usage: " << argv[0] << " <config_file> <result_to_test>\n";
            return EXIT_FAILURE;
        }

        std::ifstream in(argv[1]);
        if (!in) {
            std::cerr << "Cannot open " << argv[1] << "\n";
            return EXIT_FAILURE;
        }
        Config cfg = parse_config(in);


        std::ifstream trace_in(argv[2]);
        if (!trace_in) {
            std::cerr << "Cannot open trace " << argv[2] << ".\n";
            return EXIT_FAILURE;
        }

        //map process names to IDs
        std::unordered_map<std::string, int> proc_name_to_id;
        for (int i = 0; i < static_cast<int>(cfg.processes.size()); ++i) {
            proc_name_to_id[cfg.processes[i].name] = i;
        }

        std::unordered_map<std::string, int> stocks = cfg.initialStocks;

        const std::regex re_line(R"(^\s*(\d+)\s*:\s*([^:#\s]+)\s*$)");

        int cycle = 0;
        bool sim_started = false;
        RunPQ running_processes;

        std::string line;
        while (std::getline(trace_in, line)) {
            if (line.empty() || line[0] == '#') {
                continue; // skip empty lines and comments
            }
            std::smatch match;
            if (std::regex_match(line, match, re_line)) {
                cycle = std::stoi(match[1]);
                std::string proc_name = match[2];
                if (!sim_started) {
                    sim_started = true;
                }

                resolve_finished_processes(cycle, running_processes, stocks, cfg);

                // Check exists
                if (proc_name_to_id.find(proc_name) == proc_name_to_id.end()) {
                    std::cerr << "Process " << proc_name << " not found in configuration.\n";
                    return EXIT_FAILURE;
                }

                // Launch the process
                Process proc = cfg.processes[proc_name_to_id[proc_name]];
                running_processes.emplace(cycle + proc.delay, proc_name_to_id[proc_name]);
                for (const auto& need : proc.needs) {
                    stocks[need.name] -= need.qty;
                    if (stocks[need.name] < 0) {
                        std::cerr << "Insufficient stock of " << need.name << " to launch process " << proc_name
                                  << " at cycle " << cycle << ".\n";
                        return EXIT_FAILURE;
                    }
                }
            } else if (sim_started) {
                break;
            }
        }
        // Finish remaining processes
        while (!running_processes.empty()) {
            const RunningProcess &rp = running_processes.top();
            if (rp.finish > cycle) {
                cycle = rp.finish;
            }
            resolve_finished_processes(cycle, running_processes, stocks, cfg);
        }

        // print the trace file is valid
        std::cout << "\nTrace is valid.\n\nFinal cycle: " << cycle << "\n";
        std::cout << "\nFinal stocks:\n";
        for (const auto& [name, qty] : stocks) {
            std::cout << "  " << name << ": " << qty << "\n";
        }
        return EXIT_SUCCESS;

    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << "\n";
        return EXIT_FAILURE;
    }
}