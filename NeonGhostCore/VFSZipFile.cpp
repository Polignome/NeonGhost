#include "VFSZipFile.h"

#include "VFSZipDrive.h"
#include "VFSZipStream.h"

namespace ng::io::vfs
{
    ZipFile::ZipFile(
        std::shared_ptr<ZipDrive> drive,
        size_t index)
        : m_drive(std::move(drive)),
        m_index(index)
    {
    }


    std::string_view
        ZipFile::Name() const
    {
        return m_drive
            ->GetEntry(m_index)
            .name;
    }


    std::string_view
        ZipFile::Path() const
    {
        return m_drive
            ->GetEntry(m_index)
            .path;
    }


    uint64_t
        ZipFile::Size() const
    {
        return m_drive
            ->GetEntry(m_index)
            .size;
    }


    bool
        ZipFile::IsDirectory() const
    {
        return m_drive
            ->GetEntry(m_index)
            .directory;
    }


    StreamPtr
        ZipFile::OpenStream(
            bool writable) const
    {
        if (writable)
            return nullptr;

        if (IsDirectory())
            return nullptr;

        std::vector<uint8_t> data;

        if (!m_drive->Extract(
            m_index,
            data))
        {
            return nullptr;
        }

        return std::make_unique<ZipStream>(
            std::move(data));
    }
}