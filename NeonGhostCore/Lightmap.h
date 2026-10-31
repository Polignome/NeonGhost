#pragma once

// ----------------------------------------------------------------------
// Lightmap.h - Lightmap-Baking fuer BSPPolygon
//
// Ablauf:
//   1. EstimateAtlasSize()   - grobe Vorabschaetzung der Atlasgroesse
//   2. BuildLightmapAtlas()  - packt "polygons" (opake Struktur-Geometrie),
//                              rendert direktes Licht (Occlusion ueber
//                              den bestehenden BSPTree), schreibt die
//                              finalen Atlas-UVs in poly.verts[i].fxtexture().
//                              Optional: "extraPolys" fuer die Atlasgroessen-
//                              schaetzung mitgeben (sonst ist der Atlas
//                              oft zu klein fuer die Extras).
//   3. BakeExtraPolygonLightmaps() - packt "extraPolys" (Wasser, Glas, ...)
//                              in DENSELBEN Atlas. Diese Flaechen werden
//                              BEIDSEITIG beleuchtet, blockieren aber KEIN
//                              Licht (Occlusion laeuft weiterhin nur
//                              gegen den opaken BSPTree).
//   4. BakeRadiosity()       - indirektes Licht (Bounces), siehe
//                              LightmapRadiosity.h. Optional auch fuer Extras.
//
// Reihenfolge in der Anwendung: BuildLightmapAtlas() -> optional
// BakeExtraPolygonLightmaps() -> optional BakeRadiosity().
// ----------------------------------------------------------------------

#include "BSP.h"
#include "LightMapAtlas.h"
#include <vector>
#include <array>
#include <memory>
#include <cstdint>
#include <unordered_map>
#include <random>
#include <cmath>
#include <algorithm>
#include <limits>
#include <cstdio>

namespace ng::lightning
{
    template<class T>
    using Vec3 = ng::math::Matrix<3, 1, T>;

    // ------------------------------------------------------------
    // LightSource - fertig aufbereitete Lichtquelle fuers Baking.
    // Wird aus ng::map::LightEntity<T> (Map-Parser) konvertiert.
    // ------------------------------------------------------------
    template<class T>
    struct LightSource
    {
        Vec3<T> pos;
        Vec3<T> color;      // 0..255 pro Kanal
        T intensity = T(1);
        T radius = T(256);
    };

    struct LightmapOptions
    {
        bool   buildDirectLight = true;   // nur im Compiler-Thread wichtig
        int    startAtlasSize = 512;
        int    maxAtlasSize = 4096;
        double texelDensity = 16.0;   // Weltunits pro Lightmap-Texel
        int    maxLightmapSize = 64;    // groesste Kante eines einzelnen Polygon-Patches
        double epsilon = 1e-3;
        double packEfficiency = 0.65;  // fuer EstimateAtlasSize, siehe dort
        int    kAmbientFloor = 0;      // Sockel-Helligkeit (0..255) ohne jedes Licht
        bool   onlyCoveredTexels = false;  // true = nur Polygonflaeche (+ Rand) fuellen
        double coverageMargin = 1.0;    // Rand in Texeln fuer die bilineare Filterung
        double gain = 1;
    };

    // ------------------------------------------------------------
    // PolygonUVBasis
    //
    // Orthonormale u/v-Basis in der Polygonebene + Weltraum-Ausdehnung
    // (Bounding-Box im Parameterraum). Bestimmt Groesse und Weltraum-
    // Zuordnung des Lightmap-Patches fuer ein Polygon.
    // ------------------------------------------------------------
    template<class T>
    struct PolygonUVBasis
    {
        Vec3<T> axisU, axisV;              // orthonormal, in der Ebene
        Vec3<T> origin;                     // Weltpunkt bei (bboxMinU, bboxMinV)
        T extentU = T(0), extentV = T(0);   // BBox-Breite/Hoehe in Weltunits
    };

