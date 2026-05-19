#include "Database.hpp"

namespace sysmetrics {
	void Database::createMetric(std::string_view name) {
		std::string keyName{ name };

		if(m_metrics.find(keyName) == m_metrics.end()){
			m_metrics[keyName] = std::make_unique<MetricSeries>(name);
		}
	}

	MetricSeries* Database::getMetric(std::string_view name) {
		std::string keyName{ name };

		auto it = m_metrics.find(keyName);
		if(it != m_metrics.end()){
			return it->second.get();
		}
		return nullptr;
	}
}