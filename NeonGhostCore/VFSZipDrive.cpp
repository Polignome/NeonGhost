#include "pch.h"
#include "VFSZipDrive.h"
#include "VFSZipFile.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <unordered_map>

#include "miniz.h"

namespace ng::io::vfs
{
    ZipDrive::ZipDrive(
        const std::filesystem::path& filename)
        : m_filename(filename)
    {
        m_archive =
            new mz_zip_archive{};

        std::memset(
            m_archive,
            0,
            sizeof(mz_zip_archive));

        if (!mz_zip_reader_init_file(
            m_archive,
            m_filename.string().c_str(),
            0))
        {
            delete m_archive;
            m_archive = nullptr;
            return;
        }

        m_open = true;

        BuildDirectoryTree();
    }


    ZipDrive::~ZipDrive()
    {
        if (m_archive)
        {
            if (m_open)
            {
                mz_zip_reader_end(
                    m_archive);
            }

            delete m_archive;
            m_archive = nullptr;
        }
    }

    std::string
        ZipDrive::NormalizePath(
            std::string_view path) const
    {
        std::string result(path);

        for (char& c : result)
        {
            if (c == '\\')
                c = '/';
        }

        while (!result.empty() &&
            result.front() == '/')
        {
            result.erase(
                result.begin());
        }

        while (!result.empty() &&
            result.back() == '/')
        {
            result.pop_back();
        }

        return result;
    }

    void ZipDrive::BuildDirectoryTree()
    {
        if (!m_archive)
            return;

        const mz_uint count =
            mz_zip_reader_get_num_files(
                m_archive);

        m_entries.clear();
        m_entries.reserve(count);

        for (mz_uint i = 0;
            i < count;
            ++i)
        {
            mz_zip_archive_file_stat stat{};

            if (!mz_zip_reader_file_stat(
                m_archive,
                i,
                &stat))
            {
                continue;
            }

            if (!stat.m_filename)
                continue;

            std::string path =
                NormalizePath(
                    stat.m_filename);

            if (path.empty())
                continue;

            Entry entry;

            entry.path = path;

            entry.directory =
                stat.m_is_directory != 0;

            entry.zipIndex = i;

            entry.size =
                static_cast<uint64_t>(
                    stat.m_uncomp_size);

            const size_t slash =
                path.find_last_of('/');

            if (slash == std::string::npos)
            {
                entry.name = path;
            }
            else
            {
                entry.name =
                    path.substr(slash + 1);
            }

            m_entries.push_back(
                std::move(entry));
        }
    }


    const ZipDrive::Entry*
        ZipDrive::FindEntry(
            std::string_view path) const
    {
        const std::string normalized =
            NormalizePath(path);

        for (const auto& entry : m_entries)
        {
            if (entry.path == normalized)
                return &entry;
        }

        return nullptr;
    }

    bool ZipDrive::Exists(
        std::string_view path) const
    {
        return FindEntry(path) != nullptr;
    }

    FilePtr ZipDrive::Open(
        std::string_view path)
    {
        if (!m_open)
            return nullptr;

        const std::string normalized =
            NormalizePath(path);

        for (size_t i = 0;
            i < m_entries.size();
            ++i)
        {
            if (m_entries[i].path ==
                normalized)
            {
                return std::make_shared<ZipFile>(
                    shared_from_this(),
                    i);
            }
        }

        return nullptr;
    }


    std::vector<DirectoryEntry>
        ZipDrive::List(
            std::string_view path) const
    {
        std::vector<DirectoryEntry> result;

        const std::string directory =
            NormalizePath(path);

        std::string prefix = directory;

        if (!prefix.empty())
            prefix += '/';

        std::unordered_map<
            std::string,
            bool> found;

        for (const auto& entry : m_entries)
        {
            if (!prefix.empty())
            {
                if (entry.path.rfind(
                    prefix, 0) != 0)
                {
                    continue;
                }
            }

            std::string remainder;

            if (prefix.empty())
            {
                remainder =
                    entry.path;
            }
            else
            {
                remainder =
                    entry.path.substr(
                        prefix.size());
            }

            if (remainder.empty())
                continue;

            const size_t slash =
                remainder.find('/');

            if (slash == std::string::npos)
            {
                found[remainder] =
                    entry.directory;

                continue;
            }

            const std::string dirname =
                remainder.substr(0, slash);

            found[dirname] = true;
        }

        result.reserve(found.size());

        for (const auto& item : found)
        {
            DirectoryEntry entry;

            entry.name = item.first;
            entry.directory = item.second;

            result.push_back(
                std::move(entry));
        }

        return result;
    }



    bool ZipDrive::Extract(
        size_t index,
        std::vector<uint8_t>& data) const
    {
        if (!m_archive)
            return false;

        if (index >= m_entries.size())
            return false;

        const Entry& entry =
            m_entries[index];

        if (entry.directory)
            return false;

        void* buffer = nullptr;

        size_t size = 0;

        buffer =
            mz_zip_reader_extract_to_heap(
                m_archive,
                static_cast<mz_uint>(
                    entry.zipIndex),
                &size,
                0);

        if (!buffer)
            return false;

        data.resize(size);

        if (size > 0)
        {
            std::memcpy(
                data.data(),
                buffer,
                size);
        }

        mz_free(buffer);

        return true;
    }


}


