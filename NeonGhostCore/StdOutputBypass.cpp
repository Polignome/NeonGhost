
#include "pch.h"

#include "StdOutputBypass.h"

#include <assert.h>
#include <string>
#include <map>
#include <algorithm>

#ifdef _WIN32
#include <Windows.h>
#include <io.h>
#include <fcntl.h>
#include <cstdio>
#endif

using namespace ng;
using namespace io;


// ------------------------------------------------------------
// STATIC INSTANCE
// ------------------------------------------------------------

StdOutputBypass* StdOutputBypass::m_ptr = nullptr;

static StdOutputBypass test;


// ------------------------------------------------------------
// CONSTRUCTOR
// ------------------------------------------------------------

StdOutputBypass::StdOutputBypass()
    : old_cerr_buffer(nullptr),
    old_clog_buffer(nullptr),
    old_cout_buffer(nullptr)
{
    if (m_ptr == nullptr)
    {
        m_ptr = this;

        old_cout_buffer = std::cout.rdbuf();
        std::cout.rdbuf(GetCoutBuffer());

        old_cerr_buffer = std::cerr.rdbuf();
        std::cerr.rdbuf(GetCerrBuffer());

        old_clog_buffer = std::clog.rdbuf();
        std::clog.rdbuf(GetClogBuffer());
    }

    AddBypass(this);
}


// ------------------------------------------------------------
// ADD BYPASS
// ------------------------------------------------------------

void StdOutputBypass::AddBypass(StdOutputBypass* bypass)
{
    if (!m_ptr)
        return;

    std::lock_guard<std::mutex> lock(m_ptr->m_bypassMutex);

    for (auto* entry : m_ptr->m_bypass)
        assert(entry != bypass);

    m_ptr->m_bypass.push_back(bypass);
}


// ------------------------------------------------------------
// REMOVE BYPASS
// ------------------------------------------------------------

void StdOutputBypass::RemoveBypass(StdOutputBypass* bypass)
{
    if (!m_ptr)
        return;

    std::lock_guard<std::mutex> lock(m_ptr->m_bypassMutex);

    auto& list = m_ptr->m_bypass;

    auto it = std::find(
        list.begin(),
        list.end(),
        bypass
    );

    if (it != list.end())
        list.erase(it);
}


// ------------------------------------------------------------
// DESTRUCTOR
// ------------------------------------------------------------

StdOutputBypass::~StdOutputBypass()
{
#ifdef _WIN32
    if (m_ptr == this)
        DisablePrintfBypass();
#endif

    if (m_ptr == this)
    {
        if (old_cout_buffer != nullptr)
            std::cout.rdbuf(old_cout_buffer);

        if (old_cerr_buffer != nullptr)
            std::cerr.rdbuf(old_cerr_buffer);

        if (old_clog_buffer != nullptr)
            std::clog.rdbuf(old_clog_buffer);
    }

    RemoveBypass(this);
}


// ------------------------------------------------------------
// CALLBACK DISPATCH
// ------------------------------------------------------------

int StdOutputBypass::onCoutOverflowPrivate(char c)
{
    if (m_ptr != this)
        return 0;

    std::vector<StdOutputBypass*> bypasses;

    {
        std::lock_guard<std::mutex> lock(m_bypassMutex);
        bypasses = m_bypass;
    }

    for (auto* bypass : bypasses)
    {
        if (bypass)
            bypass->onCoutOverflow(c);
    }

    return 0;
}


int StdOutputBypass::onCerrOverflowPrivate(char c)
{
    if (m_ptr != this)
        return 0;

    std::vector<StdOutputBypass*> bypasses;

    {
        std::lock_guard<std::mutex> lock(m_bypassMutex);
        bypasses = m_bypass;
    }

    for (auto* bypass : bypasses)
    {
        if (bypass)
            bypass->onCerrOverflow(c);
    }

    return 0;
}


int StdOutputBypass::onClogOverflowPrivate(char c)
{
    if (m_ptr != this)
        return 0;

    std::vector<StdOutputBypass*> bypasses;

    {
        std::lock_guard<std::mutex> lock(m_bypassMutex);
        bypasses = m_bypass;
    }

    for (auto* bypass : bypasses)
    {
        if (bypass)
            bypass->onClogOverflow(c);
    }

    return 0;
}


// ------------------------------------------------------------
// PRINTF CALLBACK DISPATCH
// ------------------------------------------------------------

int StdOutputBypass::onPrintfOverflowPrivate(char c)
{
    if (m_ptr != this)
        return 0;

    std::vector<StdOutputBypass*> bypasses;

    {
        std::lock_guard<std::mutex> lock(m_bypassMutex);
        bypasses = m_bypass;
    }

    for (auto* bypass : bypasses)
    {
        if (bypass)
        {
            try
            {
                bypass->onPrintfOverflow(c);
            }
            catch (...)
            {
                // Callback exceptions must not terminate
                // the printf reader thread.
            }
        }
    }

    return 0;
}


// ------------------------------------------------------------
// DEFAULT OUTPUT CALLBACKS
// ------------------------------------------------------------

int StdOutputBypass::onCoutOverflow(char c)
{
    if (old_cout_buffer != nullptr)
        old_cout_buffer->sputc(c);

    return 0;
}


