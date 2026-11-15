
#pragma once

#include "Console.h"
#include "imgui.h"
#include <algorithm>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

namespace ng::console
{

    class ConsoleImGui
    {
    public:

        explicit ConsoleImGui(Console& console)
            : m_console(console)
        {
            m_input[0] = '\0';
        }


        void SetOpen(bool open)
        {
            m_open = open;
        }


        bool IsOpen() const
        {
            return m_open;
        }


        bool* GetOpenPtr()
        {
            return &m_open;
        }


        void Draw(const char* title = "NeonGhost Console")
        {
            if (!m_open)
                return;

            ImGui::SetNextWindowSize(
                ImVec2(760.0f, 480.0f),
                ImGuiCond_FirstUseEver
            );

            if (!ImGui::Begin(title, &m_open))
            {
                ImGui::End();
                return;
            }

            // Vorschläge aktualisieren.
            UpdateCompletionCandidates();

            // Platz für das Dropdown reservieren.
            const float dropdownHeight =
                GetCompletionDropdownHeight();

            // Ausgabe mit entsprechend reduzierter Höhe zeichnen.
            DrawOutput(dropdownHeight);

            ImGui::Separator();

            ImGui::SetNextItemWidth(-1.0f);

            if (m_focusInputNextFrame)
            {
                ImGui::SetKeyboardFocusHere();
                m_focusInputNextFrame = false;
            }

            const ImGuiInputTextFlags flags =
                ImGuiInputTextFlags_EnterReturnsTrue |
                ImGuiInputTextFlags_CallbackCompletion |
                ImGuiInputTextFlags_CallbackHistory;

            const bool submitted = ImGui::InputText(
                "##ConsoleInput",
                m_input,
                sizeof(m_input),
                flags,
                InputCallback,
                this
            );

            if (submitted)
            {
                if (m_input[0] != '\0')
                {
                    m_console.Submit(m_input);
                    m_scrollToBottom = true;
                }

                m_input[0] = '\0';
                m_historyPosition = -1;
                m_completionCandidates.clear();

                ImGui::SetKeyboardFocusHere(-1);
            }

            // Dropdown direkt unter dem Eingabefeld.
            DrawCompletionDropdown();

            ImGui::End();
        }


    private:

        // ------------------------------------------------------------
        // Farben
        // ------------------------------------------------------------

        static constexpr ImVec4 COLOR_COMMAND =
            ImVec4(0.35f, 0.75f, 1.00f, 1.0f);

        static constexpr ImVec4 COLOR_FUNCTION =
            ImVec4(1.00f, 0.72f, 0.30f, 1.0f);

        static constexpr ImVec4 COLOR_VARIABLE =
            ImVec4(0.45f, 0.85f, 0.55f, 1.0f);

        static constexpr ImVec4 COLOR_VALUE =
            ImVec4(0.90f, 0.90f, 0.90f, 1.0f);

        static constexpr ImVec4 COLOR_DESCRIPTION =
            ImVec4(0.60f, 0.65f, 0.72f, 1.0f);

        static constexpr ImVec4 COLOR_ERROR =
            ImVec4(1.00f, 0.35f, 0.35f, 1.0f);


        // ------------------------------------------------------------
        // Hilfsfunktionen
        // ------------------------------------------------------------

        static std::string Trim(const std::string& value)
        {
            const size_t first =
                value.find_first_not_of(" \t\r\n");

            if (first == std::string::npos)
                return {};

            const size_t last =
                value.find_last_not_of(" \t\r\n");

            return value.substr(
                first,
                last - first + 1
            );
        }


        static bool IsRegistryLine(const std::string& line)
        {
            return
                line.find("<function>") != std::string::npos ||
                line.find(" = ") != std::string::npos;
        }


        


        // ------------------------------------------------------------
        // Ausgabe
        // ------------------------------------------------------------

