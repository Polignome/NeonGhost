#pragma once

#include <cstdint>
#include <memory>
#include <string_view>

#include "VFSStream.h"

namespace ng::io::vfs
{
    class File
    {
    public:
        virtual ~File() = default;

        // Name ohne Pfad
        // z.B. "player.png"
        virtual std::string_view Name() const = 0;

        // Pfad innerhalb des jeweiligen Drives
        // z.B. "textures/player.png"
        virtual std::string_view Path() const = 0;

        virtual uint64_t Size() const = 0;

        virtual bool IsDirectory() const = 0;

        // Öffnet einen unabhängigen Stream.
        //
        // writable = true:
        // Der Backend soll einen beschreibbaren Stream liefern,
        // falls das Backend dies unterstützt.
        //
        // Kann nullptr zurückgeben.
        virtual StreamPtr OpenStream(bool writable = false) const = 0;
    };

    using FilePtr = std::shared_ptr<File>;
}