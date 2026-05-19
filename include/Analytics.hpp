#pragma once
#include <numeric>
#include <algorithm>
#include <stdexcept>
#include <iterator>

namespace sysmetrics::analytics{
	template <typename ForwardIt>
	double calculateMean(ForwardIt first, ForwardIt last) {
		// Prevent division by zero
		if (first == last) {
			throw std::invalid_argument("Cannot calculate average of {an empty range.");
		}

		double sum = std::accumulate(first, last, 0.0, 
			[](double accumulator, const auto& dataPoint) {
				return accumulator + dataPoint.value;
			}
		);
		
		return sum / std::distance(first, last);
	}

	template <typename ForwardIt>
	double findMax(ForwardIt first, ForwardIt last) {
		if (first == last) {
			throw std::invalid_argument("Cannot find maximum of an empty range.");
		}

		auto max_it = std::max_element(first, last,
			[](const auto& a, const auto& b) {
				return a.value < b.value;
			}
		);

		return max_it->value;
	}
}