        void DrawOutput(float dropdownHeight)
        {
            const float inputHeight =
                ImGui::GetFrameHeightWithSpacing() + 12.0f;

            ImGui::BeginChild(
                "##ConsoleOutput",
                ImVec2(
                    0.0f,
                    -inputHeight - dropdownHeight
                ),
                false,
                ImGuiWindowFlags_HorizontalScrollbar
            );

            const auto output = m_console.GetOutput();

            bool registryTableOpen = false;

            for (const std::string& outputEntry : output)
            {
                std::istringstream stream(outputEntry);
                std::string line;

                while (std::getline(stream, line))
                {
                    if (!line.empty() && line.back() == '\r')
                        line.pop_back();

                    line = Trim(line);

                    if (line.empty())
                        continue;

                    if (IsRegistryLine(line))
                    {
                        if (!registryTableOpen)
                            registryTableOpen = BeginRegistryTable();

                        if (registryTableOpen)
                            DrawRegistryRow(line);
                        else
                            DrawOutputLine(line);
                    }
                    else
                    {
                        if (registryTableOpen)
                        {
                            ImGui::EndTable();
                            registryTableOpen = false;
                        }

                        DrawOutputLine(line);
                    }
                }
            }

            if (registryTableOpen)
                ImGui::EndTable();

            if (m_scrollToBottom)
            {
                ImGui::SetScrollHereY(1.0f);
                m_scrollToBottom = false;
            }

            ImGui::EndChild();
        }
        float GetCompletionDropdownHeight() const
        {
            if (m_completionCandidates.empty())
                return 0.0f;

            const size_t count =
                std::min<size_t>(
                    m_completionCandidates.size(),
                    6
                );

            return
                static_cast<float>(count) *
                ImGui::GetTextLineHeightWithSpacing()
                + ImGui::GetStyle().WindowPadding.y * 2.0f;
        }

        bool BeginRegistryTable()
        {
            if (!ImGui::BeginTable(
                "##ConsoleRegistry",
                4,
                ImGuiTableFlags_RowBg |
                ImGuiTableFlags_BordersInnerV |
                ImGuiTableFlags_SizingFixedFit |
                ImGuiTableFlags_Resizable
            ))
            {
                return false;
            }

            ImGui::TableSetupColumn(
                "Name",
                ImGuiTableColumnFlags_WidthFixed,
                210.0f
            );

            ImGui::TableSetupColumn(
                "Typ",
                ImGuiTableColumnFlags_WidthFixed,
                90.0f
            );

            ImGui::TableSetupColumn(
                "Wert",
                ImGuiTableColumnFlags_WidthFixed,
                150.0f
            );

            ImGui::TableSetupColumn(
                "Beschreibung",
                ImGuiTableColumnFlags_WidthStretch
            );

            ImGui::TableHeadersRow();

            return true;
        }


        void DrawRegistryRow(const std::string& source)
        {
            std::string line = source;
            std::string name;
            std::string value;
            std::string description;
            bool isFunction = false;

            // Beschreibung abtrennen.
            const size_t hashPosition = line.find('#');

            if (hashPosition != std::string::npos)
            {
                description = Trim(
                    line.substr(hashPosition + 1)
                );

                line = Trim(
                    line.substr(0, hashPosition)
                );
            }

            // Funktion erkennen.
            const size_t functionPosition =
                line.find("<function>");

            if (functionPosition != std::string::npos)
            {
                name = Trim(
                    line.substr(0, functionPosition)
                );

                isFunction = true;
            }
            else
            {
                // Variable erkennen.
                const size_t equalPosition =
                    line.find(" = ");

                if (equalPosition == std::string::npos)
                    return;

                name = Trim(
                    line.substr(0, equalPosition)
                );

                value = Trim(
                    line.substr(equalPosition + 3)
                );
            }

            ImGui::TableNextRow();

            // Name
            ImGui::TableSetColumnIndex(0);

            ImGui::TextColored(
                isFunction ? COLOR_COMMAND : COLOR_VARIABLE,
                "%s",
                name.c_str()
            );

            // Typ
            ImGui::TableSetColumnIndex(1);

            if (isFunction)
            {
                ImGui::TextColored(
                    COLOR_FUNCTION,
                    "function"
                );
            }
            else
            {
                ImGui::TextDisabled("variable");
            }

            // Wert
            ImGui::TableSetColumnIndex(2);

            if (!value.empty())
            {
                ImGui::TextColored(
                    COLOR_VALUE,
                    "%s",
                    value.c_str()
                );
            }

            // Beschreibung
            ImGui::TableSetColumnIndex(3);

            if (!description.empty())
            {
                ImGui::TextColored(
                    COLOR_DESCRIPTION,
                    "%s",
                    description.c_str()
                );
            }
        }


        void DrawOutputLine(const std::string& line)
        {
            if (line.rfind("> ", 0) == 0)
            {
                ImGui::TextColored(
                    COLOR_COMMAND,
                    "%s",
                    line.c_str()
                );
            }
            else if (
                line.find("error") != std::string::npos ||
                line.find("Error") != std::string::npos
                )
            {
                ImGui::TextColored(
                    COLOR_ERROR,
                    "%s",
                    line.c_str()
                );
            }
            else
            {
                ImGui::TextUnformatted(line.c_str());
            }
        }


        // ------------------------------------------------------------
        // Autocomplete
        // ------------------------------------------------------------

