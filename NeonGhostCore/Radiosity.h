#pragma once
#include "Lightmap.h"

namespace ng::lightning {
    // ================================================================
        // Radiosity (indirektes Licht / Bounces), Block-basiert
        //
        // Jedes opake Polygon wird in Bloecke von "blockSize x blockSize"
        // Lightmap-Texeln unterteilt. Jeder Block ist EIN Patch mit eigener
        // Albedo (Durchschnitt der Diffuse-Textur unter diesem Block) und
        // eigener Emission (Durchschnitt des bereits gebackenen direkten
        // Lichts unter diesem Block). Das ergibt echte Farbuebertragung
        // (Color Bleeding) und einen Gradienten statt einer flachen
        // Aufhellung pro Polygon.
        //
        // Occlusion/Nearest-Hit laeuft ueber einen intern gebauten BSPTree,
        // dessen Polygone per objectId auf ihren Index in "occluderPolys"
        // zurueckverweisen (NICHT auf einen Patch-Index - ein Polygon hat ja
        // mehrere Patches). Aus Trefferpunkt + Index wird der getroffene
        // Block per UV-Projektion bestimmt.
        // ================================================================
      
     // ------------------------------------------------------------
    // RadiosityLookup
    //
    // Ordnet einem Polygon-Index (Position in occluderPolys) den
    // Bereich seiner Bloecke im "patches"-Array zu, damit ein Ray-Hit
    // (der nur den Polygon-Index kennt, via objectId) schnell auf den
    // richtigen Block-Patch abgebildet werden kann.
    // ------------------------------------------------------------
    struct RadiosityLookup
    {
        std::vector<int> patchOffset; // Index in "patches", -1 = kein Patch (zu klein/keine Lightmap)
        std::vector<int> blocksW;
        std::vector<int> blocksH;
    };

    template<class T>
    struct RadiosityOptions
    {
		bool   buildDirectRadiosity = true;   // nur im commpiler thread wichtig
        int    bounces = 4;
        int    samplesPerPatch = 32;
        double maxRayDistance = 4096.0;
        double epsilon = 1e-3;
        unsigned int randomSeed = 12345u;
        double ambientFloor = 0.0;   // gleicher Wert wie LightmapOptions::kAmbientFloor
        double indirectGain = 1.0;   // zum Testen z.B. 4.0
        // Kantenlaenge eines Patch-Blocks in Lightmap-Texeln. Kleiner =
        // feinere Gradienten, aber mehr Patches -> langsamer.
        int blockSize = 4;

        // Albedo-Clamp, verhindert Energieexplosion (nahe 1.0) bzw.
        // komplettes Aussterben des Bounce-Lichts (nahe 0.0).
        double albedoMin = 0.05;
        double albedoMax = 0.85;

        // Optionales Eigenleuchten aus der Texturfarbe (z.B. Lampen).
        // 0 = aus. Gilt aktuell fuer ALLE Polygone gleich - fuer
        // selektives Verhalten vor dem Aufruf per Materialflag filtern.
        T textureEmissionStrength = T(0);

        // Obergrenze fuer den Wert (max. Kanal) eines einzelnen Strahl-Treffers.
// Verhindert Firefly-Flecken durch kleine, sehr helle Quellen.
        double maxSampleValue = 64.0;

        // Anzahl Blur-Durchlaeufe auf dem eingehenden Licht pro Bounce (0 = aus).
        int blurPasses = 1;
    };

    template<class T>
    struct RadiosityPatch
    {
        const ng::bsp::BSPPolygon<T>* poly = nullptr; // Original aus occluderPolys
        int x0 = 0, y0 = 0, x1 = 0, y1 = 0;           // abgedeckter Lightmap-Texelbereich [x0,x1)x[y0,y1)
        Vec3<T> center;
        Vec3<T> normal;
        Vec3<T> albedo;
        Vec3<T> residual;
        Vec3<T> accumulated;
    };

   
    enum class PatchDumpMode { Albedo, Residual, Accumulated };


    template<class T>
    void BlurIncoming(std::vector<Vec3<T>>& incoming, const RadiosityLookup& lookup)
    {
        std::vector<Vec3<T>> out = incoming;

        for (size_t pi = 0; pi < lookup.patchOffset.size(); ++pi)
        {
            const int off = lookup.patchOffset[pi];
            if (off < 0) continue;

            const int bw = lookup.blocksW[pi];
            const int bh = lookup.blocksH[pi];

            for (int by = 0; by < bh; ++by)
            {
                for (int bx = 0; bx < bw; ++bx)
                {
                    Vec3<T> sum(0, 0, 0);
                    T wsum = T(0);

                    for (int dy = -1; dy <= 1; ++dy)
                    {
                        for (int dx = -1; dx <= 1; ++dx)
                        {
                            const int x = bx + dx;
                            const int y = by + dy;
                            if (x < 0 || y < 0 || x >= bw || y >= bh) continue;

                            const T w = (dx == 0 && dy == 0) ? T(4)
                                : (dx == 0 || dy == 0) ? T(2) : T(1);

                            sum = sum + incoming[off + y * bw + x] * w;
                            wsum = wsum + w;
                        }
                    }

                    out[off + by * bw + bx] = sum * (T(1) / wsum);
                }
            }
        }

        incoming.swap(out);
    }