    // Polygon im Texelraum des Lightmap-Rechtecks
    template<class T>
    struct LumelPoly2D
    {
        std::vector<std::array<T, 2>> pts;
        T orient = T(1);
    };

    template<class T>
    LumelPoly2D<T> BuildLumelPoly2D(const ng::bsp::BSPPolygon<T>& poly)
    {
        LumelPoly2D<T> r;
        const T eu2 = poly.lmEdgeU ^ poly.lmEdgeU;
        const T ev2 = poly.lmEdgeV ^ poly.lmEdgeV;

        for (const auto& v : poly.verts)
        {
            const Vec3<T> rel = v.world() - poly.lmOrigin;
            r.pts.push_back({ ((rel ^ poly.lmEdgeU) / eu2) * T(poly.lmWidth),
                              ((rel ^ poly.lmEdgeV) / ev2) * T(poly.lmHeight) });
        }

        T area = T(0);
        for (size_t i = 0; i < r.pts.size(); ++i)
        {
            const auto& a = r.pts[i];
            const auto& b = r.pts[(i + 1) % r.pts.size()];
            area += a[0] * b[1] - b[0] * a[1];
        }
        r.orient = (area >= T(0)) ? T(1) : T(-1);
        return r;
    }

    // Abstand eines Texelraum-Punkts zum Polygon (0 = innen) und naechster Polygonpunkt
    template<class T>
    T LumelDistanceToPoly(const LumelPoly2D<T>& P, T px, T py, T& qx, T& qy)
    {
        bool inside = true;
        T best = std::numeric_limits<T>::max();
        qx = px; qy = py;

        const size_t n = P.pts.size();
        for (size_t i = 0; i < n; ++i)
        {
            const auto& a = P.pts[i];
            const auto& b = P.pts[(i + 1) % n];
            const T ex = b[0] - a[0], ey = b[1] - a[1];

            if ((ex * (py - a[1]) - ey * (px - a[0])) * P.orient < T(-1e-6))
                inside = false;

            const T len2 = ex * ex + ey * ey;
            const T t = (len2 > T(0))
                ? std::clamp(((px - a[0]) * ex + (py - a[1]) * ey) / len2, T(0), T(1))
                : T(0);
            const T cx = a[0] + ex * t, cy = a[1] + ey * t;
            const T d2 = (px - cx) * (px - cx) + (py - cy) * (py - cy);
            if (d2 < best) { best = d2; qx = cx; qy = cy; }
        }

        if (inside) { qx = px; qy = py; return T(0); }
        return std::sqrt(best);
    }

    template<class T>
    PolygonUVBasis<T> ComputePolygonUVBasis(const ng::bsp::BSPPolygon<T>& poly)
    {
        PolygonUVBasis<T> basis;
        const Vec3<T> n = poly.plane.normal();

        Vec3<T> axis(0, 0, 0);
        if (std::abs(n(0)) <= std::abs(n(1)) && std::abs(n(0)) <= std::abs(n(2))) axis(0) = T(1);
        else if (std::abs(n(1)) <= std::abs(n(2)))                                 axis(1) = T(1);
        else                                                                       axis(2) = T(1);

        Vec3<T> u = axis % n; u.normalize();
        Vec3<T> v = n % u;    v.normalize();

        T minU = std::numeric_limits<T>::max(), maxU = -std::numeric_limits<T>::max();
        T minV = std::numeric_limits<T>::max(), maxV = -std::numeric_limits<T>::max();

        for (const auto& vert : poly.verts)
        {
            const Vec3<T> p = vert.world();
            const T pu = p(0) * u(0) + p(1) * u(1) + p(2) * u(2);
            const T pv = p(0) * v(0) + p(1) * v(1) + p(2) * v(2);
            minU = std::min(minU, pu); maxU = std::max(maxU, pu);
            minV = std::min(minV, pv); maxV = std::max(maxV, pv);
        }

        basis.axisU = u;
        basis.axisV = v;
        basis.extentU = std::max(T(1e-4), maxU - minU);
        basis.extentV = std::max(T(1e-4), maxV - minV);

        const Vec3<T> ref = poly.verts[0].world();
        const T refU = ref(0) * u(0) + ref(1) * u(1) + ref(2) * u(2);
        const T refV = ref(0) * v(0) + ref(1) * v(1) + ref(2) * v(2);

        basis.origin = ref + u * (minU - refU) + v * (minV - refV);
        return basis;
    }

