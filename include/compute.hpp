#ifndef COMPUTE_HPP
#define COMPUTE_HPP

#include "krpsim.hpp"
#include <queue>
#include <unordered_map>
#include <algorithm>
#include <memory>

int beam_search(const Config &cfg, int beam_width, int max_iter = 10000, double lambda = 0.01);

#endif