    // ------------------------------------------------------------
    // DumpPatchesBMP
    //
    // Schreibt pro Patch einen konstanten Block (x0..x1, y0..y1) an dessen
    // Atlas-Position in ein Bild gleicher Groesse wie der Atlas.
    //   Albedo      : albedo * 255
    //   Residual    : residual * boost            (Startwert direct * albedo)
    //   Accumulated : accumulated * gain * boost  (Ergebnis der Bounces)
    // Nicht von Patches abgedeckte Texel bleiben schwarz.
    // ------------------------------------------------------------
    template<class T>
    void DumpPatchesBMP(const LightMapAtlas& atlas,
        const std::vector<RadiosityPatch<T>>& patches,
        const char* path, PatchDumpMode mode,
        float boost = 1.0f, float gain = 1.0f)
    {
        ng::renderer::buffers::PixelBufferRGBA img(atlas.Width(), atlas.Height());

        const uint32_t black = RGB(0, 0, 0);
        for (int y = 0; y < atlas.Height(); ++y)
            for (int x = 0; x < atlas.Width(); ++x)
                img.PutPixel(x, y, black);

        auto toByte = [](double v) -> uint32_t
            {
                return (uint32_t)std::clamp(v, 0.0, 255.0);
            };

        for (const auto& p : patches)
        {
            const auto& poly = *p.poly;

            double r, g, b;
            switch (mode)
            {
            case PatchDumpMode::Albedo:
                r = p.albedo(0) * 255.0; g = p.albedo(1) * 255.0; b = p.albedo(2) * 255.0;
                break;
            case PatchDumpMode::Residual:
                r = p.residual(0) * boost; g = p.residual(1) * boost; b = p.residual(2) * boost;
                break;
            default:
                r = p.accumulated(0) * gain * boost;
                g = p.accumulated(1) * gain * boost;
                b = p.accumulated(2) * gain * boost;
                break;
            }

            const uint32_t col = RGB(toByte(r), toByte(g), toByte(b));

            for (int y = p.y0; y < p.y1; ++y)
                for (int x = p.x0; x < p.x1; ++x)
                    img.PutPixel(poly.lmPosX + x, poly.lmPosY + y, col);
        }

        img.SaveAsBMP(path);
    }
    // ------------------------------------------------------------
// ComputePolygonAlbedo
//
// Grobe Durchschnittsfarbe der Diffuse-Textur (0..1). Ohne Textur
// wird ein neutrales Grau angenommen.
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

    
    template<class T>
    void NormalizePatchAlbedo(std::vector<RadiosityPatch<T>>& patches,
        double targetMean, double maxAlbedo)
    {
        double sum = 0; size_t n = 0;
        for (const auto& p : patches)
        {
            sum += ((double)p.albedo(0) + (double)p.albedo(1) + (double)p.albedo(2)) / 3.0;
            ++n;
        }
        if (n == 0) return;

        const double mean = sum / double(n);
        if (mean < 1e-4) return;
        const double s = targetMean / mean;

        for (auto& p : patches)
        {
            const double mx = std::max({ (double)p.albedo(0), (double)p.albedo(1), (double)p.albedo(2) });
            const double ps = std::min(s, maxAlbedo / std::max(mx, 1e-4));
            p.albedo = p.albedo * T(ps);
            p.residual = p.residual * T(ps);   // gilt, solange keine Textur-Emission genutzt wird
        }
        printf("[Radiosity] Albedo normalisiert: Mittel %.3f -> Ziel %.2f (Faktor %.2f)\n",
            mean, targetMean, s);
    }
    // ------------------------------------------------------------
    // BuildRadiosityOcclusionTree
    //
    // Baut einen eigenstaendigen BSPTree aus Kopien von "occluderPolys",
    // wobei jedes Polygon per objectId auf seinen Index im patches-
    // Array zurueckverweist. So kann BSPRayNearestHit() nach einem
    // Treffer direkt den zugehoerigen Patch nachschlagen, ohne selbst
    // ueber alle Patches iterieren zu muessen.
    // ------------------------------------------------------------
    template<class T>
    std::unique_ptr<ng::bsp::BSPTree<T>> BuildRadiosityOcclusionTree(
        const std::vector<RadiosityPatch<T>>& patches)
    {
        std::vector<ng::bsp::BSPPolygon<T>> work;
        work.reserve(patches.size());

        for (size_t i = 0; i < patches.size(); ++i)
        {
            ng::bsp::BSPPolygon<T> copy = *patches[i].poly;
            copy.objectId = (uint32_t)i; // Zweckentfremdet: Patch-Index
            work.push_back(std::move(copy));
        }

        auto tree = std::make_unique<ng::bsp::BSPTree<T>>();
        tree->SetVerbose(false);
        tree->Build(work);
        // Keine Portale noetig - wir brauchen nur FindLeafId-artiges
        // Ray-Traversal (BSPRayTest), nicht BuildPortals().
        return tree;
    }

   

 
   
    
   // ------------------------------------------------------------
   // ApplyRadiosityToAtlas
   //
   // Schreibt patch.accumulated additiv ueber genau den Block
   // (x0..x1, y0..y1), den dieser Patch abdeckt.
   // ------------------------------------------------------------
   
