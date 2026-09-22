#include "pch.h"
#include "VFSFileManager.h"

namespace ng::io::vfs
{
    namespace
    {
        VirtualFileSystem g_vfs;
    }


    FilePtr VFSFileManager::Open(std::string_view path)
    {
        return g_vfs.Open(path);
    }


    bool VFSFileManager::Exists(std::string_view path)
    {
        return g_vfs.Exists(path);
    }


    std::vector<DirectoryEntry> VFSFileManager::List(std::string_view path)
    {
      
        return g_vfs.List(path);
    }


    void VFSFileManager::Mount(
        const std::string& path,
        DrivePtr drive,
        int priority,
        const std::string& name)
    {
        g_vfs.Mount(path, std::move(drive), priority, name);
    }


    void VFSFileManager::Unmount(DrivePtr drive)
    {
        g_vfs.Unmount(std::move(drive));
    }


    void VFSFileManager::Unmount(
        const std::string& path,
        DrivePtr drive)
    {
        g_vfs.Unmount(path, std::move(drive));
    }


    void VFSFileManager::ClearMounts()
    {
        g_vfs.ClearMounts();
    }


    VirtualFileSystem& VFSFileManager::GetVFS()
    {
        return g_vfs;
    }
}