    template<class T>
    inline int ComputeLightmapWidth(const PolygonUVBasis<T>& basis, const LightmapOptions& opt)
    {
        return std::clamp((int)std::ceil(basis.extentU / (T)opt.texelDensity) + 1, 2, opt.maxLightmapSize);
    }

    template<class T>
    inline int ComputeLightmapHeight(const PolygonUVBasis<T>& basis, const LightmapOptions& opt)
    {
        return std::clamp((int)std::ceil(basis.extentV / (T)opt.texelDensity) + 1, 2, opt.maxLightmapSize);
    }

    // ------------------------------------------------------------
    // LightFalloff - weicher Abfall, keine harte Kante am Radius.
    // ------------------------------------------------------------
    template<class T>
    inline T LightFalloff(T distance, T radius)
    {
        if (distance >= radius) return T(0);
        const T t = distance / radius;
        const T exponential = std::exp(-t * t * T(4));
        const T edge = T(1) - t * t * (T(3) - T(2) * t);
        return exponential * edge;
    }

    // ------------------------------------------------------------
    // ProcessPoly
    //
    // Rendert direktes Licht fuer EIN Polygon in den Atlas, Texel fuer
    // Texel. Occlusion laeuft ueber den uebergebenen BSPTree - typischer-
    // weise der bereits vorhandene, feine Struktur-Baum der Karte.
    // Extra-Polygone (Wasser/Glas) werden hier NIE als "occluderTree"
    // uebergeben - sie blockieren kein Licht.
    //
    // twoSided = true: das Polygon wird von beiden Seiten beleuchtet
    // (Glas, Wasser). Die Normale wird dann pro Licht zur Lichtseite
    // gedreht.
    // ------------------------------------------------------------
    template<class T>
    void ProcessPoly(LightMapAtlas& atlas, ng::bsp::BSPPolygon<T>& poly,
        const ng::bsp::BSPTree<T>& occluderTree,
        const std::vector<LightSource<T>>& lights, T eps,
        const LightmapOptions& opt, bool twoSided = false)
    {
        auto& buf = atlas.Buffer();
        const int W = poly.lmWidth, H = poly.lmHeight;

        // Pro Texel einmal: Samplingposition + gueltig-Flag (unabhaengig vom Licht)
        std::vector<Vec3<T>> pos((size_t)W * H);
        std::vector<char>    valid((size_t)W * H, 1);

        LumelPoly2D<T> poly2d;
        if (opt.onlyCoveredTexels) poly2d = BuildLumelPoly2D(poly);

        for (int iy = 0; iy < H; ++iy)
        {
            for (int ix = 0; ix < W; ++ix)
            {
                T sx = T(ix) + T(0.5), sy = T(iy) + T(0.5);

                if (opt.onlyCoveredTexels)
                {
                    T qx, qy;
                    const T d = LumelDistanceToPoly(poly2d, sx, sy, qx, qy);
                    if (d > (T)opt.coverageMargin) { valid[(size_t)iy * W + ix] = 0; continue; }
                    sx = qx; sy = qy;   // Randtexel: naechster Punkt auf dem Polygon
                }

                pos[(size_t)iy * W + ix] =
                    poly.lmOrigin + poly.lmEdgeU * (sx / T(W)) + poly.lmEdgeV * (sy / T(H));
            }
        }

        for (const auto& light : lights)
        {
            // Lichtseite bestimmen. Einseitig: Licht hinter der Flaeche -> ignorieren.
            // Zweiseitig: Normale zur Lichtseite drehen.
            const T side = (poly.plane.distance(light.pos) >= T(0)) ? T(1) : T(-1);
            if (side < T(0) && !twoSided) continue;
            const Vec3<T> n = poly.plane.normal() * side;

            for (int iy = 0; iy < H; ++iy)
            {
                for (int ix = 0; ix < W; ++ix)
                {
                    if (!valid[(size_t)iy * W + ix]) continue;
                    const Vec3<T>& lumel = pos[(size_t)iy * W + ix];

                    const Vec3<T> toLight = light.pos - lumel;
                    const T dist = toLight.length();
                    if (dist > light.radius || dist < eps) continue;

                    const Vec3<T> dir = toLight * (T(1) / dist);

                    const T ndotl = n ^ dir;
                    if (ndotl <= T(0)) continue;

                    if (BSPRayOccluded(occluderTree, lumel, dir, dist - eps, eps, &poly))
                        continue;

                    const T falloff = LightFalloff(dist, light.radius);
                    if (falloff <= T(0)) continue;

                    const T intensity = light.intensity * falloff * ndotl * (T)opt.gain;

                    const int xx = poly.lmPosX + ix;
                    const int yy = poly.lmPosY + iy;

                    const uint32_t existing = buf.GetPixel(xx, yy);
                    const uint32_t er = RGBToRed(existing), eg = RGBToGreen(existing), eb = RGBToBlue(existing);

                    const uint32_t rr = std::min(255u, er + (uint32_t)(intensity * light.color(0)));
                    const uint32_t gg = std::min(255u, eg + (uint32_t)(intensity * light.color(1)));
                    const uint32_t bb = std::min(255u, eb + (uint32_t)(intensity * light.color(2)));

                    buf.PutPixel(xx, yy, RGB(rr, gg, bb));
                }
            }
        }
    }