    template<class T>
    void ApplyRadiosityToAtlas(LightMapAtlas& atlas,
        const std::vector<RadiosityPatch<T>>& patches,
        const RadiosityLookup& lookup,
        const RadiosityOptions<T>& opt)
    {
        const int B = std::max(1, opt.blockSize);
        const double g = opt.indirectGain;

        for (size_t pi = 0; pi < lookup.patchOffset.size(); ++pi)
        {
            const int off = lookup.patchOffset[pi];
            if (off < 0) continue;

            const int bw = lookup.blocksW[pi];
            const int bh = lookup.blocksH[pi];
            const auto& poly = *patches[off].poly;

            auto acc = [&](int bx, int by, int c) -> double {
                return (double)patches[off + by * bw + bx].accumulated(c);
                };

            for (int y = 0; y < poly.lmHeight; ++y)
            {
                // Position im Blockraster; Blockmitte liegt bei bx*B + B/2
                const double gy = (y + 0.5) / B - 0.5;
                const int    iy0 = (int)std::floor(gy);
                const double fy = gy - iy0;
                const int    by0 = std::clamp(iy0, 0, bh - 1);
                const int    by1 = std::clamp(iy0 + 1, 0, bh - 1);

                for (int x = 0; x < poly.lmWidth; ++x)
                {
                    const double gx = (x + 0.5) / B - 0.5;
                    const int    ix0 = (int)std::floor(gx);
                    const double fx = gx - ix0;
                    const int    bx0 = std::clamp(ix0, 0, bw - 1);
                    const int    bx1 = std::clamp(ix0 + 1, 0, bw - 1);

                    uint32_t add[3];
                    for (int c = 0; c < 3; ++c)
                    {
                        const double top = acc(bx0, by0, c) * (1 - fx) + acc(bx1, by0, c) * fx;
                        const double bot = acc(bx0, by1, c) * (1 - fx) + acc(bx1, by1, c) * fx;
                        const double v = (top * (1 - fy) + bot * fy) * g;
                        add[c] = (uint32_t)std::clamp(std::round(v), 0.0, 255.0);
                    }

                    const int xx = poly.lmPosX + x;
                    const int yy = poly.lmPosY + y;
                    const uint32_t e = atlas.Buffer().GetPixel(xx, yy);

                    atlas.Buffer().PutPixel(xx, yy, RGB(
                        std::min(255u, RGBToRed(e) + add[0]),
                        std::min(255u, RGBToGreen(e) + add[1]),
                        std::min(255u, RGBToBlue(e) + add[2])));
                }
            }
        }
    }

    
    // ------------------------------------------------------------
    // TexUVAtPoint
    //
    // Diffuse-Textur-UV an einer Weltposition auf dem Polygon, per
    // Baryzentrik aus dem flaechengroessten Dreieck des (konvexen)
    // Polygons - stabil auch bei mehr als 3 Vertices.
    // ------------------------------------------------------------
    template<class T>
    ng::math::Matrix<2, 1, T> TexUVAtPoint(const ng::bsp::BSPPolygon<T>& poly, const Vec3<T>& p)
    {
        const auto& v = poly.verts;
        if (v.size() < 3) return ng::math::Matrix<2, 1, T>(0, 0);

        size_t bi = 1, bk = 2;
        T best = T(0);
        for (size_t i = 1; i + 1 < v.size(); ++i)
        {
            const T a = ((v[i].world() - v[0].world()) % (v[i + 1].world() - v[0].world())).length();
            if (a > best) { best = a; bi = i; bk = i + 1; }
        }

        const Vec3<T> e1 = v[bi].world() - v[0].world();
        const Vec3<T> e2 = v[bk].world() - v[0].world();
        const Vec3<T> d = p - v[0].world();

        const T d11 = e1 ^ e1, d12 = e1 ^ e2, d22 = e2 ^ e2;
        const T d1 = d ^ e1, d2 = d ^ e2;
        const T den = d11 * d22 - d12 * d12;

        if (std::abs(den) < T(1e-12))
            return v[0].texture();

        const T b1 = (d22 * d1 - d12 * d2) / den;
        const T b2 = (d11 * d2 - d12 * d1) / den;

        return v[0].texture() + (v[bi].texture() - v[0].texture()) * b1
            + (v[bk].texture() - v[0].texture()) * b2;
    }

