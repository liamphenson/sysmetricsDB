#include "MetricSeries.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <format>
#include <mutex>

namespace sysmetrics {
	MetricSeries::MetricSeries(std::string_view name) : m_name(name) {}

	void MetricSeries::addDataPoint(double value) {
		std::lock_guard<std::mutex> lock(m_mutex);
		m_data.emplace_back(DataPoint{std::chrono::system_clock::now(), value});
	}

	std::string_view MetricSeries::getName() const {
		return m_name;
	}

	size_t MetricSeries::size() const {
		std::lock_guard<std::mutex> lock(m_mutex);
		return m_data.size();
	}

	std::vector<DataPoint> MetricSeries::getSnapshot() const {
		std::lock_guard<std::mutex> lock(m_mutex);
		return m_data;
	}

	void MetricSeries::saveToFile(std::string_view directory) const {
		std::string filepath = std::format("{}/{}.csv", directory, m_name);

		std::ofstream outFile(filepath);

		if (!outFile.is_open()) {
			throw std::runtime_error(std::format("Failed to open file for writing: {}", filepath));
		}

		outFile << "Timestamp(ms),Value\n";

		for (const auto& point : m_data) {
			outFile << point << "\n";
		}
	}

	void MetricSeries::loadFromFile(std::string_view directory) {
		std::string filepath = std::format("{}/{}.csv", directory, m_name);
		std::ifstream inFile(filepath);

		if (!inFile.is_open()){
			throw std::runtime_error(std::format("Failed to open file for reading: {}", filepath));
		}

		m_data.clear();

		std::string line;
		std::getline(inFile, line);

		while (std::getline(inFile, line)) {
			std::stringstream ss(line);
			std::string timeStr, valueStr;

			if (std::getline(ss, timeStr, ',') && std::getline(ss, valueStr)){
				auto epoch_ms = std::stoll(timeStr);
				double val = std::stod(valueStr);

				std::chrono::milliseconds ms(epoch_ms);
				DataPoint::TimePoint tp(ms);

				m_data.emplace_back(DataPoint{ tp, val });
			}
		}
	}
}