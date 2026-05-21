#pragma once
#include <vector>
#include <utility>

namespace sysmetrics::hw {
	class IHardwarePoller {
	public:
		virtual ~IHardwarePoller() = default;

		[[nodiscard]] virtual double pollCPU() = 0;
		[[nodiscard]] virtual double pollRAM() = 0;
		[[nodiscard]] virtual std::pair<double, double> pollDisk() = 0;
		[[nodiscard]] virtual std::pair<double, double> pollNetwork() = 0;
		[[nodiscard]] virtual double pollGPU() = 0;
		[[nodiscard]] virtual std::vector<double> getCpuCoreUtilizations() = 0;
		[[nodiscard]] virtual std::vector<double> getGpuSmUtilizations() = 0;
	};
	
	IHardwarePoller* createHardwarePoller();
}