    // ------------------------------------------------------------
    // WriteLightmapUVs
    //
    // Schreibt die finalen, normalisierten Atlas-UVs (0..1) in
    // poly.verts[i].fxtexture().
    // ------------------------------------------------------------
    template<class T>
    void WriteLightmapUVs(ng::bsp::BSPPolygon<T>& poly, const PolygonUVBasis<T>& basis,
        int atlasWidth, int atlasHeight)
    {
        const Vec3<T> refP = poly.lmOrigin;
        const T refU = refP(0) * basis.axisU(0) + refP(1) * basis.axisU(1) + refP(2) * basis.axisU(2);
        const T refV = refP(0) * basis.axisV(0) + refP(1) * basis.axisV(1) + refP(2) * basis.axisV(2);

        for (auto& v : poly.verts)
        {
            const Vec3<T> p = v.world();
            const T pu = p(0) * basis.axisU(0) + p(1) * basis.axisU(1) + p(2) * basis.axisU(2);
            const T pv = p(0) * basis.axisV(0) + p(1) * basis.axisV(1) + p(2) * basis.axisV(2);

            const T fracU = (pu - refU) / basis.extentU;
            const T fracV = (pv - refV) / basis.extentV;

            const T atlasU = (T(poly.lmPosX) + fracU * T(poly.lmWidth)) / T(atlasWidth);
            const T atlasV = (T(poly.lmPosY) + fracV * T(poly.lmHeight)) / T(atlasHeight);

            v.fxtexture() = ng::math::Matrix<2, 1, T>(atlasU, atlasV);
        }
    }

    // ------------------------------------------------------------
    // EstimateAtlasSize
    // ------------------------------------------------------------
    template<class T>
    int EstimateAtlasSize(
        const std::vector<ng::bsp::BSPPolygon<T>>& polygons,
        const LightmapOptions& opt)
    {
        double totalArea = 0.0;
        size_t validCount = 0;

        for (const auto& poly : polygons)
        {
            if (poly.verts.size() < 3) continue;

            auto basis = ComputePolygonUVBasis(poly);
            const int lw = ComputeLightmapWidth(basis, opt);
            const int lh = ComputeLightmapHeight(basis, opt);

            totalArea += double(lw + 1) * double(lh + 1);
            ++validCount;
        }

        if (validCount == 0)
            return opt.startAtlasSize;

        const double neededArea = totalArea / std::max(0.05, opt.packEfficiency);

        int size = 64;
        while (double(size) * double(size) < neededArea && size < opt.maxAtlasSize)
            size *= 2;

        return std::min(size, opt.maxAtlasSize);
    }

