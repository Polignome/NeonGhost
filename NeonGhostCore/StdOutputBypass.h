
#pragma once

#include <iostream>
#include <fstream>
#include <streambuf>
#include <vector>
#include <string>
#include <mutex>
#include <atomic>
#include <thread>
#include <assert.h>

#ifdef _WIN32
#include <Windows.h>
#include <io.h>
#include <fcntl.h>
#include <cstdio>
#endif

namespace ng {
    namespace io {

        // ------------------------------------------------------------
        // COUT
        // ------------------------------------------------------------

        class NewCOUTStreambuffer : public std::streambuf
        {
        public:
            NewCOUTStreambuffer() = default;
            virtual ~NewCOUTStreambuffer() = default;

            inline std::streambuf* GetCoutBuffer()
            {
                return this;
            }

        protected:
            int_type overflow(int_type c) override
            {
                if (!traits_type::eq_int_type(c, traits_type::eof()))
                    onCoutOverflowPrivate(traits_type::to_char_type(c));

                return traits_type::not_eof(c);
            }

            virtual int onCoutOverflowPrivate(char c)
            {
                return 0;
            }
        };


        // ------------------------------------------------------------
        // CERR
        // ------------------------------------------------------------

        class NewCERRStreambuffer : public std::streambuf
        {
        public:
            NewCERRStreambuffer() = default;
            virtual ~NewCERRStreambuffer() = default;

            inline std::streambuf* GetCerrBuffer()
            {
                return this;
            }

        protected:
            int_type overflow(int_type c) override
            {
                if (!traits_type::eq_int_type(c, traits_type::eof()))
                    onCerrOverflowPrivate(traits_type::to_char_type(c));

                return traits_type::not_eof(c);
            }

            virtual int onCerrOverflowPrivate(char c)
            {
                return 0;
            }
        };


        // ------------------------------------------------------------
        // CLOG
        // ------------------------------------------------------------

        class NewCLOGStreambuffer : public std::streambuf
        {
        public:
            NewCLOGStreambuffer() = default;
            virtual ~NewCLOGStreambuffer() = default;

            inline std::streambuf* GetClogBuffer()
            {
                return this;
            }

        protected:
            int_type overflow(int_type c) override
            {
                if (!traits_type::eq_int_type(c, traits_type::eof()))
                    onClogOverflowPrivate(traits_type::to_char_type(c));

                return traits_type::not_eof(c);
            }

            virtual int onClogOverflowPrivate(char c)
            {
                return 0;
            }
        };


        // ------------------------------------------------------------
        // STD OUTPUT BYPASS
        // ------------------------------------------------------------

        class StdOutputBypass :
            private NewCOUTStreambuffer,
            private NewCERRStreambuffer,
            private NewCLOGStreambuffer
        {
        public:

            StdOutputBypass();
            virtual ~StdOutputBypass();

            StdOutputBypass(const StdOutputBypass&) = delete;
            StdOutputBypass& operator=(const StdOutputBypass&) = delete;


            static inline StdOutputBypass* GetPtr()
            {
                return m_ptr;
            }


            // --------------------------------------------------------
            // PRINTF BYPASS
            // --------------------------------------------------------

            bool EnablePrintfBypass();

            void DisablePrintfBypass();

            bool IsPrintfBypassEnabled() const
            {
#ifdef _WIN32
                return m_ptr &&
                    m_ptr->m_printfEnabled.load();
#else
                return false;
#endif
            }


        protected:

            // --------------------------------------------------------
            // OUTPUT CALLBACKS
            // --------------------------------------------------------

            virtual int onCoutOverflow(char c);
            virtual int onClogOverflow(char c);
            virtual int onCerrOverflow(char c);

            virtual int onPrintfOverflow(char c);


            // --------------------------------------------------------
            // PRIVATE CALLBACK DISPATCH
            // --------------------------------------------------------

            int onCoutOverflowPrivate(char c);
            int onCerrOverflowPrivate(char c);
            int onClogOverflowPrivate(char c);

            int onPrintfOverflowPrivate(char c);


        private:

            // --------------------------------------------------------
            // BYPASS MANAGEMENT
            // --------------------------------------------------------

            void AddBypass(StdOutputBypass* bypass);
            void RemoveBypass(StdOutputBypass* bypass);

            static StdOutputBypass* m_ptr;

            std::vector<StdOutputBypass*> m_bypass;

            typedef std::vector<StdOutputBypass*>::iterator TStdOutIta;

            std::mutex m_bypassMutex;


            // --------------------------------------------------------
            // ORIGINAL STREAM BUFFERS
            // --------------------------------------------------------

            std::streambuf* old_cerr_buffer;
            std::streambuf* old_clog_buffer;
            std::streambuf* old_cout_buffer;


            // --------------------------------------------------------
            // PRINTF READER
            // --------------------------------------------------------

#ifdef _WIN32

            void PrintfReaderThread();

            std::thread m_printfThread;

            std::atomic_bool m_printfEnabled{ false };

            std::mutex m_printfMutex;

            int m_oldStdout = -1;
            int m_pipeRead = -1;

            HANDLE m_oldStdoutHandle = INVALID_HANDLE_VALUE;

#endif

        };

    }
}