    // ------------------------------------------------------------
    // SampleAlbedoAtUV
    //
    // Diffuse-Texturfarbe (0..1) an normalisierten UV-Koordinaten, mit
    // Wrap. Ohne Textur: neutrales Grau.
    // ------------------------------------------------------------
    template<class T>
    Vec3<T> SampleAlbedoAtUV(const ng::bsp::BSPPolygon<T>& poly, T u, T v)
    {
        if (!poly.texture)
            return Vec3<T>(T(0.5), T(0.5), T(0.5));

        const int w = poly.texture->Width();
        const int h = poly.texture->Height();
        if (w <= 0 || h <= 0)
            return Vec3<T>(T(0.5), T(0.5), T(0.5));

        int x = (int)std::floor((double)u * w) % w; if (x < 0) x += w;
        int y = (int)std::floor((double)v * h) % h; if (y < 0) y += h;

        const uint32_t px = poly.texture->GetPixel(x, y);
        return Vec3<T>(T(RGBToRed(px)), T(RGBToGreen(px)), T(RGBToBlue(px))) * (T(1) / T(255));
    }

    template<class T>
    Vec3<T> ComputeBlockTextureColor(const ng::bsp::BSPPolygon<T>& poly,
        int x0, int y0, int x1, int y1)
    {
        Vec3<T> sum(0, 0, 0);
        int n = 0;
        for (int y = y0; y < y1; ++y)
            for (int x = x0; x < x1; ++x)
            {
                const T fu = (T(x) + T(0.5)) / T(poly.lmWidth);
                const T fv = (T(y) + T(0.5)) / T(poly.lmHeight);
                const Vec3<T> wp = poly.lmOrigin + poly.lmEdgeU * fu + poly.lmEdgeV * fv;
                const auto uv = TexUVAtPoint(poly, wp);
                sum = sum + SampleAlbedoAtUV(poly, uv.x(), uv.y());
                ++n;
            }
        return n ? sum * (T(1) / T(n)) : Vec3<T>(0, 0, 0);
    }
// ComputeBlockAlbedo
//
// Durchschnittliche Diffuse-Texturfarbe (0..1) unter einem Lightmap-
// Texel-Block, per Multi-Sample (Texelmitten). Der Farbton bleibt beim
// Begrenzen erhalten: es wird nur der hellste Kanal auf [albedoMin,
// albedoMax] gebracht und die anderen Kanaele proportional mitskaliert.
// ------------------------------------------------------------
    template<class T>
    Vec3<T> ComputeBlockAlbedo(const ng::bsp::BSPPolygon<T>& poly,
        int x0, int y0, int x1, int y1, T albedoMin, T albedoMax)
    {
        Vec3<T> sum(0, 0, 0);
        int n = 0;

        for (int y = y0; y < y1; ++y)
        {
            for (int x = x0; x < x1; ++x)
            {
                const T fu = (T(x) + T(0.5)) / T(poly.lmWidth);
                const T fv = (T(y) + T(0.5)) / T(poly.lmHeight);
                const Vec3<T> wp = poly.lmOrigin + poly.lmEdgeU * fu + poly.lmEdgeV * fv;

                const auto uv = TexUVAtPoint(poly, wp);
                sum = sum + SampleAlbedoAtUV(poly, uv.x(), uv.y());
                ++n;
            }
        }

        const T mid = (albedoMin + albedoMax) * T(0.5);
        if (n == 0)
            return Vec3<T>(mid, mid, mid);

        Vec3<T> a = sum * (T(1) / T(n));

        const T mx = std::max({ a(0), a(1), a(2) });

        if (mx < T(1e-4))
        {
            // praktisch schwarz: kein Farbton vorhanden -> neutrales Minimum
            a = Vec3<T>(albedoMin, albedoMin, albedoMin);
        }
        else if (mx < albedoMin)
        {
            // zu dunkel: aufhellen, Farbverhaeltnis bleibt erhalten
            a = a * (albedoMin / mx);
        }
        else if (mx > albedoMax)
        {
            // zu hell: abdunkeln, Farbverhaeltnis bleibt erhalten
            a = a * (albedoMax / mx);
        }

        return a;
    }

