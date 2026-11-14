
#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <map>
#include <cctype>
#include <StdOutputBypass.h>
namespace ng::console
{
    class ConsoleImGui : public ng::io::StdOutputBypass
    {
    public:

        explicit ConsoleImGui(Console& console)
            : m_console(console)
        {
            m_input[0] = '\0';
        }

        // ------------------------------------------------------------
        // Thread-Ausgabe
        // ------------------------------------------------------------

        struct ThreadOutput
        {
            std::string line;
            bool carriageReturn = false;
        };

        // ------------------------------------------------------------
        // Zeichen empfangen
        // Jeder Thread bekommt einen eigenen Zeilenpuffer.
        // ------------------------------------------------------------

        void AddCharacter(char c)
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            auto& buffer =
                m_threadOutput[std::this_thread::get_id()];

            // Wagenrücklauf
            if (c == '\r')
            {
                buffer.carriageReturn = true;
                return;
            }

            // Zeilenumbruch
            if (c == '\n')
            {
                m_console.PushOutput(buffer.line);

                buffer.line.clear();
                buffer.carriageReturn = false;

                m_scrollToBottom = true;
                return;
            }

            // Zeile überschreiben
            if (buffer.carriageReturn)
            {
                buffer.line.clear();
                buffer.carriageReturn = false;
            }

            // Backspace
            if (c == '\b')
            {
                if (!buffer.line.empty())
                    buffer.line.pop_back();

                return;
            }

            // Normale Zeichen und Tabs
            if (c >= 32 || c == '\t')
            {
                buffer.line += c;
            }
        }

        // ------------------------------------------------------------
        // Fenstersteuerung
        // ------------------------------------------------------------

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

        // ------------------------------------------------------------
        // ImGui Draw
        // Einmal pro Frame auf dem Hauptthread aufrufen.
        // ------------------------------------------------------------

