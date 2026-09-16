#pragma once
#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include "VFSStream.h"
#include "VFSFile.h"

namespace ng::io::vfs
{
    class DirectoryFile final : public File
    {
    public:

        explicit DirectoryFile(
            std::filesystem::path path
        );


        std::string_view Name() const override;

        std::string_view Path() const override;

        uint64_t Size() const override;

        bool IsDirectory() const override;

        StreamPtr OpenStream(bool writable = false) const override;


    private:

        std::filesystem::path m_path;

        std::string m_name;

        std::string m_virtualPath;
    };
}

