#include "pch.h"
#include "VirtualFileSystem.h"

#include <algorithm>
#include <unordered_set>
namespace ng::io::vfs
{

    // ================================================================
    // Path Helpers
    // ================================================================

    std::string VirtualFileSystem::NormalizePath(
        std::string_view path
    )
    {
        std::string result;

        result.reserve(path.size() + 1);


        // Immer mit / beginnen.
        result.push_back('/');


        bool previousSlash = true;

        for (char c : path)
        {
            if (c == '\\')
                c = '/';

            if (c == '/')
            {
                if (previousSlash)
                    continue;

                previousSlash = true;
            }
            else
            {
                previousSlash = false;
            }

            result += c;
        }


        // Abschließenden Slash entfernen,
        // außer beim Root-Verzeichnis.
        while (
            result.size() > 1 &&
            result.back() == '/'
            )
        {
            result.pop_back();
        }


        return result;
    }


    bool VirtualFileSystem::IsPathInsideMount(
        std::string_view path,
        std::string_view mountPath
    )
    {
        // Root enthält alles.
        if (mountPath == "/")
            return !path.empty() && path.front() == '/';


        // Der Pfad ist kürzer als der MountPoint.
        if (path.size() < mountPath.size())
            return false;


        // Präfix muss exakt übereinstimmen.
        if (path.substr(0, mountPath.size()) != mountPath)
            return false;


        // Exakte Übereinstimmung:
        //
        // /textures == /textures
        //
        if (path.size() == mountPath.size())
            return true;


        // Wichtig:
        //
        // /textures/wall.png     -> JA
        // /textures_hd/wall.png  -> NEIN
        //
        return path[mountPath.size()] == '/';
    }


    // ================================================================
    // Mount Management
    // ================================================================

    bool VirtualFileSystem::Mount(
        std::string_view path,
        DrivePtr drive,
        int priority,
        std::string_view name
    )
    {
        if (!drive)
            return false;


        MountPoint mount;

        mount.path =
            NormalizePath(path);

        mount.name =
            std::string(name);

        mount.drive =
            std::move(drive);

        mount.priority =
            priority;


        m_mounts.push_back(
            std::move(mount)
        );


        return true;
    }


    bool VirtualFileSystem::Unmount(
        const DrivePtr& drive
    )
    {
        if (!drive)
            return false;


        const size_t oldSize =
            m_mounts.size();


        std::erase_if(
            m_mounts,
            [&drive](const MountPoint& mount)
            {
                return mount.drive == drive;
            }
        );


        return m_mounts.size() != oldSize;
    }


    bool VirtualFileSystem::Unmount(
        std::string_view path,
        const DrivePtr& drive
    )
    {
        if (!drive)
            return false;


        const std::string normalizedPath =
            NormalizePath(path);


        const size_t oldSize =
            m_mounts.size();


        std::erase_if(
            m_mounts,
            [&](const MountPoint& mount)
            {
                return
                    mount.path == normalizedPath &&
                    mount.drive == drive;
            }
        );


        return m_mounts.size() != oldSize;
    }


    void VirtualFileSystem::ClearMounts()
    {
        m_mounts.clear();
    }



    std::vector<VirtualFileSystem::ResolvedMount>
        VirtualFileSystem::ResolveMounts(
            std::string_view path
        ) const
    {
        std::vector<ResolvedMount> result;

        const std::string normalizedPath =
            NormalizePath(path);


        for (const auto& mount : m_mounts)
        {
            if (!mount.drive)
                continue;

            if (!IsPathInsideMount(
                normalizedPath,
                mount.path))
            {
                continue;
            }


            ResolvedMount resolved;

            resolved.mount = &mount;


            // ------------------------------------------------------------
            // Relative path erzeugen
            // ------------------------------------------------------------

            if (mount.path == "/")
            {
                if (normalizedPath.size() > 1)
                {
                    resolved.relativePath =
                        normalizedPath.substr(1);
                }
            }
            else
            {
                resolved.relativePath =
                    normalizedPath.substr(
                        mount.path.size()
                    );

                if (
                    !resolved.relativePath.empty() &&
                    resolved.relativePath.front() == '/'
                    )
                {
                    resolved.relativePath.erase(
                        resolved.relativePath.begin()
                    );
                }
            }


            result.push_back(
                std::move(resolved)
            );
        }


        // ------------------------------------------------------------
        // Sortierung
        //
        // 1. Längster Mount-Pfad zuerst
        // 2. Bei gleichem Pfad: höchste Priority zuerst
        // ------------------------------------------------------------

        std::sort(
            result.begin(),
            result.end(),
            [](const ResolvedMount& a,
                const ResolvedMount& b)
            {
                const size_t lengthA =
                    a.mount->path.size();

                const size_t lengthB =
                    b.mount->path.size();


                if (lengthA != lengthB)
                    return lengthA > lengthB;


                return
                    a.mount->priority >
                    b.mount->priority;
            }
        );


        return result;
    }
    // ================================================================
    // Mount Resolution
    // ================================================================


    // ================================================================
    // File Access
    // ================================================================

    FilePtr VirtualFileSystem::Open(
        std::string_view path
    )
    {
        const auto mounts =
            ResolveMounts(path);


        for (const auto& resolved : mounts)
        {
            if (!resolved.mount ||
                !resolved.mount->drive)
            {
                continue;
            }


            FilePtr file =
                resolved.mount->drive->Open(
                    resolved.relativePath
                );


            if (file)
                return file;
        }


        return nullptr;
    }


    bool VirtualFileSystem::Exists(
        std::string_view path
    ) const
    {

        if (path.empty())
            return false;

        const auto mounts =
            ResolveMounts(path);


        for (const auto& resolved : mounts)
        {
            if (!resolved.mount ||
                !resolved.mount->drive)
            {
                continue;
            }


            if (resolved.mount->drive->Exists(
                resolved.relativePath))
            {
                return true;
            }
        }


        return false;
    }


    std::vector<DirectoryEntry>
        VirtualFileSystem::List(std::string_view path) const
    {
        

        std::vector<DirectoryEntry> result;
        std::vector<std::string> seen;

        const auto mounts = ResolveMounts(path);

        

        for (const auto& resolved : mounts)
        {
            if (!resolved.mount || !resolved.mount->drive)
                continue;

        

            const auto entries =
                resolved.mount->drive->List(resolved.relativePath);

        

            for (const auto& entry : entries)
            {
        

                bool found = false;

                for (const auto& name : seen)
                {
                    if (name == entry.name)
                    {
                        found = true;
                        break;
                    }
                }

                if (!found)
                {
        

                    seen.push_back(entry.name);
                    result.push_back(entry);
                }
            }
        }



        return result;
    }
    // ================================================================
    // Mount Information
    // ================================================================

    const std::vector<MountPoint>&
        VirtualFileSystem::MountPoints() const
    {
        return m_mounts;
    }

}