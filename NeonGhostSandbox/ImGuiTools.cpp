#include "ImGuiTools.h"
#include <filesystem>
#include "imgui.h"
#include <string>
#include <vector>
#include <windows.h>
#include <regex>
#include <Matrix.h>





#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#include <iostream>

size_t GetProcessMemoryUsage()
{
    PROCESS_MEMORY_COUNTERS pmc{};

    if (GetProcessMemoryInfo(
        GetCurrentProcess(),
        &pmc,
        sizeof(pmc)))
    {
        return static_cast<size_t>(pmc.WorkingSetSize);
    }

    return 0;
}
#else
size_t GetProcessMemoryUsage() {
    return 9;
}
#endif


static void UnicodeToUTF8(ImWchar32 c, char* out)
{
    if (c <= 0x7F)
    {
        out[0] = (char)c;
        out[1] = 0;
    }
    else if (c <= 0x7FF)
    {
        out[0] = (char)(0xC0 | (c >> 6));
        out[1] = (char)(0x80 | (c & 0x3F));
        out[2] = 0;
    }
    else if (c <= 0xFFFF)
    {
        out[0] = (char)(0xE0 | (c >> 12));
        out[1] = (char)(0x80 | ((c >> 6) & 0x3F));
        out[2] = (char)(0x80 | (c & 0x3F));
        out[3] = 0;
    }
    else
    {
        out[0] = (char)(0xF0 | (c >> 18));
        out[1] = (char)(0x80 | ((c >> 12) & 0x3F));
        out[2] = (char)(0x80 | ((c >> 6) & 0x3F));
        out[3] = (char)(0x80 | (c & 0x3F));
        out[4] = 0;
    }
}


 void ng::gui::ShowFontViewer(bool& open)
{
    if (!ImGui::Begin("Font Viewer", &open))
    {
        ImGui::End();
        return;
    }

    ImFont* font = ImGui::GetFont();

    if (!font)
    {
        ImGui::Text("Keine Font aktiv.");
        ImGui::End();
        return;
    }

    ImGui::Text("Font: %s", font->GetDebugName());
    ImGui::Text("Size: %.1f", font->Scale);

    ImGui::Separator();

    static float cellSize = 50.0f;
    static int columns = 16;

    ImGui::SliderFloat(
        "Cell Size",
        &cellSize,
        30.0f,
        100.0f);

    ImGui::SliderInt(
        "Columns",
        &columns,
        4,
        32);

    ImGui::Separator();

    ImGui::BeginChild(
        "Glyphs",
        ImVec2(0, 0),
        ImGuiChildFlags_Borders);

    int column = 0;

    //
    // Basic Latin
    //
    for (ImWchar32 c = 0x20; c <= 0x7E; ++c)
    {
        char utf8[5];
        UnicodeToUTF8(c, utf8);

        if (column > 0)
            ImGui::SameLine();

        ImGui::PushID((int)c);

        ImGui::BeginGroup();

        ImGui::Button(
            utf8,
            ImVec2(cellSize, cellSize));

        ImGui::Text(
            "U+%04X",
            (unsigned)c);

        ImGui::EndGroup();

        ImGui::PopID();

        column++;

        if (column >= columns)
            column = 0;
    }


    //
    // Latin Extended
    //
    for (ImWchar32 c = 0xA0; c <= 0x24F; ++c)
    {
        char utf8[5];
        UnicodeToUTF8(c, utf8);

        if (column > 0)
            ImGui::SameLine();

        ImGui::PushID((int)c);

        ImGui::BeginGroup();

        ImGui::Button(
            utf8,
            ImVec2(cellSize, cellSize));

        ImGui::Text(
            "U+%04X",
            (unsigned)c);

        ImGui::EndGroup();

        ImGui::PopID();

        column++;

        if (column >= columns)
            column = 0;
    }


    //
    // Private Use Area
    //
    // Hier liegen viele Nerd-Font Icons.
    //
    for (ImWchar32 c = 0xE000; c <= 0xF8FF; ++c)
    {
        char utf8[5];
        UnicodeToUTF8(c, utf8);

        if (column > 0)
            ImGui::SameLine();

        ImGui::PushID((int)c);

        ImGui::BeginGroup();

        ImGui::Button(
            utf8,
            ImVec2(cellSize, cellSize));

        if (ImGui::IsItemHovered())
        {

            ImGui::BeginTooltip();
            ImGui::SetWindowFontScale(1.5f);

            ImGui::Text(
                "Unicode: U+%04X",
                (unsigned)c);

            ImGui::Text(
                "UTF-8: %s",
                utf8);

            ImGui::EndTooltip();
        }

        ImGui::Text(
            "U+%04X",
            (unsigned)c);

        ImGui::EndGroup();

        ImGui::PopID();

        column++;

        if (column >= columns)
            column = 0;
    }

    ImGui::EndChild();

    ImGui::End();
}