    template<class T>
    int EstimateAtlasSize(
        const std::vector<ng::bsp::BSPPolygon<T>>& polygonsA,
        const std::vector<ng::bsp::BSPPolygon<T>>& polygonsB,
        const LightmapOptions& opt)
    {
        double totalArea = 0.0;
        size_t validCount = 0;

        auto accumulate = [&](const std::vector<ng::bsp::BSPPolygon<T>>& polys)
            {
                for (const auto& poly : polys)
                {
                    if (poly.verts.size() < 3) continue;
                    auto basis = ComputePolygonUVBasis(poly);
                    const int lw = ComputeLightmapWidth(basis, opt);
                    const int lh = ComputeLightmapHeight(basis, opt);
                    totalArea += double(lw + 1) * double(lh + 1);
                    ++validCount;
                }
            };

        accumulate(polygonsA);
        accumulate(polygonsB);

        if (validCount == 0)
            return opt.startAtlasSize;

        const double neededArea = totalArea / std::max(0.05, opt.packEfficiency);

        int size = 64;
        while (double(size) * double(size) < neededArea && size < opt.maxAtlasSize)
            size *= 2;

        return std::min(size, opt.maxAtlasSize);
    }

    // ------------------------------------------------------------
    // BuildLightmapAtlas
    //
    // Packt "polygons" (opake Struktur-Geometrie), rendert direktes
    // Licht. "occluderTree" ist der bereits vorhandene BSPTree derselben
    // Geometrie (z.B. euer m_bsp) - wird NUR fuer Occlusion-Tests
    // gebraucht, nicht veraendert.
    //
    // "extraPolys" (optional) wird hier NICHT gebacken, sondern nur fuer
    // die Atlasgroessen-Schaetzung beruecksichtigt, damit spaeter in
    // BakeExtraPolygonLightmaps() noch Platz ist.
    // ------------------------------------------------------------
    template<class T>
    std::unique_ptr<LightMapAtlas> BuildLightmapAtlas(
        std::vector<ng::bsp::BSPPolygon<T>>& polygons,
        const ng::bsp::BSPTree<T>& occluderTree,
        const std::vector<LightSource<T>>& lights,
        const LightmapOptions& opt = LightmapOptions(),
        const std::vector<ng::bsp::BSPPolygon<T>>* extraPolys = nullptr)
    {
        int size = std::max(opt.startAtlasSize,
            extraPolys ? EstimateAtlasSize(polygons, *extraPolys, opt)
            : EstimateAtlasSize(polygons, opt));

        while (size <= opt.maxAtlasSize)
        {
            auto atlas = std::make_unique<LightMapAtlas>(size, size);
            bool ok = true;

            std::vector<PolygonUVBasis<T>> bases(polygons.size());

            for (size_t i = 0; i < polygons.size(); ++i)
            {
                auto& poly = polygons[i];
                if (poly.verts.size() < 3) continue;

                bases[i] = ComputePolygonUVBasis(poly);

                const int lw = ComputeLightmapWidth(bases[i], opt);
                const int lh = ComputeLightmapHeight(bases[i], opt);

                auto* leaf = atlas->Reserve(lw, lh);
                if (!leaf) { ok = false; break; }

                poly.lmPosX = leaf->rect.left;
                poly.lmPosY = leaf->rect.top;
                poly.lmWidth = leaf->rect.width();
                poly.lmHeight = leaf->rect.height();

                poly.lmOrigin = bases[i].origin;
                poly.lmEdgeU = bases[i].axisU * bases[i].extentU;
                poly.lmEdgeV = bases[i].axisV * bases[i].extentV;

                const uint32_t floorColor = RGB(opt.kAmbientFloor, opt.kAmbientFloor, opt.kAmbientFloor);
                for (int y = 0; y < poly.lmHeight; ++y)
                    for (int x = 0; x < poly.lmWidth; ++x)
                        atlas->Buffer().PutPixel(poly.lmPosX + x, poly.lmPosY + y, floorColor);
            }

            if (!ok)
            {
                size *= 2;
                continue;
            }

            printf("[Lightmap] Baue direktes Licht: %zu Polygone, Atlas %dx%d, %zu Lichter\n",
                polygons.size(), size, size, lights.size());

            for (size_t i = 0; i < polygons.size(); ++i)
            {
                auto& poly = polygons[i];
                if (poly.verts.size() < 3) continue;

                ProcessPoly(*atlas, poly, occluderTree, lights, (T)opt.epsilon, opt, false);
                WriteLightmapUVs(poly, bases[i], atlas->Width(), atlas->Height());
            }

            printf("[Lightmap] fertig: Atlas %dx%d\n", size, size);
            return atlas;
        }

        printf("[Lightmap] Atlas passt selbst bei %dx%d nicht - Szene zu gross oder texelDensity zu fein.\n", size, size);
        return nullptr;
    }

