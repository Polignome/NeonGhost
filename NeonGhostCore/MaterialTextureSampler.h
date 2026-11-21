#pragma once

#include "MaterialTypes.h"

namespace ng::material
{

    class IMaterialTextureSampler
    {
    public:
        virtual ~IMaterialTextureSampler() = default;

        virtual bool Sample(
            TextureId texture,
            float u,
            float v,
            MaterialValue& result) const = 0;
    };

}