bool ng::gui::ExitPopup(bool& view_exit_dialog) {
    bool ex = false;
    if (!view_exit_dialog) return ex;

    ImGui::OpenPopup("Exit");
    if (ImGui::BeginPopupModal("Exit", &view_exit_dialog))
    {

        ImGui::Text("Wollen Sie das Programm wirklich verlassen?");
        if (ImGui::Button("Ja")) {
            ImGui::CloseCurrentPopup();
            ex = true;
            view_exit_dialog = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Nein")) {
            ImGui::CloseCurrentPopup();
            view_exit_dialog = false;
        }
        ImGui::EndPopup();
    }
    return ex;
}

inline bool MatchWildcard(
    const std::string& filename,
    const std::string& pattern)
{
    std::string regex_pattern;

    for (char c : pattern)
    {
        switch (c)
        {
        case '*':
            regex_pattern += ".*";
            break;

        case '?':
            regex_pattern += '.';
            break;

        case '.':
            regex_pattern += "\\.";
            break;

        default:
            regex_pattern += c;
        }
    }

    return std::regex_match(
        filename,
        std::regex(
            regex_pattern,
            std::regex_constants::icase));
}


std::filesystem::path ng::tools::abs_exe_path()
{
#if defined(_MSC_VER)
    wchar_t path[FILENAME_MAX] = { 0 };
    GetModuleFileNameW(nullptr, path, FILENAME_MAX);
    return std::filesystem::path(path);
#else
    char path[FILENAME_MAX];
    ssize_t count = readlink("/proc/self/exe", path, FILENAME_MAX);
    return std::filesystem::path(std::string(path, (count > 0) ? count : 0));
#endif
}

std::string ng::tools::abs_exe_directory()

{
#if defined(_MSC_VER)
    wchar_t path[FILENAME_MAX] = { 0 };
    GetModuleFileNameW(nullptr, path, FILENAME_MAX);
    return std::filesystem::path(path).parent_path().string();
#else
    char path[FILENAME_MAX];
    ssize_t count = readlink("/proc/self/exe", path, FILENAME_MAX);
    return std::filesystem::path(std::string(path, (count > 0) ? count : 0)).parent_path().string();
#endif
}

std::string ng::gui::FilePicker(bool& show, std::string wildcard) {
    if (!show) return "";
    static std::filesystem::path p = std::filesystem::current_path();
    static std::string filename = "";
    std::string path = p.string();






    std::string file_name = "";

    ImGui::OpenPopup("FilePicker");

    std::string change_drive;
    bool parent_dir = false;


    if (ImGui::BeginPopupModal("FilePicker", &show))
    {
        ImVec2 wsize = ImGui::GetWindowSize();

        if (ImGui::Button("..\\")) {
            if (p.has_parent_path()) p = p.parent_path();
            filename = "";
        }
        namespace fs = std::filesystem;

        for (char drive = 'A'; drive <= 'Z'; ++drive)
        {
            std::string root = std::string(1, drive) + ":\\";
            try {
                if (fs::exists(root))
                {
                    ImGui::SameLine();
                    if (ImGui::Button(root.c_str())) {
                        p = root; filename = "";
                    }
                }
            }
            catch (...) {}
        }



        ImGui::Separator();

        ImGui::Text("Path:"); ImGui::SameLine();
        ImGui::SetNextItemWidth(wsize.x - ImGui::CalcTextSize("Path:").x * 2);
        ImGui::BeginDisabled(true);
        std::string path_name = path + "\\" + filename;
        ImGui::InputText("##Path:", path_name.data(), path_name.length());
        ImGui::EndDisabled();
        ImGui::Separator();


        ImVec2 pos = ImGui::GetCursorPos();

        float width = (wsize.x / 3);
        if (width < 100) width = 100;
        float height = (wsize.y - pos.y - 50);



        ImGui::BeginChild("DirList", ImVec2(width, height), ImGuiChildFlags_Borders);

        for (const auto& entry : fs::directory_iterator(p))
        {
            if (entry.is_directory())
            {
                std::string help = entry.path().filename().string();

                if (ImGui::Selectable(help.c_str())) {
                    p = p.string() + std::string("\\") + help;
                    filename = "";
                }

            }
        }
        ImGui::EndChild();


        ImGui::SameLine();
        ImGui::BeginChild("FileList", ImVec2(wsize.x - width - 25, height), ImGuiChildFlags_Borders);
        for (const auto& entry : fs::directory_iterator(p))
        {
            if (entry.is_regular_file())
            {
                std::string help = entry.path().filename().string();

                if (!MatchWildcard(help, wildcard)) continue;
                bool selected = false;
                if (filename == help) selected = true;
                if (ImGui::Selectable(help.c_str(), &selected)) filename = help;;
            }
        }

        ImGui::EndChild();

        if (ImGui::Button("Select")) {
            file_name = path_name;
            show = false;
        }

        ImGui::SameLine();
        if (ImGui::Button("Chancel")) show = false;
        ImGui::SameLine();


        ImGui::EndPopup();
    }

    return file_name;
}