    // ------------------------------------------------------------
    // BakeExtraPolygonLightmaps
    //
    // Packt "extraPolys" (Wasser, Glas, ...) IN DENSELBEN Atlas.
    // Occlusion laeuft weiterhin nur gegen "occluderTree" (opake
    // Struktur) - Extra-Polygone blockieren kein Licht und werfen
    // keine Schatten. Beleuchtung ist beidseitig.
    // ------------------------------------------------------------
    template<class T>
    bool BakeExtraPolygonLightmaps(
        LightMapAtlas& atlas,
        std::vector<ng::bsp::BSPPolygon<T>>& extraPolys,
        const ng::bsp::BSPTree<T>& occluderTree,
        const std::vector<LightSource<T>>& lights,
        const LightmapOptions& opt = LightmapOptions())
    {
        bool allPacked = true;
        size_t skipped = 0;

        for (auto& poly : extraPolys)
        {
            if (poly.verts.size() < 3) continue;

            const auto basis = ComputePolygonUVBasis(poly);
            const int lw = ComputeLightmapWidth(basis, opt);
            const int lh = ComputeLightmapHeight(basis, opt);

            auto* leaf = atlas.Reserve(lw, lh);
            if (!leaf)
            {
                allPacked = false;
                ++skipped;
                continue;
            }

            poly.lmPosX = leaf->rect.left;
            poly.lmPosY = leaf->rect.top;
            poly.lmWidth = leaf->rect.width();
            poly.lmHeight = leaf->rect.height();

            poly.lmOrigin = basis.origin;
            poly.lmEdgeU = basis.axisU * basis.extentU;
            poly.lmEdgeV = basis.axisV * basis.extentV;

            const uint32_t floorColor = RGB(opt.kAmbientFloor, opt.kAmbientFloor, opt.kAmbientFloor);
            for (int y = 0; y < poly.lmHeight; ++y)
                for (int x = 0; x < poly.lmWidth; ++x)
                    atlas.Buffer().PutPixel(poly.lmPosX + x, poly.lmPosY + y, floorColor);

            ProcessPoly(atlas, poly, occluderTree, lights, (T)opt.epsilon, opt, true);
            WriteLightmapUVs(poly, basis, atlas.Width(), atlas.Height());
        }

        if (!allPacked)
            printf("[Lightmap] Atlas zu klein: %zu Extra-Polygone ohne Lightmap. "
                "BuildLightmapAtlas(..., &extraPolys) benutzen bzw. packEfficiency senken.\n", skipped);

        return allPacked;
    }

