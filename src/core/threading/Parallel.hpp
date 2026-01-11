#pragma once
#include <vector>
#include <algorithm>
#include <thread>
#include <taskflow/taskflow.hpp>
#include <taskflow/algorithm/for_each.hpp> // Explicitly include algorithms

namespace utils {

// Wrapper around tf::Taskflow::for_each to provide a consistent interface
// and leverage Taskflow's work-stealing scheduler.
template <typename Iterator, typename Func>
void parallel_for_each(tf::Taskflow& tf, Iterator begin, Iterator end, Func&& func) {
    // tf::Taskflow::for_each automatically handles partitioning and load balancing.
    // It requires Random Access Iterators (which std::vector iterators are).
    tf.for_each(begin, end, std::forward<Func>(func));
}

} // namespace utils
