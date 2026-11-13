#pragma once

#include <windows.h>
#include <vector>
#include <cstdint>
#include <algorithm>

class ConsoleFramebuffer
{
public:
    ConsoleFramebuffer(int width, int height)
        : m_width(width),
        m_height(height),
        m_pixels(width* height, 0)
    {
        m_console = GetStdHandle(STD_OUTPUT_HANDLE);
    }

    void SetPixel(int x, int y, bool value = true)
    {
        if (x < 0 || y < 0 ||
            x >= m_width || y >= m_height)
            return;

        m_pixels[y * m_width + x] = value ? 1 : 0;
    }

    bool GetPixel(int x, int y) const
    {
        if (x < 0 || y < 0 ||
            x >= m_width || y >= m_height)
            return false;

        return m_pixels[y * m_width + x] != 0;
    }

    void Clear(bool value = false)
    {
        std::fill(m_pixels.begin(),
            m_pixels.end(),
            value ? 1 : 0);
    }

    void Present()
    {
        static constexpr int bits[4][2] =
        {
            { 0, 3 },
            { 1, 4 },
            { 2, 5 },
            { 6, 7 }
        };

        std::vector<wchar_t> row(m_width / 2);

        for (int cy = 0; cy < m_height / 4; ++cy)
        {
            for (int cx = 0; cx < m_width / 2; ++cx)
            {
                uint8_t pattern = 0;

                for (int py = 0; py < 4; ++py)
                {
                    for (int px = 0; px < 2; ++px)
                    {
                        if (GetPixel(cx * 2 + px,
                            cy * 4 + py))
                        {
                            pattern |= (1 << bits[py][px]);
                        }
                    }
                }

                row[cx] = wchar_t(0x2800 + pattern);
            }

            COORD pos{ 0, SHORT(cy) };
            SetConsoleCursorPosition(m_console, pos);

            DWORD written;
            WriteConsoleW(m_console,
                row.data(),
                DWORD(row.size()),
                &written,
                nullptr);
        }
    }

private:
    int m_width;
    int m_height;

    std::vector<uint8_t> m_pixels;
    HANDLE m_console;
};