    // ------------------------------------------------------------
    // BlurLightmap
    //
    // Blurt GENAU EINE Lightmap eines Polygons. Es wird ausschliesslich
    // innerhalb des Lightmap-Rechtecks des Polygons gelesen und
    // geschrieben, es kann also nichts in benachbarte Lightmaps im
    // Atlas hineinbluten.
    //
    // 3x3 Gauss-Filter (separiert: horizontal, dann vertikal):
    //   1 2 1
    //   2 4 2   / 16
    //   1 2 1
    // ------------------------------------------------------------
    inline void BlurLightmap(
        LightMapAtlas& atlas,
        int posX,
        int posY,
        int width,
        int height)
    {
        if (width <= 1 || height <= 1)
            return;

        auto& buf = atlas.Buffer();

        const size_t count = static_cast<size_t>(width) * static_cast<size_t>(height);
        std::vector<uint32_t> temp(count);

        auto index = [width](int x, int y) -> size_t
            {
                return static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x);
            };

        // Horizontaler Durchlauf (1 2 1 / 4)
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                const int x0 = std::max(0, x - 1);
                const int x1 = x;
                const int x2 = std::min(width - 1, x + 1);

                const uint32_t c0 = buf.GetPixel(posX + x0, posY + y);
                const uint32_t c1 = buf.GetPixel(posX + x1, posY + y);
                const uint32_t c2 = buf.GetPixel(posX + x2, posY + y);

                const int r = (int)RGBToRed(c0) + 2 * (int)RGBToRed(c1) + (int)RGBToRed(c2);
                const int g = (int)RGBToGreen(c0) + 2 * (int)RGBToGreen(c1) + (int)RGBToGreen(c2);
                const int b = (int)RGBToBlue(c0) + 2 * (int)RGBToBlue(c1) + (int)RGBToBlue(c2);

                temp[index(x, y)] = RGB((uint32_t)(r / 4), (uint32_t)(g / 4), (uint32_t)(b / 4));
            }
        }

        // Vertikaler Durchlauf (1 2 1 / 4)
        for (int y = 0; y < height; ++y)
        {
            const int y0 = std::max(0, y - 1);
            const int y1 = y;
            const int y2 = std::min(height - 1, y + 1);

            for (int x = 0; x < width; ++x)
            {
                const uint32_t c0 = temp[index(x, y0)];
                const uint32_t c1 = temp[index(x, y1)];
                const uint32_t c2 = temp[index(x, y2)];

                const int r = (int)RGBToRed(c0) + 2 * (int)RGBToRed(c1) + (int)RGBToRed(c2);
                const int g = (int)RGBToGreen(c0) + 2 * (int)RGBToGreen(c1) + (int)RGBToGreen(c2);
                const int b = (int)RGBToBlue(c0) + 2 * (int)RGBToBlue(c1) + (int)RGBToBlue(c2);

                buf.PutPixel(posX + x, posY + y,
                    RGB((uint32_t)(r / 4), (uint32_t)(g / 4), (uint32_t)(b / 4)));
            }
        }
    }

    // ------------------------------------------------------------
    // BlurAllLightmaps
    //
    // Blurt jede einzelne Lightmap aller Polygone separat.
    // Optional kann passes > 1 verwendet werden, um staerker zu glaetten.
    // ------------------------------------------------------------
    template<class T>
    void BlurAllLightmaps(
        LightMapAtlas& atlas,
        const std::vector<ng::bsp::BSPPolygon<T>>& polygons,
        int passes = 1)
    {
        if (passes <= 0)
            return;

        for (const auto& poly : polygons)
        {
            if (poly.verts.size() < 3)
                continue;

            if (poly.lmWidth <= 1 || poly.lmHeight <= 1)
                continue;

            for (int i = 0; i < passes; ++i)
                BlurLightmap(atlas, poly.lmPosX, poly.lmPosY, poly.lmWidth, poly.lmHeight);
        }
    }
}