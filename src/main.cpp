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

	db.createMetric(CPUMetricName);
	db.createMetric(RAMMetricName);
	db.createMetric(GPUMetricName);

	auto* cpuMetric{ db.getMetric(CPUMetricName) };
	auto* ramMetric{ db.getMetric(RAMMetricName) };
	auto* gpuMetric{ db.getMetric(GPUMetricName) };

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

	std::cout << "Initializing Hardware Abstraction Layer...\n";
	std::unique_ptr<sysmetrics::hw::IHardwarePoller> hardwarePoller(sysmetrics::hw::createHardwarePoller());

	std::cout << "Starting background hardware polling...\n";
	std::thread backgroundCollector([&]() {
		while (true) {
			double CPULoad = hardwarePoller->pollCPU();
			double RAMUsage = hardwarePoller->pollRAM();
			double GPULoad = hardwarePoller->pollGPU();

			cpuMetric->addDataPoint(CPULoad);
			ramMetric->addDataPoint(RAMUsage);
			gpuMetric->addDataPoint(GPULoad);

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

		ImGui::Begin("Hardware Telemetry Dashboard");
		ImGui::Text("System Monitor Agent v1.0");
		ImGui::Separator();

		auto CPUSnapshot{ cpuMetric->getSnapshot() };
		auto RAMSnapshot{ ramMetric->getSnapshot() };
		auto GPUSnapshot{ gpuMetric->getSnapshot() };

		std::vector<double> xValues;
		std::vector<double> cpuValues, ramValues, gpuValues;

		for (size_t i = 0; i < CPUSnapshot.size(); ++i) {
			xValues.push_back(static_cast<double>(i));
			cpuValues.push_back(CPUSnapshot[i].value);
			ramValues.push_back(i < RAMSnapshot.size() ? RAMSnapshot[i].value : 0.0);
			gpuValues.push_back(i < GPUSnapshot.size() ? GPUSnapshot[i].value : 0.0);
		}

		if (ImPlot::BeginSubplots("Telemetry", 3, 1, ImVec2(-1, -1))) {
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

		ImGui::End();

		ImGui::Render();
		int display_w, display_h;
		glfwGetFramebufferSize(window, &display_w, &display_h);
		glViewport(0, 0, display_w, display_h);
		glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		glfwSwapBuffers(window);
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