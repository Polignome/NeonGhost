#pragma once

// ----------------------------------------------------------------------
// Lightmap.h - Lightmap-Baking fuer BSPPolygon
//
// Ablauf:
//   1. EstimateAtlasSize()   - grobe Vorabschaetzung der Atlasgroesse,
//                              spart in der Regel alle Doubling-Versuche
//   2. BuildLightmapAtlas()  - packt "polys" (opake Struktur-Geometrie),
//                              rendert direktes Licht, schreibt die
//                              finalen Atlas-UVs in poly.verts[i].fxtexture()
//   3. BakeExtraPolygonLightmaps() - packt zusaetzlich "extraPolys"
//                              (Wasser, Glas, ...) in DENSELBEN Atlas.
//                              Diese Flaechen werden selbst beleuchtet,
//                              blockieren aber KEIN Licht (sie tauchen
//                              beim Sichtbarkeitstest nicht als Occluder
//                              auf - nur "polys" tut das).
//
// Reihenfolge in der Anwendung: erst BuildLightmapAtlas() mit den
// opaken Polygonen aufrufen, danach (mit demselben Atlas-Objekt)
// BakeExtraPolygonLightmaps() fuer die Extra-Polygone.
// ----------------------------------------------------------------------

#include "BSP.h"
#include "LightMapAtlas.h"
#include <vector>
#include <cmath>
#include <algorithm>
#include <limits>
#include <cstdio>
#include <random>


namespace ng::lightmap
{
    template<class T>
    using Vec3 = ng::math::Matrix<3, 1, T>;

    // ------------------------------------------------------------
    // LightSource - fertig aufbereitete Lichtquelle fuers Baking.
    // Wird z.B. aus ng::map::LightEntity<T> (Map-Parser) konvertiert.
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
        int    startAtlasSize = 512;
        int    maxAtlasSize = 4096;
        double texelDensity = 16.0;   // Weltunits pro Lightmap-Texel (Half-Life-Stil)
        int    maxLightmapSize = 64;    // groesste Kante eines einzelnen Polygon-Patches
        double epsilon = 1e-3;
        double packEfficiency = 0.65;  // fuer EstimateAtlasSize, siehe dort
        uint8_t kAmbientFloor = 0;
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

