#include "pch.h"
#include "VFSDirectoryDrive.h"
#include "VFSDirectoryFile.h"
namespace ng::io::vfs
{


    DirectoryDrive::
         DirectoryDrive(
        const std::filesystem::path &root
    )
        : m_root(std::move(root))
    {
        /*
         * Der Root-Pfad wird einmal normalisiert.
         */
        std::error_code ec;

        m_root =
            std::filesystem::weakly_canonical(
                m_root,
                ec
            );

        if (ec)
        {
            /*
             * Falls der Pfad noch nicht existiert,
             * behalten wir den ursprünglichen Pfad.
             */
            m_root =
                std::filesystem::absolute(
                    m_root,
                    ec
                );
        }
    }


    std::filesystem::path
        DirectoryDrive::ResolvePath(
            std::string_view path
        ) const
    {
        std::string relative(path);

        /*
         * VFS benutzt immer '/'.
         * Windows akzeptiert zwar auch '/',
         * wir entfernen aber einen möglichen führenden Slash.
         */

        while (
            !relative.empty() &&
            (relative.front() == '/' ||
                relative.front() == '\\')
            )
        {
            relative.erase(
                relative.begin()
            );
        }


        return m_root /
            std::filesystem::path(relative);
    }


    bool DirectoryDrive::Exists(
        std::string_view path
    ) const
    {
        const auto physicalPath =
            ResolvePath(path);


        std::error_code ec;

        return std::filesystem::exists(
            physicalPath,
            ec
        );
    }


    FilePtr DirectoryDrive::Open(
        std::string_view path
    )
    {
        const auto physicalPath =
            ResolvePath(path);


        std::error_code ec;

        if (!std::filesystem::exists(
            physicalPath,
            ec))
        {
            return nullptr;
        }


        return std::make_shared<DirectoryFile>(
            physicalPath
        );
    }


    std::vector<DirectoryEntry>
       DirectoryDrive::List(std::string_view path) const
    {
        std::vector<DirectoryEntry> result;
        
        const std::filesystem::path directory =
            ResolvePath(path);
        

        std::error_code ec;
        

        if (!std::filesystem::exists(directory, ec))
            return result;
        

        if (!std::filesystem::is_directory(directory, ec))
            return result;
        

        for (const auto& entry :
            std::filesystem::directory_iterator(directory, ec))
        {
            if (ec)
                break;

            DirectoryEntry item;

            item.name =
                entry.path().filename().string();

            item.directory =
                entry.is_directory(ec);

            if (ec)
                continue;

            result.push_back(std::move(item));
        }
        

        return result;
    }
}