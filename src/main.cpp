#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_win32.h"
#include "implot.h"
#include "implot_internal.h"
#include "parser.h"
#include "serial_reader.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <GL/GL.h>
#include <cstdio>
#include <vector>
#include <algorithm>
#include <cmath>
#include <fstream>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Distinct colors for channels
static const ImVec4 kColors[] = {
    {1.0f, 0.3f, 0.3f, 1.0f},
    {0.3f, 0.6f, 1.0f, 1.0f},
    {0.3f, 0.9f, 0.4f, 1.0f},
    {1.0f, 0.7f, 0.0f, 1.0f},
    {0.7f, 0.3f, 1.0f, 1.0f},
    {0.0f, 0.8f, 0.8f, 1.0f},
    {1.0f, 0.4f, 0.7f, 1.0f},
    {0.6f, 0.6f, 0.2f, 1.0f},
    {0.5f, 0.5f, 0.5f, 1.0f},
    {0.2f, 0.6f, 1.0f, 1.0f},
    {0.8f, 0.5f, 0.2f, 1.0f},
    {0.5f, 0.8f, 0.2f, 1.0f},
};
static const int kNumColors = sizeof(kColors) / sizeof(kColors[0]);

static const char* kPresetCmds[] = {
    "RTTView.start(0x20001914,1024,0)",
    "RTTView.start(0x20000000,2048,0)",
    "ATZ\r\n",
    "AT\r\n",
};
static const int kNumPresets = sizeof(kPresetCmds) / sizeof(kPresetCmds[0]);

enum class DataMode { File, Serial };

struct AppState {
    DataParser parser;
    std::vector<char> visible;
    char filepath[512] = "doc/resis.txt";
    bool dataLoaded = false;
    float lineWidth = 1.5f;
    bool fitOnce = false;
    bool scatterMode = false;
    bool userAdjustedView = false;
    bool scrollToTail = false;
    int  targetFPS = 30;

    // Serial mode
    DataMode     mode = DataMode::File;
    SerialReader serial;
    std::vector<std::string> portList;
    int          portIdx = -1;
    int          baudIdx = 6;
    bool         serialConnected = false;
    bool         autoScroll = true;
    char         statusMsg[128] = "";
    char         sendBuf[256] = "";
    int          presetIdx = 0;
    char         exportPath[256] = "export.csv";
};

static const int kBaudRates[] = { 9600, 19200, 38400, 57600, 76800, 115200, 230400 };
static const int kNumBaudRates = sizeof(kBaudRates) / sizeof(kBaudRates[0]);