ng::gui::FPSCounter::FPSCounter()
{
    Reset();
}

void ng::gui::FPSCounter::Reset()
{
    m_lastTime = Clock::now();
    m_lastFPSUpdate = m_lastTime;
    m_frames = 0;
    m_fps = 0.0;
    m_deltaTime = 0.0;
}

void ng::gui::FPSCounter::Update()
{
    auto now = Clock::now();

    // Zeit seit letztem Frame
    m_deltaTime = std::chrono::duration<double>(now - m_lastTime).count();
    m_lastTime = now;

    m_frames++;

    // FPS einmal pro Sekunde aktualisieren
    double elapsed = std::chrono::duration<double>(now - m_lastFPSUpdate).count();

    if (elapsed >= 1.0)
    {
        m_fps = m_frames / elapsed;

        m_frames = 0;
        m_lastFPSUpdate = now;
    }
}

double ng::gui::FPSCounter::FPS() const
{
    return m_fps;
}

double ng::gui::FPSCounter::DeltaTime() const
{
    return m_deltaTime;
}

void ng::gui::ViewVector3(const std::string& name, const ng::math::Vector3d& v)
{
    ImGui::Text("%s x:%f y:%f z:%f", name.c_str(), v.x(), v.y(), v.z());
}

void ng::gui::ViewMatrix4(const std::string& name, const ng::math::Matrix4d& v)
{
    ImGui::Text(name.c_str());
    ImGui::Text("%10.4f %10.4f %10.4f %10.4f", v.data()[0], v.data()[1], v.data()[2], v.data()[3]);
    ImGui::Text("%10.4f %10.4f %10.4f %10.4f", v.data()[4], v.data()[5], v.data()[6], v.data()[7]);
    ImGui::Text("%10.4f %10.4f %10.4f %10.4f", v.data()[8], v.data()[9], v.data()[10], v.data()[11]);
    ImGui::Text("%10.4f %10.4f %10.4f %10.4f", v.data()[12], v.data()[13], v.data()[14], v.data()[15]);
}

void ng::gui::ViewVector3(const std::string& name, const ng::math::Vector3f& v)
{
    ImGui::Text("%s x:%f y:%f z:%f", name.c_str(), v.x(), v.y(), v.z());
}

void ng::gui::ViewMatrix4(const std::string& name, const ng::math::Matrix4f& v)
{
    ImGui::Text(name.c_str());
    ImGui::Text("%10.4f %10.4f %10.4f %10.4f", v.data()[0], v.data()[1], v.data()[2], v.data()[3]);
    ImGui::Text("%10.4f %10.4f %10.4f %10.4f", v.data()[4], v.data()[5], v.data()[6], v.data()[7]);
    ImGui::Text("%10.4f %10.4f %10.4f %10.4f", v.data()[8], v.data()[9], v.data()[10], v.data()[11]);
    ImGui::Text("%10.4f %10.4f %10.4f %10.4f", v.data()[12], v.data()[13], v.data()[14], v.data()[15]);
}

template<typename T>
static ImVec2 ProjectIso(
    const ng::math::Matrix<3, 1, T>& p,
    const ImVec2& center,
    float scale)
{
    float x = (float)(p.x() - p.z() * 0.5);
    float y = (float)(-p.y() + p.z() * 0.5);

    return ImVec2(
        center.x + x * scale,
        center.y + y * scale);
}