    // ------------------------------------------------------------
    // ComputeBlockDirectLight
    //
    // Durchschnitt des bereits gebackenen direkten Lichts (0..255)
    // unter einem Lightmap-Texel-Block.
    // ------------------------------------------------------------
    template<class T>
    Vec3<T> ComputeBlockDirectLight(LightMapAtlas& atlas, const ng::bsp::BSPPolygon<T>& poly,
        int x0, int y0, int x1, int y1)
    {
        uint64_t sr = 0, sg = 0, sb = 0;
        int n = 0;

        for (int y = y0; y < y1; ++y)
        {
            for (int x = x0; x < x1; ++x)
            {
                const uint32_t px = atlas.Buffer().GetPixel(poly.lmPosX + x, poly.lmPosY + y);
                sr += RGBToRed(px); sg += RGBToGreen(px); sb += RGBToBlue(px);
                ++n;
            }
        }

        if (n == 0) return Vec3<T>(0, 0, 0);
        return Vec3<T>(T(double(sr) / n), T(double(sg) / n), T(double(sb) / n));
    }

   
    // ------------------------------------------------------------
    // BuildRadiosityPatches
    //
    // Ein Patch pro Block (blockSize x blockSize Lightmap-Texel) pro
    // opakem Polygon. Fuellt zusaetzlich "lookup" fuer die spaetere
    // Treffer-Zuordnung.
    // ------------------------------------------------------------
    template<class T>
    std::vector<RadiosityPatch<T>> BuildRadiosityPatches(
        LightMapAtlas& atlas,
        const std::vector<ng::bsp::BSPPolygon<T>>& occluderPolys,
        const RadiosityOptions<T>& opt,
        RadiosityLookup& lookup)
    {
        std::vector<RadiosityPatch<T>> patches;
        patches.reserve(occluderPolys.size() * 4);

        lookup.patchOffset.assign(occluderPolys.size(), -1);
        lookup.blocksW.assign(occluderPolys.size(), 0);
        lookup.blocksH.assign(occluderPolys.size(), 0);

        const int B = std::max(1, opt.blockSize);

        for (size_t pi = 0; pi < occluderPolys.size(); ++pi)
        {
            const auto& poly = occluderPolys[pi];
            if (poly.verts.size() < 3 || poly.lmWidth <= 0 || poly.lmHeight <= 0)
                continue;

            // ---- DEBUG: Polygon-Info ----
            if (pi < 8)
            {
                const auto& v0 = poly.verts[0];
                printf("[RadDbg] poly=%zu tex=%s %dx%d texUV=(%.3f,%.3f) lm=%dx%d\n",
                    pi,
                    poly.texture ? "ja" : "NEIN",
                    poly.texture ? poly.texture->Width() : 0,
                    poly.texture ? poly.texture->Height() : 0,
                    (double)v0.texture().x(), (double)v0.texture().y(),
                    poly.lmWidth, poly.lmHeight);
            }

            const int bw = (poly.lmWidth + B - 1) / B;
            const int bh = (poly.lmHeight + B - 1) / B;

            lookup.patchOffset[pi] = (int)patches.size();
            lookup.blocksW[pi] = bw;
            lookup.blocksH[pi] = bh;

            for (int by = 0; by < bh; ++by)
            {
                for (int bx = 0; bx < bw; ++bx)
                {
                    const int x0 = bx * B;
                    const int y0 = by * B;
                    const int x1 = std::min(x0 + B, poly.lmWidth);
                    const int y1 = std::min(y0 + B, poly.lmHeight);

                    RadiosityPatch<T> patch;
                    patch.poly = &poly;
                    patch.x0 = x0; patch.y0 = y0; patch.x1 = x1; patch.y1 = y1;
                    patch.normal = poly.plane.normal();

                    const T fu = (T(x0 + x1) * T(0.5)) / T(poly.lmWidth);
                    const T fv = (T(y0 + y1) * T(0.5)) / T(poly.lmHeight);
                    patch.center = poly.lmOrigin + poly.lmEdgeU * fu + poly.lmEdgeV * fv
                        + patch.normal * T(0.5);

                    patch.albedo = ComputeBlockAlbedo(poly, x0, y0, x1, y1,
                        (T)opt.albedoMin, (T)opt.albedoMax);

                    Vec3<T> directLight = ComputeBlockDirectLight(atlas, poly, x0, y0, x1, y1);
                    for (int c = 0; c < 3; ++c)
                        directLight(c) = std::max(T(0), directLight(c) - T(opt.ambientFloor));


                    Vec3<T> reflected(
                        directLight(0) * patch.albedo(0),
                        directLight(1) * patch.albedo(1),
                        directLight(2) * patch.albedo(2));

                    /*
                    Vec3<T> emission(0, 0, 0);
                    if (opt.textureEmissionStrength > T(0))
                    {
                        emission = Vec3<T>(
                            patch.albedo(0) * T(255) * opt.textureEmissionStrength,
                            patch.albedo(1) * T(255) * opt.textureEmissionStrength,
                            patch.albedo(2) * T(255) * opt.textureEmissionStrength);
                    }
                    */

                    Vec3<T> emission(0, 0, 0);
                    if (opt.textureEmissionStrength > T(0))
                    {
                        const Vec3<T> raw = ComputeBlockTextureColor(poly, x0, y0, x1, y1);
                        emission = raw * (T(255) * opt.textureEmissionStrength);
                    }




                    patch.residual = reflected + emission;
                    patch.accumulated = Vec3<T>(0, 0, 0);

                    // ---- DEBUG: erster Block jedes der ersten Polygone ----
                    if (pi < 8 && bx == 0 && by == 0)
                    {
                        printf("[RadDbg]   albedo=(%.2f,%.2f,%.2f) direct=(%.0f,%.0f,%.0f)\n",
                            (double)patch.albedo(0), (double)patch.albedo(1), (double)patch.albedo(2),
                            (double)directLight(0), (double)directLight(1), (double)directLight(2));
                    }

                    patches.push_back(patch);
                }
            }
        }

        return patches;
    }