        std::string GetCurrentToken() const
        {
            const std::string input = m_input;

            const size_t position =
                input.find_last_of(" \t");

            if (position == std::string::npos)
                return input;

            return input.substr(position + 1);
        }


        void ReplaceCurrentToken(const std::string& completion)
        {
            const std::string input = m_input;

            const size_t position =
                input.find_last_of(" \t");

            std::string result;

            if (position == std::string::npos)
            {
                result = completion;
            }
            else
            {
                result =
                    input.substr(0, position + 1) +
                    completion;
            }

            std::snprintf(
                m_input,
                sizeof(m_input),
                "%s",
                result.c_str()
            );

            m_focusInputNextFrame = true;
        }


        void UpdateCompletionCandidates()
        {
            const std::string prefix = GetCurrentToken();

            m_completionCandidates.clear();

            if (prefix.empty())
                return;

            m_completionCandidates =
                m_console.GetCompletions(prefix);

            // Exakte Treffer nicht erneut anzeigen.
            m_completionCandidates.erase(
                std::remove(
                    m_completionCandidates.begin(),
                    m_completionCandidates.end(),
                    prefix
                ),
                m_completionCandidates.end()
            );
        }


        void DrawCompletionDropdown()
        {
            if (m_completionCandidates.empty())
                return;

            const size_t visibleCount =
                std::min<size_t>(
                    m_completionCandidates.size(),
                    6
                );

            const float height =
                static_cast<float>(visibleCount) *
                ImGui::GetTextLineHeightWithSpacing() +
                ImGui::GetStyle().WindowPadding.y * 2.0f;

            ImGui::BeginChild(
                "##ConsoleCompletionDropdown",
                ImVec2(0.0f, height),
                true
            );

            for (const std::string& candidate :
                m_completionCandidates)
            {
                if (ImGui::Selectable(
                    candidate.c_str(),
                    false
                ))
                {
                    ReplaceCurrentToken(candidate);
                    m_completionCandidates.clear();
                    break;
                }
            }

            ImGui::EndChild();
        }


        // ------------------------------------------------------------
        // Eingabehistorie
        // ------------------------------------------------------------

        void NavigateHistory(int direction)
        {
            const auto& history = m_console.GetHistory();

            if (history.empty())
                return;

            if (direction < 0)
            {
                if (m_historyPosition == -1)
                {
                    m_historyPosition =
                        static_cast<int>(history.size()) - 1;
                }
                else if (m_historyPosition > 0)
                {
                    --m_historyPosition;
                }
            }
            else
            {
                if (m_historyPosition >= 0)
                {
                    ++m_historyPosition;

                    if (m_historyPosition >=
                        static_cast<int>(history.size()))
                    {
                        m_historyPosition = -1;
                        m_input[0] = '\0';
                        return;
                    }
                }
            }

            if (m_historyPosition >= 0)
            {
                std::snprintf(
                    m_input,
                    sizeof(m_input),
                    "%s",
                    history[m_historyPosition].c_str()
                );
            }
        }


        // ------------------------------------------------------------
        // ImGui Input Callback
        // ------------------------------------------------------------

        static int InputCallback(ImGuiInputTextCallbackData* data)
        {
            auto* console =
                static_cast<ConsoleImGui*>(data->UserData);

            if (!console)
                return 0;

            switch (data->EventFlag)
            {
            case ImGuiInputTextFlags_CallbackCompletion:
            {
                console->UpdateCompletionCandidates();

                if (console->m_completionCandidates.size() == 1)
                {
                    console->ReplaceCurrentToken(
                        console->m_completionCandidates.front()
                    );

                    data->DeleteChars(
                        0,
                        data->BufTextLen
                    );

                    data->InsertChars(
                        0,
                        console->m_input
                    );
                }
                else if (
                    console->m_completionCandidates.size() > 1
                    )
                {
                    // Bei mehreren Treffern bleibt die Eingabe
                    // unverändert. Das Dropdown zeigt die Auswahl.
                }

                break;
            }

            case ImGuiInputTextFlags_CallbackHistory:
            {
                if (data->EventKey == ImGuiKey_UpArrow)
                {
                    console->NavigateHistory(-1);
                }
                else if (data->EventKey == ImGuiKey_DownArrow)
                {
                    console->NavigateHistory(1);
                }

                data->DeleteChars(
                    0,
                    data->BufTextLen
                );

                data->InsertChars(
                    0,
                    console->m_input
                );

                break;
            }
            }

            return 0;
        }


    private:

        Console& m_console;

        bool m_open = true;
        bool m_scrollToBottom = true;
        bool m_focusInputNextFrame = false;

        int m_historyPosition = -1;

        char m_input[512]{};

        std::vector<std::string> m_completionCandidates;
    };

}