        void Draw(const char* title = "NeonGhost Console")
        {
            if (!m_open)
                return;

            ImGui::SetNextWindowSize(
                ImVec2(680.0f, 420.0f),
                ImGuiCond_FirstUseEver
            );

            if (ImGui::Begin(title, &m_open))
            {
                DrawOutput();

                // Completion-Hinweis
                if (m_showCompletionHint &&
                    !m_completionMessage.empty())
                {
                    ImGui::TextDisabled(
                        "%s",
                        m_completionMessage.c_str()
                    );

                    m_showCompletionHint = false;
                }

                ImGui::Separator();

                ImGui::SetNextItemWidth(-1.0f);

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
                    }

                    m_input[0] = '\0';
                    m_historyPosition = -1;
                    m_scrollToBottom = true;

                    ImGui::SetKeyboardFocusHere(-1);
                }
            }

            ImGui::End();
        }

    protected:

        // ------------------------------------------------------------
        // StdOutputBypass Callbacks
        // ------------------------------------------------------------

        int onCoutOverflow(char c) override
        {
            AddCharacter(c);
            return 0;
        }

        int onCerrOverflow(char c) override
        {
            AddCharacter(c);
            return 0;
        }

        int onClogOverflow(char c) override
        {
            AddCharacter(c);
            return 0;
        }

        int onPrintfOverflow(char c) override
        {
            AddCharacter(c);
            return 0;
        }

    private:

        // ------------------------------------------------------------
        // Daten
        // ------------------------------------------------------------

        Console& m_console;

        std::mutex m_mutex;

        // Separater Puffer pro Thread
        std::map<std::thread::id, ThreadOutput> m_threadOutput;

        bool m_open = true;
        bool m_scrollToBottom = true;

        bool m_showCompletionHint = false;

        int m_historyPosition = -1;

        char m_input[512]{};

        std::string m_completionMessage;

        // ------------------------------------------------------------
        // Farben
        // ------------------------------------------------------------

        static constexpr ImVec4 COLOR_COMMAND =
            ImVec4(0.40f, 0.71f, 1.00f, 1.00f);

        static constexpr ImVec4 COLOR_PARAMETER =
            ImVec4(0.90f, 0.72f, 0.42f, 1.00f);

        static constexpr ImVec4 COLOR_VARIABLE =
            ImVec4(0.46f, 0.84f, 0.63f, 1.00f);

        static constexpr ImVec4 COLOR_VALUE =
            ImVec4(0.92f, 0.92f, 0.92f, 1.00f);

        static constexpr ImVec4 COLOR_DESCRIPTION =
            ImVec4(0.55f, 0.59f, 0.65f, 1.00f);

        static constexpr ImVec4 COLOR_ERROR =
            ImVec4(1.00f, 0.35f, 0.35f, 1.00f);

        static constexpr ImVec4 COLOR_NORMAL =
            ImVec4(0.80f, 0.82f, 0.85f, 1.00f);

        // ------------------------------------------------------------
        // Farbiger Text
        // ------------------------------------------------------------

        static void TextColored(
            const ImVec4& color,
            const std::string& text)
        {
            ImGui::TextColored(
                color,
                "%s",
                text.c_str()
            );
        }

        // ------------------------------------------------------------
        // Ausgabe einer Befehlszeile
        // ------------------------------------------------------------

        void DrawCommandLine(const std::string& line)
        {
            if (line.size() < 3)
            {
                TextColored(COLOR_NORMAL, line);
                return;
            }

            ImGui::TextColored(
                COLOR_DESCRIPTION,
                ">"
            );

            ImGui::SameLine(0.0f, 6.0f);

            const std::size_t separator = line.find(' ', 2);

            if (separator == std::string::npos)
            {
                TextColored(
                    COLOR_COMMAND,
                    line.substr(2)
                );
                return;
            }

            TextColored(
                COLOR_COMMAND,
                line.substr(2, separator - 2)
            );

            ImGui::SameLine(0.0f, 8.0f);

            TextColored(
                COLOR_PARAMETER,
                line.substr(separator + 1)
            );
        }

        // ------------------------------------------------------------
        // Ausgabe einer Variablen
        // ------------------------------------------------------------

        void DrawVariableLine(const std::string& line)
        {
            const std::size_t equal = line.find('=');

            if (equal == std::string::npos)
            {
                TextColored(COLOR_NORMAL, line);
                return;
            }

            TextColored(
                COLOR_VARIABLE,
                line.substr(0, equal)
            );

            ImGui::SameLine(0.0f, 4.0f);

            TextColored(
                COLOR_DESCRIPTION,
                "="
            );

            ImGui::SameLine(0.0f, 4.0f);

            TextColored(
                COLOR_VALUE,
                line.substr(equal + 1)
            );
        }

        // ------------------------------------------------------------
        // Ausgabe einer List-Zeile
        // ------------------------------------------------------------

        void DrawListLine(const std::string& line)
        {
            const std::size_t description = line.find('#');

            std::string content =
                description == std::string::npos
                ? line
                : line.substr(0, description);

            std::string desc =
                description == std::string::npos
                ? ""
                : line.substr(description + 1);

            while (!content.empty() &&
                std::isspace(static_cast<unsigned char>(content.back())))
            {
                content.pop_back();
            }

            const std::size_t first =
                content.find_first_not_of(" \t");

            if (first == std::string::npos)
                return;

            content.erase(0, first);

            const std::size_t equal = content.find('=');

            if (equal != std::string::npos)
            {
                std::string variable = content.substr(0, equal);
                std::string value = content.substr(equal + 1);

                auto Trim = [](std::string& str)
                    {
                        const auto start =
                            str.find_first_not_of(" \t");

                        const auto end =
                            str.find_last_not_of(" \t");

                        if (start == std::string::npos)
                        {
                            str.clear();
                            return;
                        }

                        str = str.substr(start, end - start + 1);
                    };

                Trim(variable);
                Trim(value);

                ImGui::TextColored(
                    COLOR_VARIABLE,
                    "%-24s",
                    variable.c_str()
                );

                ImGui::SameLine();

                ImGui::TextColored(
                    COLOR_DESCRIPTION,
                    "="
                );

                ImGui::SameLine();

                ImGui::TextColored(
                    COLOR_VALUE,
                    "%s",
                    value.c_str()
                );
            }
            else
            {
                const std::size_t function =
                    content.find("<function>");

                if (function != std::string::npos)
                {
                    ImGui::TextColored(
                        COLOR_COMMAND,
                        "%-24s",
                        content.substr(0, function).c_str()
                    );

                    ImGui::SameLine();

                    ImGui::TextColored(
                        COLOR_PARAMETER,
                        "<function>"
                    );
                }
                else
                {
                    TextColored(COLOR_NORMAL, content);
                }
            }

            if (!desc.empty())
            {
                ImGui::SameLine(0.0f, 12.0f);

                TextColored(
                    COLOR_DESCRIPTION,
                    "# " + desc
                );
            }
        }

        // ------------------------------------------------------------
        // Allgemeine Ausgabe
        // ------------------------------------------------------------

        void DrawOutputLine(const std::string& line)
        {
            if (line.empty())
            {
                ImGui::Dummy(ImVec2(0.0f, 4.0f));
                return;
            }

            if (line.rfind("> ", 0) == 0)
            {
                DrawCommandLine(line);
                return;
            }

            if (line.find("Error:") != std::string::npos ||
                line.find("Unknown command") == 0 ||
                line.find("Invalid value") == 0 ||
                line.find("Wrong argument") == 0 ||
                line.find("Could not parse") == 0)
            {
                TextColored(COLOR_ERROR, line);
                return;
            }

            if (line.find("<function>") != std::string::npos ||
                line.find(" # ") != std::string::npos)
            {
                DrawListLine(line);
                return;
            }

            if (line.find(" = ") != std::string::npos)
            {
                DrawVariableLine(line);
                return;
            }

            TextColored(COLOR_NORMAL, line);
        }

        // ------------------------------------------------------------
        // Konsolenausgabe
        // ------------------------------------------------------------

        void DrawOutput()
        {
            const float inputHeight =
                ImGui::GetFrameHeightWithSpacing() +
                ImGui::GetStyle().ItemSpacing.y;

            ImGui::BeginChild(
                "##ConsoleOutput",
                ImVec2(0.0f, -inputHeight),
                true,
                ImGuiWindowFlags_HorizontalScrollbar
            );

            auto output = m_console.GetOutput();

            bool scrollToBottom = false;

            {
                std::lock_guard<std::mutex> lock(m_mutex);

                // Alle aktuellen Thread-Zeilen anzeigen.
                for (const auto& entry : m_threadOutput)
                {
                    const auto& line = entry.second.line;

                    if (!line.empty())
                        output.push_back(line);
                }

                scrollToBottom = m_scrollToBottom;
                m_scrollToBottom = false;
            }

            for (const std::string& line : output)
            {
                DrawOutputLine(line);
            }

            if (scrollToBottom ||
                ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            {
                ImGui::SetScrollHereY(1.0f);
            }

            ImGui::EndChild();
        }

        // ------------------------------------------------------------
        // Input Callback
        // ------------------------------------------------------------

        static int InputCallback(
            ImGuiInputTextCallbackData* data)
        {
            auto* self =
                static_cast<ConsoleImGui*>(data->UserData);

            if (data->EventFlag ==
                ImGuiInputTextFlags_CallbackCompletion)
            {
                self->CompleteInput(data);
            }
            else if (data->EventFlag ==
                ImGuiInputTextFlags_CallbackHistory)
            {
                self->NavigateHistory(data);
            }

            return 0;
        }

        // ------------------------------------------------------------
        // Autocomplete
        // ------------------------------------------------------------

        void CompleteInput(
            ImGuiInputTextCallbackData* data)
        {
            int wordStart = data->CursorPos;

            while (wordStart > 0)
            {
                const char c = data->Buf[wordStart - 1];

                if (c == ' ' || c == '\t')
                    break;

                --wordStart;
            }

            const std::string prefix(
                data->Buf + wordStart,
                data->CursorPos - wordStart
            );

            if (prefix.empty())
                return;

            const auto matches =
                m_console.GetCompletions(prefix);

            if (matches.empty())
                return;

            if (matches.size() == 1)
            {
                data->DeleteChars(
                    wordStart,
                    data->CursorPos - wordStart
                );

                data->InsertChars(
                    wordStart,
                    matches.front().c_str()
                );

                return;
            }

            m_completionMessage.clear();

            for (const auto& match : matches)
            {
                if (!m_completionMessage.empty())
                    m_completionMessage += "    ";

                m_completionMessage += match;
            }

            m_showCompletionHint = true;
        }

        // ------------------------------------------------------------
        // History Navigation
        // ------------------------------------------------------------

        void NavigateHistory(
            ImGuiInputTextCallbackData* data)
        {
            const auto history =
                m_console.GetHistory();

            if (history.empty())
                return;

            if (data->EventKey == ImGuiKey_UpArrow)
            {
                if (m_historyPosition < 0)
                {
                    m_historyPosition =
                        static_cast<int>(history.size()) - 1;
                }
                else if (m_historyPosition > 0)
                {
                    --m_historyPosition;
                }
            }
            else if (data->EventKey == ImGuiKey_DownArrow)
            {
                if (m_historyPosition >= 0)
                {
                    ++m_historyPosition;

                    if (m_historyPosition >=
                        static_cast<int>(history.size()))
                    {
                        m_historyPosition = -1;
                    }
                }
            }

            const char* historyText =
                m_historyPosition >= 0
                ? history[
                    static_cast<std::size_t>(m_historyPosition)
                ].c_str()
                        : "";

                    data->DeleteChars(
                        0,
                        data->BufTextLen
                    );

                    data->InsertChars(
                        0,
                        historyText
                    );
        }
    };
}