    // ------------------------------------------------------------
    // BuildRadiosityOcclusionTree
    //
    // Baut einen eigenstaendigen BSPTree aus Kopien von "occluderPolys".
    // objectId verweist auf den POLYGON-Index in occluderPolys (nicht
    // auf einen Patch!) - SplitPolygon() kopiert objectId mit, bleibt
    // also auch nach BSP-Splits korrekt.
    // ------------------------------------------------------------
    template<class T>
    std::unique_ptr<ng::bsp::BSPTree<T>> BuildRadiosityOcclusionTree(
        const std::vector<ng::bsp::BSPPolygon<T>>& occluderPolys)
    {
        std::vector<ng::bsp::BSPPolygon<T>> work;
        work.reserve(occluderPolys.size());

        for (size_t i = 0; i < occluderPolys.size(); ++i)
        {
            ng::bsp::BSPPolygon<T> copy = occluderPolys[i];
            copy.objectId = (uint32_t)i;
            work.push_back(std::move(copy));
        }

        auto tree = std::make_unique<ng::bsp::BSPTree<T>>();
        tree->SetVerbose(false);
        tree->Build(work);
        return tree;
    }

    // ------------------------------------------------------------
    // CosineWeightedHemisphereSample  (unveraendert)
    // ------------------------------------------------------------
    /*
    template<class T>
    Vec3<T> CosineWeightedHemisphereSample(const Vec3<T>& normal, double u1, double u2)
    {
        const double r = std::sqrt(u1);
        const double theta = 2.0 * 3.14159265358979323846 * u2;
        const double lx = r * std::cos(theta);
        const double ly = r * std::sin(theta);
        const double lz = std::sqrt(std::max(0.0, 1.0 - u1));

        Vec3<T> axis(0, 0, 0);
        if (std::abs(normal(0)) <= std::abs(normal(1)) && std::abs(normal(0)) <= std::abs(normal(2))) axis(0) = T(1);
        else if (std::abs(normal(1)) <= std::abs(normal(2))) axis(1) = T(1);
        else axis(2) = T(1);

        Vec3<T> tangent = axis % normal; tangent.normalize();
        Vec3<T> bitangent = normal % tangent; bitangent.normalize();
        return tangent * T(lx) + bitangent * T(ly) + normal * T(lz);
    }
    */
    template<class T>
    Vec3<T> CosineWeightedHemisphereSample(const Vec3<T>& normal, double u1, double u2)
    {
        const double r = std::sqrt(u1);
        const double theta = 2.0 * 3.14159265358979323846 * u2;

        const double lx = r * std::cos(theta);
        const double ly = r * std::sin(theta);
        const double lz = std::sqrt(std::max(0.0, 1.0 - u1));

        Vec3<T> axis(0, 0, 0);
        if (std::abs(normal(0)) <= std::abs(normal(1)) && std::abs(normal(0)) <= std::abs(normal(2))) axis(0) = T(1);
        else if (std::abs(normal(1)) <= std::abs(normal(2))) axis(1) = T(1);
        else axis(2) = T(1);

        Vec3<T> tangent = axis % normal; tangent.normalize();
        Vec3<T> bitangent = normal % tangent; bitangent.normalize();

        return tangent * T(lx) + bitangent * T(ly) + normal * T(lz);
    }
    /*
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

        Vec3<T> axis(0, 0, 0);
        if (std::abs(normal(0)) <= std::abs(normal(1)) && std::abs(normal(0)) <= std::abs(normal(2))) axis(0) = T(1);
        else if (std::abs(normal(1)) <= std::abs(normal(2))) axis(1) = T(1);
        else axis(2) = T(1);

        Vec3<T> tangent = axis % normal; tangent.normalize();
        Vec3<T> bitangent = normal % tangent; bitangent.normalize();

        return tangent * T(lx) + bitangent * T(ly) + normal * T(lz);
    }
    */
    // ------------------------------------------------------------
    // FindPatchForHit
    //
    // Bestimmt aus einem Ray-Hit (Polygon-Index via objectId + Welt-
    // Trefferpunkt) den zugehoerigen Block-Patch-Index.
    // ------------------------------------------------------------
    template<class T>
    int FindPatchForHit(const bsp::BSPRayHit<T>& hit, const Vec3<T>& hitPoint,
        const std::vector<ng::bsp::BSPPolygon<T>>& occluderPolys,
        const RadiosityLookup& lookup, int blockSize)
    {
        if (!hit.hit || !hit.poly) return -1;

        const size_t polyIdx = hit.poly->objectId;
        if (polyIdx >= occluderPolys.size()) return -1;
        if (lookup.patchOffset[polyIdx] < 0) return -1;

        const auto& origPoly = occluderPolys[polyIdx];
        if (origPoly.lmWidth <= 0 || origPoly.lmHeight <= 0) return -1;

        const Vec3<T> rel = hitPoint - origPoly.lmOrigin;
        const T eu2 = origPoly.lmEdgeU ^ origPoly.lmEdgeU;
        const T ev2 = origPoly.lmEdgeV ^ origPoly.lmEdgeV;
        if (eu2 < T(1e-12) || ev2 < T(1e-12)) return -1;

        const T fu = std::clamp((rel ^ origPoly.lmEdgeU) / eu2, T(0), T(0.999999));
        const T fv = std::clamp((rel ^ origPoly.lmEdgeV) / ev2, T(0), T(0.999999));

        const int bw = lookup.blocksW[polyIdx];
        const int bh = lookup.blocksH[polyIdx];

        const int bx = std::clamp((int)(fu * origPoly.lmWidth) / blockSize, 0, bw - 1);
        const int by = std::clamp((int)(fv * origPoly.lmHeight) / blockSize, 0, bh - 1);

        return lookup.patchOffset[polyIdx] + by * bw + bx;
    }

   


