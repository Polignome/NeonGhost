#pragma once
#include "Rasterizer3D.h"
#include "BSP.h"
#include "PVS.h"
#include "Viscells.h"
#include "RenderPipeline.h"

namespace ng::lightning {

    template <class T>
    struct DynLight
    {
        ng::math::Matrix<3, 1, T> pos;
        ng::math::Matrix<3, 1, T> color{ 1, 1, 1 };
        T intensity = T(1);
        T radius = T(300);
        bool castShadow = false;
    };
    /*
    template <class T>
    void ApplyDynamicLightsDebug(ng::renderer::rasterizer::RenderPipeline<T>& pipeline,
        const std::vector<DynLight<T>>& lights,
        const ng::bsp::VisCells<T>* cells = nullptr,
        int mode = 0)            // 0 = Licht, 1 = Weltraster (Rekonstruktionstest)
    {
        using Vec3 = ng::math::Matrix<3, 1, T>;

        // am Anfang der Funktion
        const bool bgr = (RGBA(255, 0, 0, 255) & 0xFFu) == 0xFFu;   // Rot in den unteren Bits?
        const int sR = bgr ? 0 : 16, sB = bgr ? 16 : 0;
        
        
        static int frameNo = 0;
        const bool report = (frameNo++ % 30) == 0;

        int nDepth = 0, nNormal = 0, nRange = 0, nFacing = 0, nShadow = 0, nWritten = 0;

        // ---- Buffer (Referenzen, keine Kopien) ----
        auto& rast = pipeline.GetRasterizer();
        auto& tg = rast.GetTarget();
        uint32_t* color = tg.color;
        const T* depth = tg.depth;
        const uint32_t* normal = tg.normal;
        const int W = tg.width, H = tg.height;
        if (!color || !depth || W <= 0 || H <= 0) { printf("[LIGHT] Buffer ungueltig\n"); return; }

        // ---- Kamerabasis aus der echten Render-Matrix ----
        const auto vb = pipeline.GetViewBasis();
        const Vec3 pos = vb.pos, R = vb.right, U = vb.up, F = vb.forward;
        const T p00 = vb.p00, p11 = vb.p11;
        if (std::abs(p00) < T(1e-9) || std::abs(p11) < T(1e-9)) { printf("[LIGHT] p00/p11 = 0\n"); return; }

        const T cx = (T)rast.ScreenCenterX();
        const T cy = (T)rast.ScreenCenterY();
        const T kx = T(1) / (cx * p00);      // xv = (sx - cx) * z * kx
        const T ky = T(1) / (cy * p11);      // yv = -(sy - cy) * z * ky

        // Rekonstruktion eines Pixels (Mitte = +0.5)
        auto reconstruct = [&](int x, int y, T z) -> Vec3
            {
                const T xv = ((T(x) + T(0.5)) - cx) * kx;
                const T yv = -((T(y) + T(0.5)) - cy) * ky;
                return pos + R * (xv * z) + U * (yv * z) + F * z;
            };

        // ---- Diagnose: Basis + Round-Trip ----
        if (report)
        {
            printf("[LIGHT] p00=%.4f p11=%.4f  W=%d H=%d cx=%.1f cy=%.1f\n",
                (double)p00, (double)p11, W, H, (double)cx, (double)cy);
            printf("[LIGHT] pos=(%.1f %.1f %.1f) F=(%.3f %.3f %.3f) R=(%.3f %.3f %.3f) U=(%.3f %.3f %.3f)\n",
                (double)pos(0), (double)pos(1), (double)pos(2),
                (double)F(0), (double)F(1), (double)F(2),
                (double)R(0), (double)R(1), (double)R(2),
                (double)U(0), (double)U(1), (double)U(2));
            printf("[LIGHT] dot(F,R)=%.4f dot(F,U)=%.4f dot(R,U)=%.4f (muessen ~0 sein)\n",
                (double)(F ^ R), (double)(F ^ U), (double)(R ^ U));

            const int px[5] = { W / 2, W / 4, W * 3 / 4, W / 4, W * 3 / 4 };
            const int py[5] = { H / 2, H / 4, H / 4, H * 3 / 4, H * 3 / 4 };

            for (int k = 0; k < 5; ++k)
            {
                const T d = depth[(size_t)py[k] * W + px[k]];
                if (!(d > T(1e-9))) { printf("[LIGHT] RT %d: pixel(%d,%d) depth=%g (leer)\n", k, px[k], py[k], (double)d); continue; }

                const T z = T(1) / d;
                Vec3 P = reconstruct(px[k], py[k], z);

                const auto c = pipeline.ComboMatrix() >> P;      // benannte Vec3-Variable wie in HZBTest
                const T sx = cx + c.x() / c.w() * cx;
                const T sy = cy - c.y() / c.w() * cy;

                printf("[LIGHT] RT %d: pixel(%d,%d) depth=%g z=%.2f P=(%.1f %.1f %.1f) -> zurueck(%.1f,%.1f) fehler=(%.2f,%.2f)\n",
                    k, px[k], py[k], (double)d, (double)z,
                    (double)P(0), (double)P(1), (double)P(2),
                    (double)sx, (double)sy,
                    (double)(sx - T(px[k]) - T(0.5)), (double)(sy - T(py[k]) - T(0.5)));
            }
        }

        // ---- Modus 1: Weltraster statt Licht ----
        if (mode == 1)
        {
            const T cell = T(64), line = T(1.0);   // Linienbreite in Units
            for (int y = 0; y < H; ++y)
                for (int x = 0; x < W; ++x)
                {
                    const T d = depth[(size_t)y * W + x];
                    if (!(d > T(1e-9))) continue;
                    const Vec3 P = reconstruct(x, y, T(1) / d);

                    uint32_t col = 0;
                    for (int a = 0; a < 3; ++a)
                    {
                        T f = std::fmod(P(a), cell); if (f < 0) f += cell;
                        if (f < line || f > cell - line)
                            col |= (a == 0 ? 0x00FF0000u : a == 1 ? 0x0000FF00u : 0x000000FFu);
                    }
                    if (col) color[(size_t)y * W + x] = 0xFF000000u | col;   // x=rot, y=gruen, z=blau
                }
            return;
        }

        // Modus 2: Normalen als Farbe. Waende muessen je Richtung EINE feste Farbe haben
        if (mode == 2)
        {
            for (size_t i = 0; i < (size_t)W * H; ++i)
            {
                Vec3 N;
                if (!ng::renderer::rasterizer::UnpackNormal<T>(normal[i], N)) { color[i] = 0xFFFF00FFu; continue; } // magenta = nie beschrieben
                const int r = int((N(0) * 0.5 + 0.5) * 255);
                const int g = int((N(1) * 0.5 + 0.5) * 255);
                const int b = int((N(2) * 0.5 + 0.5) * 255);
                color[i] = 0xFF000000u | (r << 16) | (g << 8) | b;
            }
            return;
        }

        // Modus 3: Kugelschale um jedes Licht (Radius 20 +- 1 Unit)
        if (mode == 3)
        {
            for (const auto& L : lights)
                for (int y = 0; y < H; ++y)
                    for (int x = 0; x < W; ++x)
                    {
                        const T d = depth[(size_t)y * W + x];
                        if (!(d > T(1e-9))) continue;
                        const Vec3 P = reconstruct(x, y, T(1) / d);
                        const Vec3 t = L.pos - P;
                        const T dist = std::sqrt(t ^ t);
                        if (std::abs(dist - T(20)) < T(1)) color[(size_t)y * W + x] = 0xFFFFFFFFu;
                    }
            return;
        }














        // ---- Zellen fuer Culling ----
        const auto& visCells = pipeline.GetWorldVisCells();
        const auto& cellBounds = pipeline.GetWorldCellBounds();

        for (const auto& L : lights)
        {
            const T r2 = L.radius * L.radius;

            // PVS-/Zell-Culling
            if (cells && !visCells.empty())
            {
                bool relevant = false;
                for (int32_t c : visCells)
                {
                    if (c < 0 || (size_t)c >= cellBounds.size()) continue;
                    const auto mn = cellBounds[c].Min(), mx = cellBounds[c].Max();
                    T d2 = 0;
                    for (int k = 0; k < 3; ++k)
                    {
                        const T v = L.pos(k);
                        if (v < mn(k)) { const T e = mn(k) - v; d2 += e * e; }
                        else if (v > mx(k)) { const T e = v - mx(k); d2 += e * e; }
                    }
                    if (d2 <= r2) { relevant = true; break; }
                }
                if (!relevant) continue;
            }

            // Screen-Rechteck
            int x0 = 0, y0 = 0, x1 = W - 1, y1 = H - 1;
            const Vec3 rel = L.pos - pos;
            const T zc = rel ^ F;
            if ((rel ^ rel) > r2)
            {
                if (zc + L.radius < T(0.01)) continue;
                if (zc > L.radius)
                {
                    const T pxc = cx + ((rel ^ R) / zc) * (cx * p00);
                    const T pyc = cy - ((rel ^ U) / zc) * (cy * p11);
                    const T rpx = L.radius / (zc - L.radius) * std::abs(cx * p00);
                    const T rpy = L.radius / (zc - L.radius) * std::abs(cy * p11);
                    x0 = std::max(0, (int)std::floor(pxc - rpx));
                    x1 = std::min(W - 1, (int)std::ceil(pxc + rpx));
                    y0 = std::max(0, (int)std::floor(pyc - rpy));
                    y1 = std::min(H - 1, (int)std::ceil(pyc + rpy));
                    if (x0 > x1 || y0 > y1) continue;
                }
            }

            const T invR2 = T(1) / r2;

            for (int y = y0; y <= y1; ++y)
            {
                for (int x = x0; x <= x1; ++x)
                {
                    const size_t i = (size_t)y * W + x;
                    const T d = depth[i];
                    if (!(d > T(1e-9))) { ++nDepth; continue; }

                    const Vec3 P = reconstruct(x, y, T(1) / d);

                    Vec3 toL = L.pos - P;
                    const T d2 = toL ^ toL;
                    if (d2 >= r2) { ++nRange; continue; }

                    Vec3 N;
                    if (!ng::renderer::rasterizer::UnpackNormal<T>(normal[i], N)) { ++nNormal; continue; }
                    const T nl = N.length();
                    if (nl < T(1e-6)) { ++nNormal; continue; }
                    N = N * (T(1) / nl);

                    const T dist = std::sqrt(d2);
                    toL = toL * (T(1) / std::max(dist, T(1e-4)));
                    const T ndl = N ^ toL;
                    //if (ndl <= T(0)) { ++nFacing; continue; }

                    const T ndlAbs = std::abs(ndl);
                    if (ndlAbs < T(1e-3)) { ++nFacing; continue; }


                    T att = T(1) - d2 * invR2;
                    att *= att;
                    const T k = ndl * att * L.intensity;
                    if (k < T(1.0 / 512)) continue;

                    if (L.castShadow && cells)
                    {
                        ++nShadow;
                        const Vec3 o = P + N * T(0.5);
                        Vec3 dir = L.pos - o;
                        const T dl = dir.length();
                        dir = dir * (T(1) / dl);
                        if (ng::bsp::BSPRayOccluded(cells->Tree(), o, dir, dl - T(0.5), T(0.01)))
                            continue;
                    }

                    const uint32_t c = color[i];
                    const int r = (c >> sR) & 255, g = (c >> 8) & 255, b = (c >> sB) & 255;
                    


                    const int nr = std::min(255, r + int(r * k * L.color(0) + k * L.color(0) * 24));
                    const int ng_ = std::min(255, g + int(g * k * L.color(1) + k * L.color(1) * 24));
                    const int nb = std::min(255, b + int(b * k * L.color(2) + k * L.color(2) * 24));
                    color[i] = (c & 0xFF000000u) | (nr << sR) | (ng_ << 8) | (nb << sB);
                    ++nWritten;
                }
            }
        }

        if (report)
            printf("[LIGHT] lights=%zu depth=%d normal=%d range=%d facing=%d shadow=%d written=%d\n",
                lights.size(), nDepth, nNormal, nRange, nFacing, nShadow, nWritten);
    }
    */


template <class T>
void ApplyDynamicLightsDebug(ng::renderer::rasterizer::RenderPipeline<T>& pipeline,
    const std::vector<DynLight<T>>& lights,
    const ng::bsp::VisCells<T>* cells = nullptr,
    int mode = 0,            // 0 Licht | 1 Weltraster | 2 Normalen | 3 Kugelschale um Lichter
    bool twoSided = true)   // true: Normale zum Licht drehen (Test auf gespiegelte Normalen)
{
    using Vec3 = ng::math::Matrix<3, 1, T>;

    static int frameNo = 0;
    const bool report = (frameNo++ % 30) == 0;

    int nDepth = 0, nNormal = 0, nRange = 0, nFacing = 0, nFlipped = 0, nShadow = 0, nWritten = 0;

    // ---- Buffer (Referenzen, keine Kopien) ----
    auto& rast = pipeline.GetRasterizer();
    auto& tg = rast.GetTarget();
    uint32_t* color = tg.color;
    const T* depth = tg.depth;
    const uint32_t* normal = tg.normal;
    const int W = tg.width, H = tg.height;
    if (!color || !depth || !normal || W <= 0 || H <= 0) { printf("[LIGHT] Buffer ungueltig\n"); return; }

    // ---- Kanalreihenfolge erkennen (RGBA-Makro: Rot unten oder bei Bit 16?) ----
    const bool bgr = ((RGBA(255, 0, 0, 255)) & 0xFFu) == 0xFFu;
    const int sR = bgr ? 0 : 16;
    const int sB = bgr ? 16 : 0;

    // ---- Kamerabasis aus der echten Render-Matrix ----
    const auto vb = pipeline.GetViewBasis();
    const Vec3 pos = vb.pos, R = vb.right, U = vb.up, F = vb.forward;
    const T p00 = vb.p00, p11 = vb.p11;
    if (std::abs(p00) < T(1e-9) || std::abs(p11) < T(1e-9)) { printf("[LIGHT] p00/p11 = 0\n"); return; }

    const T cx = (T)rast.ScreenCenterX();
    const T cy = (T)rast.ScreenCenterY();
    const T kx = T(1) / (cx * p00);
    const T ky = T(1) / (cy * p11);

    auto reconstruct = [&](int x, int y, T z) -> Vec3
        {
            const T xv = ((T(x) + T(0.5)) - cx) * kx;
            const T yv = -((T(y) + T(0.5)) - cy) * ky;
            return pos + R * (xv * z) + U * (yv * z) + F * z;
        };

    // ---- Diagnose: Basis + Round-Trip + Normalen-Stichprobe ----
    if (report)
    {
        printf("[LIGHT] p00=%.4f p11=%.4f W=%d H=%d bgr=%d mode=%d twoSided=%d\n",
            (double)p00, (double)p11, W, H, (int)bgr, mode, (int)twoSided);
        printf("[LIGHT] pos=(%.1f %.1f %.1f) F=(%.3f %.3f %.3f) R=(%.3f %.3f %.3f) U=(%.3f %.3f %.3f)\n",
            (double)pos(0), (double)pos(1), (double)pos(2),
            (double)F(0), (double)F(1), (double)F(2),
            (double)R(0), (double)R(1), (double)R(2),
            (double)U(0), (double)U(1), (double)U(2));

        const int px[5] = { W / 2, W / 4, W * 3 / 4, W / 4, W * 3 / 4 };
        const int py[5] = { H / 2, H / 4, H / 4, H * 3 / 4, H * 3 / 4 };

        for (int k = 0; k < 5; ++k)
        {
            const size_t i = (size_t)py[k] * W + px[k];
            const T d = depth[i];
            if (!(d > T(1e-9))) { printf("[LIGHT] RT %d: depth=%g (leer)\n", k, (double)d); continue; }

            const Vec3 P = reconstruct(px[k], py[k], T(1) / d);
            const auto c = pipeline.ComboMatrix() >> ng::math::Matrix<4,1,T> (P,0);
            const T sx = cx + c.x() / c.w() * cx;
            const T sy = cy - c.y() / c.w() * cy;

            Vec3 N(0, 0, 0);
            const bool hasN = ng::renderer::rasterizer::UnpackNormal<T>(normal[i], N);
            if (hasN) N = N * (T(1) / std::max(N.length(), T(1e-6)));

            printf("[LIGHT] RT %d: P=(%.1f %.1f %.1f) fehler=(%.2f,%.2f) N=%s(%.2f %.2f %.2f)\n",
                k, (double)P(0), (double)P(1), (double)P(2),
                (double)(sx - T(px[k]) - T(0.5)), (double)(sy - T(py[k]) - T(0.5)),
                hasN ? "" : "FEHLT ", (double)N(0), (double)N(1), (double)N(2));
        }
    }

    // ---- Modus 1: Weltraster ----
    if (mode == 1)
    {
        const T cell = T(64), line = T(1.0);
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
            {
                const T d = depth[(size_t)y * W + x];
                if (!(d > T(1e-9))) continue;
                const Vec3 P = reconstruct(x, y, T(1) / d);

                uint32_t col = 0;
                for (int a = 0; a < 3; ++a)
                {
                    T f = std::fmod(P(a), cell); if (f < 0) f += cell;
                    if (f < line || f > cell - line)
                        col |= (a == 0 ? (0xFFu << sR) : a == 1 ? (0xFFu << 8) : (0xFFu << sB));
                }
                if (col) color[(size_t)y * W + x] = 0xFF000000u | col;
            }
        return;
    }

    // ---- Modus 2: Normalen als Farbe (magenta = nie beschrieben) ----
    if (mode == 2)
    {
        for (size_t i = 0; i < (size_t)W * H; ++i)
        {
            Vec3 N;
            if (!ng::renderer::rasterizer::UnpackNormal<T>(normal[i], N)) { color[i] = 0xFFFF00FFu; continue; }
            const uint32_t r = (uint32_t)std::clamp(int((N(0) * 0.5 + 0.5) * 255), 0, 255);
            const uint32_t g = (uint32_t)std::clamp(int((N(1) * 0.5 + 0.5) * 255), 0, 255);
            const uint32_t b = (uint32_t)std::clamp(int((N(2) * 0.5 + 0.5) * 255), 0, 255);
            color[i] = 0xFF000000u | (r << sR) | (g << 8) | (b << sB);
        }
        return;
    }

    // ---- Modus 3: Kugelschale (Radius 20 +- 1) um jedes Licht ----
    if (mode == 3)
    {
        for (const auto& L : lights)
            for (int y = 0; y < H; ++y)
                for (int x = 0; x < W; ++x)
                {
                    const T d = depth[(size_t)y * W + x];
                    if (!(d > T(1e-9))) continue;
                    const Vec3 t = L.pos - reconstruct(x, y, T(1) / d);
                    if (std::abs(std::sqrt(t ^ t) - T(20)) < T(1)) color[(size_t)y * W + x] = 0xFFFFFFFFu;
                }
        return;
    }

    // ---- Modus 0: Licht ----
    const auto& visCells = pipeline.GetWorldVisCells();
    const auto& cellBounds = pipeline.GetWorldCellBounds();

    for (const auto& L : lights)
    {
        const T r2 = L.radius * L.radius;

        if (cells && !visCells.empty())
        {
            bool relevant = false;
            for (int32_t c : visCells)
            {
                if (c < 0 || (size_t)c >= cellBounds.size()) continue;
                const auto mn = cellBounds[c].Min(), mx = cellBounds[c].Max();
                T d2 = 0;
                for (int k = 0; k < 3; ++k)
                {
                    const T v = L.pos(k);
                    if (v < mn(k)) { const T e = mn(k) - v; d2 += e * e; }
                    else if (v > mx(k)) { const T e = v - mx(k); d2 += e * e; }
                }
                if (d2 <= r2) { relevant = true; break; }
            }
            if (!relevant) continue;
        }

        int x0 = 0, y0 = 0, x1 = W - 1, y1 = H - 1;
        const Vec3 rel = L.pos - pos;
        const T zc = rel ^ F;
        if ((rel ^ rel) > r2)
        {
            if (zc + L.radius < T(0.01)) continue;
            if (zc > L.radius)
            {
                const T pxc = cx + ((rel ^ R) / zc) * (cx * p00);
                const T pyc = cy - ((rel ^ U) / zc) * (cy * p11);
                const T rpx = L.radius / (zc - L.radius) * std::abs(cx * p00);
                const T rpy = L.radius / (zc - L.radius) * std::abs(cy * p11);
                x0 = std::max(0, (int)std::floor(pxc - rpx));
                x1 = std::min(W - 1, (int)std::ceil(pxc + rpx));
                y0 = std::max(0, (int)std::floor(pyc - rpy));
                y1 = std::min(H - 1, (int)std::ceil(pyc + rpy));
                if (x0 > x1 || y0 > y1) continue;
            }
        }

        const T invR2 = T(1) / r2;

        for (int y = y0; y <= y1; ++y)
        {
            for (int x = x0; x <= x1; ++x)
            {
                const size_t i = (size_t)y * W + x;
                const T d = depth[i];
                if (!(d > T(1e-9))) { ++nDepth; continue; }

                const Vec3 P = reconstruct(x, y, T(1) / d);

                Vec3 toL = L.pos - P;
                const T d2 = toL ^ toL;
                if (d2 >= r2) { ++nRange; continue; }

                Vec3 N;
                if (!ng::renderer::rasterizer::UnpackNormal<T>(normal[i], N)) { ++nNormal; continue; }
                const T nl = N.length();
                if (nl < T(1e-6)) { ++nNormal; continue; }
                N = N * (T(1) / nl);

                const T dist = std::sqrt(d2);
                toL = toL * (T(1) / std::max(dist, T(1e-4)));
                T ndl = N ^ toL;

                if (twoSided)
                {
                    if (ndl < T(0)) { ndl = -ndl; N = N * T(-1); ++nFlipped; }
                    if (ndl < T(1e-3)) { ++nFacing; continue; }
                }
                else if (ndl <= T(0)) { ++nFacing; continue; }

                T att = T(1) - d2 * invR2;
                att *= att;
                const T k = ndl * att * L.intensity;
                if (k < T(1.0 / 512)) continue;

                if (L.castShadow && cells)
                {
                    ++nShadow;
                    const Vec3 o = P + N * T(0.5);
                    Vec3 dir = L.pos - o;
                    const T dl = dir.length();
                    dir = dir * (T(1) / dl);
                    if (ng::bsp::BSPRayOccluded(cells->Tree(), o, dir, dl - T(0.5), T(0.01)))
                        continue;
                }

                const uint32_t c = color[i];
                const int r = (c >> sR) & 255, g = (c >> 8) & 255, b = (c >> sB) & 255;
                const int nr = std::min(255, r + int(r * k * L.color(0) + k * L.color(0) * 24));
                const int ng_ = std::min(255, g + int(g * k * L.color(1) + k * L.color(1) * 24));
                const int nb = std::min(255, b + int(b * k * L.color(2) + k * L.color(2) * 24));
                color[i] = (c & 0xFF000000u) | ((uint32_t)nr << sR) | ((uint32_t)ng_ << 8) | ((uint32_t)nb << sB);
                ++nWritten;
            }
        }
    }

    if (report)
        printf("[LIGHT] lights=%zu depth=%d normal=%d range=%d facing=%d flipped=%d shadow=%d written=%d\n",
            lights.size(), nDepth, nNormal, nRange, nFacing, nFlipped, nShadow, nWritten);
}

}