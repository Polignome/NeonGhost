#pragma once

#include <fstream>
#include <filesystem>

#include "VFSStream.h"

namespace ng::io::vfs
{
    class FileStream final : public Stream
    {
    public:

        FileStream(
            const std::filesystem::path& path,
            bool writable = false
        );


        ~FileStream() override;


        bool IsOpen() const
        {
            return m_file.is_open();
        }


        size_t Read(
            void* buffer,
            size_t size
        ) override;


        size_t Write(
            const void* buffer,
            size_t size
        ) override;


        bool Seek(
            int64_t offset,
            SeekOrigin origin
        ) override;


        uint64_t Tell() const override;

        uint64_t Size() const override;

        bool Eof() const override;

        bool CanRead() const override;

        bool CanWrite() const override;


    private:

        std::fstream m_file;

        uint64_t m_size = 0;

        bool m_readable = false;

        bool m_writable = false;
    };
}