// WarpPortal.h
#pragma once
#include "BSP.h"

namespace ng::bsp
{
    template<class T>
    struct WarpPortal
    {
        using Vec3 = ng::math::Matrix<3, 1, T>;

        Vec3 center, normal, right, up;   // normal zeigt in den Raum (Vorderseite)
        T halfW = T(32), halfH = T(48);
        int leafId = -1;                  // Leaf im ZELLENBAUM (fuer das PVS)
        int target = -1;
        uint8_t id = 0;                   // Stencil-ID, 1..255

        void BuildFrame()
        {
            normal.normalize();
            Vec3 worldUp(0, 0, 1);        // Half-Life: Z ist oben
            if (std::abs(normal ^ worldUp) > T(0.99)) worldUp = Vec3(0, 1, 0);
            right = worldUp % normal; right.normalize();
            up = normal % right;      up.normalize();
        }

        Vec3 Corner(int i) const
        {
            const T sx = (i == 0 || i == 3) ? T(-1) : T(1);
            const T sy = (i < 2) ? T(-1) : T(1);
            return center + right * (sx * halfW) + up * (sy * halfH);
        }
    };

    template<class T>
    ng::math::Matrix<3, 1, T> PortalPoint(const WarpPortal<T>& A, const WarpPortal<T>& B,
        const ng::math::Matrix<3, 1, T>& p)
    {
        const auto r = p - A.center;
        return B.center + B.right * (-(r ^ A.right)) + B.up * (r ^ A.up) + B.normal * (-(r ^ A.normal));
    }

    template<class T>
    ng::math::Matrix<3, 1, T> PortalDir(const WarpPortal<T>& A, const WarpPortal<T>& B,
        const ng::math::Matrix<3, 1, T>& d)
    {
        return B.right * (-(d ^ A.right)) + B.up * (d ^ A.up) + B.normal * (-(d ^ A.normal));
    }

    // Zwei Portale verknuepfen und ihre Leaves im Zellenbaum bestimmen.
    template<class T>
    bool LinkWarpPortals(std::vector<WarpPortal<T>>& v, int a, int b, const BSPTree<T>& cellTree)
    {
        v[a].target = b; v[b].target = a;
        v[a].id = (uint8_t)(a + 1);
        v[b].id = (uint8_t)(b + 1);

        for (int i : { a, b })
        {
            v[i].BuildFrame();
            v[i].leafId = cellTree.FindLeafId(v[i].center + v[i].normal * T(0.5));

            const auto leaf = cellTree.GetLeaf(v[i].leafId);
            if (!leaf || (leaf->State() & INTOTHEVOID))
            {
                printf("[Warp] Portal %d liegt im Void oder ausserhalb der Karte\n", i);
                return false;
            }
        }
        return true;
    }
}
