#pragma once
#include "Global.h"
#include <vector>
#include <string>
#include <string_view>

#include "VFSMountPoint.h"

namespace ng::io::vfs
{
    class VirtualFileSystem
    {
    public:

        // ------------------------------------------------------------
        // Mount Management
        // ------------------------------------------------------------

        bool Mount(
            std::string_view path,
            DrivePtr drive,
            int priority = 0,
            std::string_view name = {}
        );


        bool Unmount(
            const DrivePtr& drive
        );


        bool Unmount(
            std::string_view path,
            const DrivePtr& drive
        );


        void ClearMounts();


        // ------------------------------------------------------------
        // File Access
        // ------------------------------------------------------------

        FilePtr Open(
            std::string_view path
        );


        bool Exists(
            std::string_view path
        ) const;


        std::vector<DirectoryEntry>
            List(std::string_view path) const;



        // ------------------------------------------------------------
        // Mount Information
        // ------------------------------------------------------------

        const std::vector<MountPoint>&
            MountPoints() const;


    private:

        struct ResolvedMount
        {
            const MountPoint* mount = nullptr;
            std::string relativePath;
        };
       
        std::vector<ResolvedMount> ResolveMounts(
            std::string_view path
        ) const;


        static std::string NormalizePath(
            std::string_view path
        );


        static bool IsPathInsideMount(
            std::string_view path,
            std::string_view mountPath
        );


      


    private:

        std::vector<MountPoint> m_mounts;
    };
}