static void renderUI(AppState& app) {
    ImGui::Begin("Channels", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::SetWindowPos({10, 30}, ImGuiCond_FirstUseEver);
    ImGui::SetWindowSize({280, 600}, ImGuiCond_FirstUseEver);

    // --- Mode selection ---
    if (ImGui::RadioButton("File", app.mode == DataMode::File))  app.mode = DataMode::File;
    ImGui::SameLine();
    if (ImGui::RadioButton("Serial", app.mode == DataMode::Serial)) app.mode = DataMode::Serial;

    ImGui::Separator();

    if (app.mode == DataMode::File) {
        ImGui::InputText("##path", app.filepath, sizeof(app.filepath));
        ImGui::SameLine();
        if (ImGui::Button("Load")) {
            app.dataLoaded = app.parser.load(app.filepath);
            if (app.dataLoaded) {
                app.visible.assign(app.parser.channelCount(), 1);
                app.fitOnce = true;
            }
        }
    } else {
        // --- Serial mode ---
        // COM port selection
        ImGui::SetNextItemWidth(90);
        if (ImGui::BeginCombo("##port", app.portIdx >= 0 ? app.portList[app.portIdx].c_str() : "COM?")) {
            for (int i = 0; i < (int)app.portList.size(); i++) {
                if (ImGui::Selectable(app.portList[i].c_str(), app.portIdx == i)) {
                    app.portIdx = i;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();

        ImGui::SetNextItemWidth(70);
        if (ImGui::BeginCombo("##baud", std::to_string(kBaudRates[app.baudIdx]).c_str())) {
            for (int i = 0; i < kNumBaudRates; i++) {
                if (ImGui::Selectable(std::to_string(kBaudRates[i]).c_str(), app.baudIdx == i)) {
                    app.baudIdx = i;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();

        if (ImGui::Button("Scan")) {
            app.portList = SerialReader::scanPorts();
            if (app.portIdx < 0 || app.portIdx >= (int)app.portList.size()) {
                app.portIdx = app.portList.empty() ? -1 : 0;
            }
        }
        ImGui::SameLine();

        if (!app.serialConnected) {
            if (ImGui::Button("Connect") && app.portIdx >= 0) {
                const char* port = app.portList[app.portIdx].c_str();
                if (app.serial.open(port, kBaudRates[app.baudIdx])) {
                    app.serialConnected = true;
                    if (app.dataLoaded) app.parser.clear();
                    app.dataLoaded = false;
                    app.visible.clear();
                    std::snprintf(app.statusMsg, sizeof(app.statusMsg),
                                  "Connected %s @ %d", port, kBaudRates[app.baudIdx]);
                } else {
                    std::snprintf(app.statusMsg, sizeof(app.statusMsg),
                                  "Failed to open %s", port);
                }
            }
        } else {
            if (ImGui::Button("Disconnect")) {
                app.serial.close();
                app.serialConnected = false;
                std::snprintf(app.statusMsg, sizeof(app.statusMsg), "Disconnected");
            }
        }

        // Status
        if (app.serialConnected) {
            ImGui::TextColored({0.3f, 1.0f, 0.3f, 1.0f}, "%s", app.statusMsg);
        } else if (app.statusMsg[0]) {
            ImGui::TextColored({1.0f, 0.5f, 0.3f, 1.0f}, "%s", app.statusMsg);
        }

        if (ImGui::Checkbox("Auto-scroll", &app.autoScroll)) {
            if (app.autoScroll) app.userAdjustedView = false;
        }

        ImGui::Separator();

        // --- Send section ---
        ImGui::Text("Send:");
        ImGui::SetNextItemWidth(200);
        if (ImGui::BeginCombo("##preset", kPresetCmds[app.presetIdx])) {
            for (int i = 0; i < kNumPresets; i++) {
                if (ImGui::Selectable(kPresetCmds[i], app.presetIdx == i)) {
                    app.presetIdx = i;
                    std::snprintf(app.sendBuf, sizeof(app.sendBuf), "%s", kPresetCmds[i]);
                }
            }
            ImGui::EndCombo();
        }
        ImGui::InputTextMultiline("##send", app.sendBuf, sizeof(app.sendBuf),
                                  ImVec2(-1, 40));
        ImGui::BeginDisabled(!app.serialConnected);
        if (ImGui::Button("Send")) {
            app.serial.sendString(app.sendBuf);
            // Append \n if not present
            size_t len = strlen(app.sendBuf);
            if (len == 0 || app.sendBuf[len - 1] != '\n') {
                app.serial.sendString("\n");
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Send\\n")) {
            app.serial.sendString(app.sendBuf);
            app.serial.sendString("\r\n");
        }
        ImGui::EndDisabled();
    }

    ImGui::Separator();

    if (!app.dataLoaded) {
        ImGui::TextDisabled("No data. Load file or connect serial.");
        ImGui::End();
        return;
    }

    ImGui::Text("%zu channels, %zu records",
                app.parser.channelCount(), app.parser.recordCount());

    ImGui::Separator();

    if (ImGui::Button("All ON"))  std::fill(app.visible.begin(), app.visible.end(), 1);
    ImGui::SameLine();
    if (ImGui::Button("All OFF")) std::fill(app.visible.begin(), app.visible.end(), 0);

    if (ImGui::Button("Fit")) {
        app.scrollToTail = true;
        app.userAdjustedView = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset")) {
        ImPlot::SetNextAxesToFit();
        app.userAdjustedView = false;
    }
    ImGui::SliderFloat("Line", &app.lineWidth, 0.5f, 4.0f);
    ImGui::SliderInt("FPS", &app.targetFPS, 5, 120);
    ImGui::SameLine();
    ImGui::Checkbox("Scatter", &app.scatterMode);

    ImGui::Separator();

    const auto& series = app.parser.series();
    for (size_t i = 0; i < series.size(); i++) {
        ImVec4 color = kColors[i % kNumColors];
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        char label[128];
        std::snprintf(label, sizeof(label), "##ch%zu", i);
        bool v = app.visible[i] != 0;
        ImGui::Checkbox(label, &v);
        app.visible[i] = v ? 1 : 0;
        ImGui::PopStyleColor();
        ImGui::SameLine();

        float minY = 0, maxY = 0;
        if (!series[i].yValues.empty()) {
            auto [mn, mx] = std::minmax_element(
                series[i].yValues.begin(), series[i].yValues.end());
            minY = *mn; maxY = *mx;
        }
        ImGui::Text("%s [%.1f - %.1f] (%zu)", series[i].name.c_str(),
                    minY, maxY, series[i].yValues.size());
    }

    ImGui::End();

    // Main plot
    ImGui::Begin("Data Scope", nullptr, ImGuiWindowFlags_NoCollapse);
    ImGui::SetWindowPos({290, 30}, ImGuiCond_FirstUseEver);
    ImGui::SetWindowSize({1000, 700}, ImGuiCond_FirstUseEver);

    if (app.fitOnce) {
        ImPlot::SetNextAxesToFit();
        app.fitOnce = false;
    }
    if (app.dataLoaded && ImPlot::BeginPlot("##plot", ImVec2(-1, -1),
                                             ImPlotFlags_Crosshairs)) {
        ImPlot::SetupAxes("Line #", "Value");

        // Scroll X to follow latest data, preserving current X zoom width
        if (app.scrollToTail) {
            app.scrollToTail = false;
            float lastX = 0;
            for (const auto& s : series) {
                if (!s.xValues.empty() && s.xValues.back() > lastX)
                    lastX = s.xValues.back();
            }
            ImPlotContext& gp = *GImPlot;
            double xMin = gp.CurrentPlot->Axes[ImAxis_X1].Range.Min;
            double xMax = gp.CurrentPlot->Axes[ImAxis_X1].Range.Max;
            double xRange = xMax - xMin;
            if (xRange > 0) {
                gp.CurrentPlot->Axes[ImAxis_X1].SetRange(lastX - xRange, lastX);
                gp.CurrentPlot->Axes[ImAxis_X1].HasRange = true;
                gp.CurrentPlot->Axes[ImAxis_X1].RangeCond = ImPlotCond_Always;
            }
        }

        for (size_t i = 0; i < series.size(); i++) {
            if (!app.visible[i] || series[i].yValues.empty()) continue;
            ImPlot::SetNextLineStyle(kColors[i % kNumColors], app.lineWidth);
            if (app.scatterMode) {
                ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle, app.lineWidth * 1.5f,
                                           kColors[i % kNumColors]);
                ImPlot::PlotScatter(series[i].name.c_str(),
                                    series[i].xValues.data(),
                                    series[i].yValues.data(),
                                    (int)series[i].yValues.size());
            } else {
                ImPlot::PlotLine(series[i].name.c_str(),
                                 series[i].xValues.data(),
                                 series[i].yValues.data(),
                                 (int)series[i].yValues.size());
            }
        }

        // Detect user manually adjusting the view (zoom only)
        if (!app.userAdjustedView && ImPlot::IsPlotHovered()) {
            if (ImGui::GetIO().MouseWheel != 0)
                app.userAdjustedView = true;
        }

        ImPlot::EndPlot();
    }

    ImGui::End();

    // --- Data Table ---
    ImGui::Begin("Data Table", nullptr, ImGuiWindowFlags_NoCollapse);
    ImGui::SetWindowPos({1295, 30}, ImGuiCond_FirstUseEver);
    ImGui::SetWindowSize({400, 700}, ImGuiCond_FirstUseEver);

    if (app.dataLoaded) {
        // Collect visible channel indices
        std::vector<size_t> visIdx;
        for (size_t i = 0; i < series.size(); i++) {
            if (app.visible[i]) visIdx.push_back(i);
        }

        if (visIdx.empty()) {
            ImGui::TextDisabled("No channels selected");
        } else {
            // --- Export CSV ---
            ImGui::SetNextItemWidth(160);
            ImGui::InputText("##export", app.exportPath, sizeof(app.exportPath));
            ImGui::SameLine();
            if (ImGui::Button("Export CSV")) {
                std::ofstream f(app.exportPath);
                if (f.is_open()) {
                    f << "#";
                    for (size_t ci = 0; ci < visIdx.size(); ci++) {
                        f << "," << series[visIdx[ci]].name;
                    }
                    f << "\n";
                    size_t maxRows = 0;
                    for (size_t idx : visIdx) {
                        maxRows = (std::max)(maxRows, series[idx].yValues.size());
                    }
                    for (size_t row = 0; row < maxRows; row++) {
                        f << (row + 1);
                        for (size_t ci = 0; ci < visIdx.size(); ci++) {
                            f << ",";
                            const auto& ch = series[visIdx[ci]];
                            if (row < ch.yValues.size()) {
                                f << ch.yValues[row];
                            }
                        }
                        f << "\n";
                    }
                    f.close();
                    std::snprintf(app.statusMsg, sizeof(app.statusMsg),
                                  "Exported %zu rows to %s", maxRows, app.exportPath);
                } else {
                    std::snprintf(app.statusMsg, sizeof(app.statusMsg),
                                  "Failed to write %s", app.exportPath);
                }
            }
            if (app.statusMsg[0]) {
                ImGui::SameLine();
                ImGui::TextDisabled("%s", app.statusMsg);
            }

            size_t maxRows = 0;
            for (size_t idx : visIdx) {
                maxRows = (std::max)(maxRows, series[idx].yValues.size());
            }

            if (ImGui::BeginTable("##datatable", (int)visIdx.size() + 1,
                                  ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY |
                                  ImGuiTableFlags_Hideable)) {
                ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 40.0f);
                for (size_t ci = 0; ci < visIdx.size(); ci++) {
                    const auto& ch = series[visIdx[ci]];
                    char hdr[64];
                    std::snprintf(hdr, sizeof(hdr), "%s", ch.name.c_str());
                    ImGui::TableSetupColumn(hdr, ImGuiTableColumnFlags_WidthFixed, 80.0f);
                }
                ImGui::TableHeadersRow();

                ImGuiListClipper clipper;
                clipper.Begin((int)maxRows);
                while (clipper.Step()) {
                    for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("%d", row + 1);
                        for (size_t ci = 0; ci < visIdx.size(); ci++) {
                            ImGui::TableSetColumnIndex((int)ci + 1);
                            const auto& ch = series[visIdx[ci]];
                            if ((size_t)row < ch.yValues.size()) {
                                ImGui::Text("%.1f", ch.yValues[row]);
                            } else {
                                ImGui::TextDisabled("--");
                            }
                        }
                    }
                }
                ImGui::EndTable();
            }
        }
    } else {
        ImGui::TextDisabled("No data loaded");
    }

    ImGui::End();
}

// --- WGL helpers ---
struct WGL_WindowData { HDC hDC; };
static HGLRC   g_hRC;
static int      g_Width, g_Height;

static bool CreateDeviceWGL(HWND hWnd, WGL_WindowData* data) {
    HDC hDc = ::GetDC(hWnd);
    PIXELFORMATDESCRIPTOR pfd = { sizeof(pfd) };
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    const int pf = ::ChoosePixelFormat(hDc, &pfd);
    if (pf == 0) return false;
    if (::SetPixelFormat(hDc, pf, &pfd) == FALSE) return false;
    ::ReleaseDC(hWnd, hDc);
    data->hDC = ::GetDC(hWnd);
    if (!g_hRC) g_hRC = wglCreateContext(data->hDC);
    return true;
}

static void CleanupDeviceWGL(HWND hWnd, WGL_WindowData* data) {
    wglMakeCurrent(nullptr, nullptr);
    ::ReleaseDC(hWnd, data->hDC);
}

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;
    switch (msg) {
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED) {
            g_Width = LOWORD(lParam);
            g_Height = HIWORD(lParam);
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    WNDCLASSEXW wc = { sizeof(wc), CS_OWNDC, WndProc, 0L, 0L,
                       hInstance, nullptr, nullptr, nullptr, nullptr,
                       L"DataScope", nullptr };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"Data Scope",
                                WS_OVERLAPPEDWINDOW,
                                100, 100, 1280, 780,
                                nullptr, nullptr, wc.hInstance, nullptr);

    WGL_WindowData wglData{};
    if (!CreateDeviceWGL(hwnd, &wglData)) {
        CleanupDeviceWGL(hwnd, &wglData);
        ::DestroyWindow(hwnd);
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }
    wglMakeCurrent(wglData.hDC, g_hRC);
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;

    ImGui::StyleColorsDark();
    ImPlot::StyleColorsDark();

    ImGui_ImplWin32_InitForOpenGL(hwnd);
    ImGui_ImplOpenGL3_Init();

    AppState app;
    // Resolve default filepath relative to exe directory
    {
        char exePath[MAX_PATH];
        GetModuleFileNameA(nullptr, exePath, MAX_PATH);
        std::string dir(exePath);
        dir.resize(dir.rfind('\\') + 1);
        std::snprintf(app.filepath, sizeof(app.filepath), "%s../doc/resis.txt", dir.c_str());
    }
    // Auto-scan ports on startup
    app.portList = SerialReader::scanPorts();
    app.portIdx = app.portList.empty() ? -1 : 0;

    LARGE_INTEGER perfFreq, perfStart, perfEnd;
    QueryPerformanceFrequency(&perfFreq);
    QueryPerformanceCounter(&perfStart);

    bool done = false;
    while (!done) {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT) done = true;
        }
        if (done) break;
        if (::IsIconic(hwnd)) { ::Sleep(10); continue; }

        // --- Serial polling ---
        if (app.serialConnected && app.serial.isOpen()) {
            std::string line;
            bool gotNew = false;
            while (app.serial.tryReadLine(line)) {
                if (app.parser.appendLine(line)) {
                    gotNew = true;
                }
            }
            if (gotNew) {
                bool firstData = !app.dataLoaded;
                if (firstData) {
                    app.dataLoaded = true;
                    app.visible.assign(app.parser.channelCount(), 1);
                }
                if (app.parser.channelCount() > app.visible.size()) {
                    app.visible.resize(app.parser.channelCount(), 1);
                }
                if (firstData) {
                    app.fitOnce = true;
                } else if (app.autoScroll && !app.userAdjustedView) {
                    app.scrollToTail = true;
                }
            }
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        renderUI(app);

        ImGui::Render();
        glViewport(0, 0, g_Width, g_Height);
        glClearColor(0.12f, 0.12f, 0.13f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        ::SwapBuffers(wglData.hDC);

        // Frame rate limiter
        QueryPerformanceCounter(&perfEnd);
        double elapsedMs = (double)(perfEnd.QuadPart - perfStart.QuadPart) * 1000.0 / perfFreq.QuadPart;
        double targetMs = 1000.0 / app.targetFPS;
        if (elapsedMs < targetMs) {
            DWORD sleepMs = (DWORD)(targetMs - elapsedMs);
            if (sleepMs > 0) ::Sleep(sleepMs);
        }
        QueryPerformanceCounter(&perfStart);
    }

    if (app.serialConnected) app.serial.close();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    CleanupDeviceWGL(hwnd, &wglData);
    wglDeleteContext(g_hRC);
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return 0;
}
