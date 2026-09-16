#pragma once
#include "Global.h"

#include <string>
#include <memory>

#include "VFSDrive.h"

namespace ng::io::vfs
{
    struct MountPoint
    {
        // Virtueller Pfad, z.B.:
        //
        // /
        // /textures
        // /maps
        //
        std::string path;

        // Optionaler Name für Debugging und Listing.
        //
        // "GameData"
        // "BaseTextures"
        // "HDTexturePack"
        //
        std::string name;

        DrivePtr drive;

        // Höhere Priorität überschreibt niedrigere
        // MountPoints am gleichen Pfad.
        int priority = 0;
    };
}
