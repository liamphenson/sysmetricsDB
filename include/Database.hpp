#pragma once
#include "MetricSeries.hpp"
#include <unordered_map>
#include <memory>
#include <string>
#include <string_view>

namespace sysmetrics {
	class Database {
		private:
			std::unordered_map<std::string, std::unique_ptr<MetricSeries>> m_metrics;

		public:
			Database() = default;
			~Database() = default;

			Database(const Database&) = delete;
			Database& operator=(const Database&) = delete;

			void createMetric(std::string_view name);

			MetricSeries* getMetric(std::string_view name);
	};
}