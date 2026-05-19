#pragma once
#include <chrono>
#include <ostream>

namespace sysmetrics {
	struct DataPoint {
		using TimePoint = std::chrono::system_clock::time_point;

		TimePoint timestamp;
		double value;

		auto operator <=> (const DataPoint&) const = default;

		friend std::ostream& operator<<(std::ostream& os, const DataPoint& dp) {
			auto epoch_ms = std::chrono::duration_cast<std::chrono::milliseconds>(dp.timestamp.time_since_epoch()).count();
			return os << epoch_ms << "," << dp.value;
		}
	};
}