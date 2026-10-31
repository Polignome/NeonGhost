
#include "Global.h"
#include "DarkGhostNeonBaseApplication.h"

#include <iostream>
#include <exception>

namespace ng::app
{

    void DarkGhostNeonBaseApplication::Event_InitArguments()
    {
        RegisterArgument(
            "-help",
            "Zeigt die Hilfe an",
            [this](const std::string&)
            {
                PrintHelp();
            });

        RegisterArgument(
            "-test",
            "gibt eine string aus -test [string]",
            [this](const std::string& s)
            {
                std::cout << "Test Output " << s << std::endl;
            });

    }


    DarkGhostNeonBaseApplication::DarkGhostNeonBaseApplication(int argc, char** argv)
       
    {
        m_arguments.reserve(argc);

        for (int i = 0; i < argc; ++i)
        {
            m_arguments.emplace_back(argv[i]);
        }
    }

    ErrorAction DarkGhostNeonBaseApplication::Event_Error(
        const ErrorInfo& error)
    {
        // Standardverhalten der BaseApplication
        std::cerr << "[NeonGhost Error] "
            << error.source << ": "
            << error.message << '\n';

        return ErrorAction::Abort;
    }

    ErrorAction DarkGhostNeonBaseApplication::ReportError(
        const std::string& message,
        const std::string& source)
    {
        return Event_Error({ message, source });
    }

    int DarkGhostNeonBaseApplication::Event_Run()
    {
        // Hier später Initialisierung, Game Loop usw.
        return 0;
    }

    int DarkGhostNeonBaseApplication::Run()
    {
        try
        {
            
            Event_InitArguments();
            ParseArguments();
            


            return Event_Run();
        }
        catch (const std::exception& e)
        {
            if (ReportError(e.what(), "Exception") == ErrorAction::Abort)
                return -1;

            return 0;
        }
        catch (...)
        {
            if (ReportError("Unbekannte Exception", "Exception")
                == ErrorAction::Abort)
            {
                return -1;
            }

            return 0;
        }
    }
    
    void DarkGhostNeonBaseApplication::RegisterArgument(
        const std::string& name,
        const std::string& description,
        std::function<void(const std::string&)> callback)
    {
        m_argumentDefinitions[name] = {
            description,
            std::move(callback)
        };
    }

    void DarkGhostNeonBaseApplication::PrintHelp() const
    {
        for (const auto& [name, definition] : m_argumentDefinitions)
        {
            std::cout << "  "
                << name << "\t"
                << definition.description
                << '\n';
        }
    }

    void DarkGhostNeonBaseApplication::ParseArguments()
    {
        size_t i = 1;

        while (i < m_arguments.size())
        {
            const std::string& name = m_arguments[i];

            // Leere oder ungültige Argumentnamen
            if (name.empty() || name[0] != '-')
            {
                ReportError("Unerwarteter Wert ohne Argument: " + name);
                break;
            }

            auto it = m_argumentDefinitions.find(name);

            if (it == m_argumentDefinitions.end())
            {
                ReportError("Unbekanntes Argument: " + name);
                break;
            }

            std::string value;

            // Nächstes Token prüfen
            if (i + 1 < m_arguments.size())
            {
                const std::string& next = m_arguments[i + 1];

                // Ist das nächste Token kein Argumentname, ist es ein Wert.
                if (next.empty() || next[0] != '-')
                {
                    value = next;
                    ++i;

                    // Prüfen, ob danach unerlaubt ein weiterer Wert folgt.
                    if (i + 1 < m_arguments.size())
                    {
                        const std::string& following = m_arguments[i + 1];

                        if (!following.empty() && following[0] != '-')
                        {
                            ReportError(
                                "Unerwarteter zusätzlicher Wert: " + following);
                            break;
                        }
                    }
                }
            }

            try
            {
                it->second.callback(value);
            }
            catch (const std::exception& e)
            {
                ReportError("Fehler bei " + name + ": " + e.what());
                break;
            }

            ++i;
        }
    }


}