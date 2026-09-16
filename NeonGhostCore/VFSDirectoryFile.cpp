#include "pch.h"
#include "VFSDirectoryFile.h"
#include "VFSFileStream.h"

namespace ng::io::vfs
{
    DirectoryFile::DirectoryFile(
        std::filesystem::path path
    )
        : m_path(std::move(path))
    {
        m_name =
            m_path.filename().string();

        m_virtualPath =
            m_path.generic_string();
    }


    std::string_view
        DirectoryFile::Name() const
    {
        return m_name;
    }


    std::string_view
        DirectoryFile::Path() const
    {
        return m_virtualPath;
    }


    uint64_t DirectoryFile::Size() const
    {
        if (IsDirectory())
            return 0;


        std::error_code ec;

        const auto size =
            std::filesystem::file_size(
                m_path,
                ec
            );


        if (ec)
            return 0;


        return size;
    }


    bool DirectoryFile::IsDirectory() const
    {
        std::error_code ec;

        return std::filesystem::is_directory(
            m_path,
            ec
        );
    }


    StreamPtr
        DirectoryFile::OpenStream(bool writable) const
    {
        if (IsDirectory())
            return nullptr;


        auto stream =
            std::make_unique<FileStream>(
                m_path,
                false
            );


        if (!stream->IsOpen())
            return nullptr;


        return stream;
    }
}