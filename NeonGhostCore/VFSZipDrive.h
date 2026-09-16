#pragma once
#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include "miniz.h"
#include "VFSDrive.h"



namespace ng::io::vfs
{
    class ZipFile;

    class ZipDrive final
        : public Drive,
        public std::enable_shared_from_this<ZipDrive>
    {
    public:

        explicit ZipDrive(
            const std::filesystem::path& filename);

        ~ZipDrive() override;


        bool Exists(
            std::string_view path) const override;


        FilePtr Open(
            std::string_view path) override;


        std::vector<DirectoryEntry>
            List(std::string_view path) const override;


        const std::filesystem::path& Filename() const
        {
            return m_filename;
        }


        bool IsOpen() const
        {
            return m_open;
        }


    private:

        struct Entry
        {
            std::string path;
            std::string name;

            bool directory = false;

            uint64_t size = 0;

            size_t zipIndex = 0;
        };


        std::string NormalizePath(
            std::string_view path) const;


        const Entry* FindEntry(
            std::string_view path) const;


        void BuildDirectoryTree();
        friend class ZipFile;

        const Entry& GetEntry(
            size_t index) const
        {
            return m_entries.at(index);
        }

        bool Extract(
            size_t index,
            std::vector<uint8_t>& data) const;


    private:

        std::filesystem::path m_filename;

        mz_zip_archive* m_archive = nullptr;

        bool m_open = false;

        std::vector<Entry> m_entries;
    };
}