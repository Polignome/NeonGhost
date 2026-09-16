#include "pch.h"
#include "VFSFileStream.h"
#include <algorithm>
#include <cstring>

namespace ng::io::vfs
{
    FileStream::FileStream(
        const std::filesystem::path& path,
        bool writable
    )
    {
        std::ios::openmode mode =
            std::ios::binary |
            std::ios::in;

        m_readable = true;

        if (writable)
        {
            mode |= std::ios::out;
            m_writable = true;
        }


        m_file.open(
            path,
            mode
        );


        if (!m_file.is_open())
        {
            m_readable = false;
            m_writable = false;
            return;
        }


        std::error_code ec;

        const auto fileSize =
            std::filesystem::file_size(
                path,
                ec
            );

        if (!ec)
            m_size = fileSize;
    }


    FileStream::~FileStream()
    {
        if (m_file.is_open())
            m_file.close();
    }


    size_t FileStream::Read(
        void* buffer,
        size_t size
    )
    {
        if (!m_file.is_open() ||
            !m_readable ||
            !buffer ||
            size == 0)
        {
            return 0;
        }


        m_file.read(
            static_cast<char*>(buffer),
            static_cast<std::streamsize>(size)
        );


        return static_cast<size_t>(
            m_file.gcount()
            );
    }


    size_t FileStream::Write(
        const void* buffer,
        size_t size
    )
    {
        if (!m_file.is_open() ||
            !m_writable ||
            !buffer ||
            size == 0)
        {
            return 0;
        }


        m_file.write(
            static_cast<const char*>(buffer),
            static_cast<std::streamsize>(size)
        );


        if (!m_file)
            return 0;


        return size;
    }


    bool FileStream::Seek(
        int64_t offset,
        SeekOrigin origin
    )
    {
        if (!m_file.is_open())
            return false;


        std::ios_base::seekdir direction;


        switch (origin)
        {
        case SeekOrigin::Begin:
            direction = std::ios::beg;
            break;

        case SeekOrigin::Current:
            direction = std::ios::cur;
            break;

        case SeekOrigin::End:
            direction = std::ios::end;
            break;

        default:
            return false;
        }


        /*
         * Wichtig:
         *
         * fstream besitzt getrennte get/put Positionen.
         * Für unser VFS wollen wir aber einen gemeinsamen Cursor.
         *
         * Deshalb setzen wir beide.
         */

        m_file.clear();

        m_file.seekg(
            offset,
            direction
        );

        if (m_writable)
        {
            m_file.clear();

            m_file.seekp(
                offset,
                direction
            );
        }


        return !m_file.fail();
    }


    uint64_t FileStream::Tell() const
    {
        if (!m_file.is_open())
            return 0;


        /*
         * tellg() ist leider nicht const.
         */
        auto& file =
            const_cast<std::fstream&>(m_file);


        const auto position =
            file.tellg();


        if (position < 0)
            return 0;


        return static_cast<uint64_t>(
            position
            );
    }


    uint64_t FileStream::Size() const
    {
        return m_size;
    }


    bool FileStream::Eof() const
    {
        if (!m_file.is_open())
            return true;

        return m_file.eof();
    }


    bool FileStream::CanRead() const
    {
        return m_readable;
    }


    bool FileStream::CanWrite() const
    {
        return m_writable;
    }
}