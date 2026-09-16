#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "VFSFile.h"
#include "VFSDrive.h"
#include "VirtualFileSystem.h"

namespace ng::io::vfs
{
    class VFSFileManager
    {
    public:
        // ------------------------------------------------------------
        // Dateien
        // ------------------------------------------------------------

        static FilePtr Open(std::string_view path);

        static bool Exists(std::string_view path);

        static std::vector<DirectoryEntry>
            List(std::string_view path);


        // ------------------------------------------------------------
        // Drives
        // ------------------------------------------------------------

        static void Mount(
            const std::string& path,
            DrivePtr drive,
            int priority = 0,
            const std::string& name = {});

        static void Unmount(DrivePtr drive);

        static void Unmount(
            const std::string& path,
            DrivePtr drive);

        static void ClearMounts();


        // ------------------------------------------------------------
        // Zugriff auf das eigentliche VFS
        // ------------------------------------------------------------

        static VirtualFileSystem& GetVFS();

    private:
        VFSFileManager() = delete;
    };
}