void ng::gui::DrawDirectionIndicator3D(float size, ng::math::Vector3d direction)
{
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(size, size));

    ImDrawList* draw = ImGui::GetWindowDrawList();




    ImVec2 center(
        pos.x + size * 0.5f,
        pos.y + size * 0.5f);

    float scale = size * 0.3f;

    draw->AddCircle(center, scale, IM_COL32(255, 0, 0, 255 / 2));
    draw->AddEllipse(center, ImVec2(scale / 2, scale), IM_COL32(0, 255, 0, 255 / 2));
    draw->AddEllipse(center, ImVec2(scale, scale / 2), IM_COL32(0, 0, 255, 255 / 2));


    auto ToScreen = [&](const ng::math::Vector3d& p)
        {
            ImVec2 v = ProjectIso(p);

            return ImVec2(
                center.x + v.x * scale,
                center.y + v.y * scale);
        };

    // Achsen

    draw->AddLine(
        ToScreen({ 0,0,0 }),
        ToScreen({ 1,0,0 }),
        IM_COL32(255, 0, 0, 255),
        2.0f);

    draw->AddLine(
        ToScreen({ 0,0,0 }),
        ToScreen({ 0,1,0 }),
        IM_COL32(0, 255, 0, 255),
        2.0f);

    draw->AddLine(
        ToScreen({ 0,0,0 }),
        ToScreen({ 0,0,1 }),
        IM_COL32(0, 128, 255, 255),
        2.0f);

    // Richtungsvektor

    double len = sqrt(
        direction.x() * direction.x() +
        direction.y() * direction.y() +
        direction.z() * direction.z());

    if (len > 0.00001)
    {
        ng::math::Vector3d dir =
        {
            direction.x() / len,
            direction.y() / len,
            direction.z() / len
        };
        int c = std::clamp((int)(128.0 + 127.0 * direction.z()), 0, 255);


        draw->AddLine(
            ToScreen({ 0,0,0 }),
            ToScreen(dir),
            IM_COL32(c, c, c, 255),
            3.0f);
    }



    draw->AddCircleFilled(
        center,
        3.0f,
        IM_COL32(255, 255, 255, 255));
}

void ng::gui::DrawDirectionIndicator3D(float size, ng::math::Vector3f direction)
{
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(size, size));

    ImDrawList* draw = ImGui::GetWindowDrawList();




    ImVec2 center(
        pos.x + size * 0.5f,
        pos.y + size * 0.5f);

    float scale = size * 0.3f;

    draw->AddCircle(center, scale, IM_COL32(255, 0, 0, 255 / 2));
    draw->AddEllipse(center, ImVec2(scale / 2, scale), IM_COL32(0, 255, 0, 255 / 2));
    draw->AddEllipse(center, ImVec2(scale, scale / 2), IM_COL32(0, 0, 255, 255 / 2));


    auto ToScreen = [&](const ng::math::Vector3f& p)
        {
            ImVec2 v = ProjectIso(p);

            return ImVec2(
                center.x + v.x * scale,
                center.y + v.y * scale);
        };

    // Achsen

    draw->AddLine(
        ToScreen({ 0,0,0 }),
        ToScreen({ 1,0,0 }),
        IM_COL32(255, 0, 0, 255),
        2.0f);

    draw->AddLine(
        ToScreen({ 0,0,0 }),
        ToScreen({ 0,1,0 }),
        IM_COL32(0, 255, 0, 255),
        2.0f);

    draw->AddLine(
        ToScreen({ 0,0,0 }),
        ToScreen({ 0,0,1 }),
        IM_COL32(0, 128, 255, 255),
        2.0f);

    // Richtungsvektor

    float len = sqrt(
        direction.x() * direction.x() +
        direction.y() * direction.y() +
        direction.z() * direction.z());

    if (len > 0.00001)
    {
        ng::math::Vector3f dir =
        {
            direction.x() / len,
            direction.y() / len,
            direction.z() / len
        };
        int c = std::clamp((int)(128.0 + 127.0 * direction.z()), 0, 255);


        draw->AddLine(
            ToScreen({ 0,0,0 }),
            ToScreen(dir),
            IM_COL32(c, c, c, 255),
            3.0f);
    }



    draw->AddCircleFilled(
        center,
        3.0f,
        IM_COL32(255, 255, 255, 255));
}