    template<class T>
    PolygonUVBasis<T> ComputePolygonUVBasis(const ng::bsp::BSPPolygon<T>& poly)
    {
        PolygonUVBasis<T> basis;
        const Vec3<T> n = poly.plane.normal();

        // Beliebige Achse waehlen, die nicht (fast) parallel zu n ist.
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

        // Referenzpunkt (Vertex 0) auf (minU, minV) zurueckprojizieren,
        // um einen konkreten Weltpunkt fuer den Patch-Ursprung zu haben.
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
    // RayIntersectsPolygon
    //
    // Ray-Test gegen ein konvexes, planares Polygon (trifft bei euch
    // immer zu, da alle BSPPolygon aus BSP-Splits konvex sind).
    // ------------------------------------------------------------
    template<class T>
    bool RayIntersectsPolygon(const Vec3<T>& origin, const Vec3<T>& dir,
        const ng::bsp::BSPPolygon<T>& poly, T& outT, T eps)
    {
        const Vec3<T> n = poly.plane.normal();
        const T denom = n(0) * dir(0) + n(1) * dir(1) + n(2) * dir(2);
        if (std::abs(denom) < eps) return false;

        const T t = -poly.plane.distance(origin) / denom;
        if (t < eps) return false;

        outT = t;
        const Vec3<T> p = origin + dir * t;

        const size_t cnt = poly.verts.size();
        bool sign = false;
        bool first = true;

        for (size_t i = 0; i < cnt; ++i)
        {
            const Vec3<T> a = poly.verts[i].world();
            const Vec3<T> b = poly.verts[(i + 1) % cnt].world();
            const Vec3<T> edge = b - a;
            const Vec3<T> toP = p - a;
            const Vec3<T> c = edge % toP;
            const T d = c(0) * n(0) + c(1) * n(1) + c(2) * n(2);

            const bool s = d >= T(0);
            if (first) { sign = s; first = false; }
            else if (s != sign) return false; // ausserhalb des konvexen Polygons
        }

        return true;
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
    // Texel. "occluders" bestimmt, was Sicht/Licht blockieren darf:
    //   - fuer opake Struktur-Polygone: alle opaken Polygone
    //   - fuer Extra-Polygone (Wasser/Glas): NUR die opaken Polygone,
    //     nie andere Extra-Polygone - Wasser/Glas soll kein Licht
    //     blockieren und keinen Schatten werfen.
    // ------------------------------------------------------------
    template<class T>
    void ProcessPoly(LightMapAtlas& atlas, ng::bsp::BSPPolygon<T>& poly,
        const std::vector<const ng::bsp::BSPPolygon<T>*>& occluders,
        const std::vector<LightSource<T>>& lights, T eps)
    {
        auto& buf = atlas.Buffer();

        for (const auto& light : lights)
        {
            // Liegt das Licht ueberhaupt vor der Polygonebene?
            if (poly.plane.distance(light.pos) < T(0)) continue;

            // Grobe Vorfilterung: nur Occluder in Lichtreichweite testen.
            std::vector<const ng::bsp::BSPPolygon<T>*> nearby;
            nearby.reserve(occluders.size());
            for (const auto* p : occluders)
            {
                if (p == &poly) continue;
                bool close = false;
                for (const auto& v : p->verts)
                {
                    if ((v.world() - light.pos).length() < light.radius * T(2)) { close = true; break; }
                }
                if (close) nearby.push_back(p);
            }

            for (int iy = 0; iy < poly.lmHeight; ++iy)
            {
                for (int ix = 0; ix < poly.lmWidth; ++ix)
                {
                    const T ufactor = T(ix) / T(poly.lmWidth);
                    const T vfactor = T(iy) / T(poly.lmHeight);

                    // Weltposition dieses Lightmap-Texels
                    const Vec3<T> lumel = poly.lmOrigin + poly.lmEdgeU * ufactor + poly.lmEdgeV * vfactor;

                    const Vec3<T> toLight = light.pos - lumel;
                    const T dist = toLight.length();
                    if (dist > light.radius || dist < eps) continue;

                    const Vec3<T> dir = toLight * (T(1) / dist);

                    // Sichtlinie Texel -> Licht auf Verdeckung pruefen
                    bool occluded = false;
                    for (const auto* p : nearby)
                    {
                        T t;
                        if (RayIntersectsPolygon(lumel, dir, *p, t, eps) && t < dist - eps)
                        {
                            occluded = true;
                            break;
                        }
                    }
                    if (occluded) continue;

                    const T falloff = LightFalloff(dist, light.radius);
                    if (falloff <= T(0)) continue;

                    const T intensity = light.intensity * falloff;

                    const int xx = poly.lmPosX + ix;
                    const int yy = poly.lmPosY + iy;

                    // additiv auf bereits vorhandenes Licht drauf (mehrere
                    // Lichtquellen summieren sich, geclampt auf 255)
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
    // poly.verts[i].fxtexture(), aus der u/v-Basis + Atlas-Position.
    // Diese UVs landen ueber BringToScreen()/ScreenVertex in
    // Fragment::fxu/fxv und werden dort von ColorTextureLightmap()
    // zum Sampeln der Lightmap-Textur genutzt.
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
    //
    // Schaetzt die kleinste sinnvolle (Power-of-Two) Atlasgroesse, OHNE
    // zu packen. Summiert die (durch 1px Rand vergroesserte) Flaeche
    // aller Patches und teilt durch einen Packeffizienz-Faktor, da ein
    // Guillotine-Packer nie 100% Fuellgrad erreicht.
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

    // Ueberladung: Schaetzung ueber zwei Polygonlisten zusammen (opake
    // Struktur + Extra-Polygone), damit der Atlas gleich gross genug
    // fuer beides angelegt wird - vermeidet, dass
    // BakeExtraPolygonLightmaps() spaeter "Atlas zu klein" meldet.
    // ------------------------------------------------------------
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
    // Licht, vergroessert den Atlas automatisch bei Ueberlauf (voller
    // Rebuild-Versuch mit doppelter Kantenlaenge). Occluder fuer die
    // Sichtbarkeitstests = die Polygone selbst.
    // ------------------------------------------------------------
    template<class T>
    std::unique_ptr<LightMapAtlas> BuildLightmapAtlas(
        std::vector<ng::bsp::BSPPolygon<T>>& polygons,
        const std::vector<LightSource<T>>& lights,
        const LightmapOptions& opt = LightmapOptions())
    {
        int size = std::max(opt.startAtlasSize, EstimateAtlasSize(polygons, opt));

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



                for (int y = 0; y < poly.lmHeight; ++y)
                    for (int x = 0; x < poly.lmWidth; ++x)
                        atlas->Buffer().PutPixel(poly.lmPosX + x, poly.lmPosY + y,
                            RGB(opt.kAmbientFloor, opt.kAmbientFloor, opt.kAmbientFloor));
            }

            if (!ok)
            {
                size *= 2;
                continue; // kompletter Neuversuch mit groesserem Atlas
            }

            // Occluder-Liste = alle opaken Polygone selbst.
            std::vector<const ng::bsp::BSPPolygon<T>*> occluders;
            occluders.reserve(polygons.size());
            for (const auto& p : polygons)
                if (p.verts.size() >= 3) occluders.push_back(&p);

            printf("[Lightmap] Baue direktes Licht: %zu Polygone, Atlas %dx%d, %zu Lichter\n",
                polygons.size(), size, size, lights.size());

            for (size_t i = 0; i < polygons.size(); ++i)
            {
                auto& poly = polygons[i];
                if (poly.verts.size() < 3) continue;

                ProcessPoly(*atlas, poly, occluders, lights, (T)opt.epsilon);
                WriteLightmapUVs(poly, bases[i], atlas->Width(), atlas->Height());

                if (i < 3) // NEU: Stichprobe
                    printf("[Lightmap] nach WriteLightmapUVs: poly=%zu v0.fxtexture=(%.4f,%.4f)\n",
                        i, (double)poly.verts[0].fxtexture().x(), (double)poly.verts[0].fxtexture().y());
            

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
    // Packt "extraPolys" (Wasser, Glas, ...) IN DENSELBEN, bereits
    // fertigen Atlas und beleuchtet sie. Occluder = NUR "occluderPolys"
    // (die opaken Struktur-Polygone aus BuildLightmapAtlas) - Extra-
    // Polygone blockieren untereinander und fuer sich selbst KEIN Licht
    // und werfen keine Schatten.
    //
    // MUSS nach BuildLightmapAtlas() mit demselben Atlas aufgerufen
    // werden. Wird der Atlas dabei voll, wird NICHT automatisch
    // vergroessert (der bereits gepackte Inhalt der opaken Polygone
    // muesste sonst komplett neu gepackt werden) - stattdessen gibt es
    // eine Warnung, und die ueberzaehligen Polygone bleiben ohne
    // Lightmap-Zuordnung (lmWidth/lmHeight bleiben 0).
    //
    // Um diesen Fall von vornherein zu vermeiden: vor
    // BuildLightmapAtlas() EstimateAtlasSize(polys, extraPolys, opt)
    // statt EstimateAtlasSize(polys, opt) verwenden.
    // ------------------------------------------------------------
    template<class T>
    bool BakeExtraPolygonLightmaps(
        LightMapAtlas& atlas,
        std::vector<ng::bsp::BSPPolygon<T>>& extraPolys,
        const std::vector<ng::bsp::BSPPolygon<T>>& occluderPolys,
        const std::vector<LightSource<T>>& lights,
        const LightmapOptions& opt = LightmapOptions())
    {
        std::vector<const ng::bsp::BSPPolygon<T>*> occluders;
        occluders.reserve(occluderPolys.size());
        for (const auto& p : occluderPolys)
            if (p.verts.size() >= 3) occluders.push_back(&p);

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
                continue; // dieses Polygon bekommt keine Lightmap-Zuordnung
            }

            poly.lmPosX = leaf->rect.left;
            poly.lmPosY = leaf->rect.top;
            poly.lmWidth = leaf->rect.width();
            poly.lmHeight = leaf->rect.height();

            poly.lmOrigin = basis.origin;
            poly.lmEdgeU = basis.axisU * basis.extentU;
            poly.lmEdgeV = basis.axisV * basis.extentV;

            for (int y = 0; y < poly.lmHeight; ++y)
                for (int x = 0; x < poly.lmWidth; ++x)
                    atlas.Buffer().PutPixel(poly.lmPosX + x, poly.lmPosY + y, RGB(0, 0, 0));

            ProcessPoly(atlas, poly, occluders, lights, (T)opt.epsilon);
            WriteLightmapUVs(poly, basis, atlas.Width(), atlas.Height());
        }

        if (!allPacked)
            printf("[Lightmap] Atlas zu klein: %zu Extra-Polygone ohne Lightmap. "
                "EstimateAtlasSize(polys, extraPolys, opt) vor BuildLightmapAtlas() nutzen.\n", skipped);

        return allPacked;
    }

    // ------------------------------------------------------------
    // Radiosity (indirektes Licht / Bounces)
    //
    // Patch-basiert, EIN Patch pro opakem Polygon (keine Subdivision -
    // grobe, aber schnelle erste Ausbaustufe. Grosse Flaechen bekommen
    // dadurch ein gleichmaessiges indirektes Licht ohne Gradient; feinere
    // Ergebnisse braeuchten eine Patch-Unterteilung grosser Polygone).
    //
    // Ablauf: BuildLightmapAtlas() (direktes Licht) MUSS vorher gelaufen
    // sein - die Patches lesen ihre "Emission" (bounce 0) direkt aus dem
    // bereits gebackenen Atlas-Bereich des jeweiligen Polygons aus.
    // ------------------------------------------------------------

    template<class T>
    struct RadiosityOptions
    {
        int    bounces = 3;
        int    samplesPerPatch = 48;   // Hemisphere-Samples pro Patch und Bounce
        double maxRayDistance = 4096.0;
        double epsilon = 1e-3;
        unsigned int randomSeed = 12345u;
    };

    template<class T>
    struct RadiosityPatch
    {
        const ng::bsp::BSPPolygon<T>* poly = nullptr;
        Vec3<T> center;
        Vec3<T> normal;
        Vec3<T> albedo;       // 0..1 pro Kanal
        Vec3<T> residual;     // was dieser Patch im NAECHSTEN Bounce "aussendet"
        Vec3<T> accumulated;  // Summe allen empfangenen indirekten Lichts (0..255-Skala)
        // Bounding-Sphaere fuer schnellen Ray-Ausschluss
        Vec3<T> bsCenter;
        T bsRadius = T(0);
    };

    // ------------------------------------------------------------
    // ComputePolygonAlbedo
    //
    // Grobe Durchschnittsfarbe der Diffuse-Textur (0..1). Ohne Textur
    // (poly.texture leer) wird ein neutrales Grau angenommen. Aus
    // Performancegruenden wird nur ein Sub-Sample der Pixel gelesen
    // (jedes 4. Pixel in jeder Achse) statt der kompletten Textur.
    // ------------------------------------------------------------
    template<class T>
    Vec3<T> ComputePolygonAlbedo(const ng::bsp::BSPPolygon<T>& poly)
    {
        if (!poly.texture)
            return Vec3<T>(T(0.5), T(0.5), T(0.5));

        const int w = poly.texture->Width();
        const int h = poly.texture->Height();
        if (w <= 0 || h <= 0)
            return Vec3<T>(T(0.5), T(0.5), T(0.5));

        const int stride = 4;
        uint64_t sr = 0, sg = 0, sb = 0, count = 0;

        for (int y = 0; y < h; y += stride)
        {
            for (int x = 0; x < w; x += stride)
            {
                const uint32_t px = poly.texture->GetPixel(x, y);
                sr += RGBToRed(px);
                sg += RGBToGreen(px);
                sb += RGBToBlue(px);
                ++count;
            }
        }

        if (count == 0)
            return Vec3<T>(T(0.5), T(0.5), T(0.5));

        return Vec3<T>(
            T(double(sr) / double(count)) / T(255),
            T(double(sg) / double(count)) / T(255),
            T(double(sb) / double(count)) / T(255));
    }

    // ------------------------------------------------------------
    // BuildRadiosityPatches
    //
    // EIN Patch pro opakem Polygon. Liest die "Emission" (bereits
    // gebackenes direktes Licht) als Durchschnitt aus dem Atlas-Bereich
    // dieses Polygons - BuildLightmapAtlas() muss also vorher gelaufen
    // sein und dieselben lmPosX/Y/Width/Height gesetzt haben.
    // ------------------------------------------------------------
    template<class T>
    std::vector<RadiosityPatch<T>> BuildRadiosityPatches(
         LightMapAtlas& atlas,
        const std::vector<ng::bsp::BSPPolygon<T>>& polygons)
    {
        std::vector<RadiosityPatch<T>> patches;
        patches.reserve(polygons.size());

        for (const auto& poly : polygons)
        {
            if (poly.verts.size() < 3 || poly.lmWidth <= 0 || poly.lmHeight <= 0)
                continue;

            RadiosityPatch<T> patch;
            patch.poly = &poly;
            patch.normal = poly.plane.normal();
            patch.albedo = ComputePolygonAlbedo(poly);

            // Zentrum + Bounding-Sphaere aus den Vertices
            Vec3<T> c(0, 0, 0);
            T maxR = T(0);
            for (const auto& v : poly.verts) c = c + v.world();
            c = c * (T(1) / T(poly.verts.size()));
            for (const auto& v : poly.verts)
                maxR = std::max(maxR, (v.world() - c).length());

            patch.center = c + patch.normal * T(0.5); // leicht ueber die Flaeche versetzt (Self-Intersection vermeiden)
            patch.bsCenter = c;
            patch.bsRadius = maxR;

            // Durchschnitt des bereits gebackenen direkten Lichts aus dem Atlas
            uint64_t sr = 0, sg = 0, sb = 0;
            const int total = poly.lmWidth * poly.lmHeight;
            for (int y = 0; y < poly.lmHeight; ++y)
            {
                for (int x = 0; x < poly.lmWidth; ++x)
                {
                    const uint32_t px = atlas.Buffer().GetPixel(poly.lmPosX + x, poly.lmPosY + y);
                    sr += RGBToRed(px);
                    sg += RGBToGreen(px);
                    sb += RGBToBlue(px);
                }
            }

            Vec3<T> directLight(0, 0, 0);
            if (total > 0)
            {
                directLight = Vec3<T>(
                    T(double(sr) / double(total)),
                    T(double(sg) / double(total)),
                    T(double(sb) / double(total)));
            }

            // Bounce 0: das Residual, das dieser Patch aussendet, ist das
            // direkte Licht multipliziert mit der Albedo (nur der reflektierte
            // Anteil traegt zur naechsten Bounce-Stufe bei).
            patch.residual = Vec3<T>(
                directLight(0) * patch.albedo(0),
                directLight(1) * patch.albedo(1),
                directLight(2) * patch.albedo(2));

            patch.accumulated = Vec3<T>(0, 0, 0);

            patches.push_back(patch);
        }

        return patches;
    }

    // ------------------------------------------------------------
    // CosineWeightedHemisphereSample
    //
    // Erzeugt eine kosinus-gewichtete Zufallsrichtung um "normal".
    // Kosinusgewichtung ist wichtig: dadurch entfaellt der zusaetzliche
    // N*L-Term bei der Auswertung - jeder Treffer zaehlt gleich, das
    // Sample-Muster selbst bildet die Kosinus-Verteilung ab.
    // ------------------------------------------------------------
    template<class T>
    Vec3<T> CosineWeightedHemisphereSample(const Vec3<T>& normal, std::mt19937& rng)
    {
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        const double u1 = dist(rng);
        const double u2 = dist(rng);

        const double r = std::sqrt(u1);
        const double theta = 2.0 * 3.14159265358979323846 * u2;

        const double lx = r * std::cos(theta);
        const double ly = r * std::sin(theta);
        const double lz = std::sqrt(std::max(0.0, 1.0 - u1));

        // Lokale Basis um "normal"
        Vec3<T> axis(0, 0, 0);
        if (std::abs(normal(0)) <= std::abs(normal(1)) && std::abs(normal(0)) <= std::abs(normal(2))) axis(0) = T(1);
        else if (std::abs(normal(1)) <= std::abs(normal(2))) axis(1) = T(1);
        else axis(2) = T(1);

        Vec3<T> tangent = axis % normal; tangent.normalize();
        Vec3<T> bitangent = normal % tangent; bitangent.normalize();

        return tangent * T(lx) + bitangent * T(ly) + normal * T(lz);
    }

    // ------------------------------------------------------------
    // FindNearestPatchHit
    //
    // Wie RayIntersectsPolygon, aber sucht ueber ALLE Patches den
    // naechstgelegenen Treffer und liefert dessen Index. Nutzt die
    // Bounding-Sphaere jedes Patches zum schnellen Vorab-Ausschluss.
    // ------------------------------------------------------------
    template<class T>
    int FindNearestPatchHit(const Vec3<T>& origin, const Vec3<T>& dir,
        const std::vector<RadiosityPatch<T>>& patches, int excludeIdx, T maxDist, T eps)
    {
        int bestIdx = -1;
        T bestT = maxDist;

        for (size_t i = 0; i < patches.size(); ++i)
        {
            if ((int)i == excludeIdx) continue;

            const auto& p = patches[i];

            // Grober Kugel-Ausschluss: liegt die Kugel ueberhaupt in
            // Strahlrichtung nah genug dran?
            const Vec3<T> toSphere = p.bsCenter - origin;
            const T proj = toSphere(0) * dir(0) + toSphere(1) * dir(1) + toSphere(2) * dir(2);
            if (proj < T(0) || proj > bestT + p.bsRadius) continue;
            const Vec3<T> closest = origin + dir * proj;
            if ((closest - p.bsCenter).length() > p.bsRadius + eps) continue;

            T t;
            if (RayIntersectsPolygon(origin, dir, *p.poly, t, eps) && t < bestT)
            {
                bestT = t;
                bestIdx = (int)i;
            }
        }

        return bestIdx;
    }

    // ------------------------------------------------------------
    // RunRadiosityBounces
    //
    // Jacobi-Iteration: pro Bounce werden alle Patches parallel (in
    // Sicht der Daten, nicht zwingend Multithreading) aktualisiert, damit
    // keine Reihenfolge-Abhaengigkeit entsteht.
    // ------------------------------------------------------------
    template<class T>
    void RunRadiosityBounces(std::vector<RadiosityPatch<T>>& patches,
        const RadiosityOptions<T>& opt)
    {
        std::mt19937 rng(opt.randomSeed);

        std::vector<Vec3<T>> incoming(patches.size());

        for (int bounce = 0; bounce < opt.bounces; ++bounce)
        {
            printf("[Radiosity] Bounce %d / %d ...\r", bounce + 1, opt.bounces);

            for (size_t i = 0; i < patches.size(); ++i)
            {
                const auto& receiver = patches[i];
                Vec3<T> sum(0, 0, 0);
                int hits = 0;

                for (int s = 0; s < opt.samplesPerPatch; ++s)
                {
                    const Vec3<T> dir = CosineWeightedHemisphereSample(receiver.normal, rng);

                    const int hitIdx = FindNearestPatchHit(receiver.center, dir, patches,
                        (int)i, (T)opt.maxRayDistance, (T)opt.epsilon);

                    if (hitIdx < 0) continue;

                    sum = sum + patches[hitIdx].residual;
                    ++hits;
                }

                incoming[i] = (hits > 0) ? (sum * (T(1) / T(opt.samplesPerPatch))) : Vec3<T>(0, 0, 0);
            }

            // Erst NACH der kompletten Bounce-Runde uebernehmen (Jacobi-Style):
            for (size_t i = 0; i < patches.size(); ++i)
            {
                patches[i].accumulated = patches[i].accumulated + incoming[i];

                patches[i].residual = Vec3<T>(
                    incoming[i](0) * patches[i].albedo(0),
                    incoming[i](1) * patches[i].albedo(1),
                    incoming[i](2) * patches[i].albedo(2));
            }
        }
    }

    // ------------------------------------------------------------
    // ApplyRadiosityToAtlas
    //
    // Schreibt patch.accumulated additiv (gleichmaessig, ohne Gradient)
    // ueber alle Lightmap-Texel des zugehoerigen Polygons in den Atlas.
    // Muss NACH RunRadiosityBounces() aufgerufen werden.
    // ------------------------------------------------------------
    template<class T>
    void ApplyRadiosityToAtlas(LightMapAtlas& atlas, const std::vector<RadiosityPatch<T>>& patches)
    {
        for (const auto& patch : patches)
        {
            const auto& poly = *patch.poly;
            if (poly.lmWidth <= 0 || poly.lmHeight <= 0) continue;

            const uint32_t addR = (uint32_t)std::clamp((double)patch.accumulated(0), 0.0, 255.0);
            const uint32_t addG = (uint32_t)std::clamp((double)patch.accumulated(1), 0.0, 255.0);
            const uint32_t addB = (uint32_t)std::clamp((double)patch.accumulated(2), 0.0, 255.0);

            for (int y = 0; y < poly.lmHeight; ++y)
            {
                for (int x = 0; x < poly.lmWidth; ++x)
                {
                    const int xx = poly.lmPosX + x;
                    const int yy = poly.lmPosY + y;

                    const uint32_t existing = atlas.Buffer().GetPixel(xx, yy);
                    const uint32_t er = RGBToRed(existing), eg = RGBToGreen(existing), eb = RGBToBlue(existing);

                    const uint32_t r = std::min(255u, er + addR);
                    const uint32_t g = std::min(255u, eg + addG);
                    const uint32_t b = std::min(255u, eb + addB);

                    atlas.Buffer().PutPixel(xx, yy, RGB(r, g, b));
                }
            }
        }
    }

    // ------------------------------------------------------------
    // BakeRadiosity
    //
    // Komplettablauf: Patches bauen, Bounces rechnen, ins Atlas schreiben.
    // "occluderPolys" = dieselbe opake Polygonliste, die auch fuer
    // BuildLightmapAtlas() als "polygons" diente (Wasser/Glas bewusst
    // aussen vor - siehe Begruendung zu Occludern bei ProcessPoly).
    // ------------------------------------------------------------
    template<class T>
    void BakeRadiosity(LightMapAtlas& atlas,
        const std::vector<ng::bsp::BSPPolygon<T>>& occluderPolys,
        const RadiosityOptions<T>& opt = RadiosityOptions<T>())
    {
        auto patches = BuildRadiosityPatches(atlas, occluderPolys);

        printf("[Radiosity] %zu Patches, %d Bounces, %d Samples/Patch\n",
            patches.size(), opt.bounces, opt.samplesPerPatch);

        if (patches.empty())
        {
            printf("[Radiosity] keine Patches - uebersprungen.\n");
            return;
        }

        RunRadiosityBounces(patches, opt);
        printf("\n");


        // TEMP-DEBUG
        double maxAcc = 0.0, sumAcc = 0.0;
        size_t nonZero = 0;
        for (const auto& p : patches)
        {
            const double m = std::max({ (double)p.accumulated(0), (double)p.accumulated(1), (double)p.accumulated(2) });
            maxAcc = std::max(maxAcc, m);
            sumAcc += m;
            if (m > 0.01) ++nonZero;
        }
        printf("[Radiosity] accumulated: max=%.3f avg=%.3f nonZero=%zu/%zu\n",
            maxAcc, patches.empty() ? 0.0 : sumAcc / patches.size(), nonZero, patches.size());





        ApplyRadiosityToAtlas(atlas, patches);

        printf("[Radiosity] fertig.\n");
    }

}