#pragma once
#
#include <cstdint>
#include <cstddef>
#include <memory>

namespace ng::io::vfs
{
    enum class SeekOrigin
    {
        Begin,
        Current,
        End
    };

    class Stream
    {
    public:
        virtual ~Stream() = default;

        virtual size_t Read(void* buffer, size_t size) = 0;
        virtual size_t Write(const void* buffer, size_t size) = 0;

        virtual bool Seek(int64_t offset, SeekOrigin origin) = 0;

        virtual uint64_t Tell() const = 0;
        virtual uint64_t Size() const = 0;

        virtual bool Eof() const = 0;

        virtual bool CanRead() const = 0;
        virtual bool CanWrite() const = 0;
    };

    using StreamPtr = std::unique_ptr<Stream>;
}