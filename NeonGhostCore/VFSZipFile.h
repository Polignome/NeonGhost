#pragma once

#include <memory>
#include <string>
#include <string_view>

#include "VFSFile.h"

namespace ng::io::vfs
{
    class ZipDrive;

    class ZipFile final
        : public File
    {
    public:

        ZipFile(
            std::shared_ptr<ZipDrive> drive,
            size_t index);


        std::string_view Name() const override;

        std::string_view Path() const override;

        uint64_t Size() const override;

        bool IsDirectory() const override;


        StreamPtr OpenStream(
            bool writable = false) const override;


    private:

        std::shared_ptr<ZipDrive> m_drive;

        size_t m_index = 0;
    };
}
