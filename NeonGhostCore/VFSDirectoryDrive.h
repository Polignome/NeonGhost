#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "VFSDrive.h"
#include "VFSFile.h"

#
namespace ng::io::vfs
{
    class DirectoryDrive final : public Drive
    {
    public:

        explicit DirectoryDrive(
            const std::filesystem::path& root);

        bool Exists(
            std::string_view path) const override;

        FilePtr Open(
            std::string_view path) override;

        std::vector<DirectoryEntry>
            List(std::string_view path) const override;

        const std::filesystem::path& Root() const
        {
            return m_root;
        }

    private:

        std::filesystem::path ResolvePath(
            std::string_view path) const;

    private:

        std::filesystem::path m_root;
    };
}