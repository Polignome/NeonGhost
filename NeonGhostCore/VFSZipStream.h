#pragma once

#include <cstdint>
#include <vector>

#include "VFSStream.h"

namespace ng::io::vfs
{
    class ZipStream final
        : public Stream
    {
    public:

        explicit ZipStream(
            std::vector<uint8_t>&& data);


        size_t Read(
            void* buffer,
            size_t size) override;


        size_t Write(
            const void* buffer,
            size_t size) override;


        bool Seek(
            int64_t offset,
            SeekOrigin origin) override;


        uint64_t Tell() const override;


        uint64_t Size() const override;


        bool Eof() const override;


        bool CanRead() const override;


        bool CanWrite() const override;


    private:

        std::vector<uint8_t> m_data;

        size_t m_position = 0;
    };
}
