/*!
 *  @file parsing.cpp
 *  @brief Implementation of the configuration parsing functionality.
 *
 *  This file contains the implementation of functions to parse a configuration
 *  file for a resource management system.
 */


#include "helper.hpp"
#include "parsing.hpp"

/**
 * @brief Parse a single item from a string in the format "name:qty".
 *
 * @param token The string to parse.
 * @return An Item struct containing the name and quantity.
 * @throws std::runtime_error if the format is incorrect.
 */
static Item parse_item(std::string token) {
    auto colon = token.find(':');
    if (colon == std::string::npos)
        throw std::runtime_error("Bad item (no colon): '" + std::string(token) + "'");

    std::string name_sv = trim(token.substr(0, colon));
    if (name_sv.empty())
        throw std::runtime_error("Bad item (empty name): '" + std::string(token) + "'");

    std::string qty_sv = trim(token.substr(colon + 1));
    int qty = 0;
    auto [ptr, ec] = std::from_chars(qty_sv.data(), qty_sv.data() + qty_sv.size(), qty);
    if (ec != std::errc{})
        throw std::runtime_error("Bad item (qty): '" + std::string(token) + "'");

    return {std::string(name_sv), qty};
}

/**
 * @brief Parse a list of items from a string, where items are separated by semicolons.
 *
 * @param list The string containing the list of items.
 * @return A vector of Item structs parsed from the string.
 */
static std::vector<Item> parse_item_list(std::string list) {
    std::vector<Item> items;
    size_t start = 0;
    while (start < list.size()) {
        size_t end = list.find(';', start);
        std::string tok = (end == std::string::npos) ? list.substr(start) : list.substr(start, end - start);
        tok = trim(tok);
        if (!tok.empty()) items.push_back(parse_item(tok));
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return items;
}

///< @brief Namespace for regex patterns used in parsing.
namespace detail {
    const std::regex re_stock(R"(^\s*([^:#\s]+)\s*:\s*(\d+)\s*$)");
    const std::regex re_process(R"(^\s*([^:]+?)\s*:\s*\(([^)]*)\)\s*:\s*(?:\(([^)]*)\))?\s*:\s*(\d+)\s*$)");
    const std::regex re_optimize(R"(^\s*optimize\s*:\s*\(([^)]*)\)\s*$)", std::regex::icase);
}

static std::unordered_map<std::string,double>
build_reward_weights(const std::vector<Process>             &processes,
                     const std::vector<std::string>         &optimizeKeys,
                     double goalWeight, double minPrecursorWeight, double decay)
{
    // Map stock -> producer processes
    std::unordered_map<std::string, std::vector<const Process*>> producers;
    for (const Process &p : processes)
        for (const Item &out : p.results)
            producers[out.name].push_back(&p);

    std::unordered_map<std::string,double> weight;
    std::queue<std::pair<std::string,double>> q;

    // Seed with goal stocks
    for (const std::string &g : optimizeKeys) {
        weight[g] = goalWeight;
        q.push({g, goalWeight});
    }

    // BFS
    while (!q.empty()) {
        auto [stock, w] = q.front(); q.pop();
        double nextW = std::max(minPrecursorWeight, w * decay);
        if (nextW < minPrecursorWeight) continue; // nothing new to add

        for (const Process *producer : producers[stock]) {
            for (const Item &need : producer->needs) {
                auto it = weight.find(need.name);
                if (it == weight.end() || it->second < nextW) {
                    weight[need.name] = nextW * need.qty / producer->results[0].qty; // scale by output
                    q.push({need.name, nextW});
                }
            }
        }
    }

    return weight;
}


Config parse_config(std::istream &in) {
    Config cfg;
    std::string line;
    size_t lineno = 0;

    enum class Section { STOCKS, PROCESSES, OPTIMIZE } section = Section::STOCKS;

    while (std::getline(in, line)) {
        ++lineno;
        std::string trimmed(line);
        trimmed = std::string(trim(trimmed));
        if (trimmed.empty() || trimmed.front() == '#') continue;

        std::smatch m;
        switch (section) {
            case Section::STOCKS:
                if (std::regex_match(trimmed, m, detail::re_stock)) {
                    cfg.initialStocks.emplace(m[1].str(), std::stoll(m[2].str()));
                    break;
                } else if (std::regex_match(trimmed, m, detail::re_process)) {
                    section = Section::PROCESSES; // fall‑through to parse as process
                } else {
                    throw std::runtime_error("Expected stock or process at line " + std::to_string(lineno));
                }
            case Section::PROCESSES:
                if (std::regex_match(trimmed, m, detail::re_process)) {
                    Process p;
                    p.name = trim(m[1].str());
                    p.needs = parse_item_list(m[2].str());
                    p.results = parse_item_list(m[3].str());
                    p.delay = std::stoll(m[4].str());
                    cfg.processes.emplace_back(std::move(p));
                    break;
                } else if (std::regex_match(trimmed, m, detail::re_optimize)) {
                    section = Section::OPTIMIZE;
                } else {
                    throw std::runtime_error("Expected process or optimize at line " + std::to_string(lineno));
                }
            case Section::OPTIMIZE:
                if (std::regex_match(trimmed, m, detail::re_optimize)) {
                    std::string inside = m[1];
                    size_t start = 0;
                    while (start < inside.size()) {
                        size_t end = inside.find(';', start);
                        std::string tok{trim(inside.substr(start, end - start))};
                        if (!tok.empty()) cfg.optimizeKeys.push_back(tok);
                        if (end == std::string::npos) break;
                        start = end + 1;
                    }
                } else {
                    throw std::runtime_error("Unexpected content after optimize at line " + std::to_string(lineno));
                }
                break;
        }
    }

    if (cfg.optimizeKeys.empty())
        throw std::runtime_error("Missing optimize section");

    cfg.weights = build_reward_weights(cfg.processes, cfg.optimizeKeys, 10.0, 0.001, 0.1);

    // print weights
    for (const auto &[stock, weight] : cfg.weights) {
        std::cout << "Weight for '" << stock << "': " << weight << '\n';
    }

    return cfg;
}