    // ------------------------------------------------------------
// RunRadiosityBounces
//
// Jacobi-Iteration ueber alle Patches. Pro Empfaenger werden
// stratifizierte, cosinus-gewichtete Strahlen geschossen (n x n Raster
// mit Jitter), jeder Treffer wird auf "maxSampleValue" begrenzt und das
// Ergebnis anschliessend innerhalb des Polygons geglaettet.
// ------------------------------------------------------------
    template<class T>
    void RunRadiosityBounces(std::vector<RadiosityPatch<T>>& patches,
        const ng::bsp::BSPTree<T>& occlusionTree,
        const std::vector<ng::bsp::BSPPolygon<T>>& occluderPolys,
        const RadiosityLookup& lookup,
        const RadiosityOptions<T>& opt)
    {
        std::mt19937 rng(opt.randomSeed);
        std::uniform_real_distribution<double> jit(0.0, 1.0);
        std::vector<Vec3<T>> incoming(patches.size());

        // Stratifiziertes Raster: n x n Zellen
        const int n = std::max(1, (int)std::ceil(std::sqrt((double)std::max(1, opt.samplesPerPatch))));
        const int totalSamples = n * n;
        const T invTotal = T(1) / T(totalSamples);
        const T maxSample = T(opt.maxSampleValue);

        for (int bounce = 0; bounce < opt.bounces; ++bounce)
        {
            long long totalRays = 0, totalHits = 0;

            for (size_t i = 0; i < patches.size(); ++i)
            {
                const auto& receiver = patches[i];
                Vec3<T> sum(0, 0, 0);

                for (int sy = 0; sy < n; ++sy)
                {
                    for (int sx = 0; sx < n; ++sx)
                    {
                        ++totalRays;

                        const double u1 = (sx + jit(rng)) / n;
                        const double u2 = (sy + jit(rng)) / n;
                        const Vec3<T> dir = CosineWeightedHemisphereSample(receiver.normal, u1, u2);

                        auto hit = BSPRayNearestHit(occlusionTree, receiver.center, dir,
                            (T)opt.maxRayDistance, (T)opt.epsilon);

                        if (!hit.hit) continue;
                        ++totalHits;

                        const Vec3<T> hitPoint = receiver.center + dir * hit.t;
                        const int patchIdx = FindPatchForHit(hit, hitPoint, occluderPolys, lookup, opt.blockSize);
                        if (patchIdx < 0 || (size_t)patchIdx >= patches.size()) continue;

                        // Firefly-Clamp: max. Kanal begrenzen, Farbton erhalten
                        Vec3<T> r = patches[patchIdx].residual;
                        const T m = std::max({ r(0), r(1), r(2) });
                        if (maxSample > T(0) && m > maxSample)
                            r = r * (maxSample / m);

                        sum = sum + r;
                    }
                }

                // Cosinus-gewichtetes Sampling: Mittelwert der Treffer-Radiosity
                incoming[i] = sum * invTotal;
            }

            // Rauschen im Patch-Raum glaetten (nur innerhalb eines Polygons)
            for (int b = 0; b < opt.blurPasses; ++b)
                BlurIncoming(incoming, lookup);

            for (size_t i = 0; i < patches.size(); ++i)
            {
                patches[i].accumulated = patches[i].accumulated + incoming[i];
                patches[i].residual = Vec3<T>(
                    incoming[i](0) * patches[i].albedo(0),
                    incoming[i](1) * patches[i].albedo(1),
                    incoming[i](2) * patches[i].albedo(2));
            }

            printf("[Radiosity] Bounce %d / %d: Hit-Rate %lld/%lld (%.1f%%), Patches=%zu\n",
                bounce + 1, opt.bounces, totalHits, totalRays,
                totalRays > 0 ? 100.0 * totalHits / totalRays : 0.0, patches.size());
        }
    }









  
   // ------------------------------------------------------------
// BakeRadiosity
// ------------------------------------------------------------
    template<class T>
    void BakeRadiosity(LightMapAtlas& atlas,
        const std::vector<ng::bsp::BSPPolygon<T>>& occluderPolys,
        const RadiosityOptions<T>& opt = RadiosityOptions<T>())
    {
        printf("[Radiosity] BUILD-MARKER v2\n");   // <-- LOG 1
        printf("[Radiosity] indirectGain=%.1f ambientFloor=%.1f\n", opt.indirectGain, opt.ambientFloor);
        
        RadiosityLookup lookup;
        auto patches = BuildRadiosityPatches(atlas, occluderPolys, opt, lookup);
        NormalizePatchAlbedo(patches, 0.5, opt.albedoMax);
        // Initialisierung
        DumpPatchesBMP(atlas, patches, "rad_init_albedo.bmp", PatchDumpMode::Albedo);
        DumpPatchesBMP(atlas, patches, "rad_init_residual.bmp", PatchDumpMode::Residual, 4.0f);

        printf("[Radiosity] %zu Patches (Blockgroesse %d), %d Bounces, %d Samples/Patch\n",
            patches.size(), opt.blockSize, opt.bounces, opt.samplesPerPatch);

        if (patches.empty())
        {
            printf("[Radiosity] keine Patches - uebersprungen.\n");
            return;
        }

        // --- LOG 2: Sender-Statistik (vor den Bounces) ---
        {
            size_t lit = 0;
            double maxRes = 0;
            for (const auto& p : patches)
            {
                const double m = std::max({ (double)p.residual(0), (double)p.residual(1), (double)p.residual(2) });
                if (m > 0.5) ++lit;
                maxRes = std::max(maxRes, m);
            }
            printf("[Radiosity] Sender: %zu von %zu Patches mit residual > 0.5, max residual=%.1f\n",
                lit, patches.size(), maxRes);
        }

        auto occlusionTree = BuildRadiosityOcclusionTree(occluderPolys);

        RunRadiosityBounces(patches, *occlusionTree, occluderPolys, lookup, opt);

        DumpPatchesBMP(atlas, patches, "rad_accumulated.bmp", PatchDumpMode::Accumulated,
            4.0f, (float)opt.indirectGain);

        // --- LOG 3: Ergebnis-Statistik (nach den Bounces) ---
        {
            double maxAcc = 0, sumAcc = 0;
            for (const auto& p : patches)
            {
                const double m = std::max({ (double)p.accumulated(0), (double)p.accumulated(1), (double)p.accumulated(2) });
                maxAcc = std::max(maxAcc, m);
                sumAcc += m;
            }
            printf("[Radiosity] accumulated: max=%.1f avg=%.2f\n", maxAcc, sumAcc / patches.size());
        }

        //ApplyRadiosityToAtlas(atlas, patches, opt);   // <-- mit opt!
        ApplyRadiosityToAtlas(atlas, patches, lookup, opt);
        printf("[Radiosity] fertig.\n");
    }
}