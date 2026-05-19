#pragma once
#include "DataPoint.hpp"
#include <string>
#include <string_view>
#include <vector>
#include <mutex>

namespace sysmetrics {
	class MetricSeries {
	private:
		std::string m_name;
		std::vector<DataPoint> m_data;

		mutable std::mutex m_mutex;

	public:
		explicit MetricSeries(std::string_view name);
		void addDataPoint(double value);

		[[nodiscard]] std::string_view getName() const;
		[[nodiscard]] size_t size() const;
		[[nodiscard]] std::vector<DataPoint> getSnapshot() const;

		void saveToFile(std::string_view directory) const;
		void loadFromFile(std::string_view directory);
	};
}