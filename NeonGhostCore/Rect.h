#pragma once

#pragma once
#include <algorithm>

namespace ng::lightmap
{
    struct Rect
    {
        int left = 0, top = 0, right = 0, bottom = 0;

        Rect() = default;
        Rect(int l, int t, int r, int b) : left(l), top(t), right(r), bottom(b) {}

        inline int width()  const { return right - left; }
        inline int height() const { return bottom - top; }
    };
}