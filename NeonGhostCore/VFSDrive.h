#pragma once
#pragma once
#include "Global.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>


#include "VFSFile.h"



namespace ng::io::vfs
{
    struct DirectoryEntry
    {
        std::string name;
        bool directory = false;
    };


    class Drive
    {
    public:
        virtual ~Drive() = default;

        virtual bool Exists(std::string_view path) const = 0;

        virtual FilePtr Open(std::string_view path) = 0;

        virtual std::vector<DirectoryEntry>
            List(std::string_view path) const = 0;
    };


    using DrivePtr = std::shared_ptr<Drive>;
}