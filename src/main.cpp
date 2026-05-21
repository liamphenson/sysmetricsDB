#include "MetricSeries.hpp"
#include "Database.hpp"
#include "Analytics.hpp"
#include "HardwarePoller.hpp"

#include <httplib.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <implot.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <vector>
#include <format>
#include <thread>
#include <filesystem>
#include <chrono>

int max(int a, int b) {
	return a > b ? a : b;
}

int main() {
	// Multithreaded implementation of the system metrics collection and analysis
	sysmetrics::Database db;
	constexpr std::string_view CPUMetricName{ "CPU_Usage" };
	constexpr std::string_view RAMMetricName{ "RAM_Usage" };
	constexpr std::string_view GPUMetricName{ "GPU_Usage" };
	constexpr std::string_view DiskReadMetricName{ "Disk_Read_BytesPerSec" };
	constexpr std::string_view DiskWriteMetricName{ "Disk_Write_BytesPerSec" };
	constexpr std::string_view NetworkRxMetricName{ "Network_Rx_BytesPerSec" };
	constexpr std::string_view NetworkTxMetricName{ "Network_Tx_BytesPerSec" };

	db.createMetric(CPUMetricName);
	db.createMetric(RAMMetricName);
	db.createMetric(GPUMetricName);
	db.createMetric(DiskReadMetricName);
	db.createMetric(DiskWriteMetricName);
	db.createMetric(NetworkRxMetricName);
	db.createMetric(NetworkTxMetricName);

	auto* cpuMetric{ db.getMetric(CPUMetricName) };
	auto* ramMetric{ db.getMetric(RAMMetricName) };
	auto* gpuMetric{ db.getMetric(GPUMetricName) };
	auto* diskReadMetric{ db.getMetric(DiskReadMetricName) };
	auto* diskWriteMetric{ db.getMetric(DiskWriteMetricName) };
	auto* networkRxMetric{ db.getMetric(NetworkRxMetricName) };
	auto* networkTxMetric{ db.getMetric(NetworkTxMetricName) };

	if (!cpuMetric) {
		std::cerr << std::format("Fatal Error: Metric '{}' could not be found or created.\n", CPUMetricName);
		return 1;
	}

	if (!ramMetric){
		std::cerr << std::format("Fatal Error: Metric '{}' could not be found or created.\n", RAMMetricName);
		return 1;
	}

	if (!gpuMetric) {
		std::cerr << std::format("Fatal Error: Metric '{}' coudl not be found or created.\n", GPUMetricName);
		return 1;
	}

	if (!diskReadMetric) {
		std::cerr << std::format("Fatal Error: Metric '{}' could not be found or created.\n", DiskReadMetricName);
		return 1;
	}

	if (!diskWriteMetric) {
		std::cerr << std::format("Fatal Error: Metric '{}' could not be found or created.\n", DiskWriteMetricName);
		return 1;
	}

	if (!networkRxMetric) {
		std::cerr << std::format("Fatal Error: Metric '{}' could not be found or created.\n", NetworkRxMetricName);
		return 1;
	}

	if (!networkTxMetric) {
		std::cerr << std::format("Fatal Error: Metric '{}' could not be found or created.\n", NetworkTxMetricName);
		return 1;
	}

	std::cout << "Initializing Hardware Abstraction Layer...\n";
	std::unique_ptr<sysmetrics::hw::IHardwarePoller> hardwarePoller(sysmetrics::hw::createHardwarePoller());
	std::mutex gridMutex;
	std::vector<double> latestCpuCores;
	std::vector<double> latestGpuSms;

	std::cout << "Starting background hardware polling...\n";
	std::thread backgroundCollector([&]() {
		while (true) {
			double CPULoad{ hardwarePoller->pollCPU() };
			double RAMUsage{ hardwarePoller->pollRAM() };
			double GPULoad{ hardwarePoller->pollGPU() };
			std::pair<double, double> diskStats{ hardwarePoller->pollDisk() };
			std::pair<double, double> networkStats{ hardwarePoller->pollNetwork() };

			cpuMetric->addDataPoint(CPULoad);
			ramMetric->addDataPoint(RAMUsage);
			gpuMetric->addDataPoint(GPULoad);
			diskReadMetric->addDataPoint(diskStats.first);
			diskWriteMetric->addDataPoint(diskStats.second);
			networkRxMetric->addDataPoint(networkStats.first);
			networkTxMetric->addDataPoint(networkStats.second);

			auto cpuGrid{ hardwarePoller->getCpuCoreUtilizations() };
			auto gpuGrid{ hardwarePoller->getGpuSmUtilizations() };
			{
				std::lock_guard<std::mutex> lock(gridMutex);
				latestCpuCores = cpuGrid;
				latestGpuSms = gpuGrid;
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		}
	});

	if (!glfwInit()) return 1;

	GLFWwindow* window = glfwCreateWindow(1920, 1080, "SysMetricsDB - Live Telemetry Dashboard", nullptr, nullptr);
	if (!window) return 1;

	glfwMakeContextCurrent(window);
	glfwSwapInterval(1); // Enable vsync

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImPlot::CreateContext();

	ImGuiIO& io{ ImGui::GetIO() };
	io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
	ImFont* defaultFont{ io.Fonts->AddFontFromFileTTF("assets/Inter_18pt-Black.ttf", 18.0f) };

	if (defaultFont == nullptr) {
		std::cerr << "Error: Failed to load font from 'assets/Inter_18pt-Black.ttf'. Please ensure the file exists and is a valid TTF font.\n";
		io.Fonts->AddFontDefault();
	}

	io.Fonts->Build();

	ImGui::StyleColorsDark();
	ImGuiStyle& style{ ImGui::GetStyle() };

	// Set style parameters
	style.WindowRounding = 8.0f;
	style.ChildRounding = 6.0f;
	style.PopupRounding = 6.0f;
	style.FrameRounding = 4.0f;
	style.ScrollbarRounding = 12.0f;
	style.GrabRounding = 4.0f;

	style.WindowBorderSize = 0.0f;
	style.FrameBorderSize = 0.0f;
	style.PopupBorderSize = 1.0f;

	style.WindowPadding = ImVec2(16.0f, 16.0f);
	style.FramePadding = ImVec2(8.0f, 6.0f);
	style.ItemSpacing = ImVec2(12.0f, 8.0f);

	style.Colors[ImGuiCol_WindowBg] = ImVec4(0.09f, 0.09f, 0.11f, 1.00f);
	style.Colors[ImGuiCol_FrameBg] = ImVec4(0.14f, 0.15f, 0.18f, 1.00f);
	style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.12f, 0.15f, 1.00f);

	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 130");

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
		ImGui::DockSpaceOverViewport();

		ImGui::Begin("Hardware Telemetry Dashboard");
		ImGui::Text("System Monitor Agent v1.0");
		ImGui::Separator();

		auto CPUSnapshot{ cpuMetric->getSnapshot() };
		auto RAMSnapshot{ ramMetric->getSnapshot() };
		auto GPUSnapshot{ gpuMetric->getSnapshot() };
		auto DiskReadSnapshot{ diskReadMetric->getSnapshot() };
		auto DiskWriteSnapshot{ diskWriteMetric->getSnapshot() };
		auto NetworkRxSnapshot{ networkRxMetric->getSnapshot() };
		auto NetworkTxSnapshot{ networkTxMetric->getSnapshot() };

		std::vector<double> xValues;
		std::vector<double> cpuValues, ramValues, gpuValues, diskReadValues, diskWriteValues, networkRxValues, networkTxValues;

		for (size_t i{}; i < CPUSnapshot.size(); ++i) {
			xValues.push_back(static_cast<double>(i));
			cpuValues.push_back(CPUSnapshot[i].value);
			ramValues.push_back(i < RAMSnapshot.size() ? RAMSnapshot[i].value : 0.0);
			gpuValues.push_back(i < GPUSnapshot.size() ? GPUSnapshot[i].value : 0.0);
			diskReadValues.push_back(i < DiskReadSnapshot.size() ? DiskReadSnapshot[i].value/(1024*1024) : 0.0);
			diskWriteValues.push_back(i < DiskWriteSnapshot.size() ? DiskWriteSnapshot[i].value/(1024*1024) : 0.0);
			networkRxValues.push_back(i < NetworkRxSnapshot.size() ? NetworkRxSnapshot[i].value/(1024*1024) : 0.0);
			networkTxValues.push_back(i < NetworkTxSnapshot.size() ? NetworkTxSnapshot[i].value/(1024*1024) : 0.0);
		}

		if (ImPlot::BeginSubplots("Telemetry", 5, 1, ImVec2(-1, -1))) {
			if (ImPlot::BeginPlot("CPU Utilization")) {
				ImPlot::SetupAxes(nullptr, "%");
				ImPlot::SetupAxesLimits(max(static_cast<int>(xValues.size()) - 60, 0), static_cast<int>(xValues.size()), 0, 100, ImPlotCond_Always);
				if (!xValues.empty()) {
					ImPlotSpec spec;
					spec.LineColor = ImVec4(0.2f, 0.6f, 1.0f, 1.0f);
					spec.LineWeight = 2.0f;
					spec.FillColor = ImVec4(0.2f, 0.6f, 1.0f, 1.0f);
					spec.FillAlpha = 0.3f;
					spec.Flags = ImPlotLineFlags_Shaded;

					ImPlot::PlotLine("CPU", xValues.data(), cpuValues.data(), static_cast<int>(xValues.size()), spec);
				}
				ImPlot::EndPlot();
			}

			if (ImPlot::BeginPlot("RAM Utilization")){
				ImPlot::SetupAxes(nullptr, "%");
				ImPlot::SetupAxesLimits(max(static_cast<int>(xValues.size()) - 60, 0), static_cast<int>(xValues.size()), 0, 100, ImPlotCond_Always);
				if (!xValues.empty()) {
					ImPlotSpec spec;
					spec.LineColor = ImVec4(1.0f, 0.6f, 0.2f, 1.0f);
					spec.LineWeight = 2.0f;
					spec.FillColor = ImVec4(1.0f, 0.6f, 0.2f, 1.0f);
					spec.FillAlpha = 0.3f;
					spec.Flags = ImPlotLineFlags_Shaded;

					ImPlot::PlotLine("RAM", xValues.data(), ramValues.data(), static_cast<int>(xValues.size()), spec);
				}
				ImPlot::EndPlot();
			}

			if (ImPlot::BeginPlot("GPU Utilization")){
				ImPlot::SetupAxes("Time (Seconds)", "%");
				ImPlot::SetupAxesLimits(max(static_cast<int>(xValues.size()) - 60, 0), static_cast<int>(xValues.size()), 0, 100, ImPlotCond_Always);
				if (!xValues.empty()) {
					ImPlotSpec spec;
					spec.LineColor = ImVec4(0.4f, 0.8f, 0.2f, 1.0f);
					spec.LineWeight = 2.0f;
					spec.FillColor = ImVec4(0.4f, 0.8f, 0.2f, 1.0f);
					spec.FillAlpha = 0.3f;
					spec.Flags = ImPlotLineFlags_Shaded;

					ImPlot::PlotLine("GPU", xValues.data(), gpuValues.data(), static_cast<int>(xValues.size()), spec);
				}
				ImPlot::EndPlot();
			}

			if (ImPlot::BeginPlot("Disk Utilization")) {
				ImPlot::SetupAxes("Time (Seconds)", "MB/s");
				ImPlot::SetupAxesLimits(max(static_cast<int>(xValues.size()) - 60, 0), static_cast<int>(xValues.size()), 0, 100, ImPlotCond_Always);
				if (!xValues.empty()) {
					ImPlotSpec spec;
					spec.LineColor = ImVec4(0.0f, 0.0f, 1.0f, 1.0f);
					spec.LineWeight = 2.0f;
					spec.FillColor = ImVec4(0.0f, 0.0f, 1.0f, 1.0f);
					spec.FillAlpha = 0.3f;
					spec.Flags = ImPlotLineFlags_Shaded;

					ImPlot::PlotLine("Disk Read", xValues.data(), diskReadValues.data(), static_cast<int>(xValues.size()), spec);

					ImPlotSpec spec2;
					spec2.LineColor = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
					spec2.LineWeight = 2.0f;
					spec2.FillColor = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
					spec2.FillAlpha = 0.3f;
					spec2.Flags = ImPlotLineFlags_Shaded;

					ImPlot::PlotLine("Disk Write", xValues.data(), diskWriteValues.data(), static_cast<int>(xValues.size()), spec2);
				}
				ImPlot::EndPlot();
			}

			if (ImPlot::BeginPlot("Network Utilization")) {
				ImPlot::SetupAxes("Time (Seconds)", "MB/s");
				ImPlot::SetupAxesLimits(max(static_cast<int>(xValues.size()) - 60, 0), static_cast<int>(xValues.size()), 0, 100, ImPlotCond_Always);
				if (!xValues.empty()) {
					ImPlotSpec spec;
					spec.LineColor = ImVec4(0.0f, 0.0f, 1.0f, 1.0f);
					spec.LineWeight = 2.0f;
					spec.FillColor = ImVec4(0.0f, 0.0f, 1.0f, 1.0f);
					spec.FillAlpha = 0.3f;
					spec.Flags = ImPlotLineFlags_Shaded;

					ImPlot::PlotLine("Network Tx", xValues.data(), networkTxValues.data(), static_cast<int>(xValues.size()), spec);

					ImPlotSpec spec2;
					spec2.LineColor = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
					spec2.LineWeight = 2.0f;
					spec2.FillColor = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
					spec2.FillAlpha = 0.3f;
					spec2.Flags = ImPlotLineFlags_Shaded;

					ImPlot::PlotLine("Network Rx", xValues.data(), networkRxValues.data(), static_cast<int>(xValues.size()), spec2);
				}
				ImPlot::EndPlot();
			}

			ImPlot::EndSubplots();
		}

		if (!CPUSnapshot.empty()) {
			ImGui::Text("Current CPU Load: %.1f%%", CPUSnapshot.back().value);
		}

		if (!RAMSnapshot.empty()) {
			ImGui::Text("Current RAM Usage: %.1f%%", RAMSnapshot.back().value);
		}

		if (!GPUSnapshot.empty()) {
			ImGui::Text("Current GPU Load: %.1f%%", GPUSnapshot.back().value);
		}

		if (!DiskReadSnapshot.empty() && !DiskWriteSnapshot.empty()) {
			ImGui::Text("Current Disk Read: %.1f MB/s", DiskReadSnapshot.back().value / (1024 * 1024));
			ImGui::Text("Current Disk Write: %.1f MB/s", DiskWriteSnapshot.back().value / (1024 * 1024));
		}

		if (!NetworkRxSnapshot.empty() && !NetworkTxSnapshot.empty()) {
			ImGui::Text("Current Network Rx: %.1f MB/s", NetworkRxSnapshot.back().value / (1024 * 1024));
			ImGui::Text("Current Network Tx: %.1f MB/s", NetworkTxSnapshot.back().value / (1024 * 1024));
		}

		ImGui::End();

		ImGui::Begin("Architecture Load Distribution");

		std::vector<double> uiCpuCores, uiGpuSms;
		{
			std::lock_guard<std::mutex> lock(gridMutex);
			uiCpuCores = latestCpuCores;
			uiGpuSms = latestGpuSms;
		}

		ImPlot::PushColormap(ImPlotColormap_Plasma);

		if (!uiCpuCores.empty()) {
			size_t cpuCols{ 8 };
			size_t cpuRows{ (uiCpuCores.size() + cpuCols - 1) / cpuCols };
			uiCpuCores.resize(cpuRows * cpuCols, 0.0); // Pad with zeros if necessary

			if(ImPlot::BeginPlot("Cpu Cores", ImVec2(-1, 150))){
				ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_NoDecorations);
				ImPlot::PlotHeatmap("##cpu", uiCpuCores.data(), static_cast<int>(cpuRows), static_cast<int>(cpuCols), 0.0, 100.0, "%.0f%%");
				ImPlot::EndPlot();
			}
		}

		if (!uiGpuSms.empty()) {
			size_t gpuCols{ 16 };
			size_t gpuRows{ (uiGpuSms.size() + gpuCols - 1) / gpuCols };
			uiGpuSms.resize(gpuRows * gpuCols, 0.0);

			if (ImPlot::BeginPlot("Gpu Streaming Multiprocessors", ImVec2(-1, 150))) {
				ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_NoDecorations);
				ImPlot::PlotHeatmap("##gpu", uiGpuSms.data(), static_cast<int>(gpuRows), static_cast<int>(gpuCols), 0.0, 100.0, "%.0f%%");
				ImPlot::EndPlot();
			}
		}

		ImPlot::PopColormap();

		ImGui::End();

		ImGui::Render();
		int display_w, display_h;
		glfwGetFramebufferSize(window, &display_w, &display_h);
		glViewport(0, 0, display_w, display_h);
		glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		glfwSwapBuffers(window);

		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
			GLFWwindow* backup_current_context = glfwGetCurrentContext();
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
			glfwMakeContextCurrent(backup_current_context);
		}
	}

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImPlot::DestroyContext();
	ImGui::DestroyContext();
	glfwDestroyWindow(window);
	glfwTerminate();

	backgroundCollector.detach();

	return 0;
}