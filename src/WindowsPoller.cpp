#ifdef _WIN32
#include "HardwarePoller.hpp"
#include <windows.h>
#include <pdh.h>
#include <nvml.h>
#include <stdexcept>
#include <format>
#include <thread>
#include <algorithm>
#include <vector>

#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "nvml.lib")

namespace sysmetrics::hw {
	class WindowsPoller : public IHardwarePoller {
	private:
		PDH_HQUERY m_query;
		PDH_HCOUNTER m_cpuCounter;

		nvmlDevice_t m_gpuDevice;
		bool m_nvmlInitialized{ false };

		std::vector<PDH_HCOUNTER> m_cpuCoreCounters;
		int m_numLogicalCores;
		int m_simulatedGpuSmCount{ 64 };

	public:
		WindowsPoller() {
			if (PdhOpenQuery(nullptr, 0, &m_query) != ERROR_SUCCESS) {
				throw std::runtime_error("Failed to open PDH query.");
			}

			if(PdhAddEnglishCounter(m_query, "\\Processor Information(_Total)\\% Processor Time", 0, &m_cpuCounter) != ERROR_SUCCESS) {
				throw std::runtime_error("Failed to add PDH counter.");
			}

			PdhCollectQueryData(m_query);

			if (nvmlInit() == NVML_SUCCESS) {
				if (nvmlDeviceGetHandleByIndex(0, &m_gpuDevice) == NVML_SUCCESS) {
					m_nvmlInitialized = true;
				}
			}

			m_numLogicalCores = std::thread::hardware_concurrency();
			m_cpuCoreCounters.resize(m_numLogicalCores);

			for (int i{}; i < m_numLogicalCores; ++i) {
				std::string counterPath = std::format("\\Processor Information({})\\% Processor Time", i);
				if (PdhAddEnglishCounter(m_query, counterPath.c_str(), 0, &m_cpuCoreCounters[i]) != ERROR_SUCCESS) {
					throw std::runtime_error(std::format("Failed to add PDH counter for CPU core {}", i));
				}
			}
		}

		~WindowsPoller() override {
			if (m_query) {
				PdhCloseQuery(m_query);
			}

			if (m_nvmlInitialized) {
				nvmlShutdown();
				m_nvmlInitialized = false;
			}
		}

		[[nodiscard]] double pollCPU() override {
			PdhCollectQueryData(m_query);

			PDH_FMT_COUNTERVALUE counterVal;
			if (PdhGetFormattedCounterValue(m_cpuCounter, PDH_FMT_DOUBLE, nullptr, &counterVal) == ERROR_SUCCESS) {
				return counterVal.doubleValue;
			}

			return 0.0;
		}

		[[nodiscard]] double pollRAM() override{
			MEMORYSTATUSEX memInfo;
			memInfo.dwLength = sizeof(MEMORYSTATUSEX);

			if (GlobalMemoryStatusEx(&memInfo)){
				DWORDLONG totalPhys = memInfo.ullTotalPhys;
				DWORDLONG usedPhys = memInfo.ullTotalPhys - memInfo.ullAvailPhys;

				return (static_cast<double>(usedPhys) / static_cast<double>(totalPhys)) * 100.0;
			}

			return 0.0;
		}

		[[nodiscard]] double pollDisk() override {
			// Placeholder implementation
			return 0.0;
		}

		[[nodiscard]] double pollNetwork() override {
			// Placeholder implementation
			return 0.0;
		}

		[[nodiscard]] double pollGPU() override {
			if (!m_nvmlInitialized) return 0.0;

			nvmlUtilization_t utilization;
			if (nvmlDeviceGetUtilizationRates(m_gpuDevice, &utilization) == NVML_SUCCESS) {
				return static_cast<double>(utilization.gpu);
			}

			return 0.0;
		}

		[[nodiscard]] std::vector<double> getCpuCoreUtilizations() override {
			std::vector<double> cores(m_numLogicalCores, 0.0);

			for (int i{}; i < m_numLogicalCores; ++i) {
				PDH_FMT_COUNTERVALUE val;
				if (PdhGetFormattedCounterValue(m_cpuCoreCounters[i], PDH_FMT_DOUBLE, nullptr, &val) == ERROR_SUCCESS) {
					cores[i] = val.doubleValue;
				}
			}
			return cores;
		}

		[[nodiscard]] std::vector<double> getGpuSmUtilizations() override {
			std::vector<double> sms(m_simulatedGpuSmCount, 0.0);
			double baseUtil{ pollGPU() };

			for (int i{}; i < m_simulatedGpuSmCount; ++i) {
				double noise = (rand() % 30) - 15.0;
				sms[i] = std::clamp(baseUtil + noise, 0.0, 100.0);
			}
			return sms;
		}
	};

	IHardwarePoller* createHardwarePoller() {
		return new WindowsPoller();
	}
}

#endif