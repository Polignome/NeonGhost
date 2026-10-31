#pragma once
#include <string>
#include <vector>
#include <stdexcept>
#include <map>
#include <functional>




namespace ng::app
{
    struct ArgumentDefinition
    {
        std::string description;

        // Callback für die Verarbeitung des Arguments
        std::function<void(const std::string& value)> callback;
    };

    using ArgumentMap = std::map<std::string, ArgumentDefinition>;
}

namespace ng::app
{
	enum class ErrorAction
	{
		Continue,   // Fehler wurde behandelt, weiterlaufen
		Abort       // Anwendung beenden
	};

	struct ErrorInfo
	{
		std::string message;
		std::string source;
	};

}



namespace ng::app
{
    class DarkGhostNeonBaseApplication
    {
    public:
        DarkGhostNeonBaseApplication(int argc, char** argv);
        virtual ~DarkGhostNeonBaseApplication() = default;

        // Zentraler Einstiegspunkt mit Exception-Behandlung
        int Run();

    protected:






        void ParseArguments();


        void RegisterArgument(
            const std::string& name,
            const std::string& description,
            std::function<void(const std::string&)> callback);

            void PrintHelp() const;
        
        
            virtual void Event_InitArguments();
            virtual ErrorAction Event_Error(const ErrorInfo& error);


        // Hier läuft die eigentliche Anwendung
            virtual int Event_Run();

        // Einheitlicher Fehler-Einstieg für Engine-Module
        ErrorAction ReportError(
            const std::string& message,
            const std::string& source = "Application");





    private:
        std::vector<std::string> m_arguments;
        std::map<std::string, ArgumentDefinition> m_argumentDefinitions;
    };
}