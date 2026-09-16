#include "pch.h"
#include "VFSZipStream.h"

#include <algorithm>
#include <cstring>

namespace ng::io::vfs
{
    ZipStream::ZipStream(
        std::vector<uint8_t>&& data)
        : m_data(std::move(data))
    {
    }


    size_t ZipStream::Read(
        void* buffer,
        size_t size)
    {
        if (!buffer || size == 0)
            return 0;

        if (m_position >= m_data.size())
            return 0;

        const size_t remaining =
            m_data.size() - m_position;

        const size_t count =
            std::min(size, remaining);

        std::memcpy(
            buffer,
            m_data.data() + m_position,
            count);

        m_position += count;

        return count;
    }


    size_t ZipStream::Write(
        const void*,
        size_t)
    {
        // ZIP-Drive ist read-only.
        return 0;
    }


    bool ZipStream::Seek(
        int64_t offset,
        SeekOrigin origin)
    {
        int64_t base = 0;

        switch (origin)
        {
        case SeekOrigin::Begin:
            base = 0;
            break;

        case SeekOrigin::Current:
            base = static_cast<int64_t>(m_position);
            break;

        case SeekOrigin::End:
            base = static_cast<int64_t>(m_data.size());
            break;
        }

        const int64_t position =
            base + offset;

        if (position < 0)
            return false;

        if (position >
            static_cast<int64_t>(m_data.size()))
        {
            return false;
        }

        m_position =
            static_cast<size_t>(position);

        return true;
    }


    uint64_t ZipStream::Tell() const
    {
        return static_cast<uint64_t>(m_position);
    }


    uint64_t ZipStream::Size() const
    {
        return static_cast<uint64_t>(m_data.size());
    }


    bool ZipStream::Eof() const
    {
        return m_position >= m_data.size();
    }


    bool ZipStream::CanRead() const
    {
        return true;
    }


    bool ZipStream::CanWrite() const
    {
        return false;
    }
}