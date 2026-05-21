#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <ws2ipdef.h>
#include <windows.h>
#include <iphlpapi.h>
#include <netioapi.h>

#include "HardwarePoller.hpp"
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
		PDH_HCOUNTER m_diskReadCounter;
		PDH_HCOUNTER m_diskWriteCounter;

		ULONG64 m_prevNetworkRx{ 0 };
		ULONG64 m_prevNetworkTx{ 0 };
		bool m_networkInitialized{ false };

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

			PdhAddEnglishCounterA(m_query, "\\PhysicalDisk(_Total)\\Disk Read Bytes/sec", 0, &m_diskReadCounter);
			PdhAddEnglishCounterA(m_query, "\\PhysicalDisk(_Total)\\Disk Write Bytes/sec", 0, &m_diskWriteCounter);
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

		[[nodiscard]] std::pair<double, double> pollDisk() override {
			PDH_FMT_COUNTERVALUE readVal, writeVal;
			double readBps{ 0.0 }, writeBps{ 0.0 };

			if(PdhGetFormattedCounterValue(m_diskReadCounter, PDH_FMT_DOUBLE, nullptr, &readVal) == ERROR_SUCCESS) {
				readBps = readVal.doubleValue;
			}
			if (PdhGetFormattedCounterValue(m_diskWriteCounter, PDH_FMT_DOUBLE, nullptr, &writeVal) == ERROR_SUCCESS) {
				writeBps = writeVal.doubleValue;
			}

			return { readBps, writeBps };
		}

		[[nodiscard]] std::pair<double, double> pollNetwork() override {
			PMIB_IF_TABLE2 ifTable;

			if (GetIfTable2(&ifTable) != NO_ERROR) {
				return { 0.0, 0.0 };
			}

			ULONG64 currentRx{ 0 }, currentTx{ 0 };

			for (ULONG64 i{}; i < ifTable->NumEntries; i++) {
				if (ifTable->Table[i].Type != IF_TYPE_SOFTWARE_LOOPBACK) {
					currentRx += ifTable->Table[i].InOctets;
					currentTx += ifTable->Table[i].OutOctets;
				}
			}

			FreeMibTable(ifTable);

			if (!m_networkInitialized) {
				m_prevNetworkRx = currentRx;
				m_prevNetworkTx = currentTx;
				m_networkInitialized = true;
				return { 0.0, 0.0 };
			}

			double rxDelta = static_cast<double>(currentRx - m_prevNetworkRx);
			double txDelta = static_cast<double>(currentTx - m_prevNetworkTx);

			m_prevNetworkRx = currentRx;
			m_prevNetworkTx = currentTx;

			return { rxDelta, txDelta };
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