int StdOutputBypass::onClogOverflow(char c)
{
    if (old_clog_buffer != nullptr)
        old_clog_buffer->sputc(c);

    return 0;
}


int StdOutputBypass::onCerrOverflow(char c)
{
    if (old_cerr_buffer != nullptr)
        old_cerr_buffer->sputc(c);

    return 0;
}


// ------------------------------------------------------------
// PRINTF DEFAULT CALLBACK
// ------------------------------------------------------------

int StdOutputBypass::onPrintfOverflow(char c)
{
#ifdef _WIN32

    // Only the primary instance forwards to the
    // original stdout handle.

    if (m_ptr == this &&
        m_oldStdoutHandle != INVALID_HANDLE_VALUE)
    {
        DWORD written = 0;

        WriteFile(
            m_oldStdoutHandle,
            &c,
            1,
            &written,
            nullptr
        );
    }

#endif

    return 0;
}


// ------------------------------------------------------------
// ENABLE PRINTF BYPASS
// ------------------------------------------------------------

bool StdOutputBypass::EnablePrintfBypass()
{
#ifdef _WIN32

    StdOutputBypass* root = m_ptr;

    if (!root)
        return false;

    std::lock_guard<std::mutex> lock(root->m_printfMutex);

    if (root->m_printfEnabled.load())
        return true;

    if (root->m_printfThread.joinable())
        return false;

    fflush(stdout);

    const int stdoutFd = _fileno(stdout);

    if (stdoutFd == -1)
        return false;


    // Save original stdout.

    root->m_oldStdout = _dup(stdoutFd);

    if (root->m_oldStdout == -1)
        return false;


    // Get original Windows handle.

    intptr_t handle = _get_osfhandle(root->m_oldStdout);

    if (handle == -1)
    {
        _close(root->m_oldStdout);
        root->m_oldStdout = -1;
        return false;
    }

    root->m_oldStdoutHandle = reinterpret_cast<HANDLE>(handle);


    // Create pipe.

    int pipeFd[2] = { -1, -1 };

    if (_pipe(pipeFd, 4096, _O_BINARY) != 0)
    {
        _close(root->m_oldStdout);

        root->m_oldStdout = -1;
        root->m_oldStdoutHandle = INVALID_HANDLE_VALUE;

        return false;
    }


    // Redirect stdout to pipe.

    if (_dup2(pipeFd[1], stdoutFd) != 0)
    {
        _close(pipeFd[0]);
        _close(pipeFd[1]);
        _close(root->m_oldStdout);

        root->m_oldStdout = -1;
        root->m_oldStdoutHandle = INVALID_HANDLE_VALUE;

        return false;
    }


    // Close duplicated write descriptor.
    // stdout now owns the pipe write end.

    _close(pipeFd[1]);

    root->m_pipeRead = pipeFd[0];

    root->m_printfEnabled = true;


    // Start reader thread.

    try
    {
        root->m_printfThread = std::thread(
            &StdOutputBypass::PrintfReaderThread,
            root
        );
    }
    catch (...)
    {
        fflush(stdout);

        _dup2(root->m_oldStdout, stdoutFd);

        _close(root->m_oldStdout);
        _close(root->m_pipeRead);

        root->m_oldStdout = -1;
        root->m_pipeRead = -1;
        root->m_oldStdoutHandle = INVALID_HANDLE_VALUE;

        root->m_printfEnabled = false;

        return false;
    }

    return true;

#else

    return false;

#endif
}


// ------------------------------------------------------------
// DISABLE PRINTF BYPASS
// ------------------------------------------------------------

void StdOutputBypass::DisablePrintfBypass()
{
#ifdef _WIN32

    StdOutputBypass* root = m_ptr;

    if (!root)
        return;

    std::lock_guard<std::mutex> lock(root->m_printfMutex);

    if (!root->m_printfEnabled.load())
        return;


    // Flush pending printf output.

    fflush(stdout);


    // Restore original stdout.
    // This closes the pipe write end.

    _dup2(
        root->m_oldStdout,
        _fileno(stdout)
    );


    // Reader receives EOF and exits.

    if (root->m_printfThread.joinable())
        root->m_printfThread.join();


    // Release resources.

    if (root->m_oldStdout != -1)
    {
        _close(root->m_oldStdout);
        root->m_oldStdout = -1;
    }

    if (root->m_pipeRead != -1)
    {
        _close(root->m_pipeRead);
        root->m_pipeRead = -1;
    }

    root->m_oldStdoutHandle = INVALID_HANDLE_VALUE;

    root->m_printfEnabled = false;

#endif
}


// ------------------------------------------------------------
// PRINTF READER THREAD
// ------------------------------------------------------------

void StdOutputBypass::PrintfReaderThread()
{
#ifdef _WIN32

    char buffer[1024];

    while (true)
    {
        int bytes = _read(
            m_pipeRead,
            buffer,
            sizeof(buffer)
        );

        if (bytes <= 0)
            break;

        for (int i = 0; i < bytes; ++i)
        {
            onPrintfOverflowPrivate(buffer[i]);
        }
    }

#endif
}
