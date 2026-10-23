#pragma once
#include "Global.h"
#include <cstdint>
#include <vector>
#include <map>
#include <algorithm>
#include <cmath>
#include <random>


#include "postfx.h"
#include "Matrix.h"
///////////////////////////////////////////////////////////////////////////
//                                                                       //
//   PostFX - Effekte                                                   //
//   -----------------                                                  //
//   Ab hier: konkrete Implementierungen. "detail" enthaelt die reinen  //
//   Rechenroutinen (Sampling, Grid-Generierung, Block-Renderer), die    //
//   Effekt-Klassen darunter binden das ans Interface an.               //
//                                                                       //
///////////////////////////////////////////////////////////////////////////


namespace ng
{
    namespace renderer
    {
        namespace postfx
        {

            // ================================================================
            // PostFX Parameter
            // ================================================================

            template <class T>
            struct PostFXParams
            {
                // Verzerrung
                T ampX, freqX, speedX;
                T ampY, freqY, speedY;

                // Farbstich
                uint8_t tintR, tintG, tintB;
                uint8_t tintAmount;

                // Kaustik / Helligkeitsmodulation
                T causticStrength;
                T causticFreq;
                T causticSpeed;
            };


            // ================================================================
            // Presets
            // ================================================================
            template <class T>
            inline constexpr PostFXParams<T> FX_WATER =
            {
                (T)4.0f, (T)0.05f, (T)1.3f,
                (T)3.0f, (T)0.07f, (T)1.7f,

                0, 40, 80, 70,

                (T)0.15f, (T)0.08f, (T)2.0f
            };

            template <class T>
            inline constexpr PostFXParams<T> FX_LAVA =
            {
                (T)6.0f, (T)0.04f, (T)0.6f,
                (T)5.0f, (T)0.05f, (T)0.5f,

                200, 40, 0, 90,

                (T)0.25f, (T)0.06f, (T)1.0f
            };

            template <class T>
            inline constexpr PostFXParams<T> FX_TOXIC =
            {
                (T)3.0f, (T)0.06f, (T)1.0f,
                (T)3.0f, (T)0.06f, (T)1.0f,

                40, 220, 20, 60,

                (T)0.2f, (T)0.1f, (T)1.5f
            };


            // ================================================================
            // GridPointUVG
            // ================================================================

            struct GridPointUVG
            {
                int32_t u;
                int32_t v;
                int32_t g;
            };


            namespace detail
            {

                // ============================================================
                // Bilineares Sampling
                // ============================================================

                inline uint32_t SampleBilinearFP(
                    const uint32_t* src,
                    int width,
                    int height,
                    int32_t u,
                    int32_t v)
                {
                    if (!src || width <= 0 || height <= 0)
                        return 0xff000000u;

                    u = std::max<int32_t>(0, u);
                    v = std::max<int32_t>(0, v);

                    const int32_t umax =
                        static_cast<int32_t>(width - 1) << 16;

                    const int32_t vmax =
                        static_cast<int32_t>(height - 1) << 16;

                    u = std::min(u, umax);
                    v = std::min(v, vmax);

                    const int x0 = u >> 16;
                    const int y0 = v >> 16;

                    const int x1 =
                        (x0 + 1 < width) ? x0 + 1 : x0;

                    const int y1 =
                        (y0 + 1 < height) ? y0 + 1 : y0;

                    const int32_t fx = (u >> 8) & 0xff;
                    const int32_t fy = (v >> 8) & 0xff;

                    const uint32_t* row0 =
                        src + static_cast<size_t>(y0) * width;

                    const uint32_t* row1 =
                        src + static_cast<size_t>(y1) * width;

                    const uint32_t c00 = row0[x0];
                    const uint32_t c10 = row0[x1];
                    const uint32_t c01 = row1[x0];
                    const uint32_t c11 = row1[x1];

                    auto lerp8 =
                        [](int32_t a, int32_t b, int32_t t) -> int32_t
                        {
                            return a + (((b - a) * t) >> 8);
                        };

                    const int32_t r =
                        lerp8(
                            lerp8(
                                (c00 >> 16) & 0xff,
                                (c10 >> 16) & 0xff,
                                fx),
                            lerp8(
                                (c01 >> 16) & 0xff,
                                (c11 >> 16) & 0xff,
                                fx),
                            fy);

                    const int32_t g =
                        lerp8(
                            lerp8(
                                (c00 >> 8) & 0xff,
                                (c10 >> 8) & 0xff,
                                fx),
                            lerp8(
                                (c01 >> 8) & 0xff,
                                (c11 >> 8) & 0xff,
                                fx),
                            fy);

                    const int32_t b =
                        lerp8(
                            lerp8(
                                c00 & 0xff,
                                c10 & 0xff,
                                fx),
                            lerp8(
                                c01 & 0xff,
                                c11 & 0xff,
                                fx),
                            fy);

                    return
                        0xff000000u |
                        (static_cast<uint32_t>(r) << 16) |
                        (static_cast<uint32_t>(g) << 8) |
                        static_cast<uint32_t>(b);
                }


                // ============================================================
                // Tint + Helligkeit
                // ============================================================

                inline uint32_t ApplyTint(
                    uint32_t color,
                    uint8_t tintR,
                    uint8_t tintG,
                    uint8_t tintB,
                    uint8_t tintAmount,
                    int32_t shade)
                {
                    uint32_t r = (color >> 16) & 0xff;
                    uint32_t g = (color >> 8) & 0xff;
                    uint32_t b = color & 0xff;

                    r =
                        (r * (255 - tintAmount) +
                            static_cast<uint32_t>(tintR) * tintAmount) / 255;

                    g =
                        (g * (255 - tintAmount) +
                            static_cast<uint32_t>(tintG) * tintAmount) / 255;

                    b =
                        (b * (255 - tintAmount) +
                            static_cast<uint32_t>(tintB) * tintAmount) / 255;

                    uint32_t s =
                        static_cast<uint32_t>(
                            std::max<int32_t>(0, shade) >> 8);

                    s = std::min<uint32_t>(s, 256);

                    r = (r * s) >> 8;
                    g = (g * s) >> 8;
                    b = (b * s) >> 8;

                    return
                        0xff000000u |
                        (r << 16) |
                        (g << 8) |
                        b;
                }


                // ============================================================
                // 8x8 Block
                // ============================================================

                inline void RenderBlock8x8(
                    const GridPointUVG& a,
                    const GridPointUVG& b,
                    const GridPointUVG& c,
                    const GridPointUVG& d,

                    uint32_t* dest,
                    int destStride,

                    const uint32_t* src,
                    int srcWidth,
                    int srcHeight,

                    uint8_t tintR,
                    uint8_t tintG,
                    uint8_t tintB,
                    uint8_t tintAmount)
                {
                    const int32_t dudy_left =
                        (b.u - a.u) / 8;

                    const int32_t dvdy_left =
                        (b.v - a.v) / 8;

                    const int32_t dgdy_left =
                        (b.g - a.g) / 8;

                    const int32_t dudy_right =
                        (d.u - c.u) / 8;

                    const int32_t dvdy_right =
                        (d.v - c.v) / 8;

                    const int32_t dgdy_right =
                        (d.g - c.g) / 8;

                    GridPointUVG left = a;
                    GridPointUVG right = c;

                    for (int y = 0; y < 8; ++y)
                    {
                        const int32_t dudx =
                            (right.u - left.u) / 8;

                        const int32_t dvdx =
                            (right.v - left.v) / 8;

                        const int32_t dgdx =
                            (right.g - left.g) / 8;

                        int32_t u = left.u;
                        int32_t v = left.v;
                        int32_t g = left.g;

                        for (int x = 0; x < 8; ++x)
                        {
                            const uint32_t color =
                                SampleBilinearFP(
                                    src,
                                    srcWidth,
                                    srcHeight,
                                    u,
                                    v);

                            dest[x] =
                                ApplyTint(
                                    color,
                                    tintR,
                                    tintG,
                                    tintB,
                                    tintAmount,
                                    g);

                            u += dudx;
                            v += dvdx;
                            g += dgdx;
                        }

                        dest += destStride;

                        left.u += dudy_left;
                        left.v += dvdy_left;
                        left.g += dgdy_left;

                        right.u += dudy_right;
                        right.v += dvdy_right;
                        right.g += dgdy_right;
                    }
                }


                // ============================================================
                // Screen rendern
                // ============================================================

                template <class T>
                inline void RenderScreen8x8(
                    GridPointUVG* grid,
                    uint32_t* dest,
                    int destWidth,

                    const uint32_t* src,
                    int srcWidth,
                    int srcHeight,

                    unsigned blocksX,
                    unsigned blocksY,

                    const PostFXParams<T>& fx)
                {
                    if (!grid || !dest || !src)
                        return;

                    GridPointUVG* line = grid;

                    GridPointUVG* nextline =
                        grid + blocksX + 1;

                    for (unsigned y = 0; y < blocksY; ++y)
                    {
                        for (unsigned x = 0; x < blocksX; ++x)
                        {
                            RenderBlock8x8(
                                line[x],
                                nextline[x],
                                line[x + 1],
                                nextline[x + 1],

                                dest + x * 8,
                                destWidth,

                                src,
                                srcWidth,
                                srcHeight,

                                fx.tintR,
                                fx.tintG,
                                fx.tintB,
                                fx.tintAmount);
                        }

                        dest +=
                            static_cast<size_t>(destWidth) * 8;

                        line = nextline;

                        nextline += blocksX + 1;
                    }
                }


                // ============================================================
                // Wave Grid erzeugen
                // ============================================================

                template <class T>
                inline void MakeWaveGrid(
                    T time,
                    GridPointUVG* grid,

                    int width,
                    int height,

                    const PostFXParams<T>& fx,

                    T cameraYaw,
                    T cameraPitch)
                {
                    if (!grid || width <= 0 || height <= 0)
                        return;

                    const int blocksX = width / 8;
                    const int blocksY = height / 8;

                    const T yawShift =
                        cameraYaw * static_cast<T>(width) * static_cast<T>(0.6);

                    const T pitchShift =
                        cameraPitch * static_cast<T>(height) * static_cast<T>(0.6);

                    GridPointUVG* data = grid;

                    for (int y = 0; y <= blocksY; ++y)
                    {
                        const T py =
                            static_cast<T>(y * 8) + pitchShift;

                        for (int x = 0; x <= blocksX; ++x)
                        {
                            const T px =
                                static_cast<T>(x * 8) + yawShift;

                            const T dx =
                                std::sin(
                                    py * fx.freqY +
                                    time * fx.speedX)
                                * fx.ampX
                                +
                                std::sin(
                                    py * fx.freqY * static_cast<T>(2.3) +
                                    time * fx.speedX * static_cast<T>(1.7))
                                * fx.ampX * static_cast<T>(0.4);

                            const T dy =
                                std::sin(
                                    px * fx.freqX +
                                    time * fx.speedY)
                                * fx.ampY
                                +
                                std::cos(
                                    px * fx.freqX * static_cast<T>(1.9) +
                                    time * fx.speedY * static_cast<T>(1.3))
                                * fx.ampY * static_cast<T>(0.3);

                            data->u =
                                static_cast<int32_t>(
                                    (static_cast<T>(x * 8) + dx) *
                                    static_cast<T>(65536));

                            data->v =
                                static_cast<int32_t>(
                                    (static_cast<T>(y * 8) + dy) *
                                    static_cast<T>(65536));

                            T caustic =
                                static_cast<T>(1.0) -
                                fx.causticStrength;

                            caustic +=
                                fx.causticStrength *
                                static_cast<T>(0.5) *
                                (
                                    static_cast<T>(1.0) +
                                    std::sin(
                                        px * fx.causticFreq +
                                        py * fx.causticFreq * static_cast<T>(0.6) +
                                        time * fx.causticSpeed)
                                    *
                                    std::sin(
                                        px * fx.causticFreq * static_cast<T>(0.4) -
                                        py * fx.causticFreq * static_cast<T>(1.1) +
                                        time * fx.causticSpeed * static_cast<T>(0.65))
                                    );

                            caustic =
                                std::clamp(
                                    caustic,
                                    static_cast<T>(0.0),
                                    static_cast<T>(1.0));

                            data->g =
                                static_cast<int32_t>(
                                    caustic *
                                    static_cast<T>(65536));

                            ++data;
                        }
                    }
                }

            } // namespace detail


            // ================================================================
            // UnderwaterWarpEffect
            // ================================================================

            template <class T>
            class UnderwaterWarpEffect
                : public IPostFXEffect<T>
            {
            public:

                explicit UnderwaterWarpEffect(
                    const PostFXParams<T>& params = FX_WATER<T>)
                    : m_params(params)
                {
                }


                void SetParams(
                    const PostFXParams<T>& p)
                {
                    m_params = p;
                }


                const PostFXParams<T>& GetParams() const
                {
                    return m_params;
                }


                void SetCamera(
                    T yaw,
                    T pitch)
                {
                    m_yaw = yaw;
                    m_pitch = pitch;
                }


                void Apply(
                    GBuffer<T>& gbuffer,
                    T time) override
                {
                    if (!gbuffer.HasColor())
                        return;

                    const int width = gbuffer.width;
                    const int height = gbuffer.height;

                    if (width <= 0 || height <= 0)
                        return;

                    const int blocksX = width / 8;
                    const int blocksY = height / 8;

                    if (blocksX <= 0 || blocksY <= 0)
                        return;

                    m_sourceCopy.assign(
                        gbuffer.color,
                        gbuffer.color + gbuffer.PixelCount());

                    m_grid.resize(
                        static_cast<size_t>(blocksX + 1) *
                        static_cast<size_t>(blocksY + 1));

                    detail::MakeWaveGrid(
                        time,
                        m_grid.data(),
                        width,
                        height,
                        m_params,
                        m_yaw,
                        m_pitch);

                    detail::RenderScreen8x8(
                        m_grid.data(),

                        gbuffer.color,
                        width,

                        m_sourceCopy.data(),
                        width,
                        height,

                        static_cast<unsigned>(blocksX),
                        static_cast<unsigned>(blocksY),

                        m_params);
                }


                const char* GetName() const override
                {
                    return "UnderwaterWarp";
                }


            private:

                PostFXParams<T> m_params;

                T m_yaw = static_cast<T>(0);
                T m_pitch = static_cast<T>(0);

                std::vector<uint32_t> m_sourceCopy;
                std::vector<GridPointUVG> m_grid;
            };


            // ================================================================
            // Depth Fog
            // ================================================================

            template <class T>
            class DepthFogEffect
                : public IPostFXEffect<T>
            {
            public:

                explicit DepthFogEffect(
                    uint32_t fogColor = RGBA(80,80,80,255),
                    T density = static_cast<T>(0.02),
                    T startDepth = static_cast<T>(0.0))
                    : m_fogColor(fogColor),
                    m_density(density),
                    m_start(startDepth)
                {
                }

                void Apply(GBuffer<T>& gbuffer, T /*time*/) override
                {
                    if (!gbuffer.HasColor() || !gbuffer.HasDepth())
                        return;
                    const size_t n = gbuffer.PixelCount();
                    for (size_t i = 0; i < n; ++i)
                    {
                        const T invW = gbuffer.depth[i];
                        // Ungeschriebene Pixel (Clear-Wert, meist 0 = "unendlich weit weg")
                        // sollen als maximal vernebelt gelten, nicht als "unendlich nah".
                        T z;
                        if (invW <= static_cast<T>(1e-8))
                            z = std::numeric_limits<T>::max();
                        else
                            z = static_cast<T>(1) / invW; // echten Kamera-Abstand zurueckrechnen

                        T f = std::max(static_cast<T>(0), z - m_start) * m_density;
                        f = std::min(f, static_cast<T>(1));
                        const uint32_t t = static_cast<uint32_t>(f * static_cast<T>(255));
                        gbuffer.color[i] = LerpColor(gbuffer.color[i], m_fogColor, t);
                    }
                }


                std::vector<BufferSlot>
                    GetRequiredInputs() const override
                {
                    return
                    {
                        BufferSlot::Color,
                        BufferSlot::Depth
                    };
                }


                const char* GetName() const override
                {
                    return "DepthFog";
                }


            private:

                static uint32_t LerpColor(uint32_t a, uint32_t b, uint32_t t)
                {
                    auto ch = [&](int shift) -> uint32_t
                        {
                            const uint32_t ca = (a >> shift) & 0xff;
                            const uint32_t cb = (b >> shift) & 0xff;
                            return (ca * (255 - t) + cb * t) / 255; // NUR 0..255, kein <<shift mehr
                        };
                    return RGB(ch(16), ch(8), ch(0)); // R, G, B - passend zu eurer ARGB-Konvention
                }


                uint32_t m_fogColor;

                T m_density;
                T m_start;
            };


            // ================================================================
            // Gravity Well
            // ================================================================

            template <class T>
            struct GravityWell
            {
                T x;
                T y;

                T radius;
                T strength;

                T startTime;
                T duration;

                T frequency;
                T damping;
            };


            template <class T>
            class GravityWellEffect
                : public IPostFXEffect<T>
            {
            public:

                // ------------------------------------------------------------
                // Neues Gravity Well erzeugen
                // ------------------------------------------------------------

                void Spawn(
                    T x,
                    T y,
                    T currentTime,

                    T radius =
                    static_cast<T>(80.0),

                    T strength =
                    static_cast<T>(24.0),

                    T duration =
                    static_cast<T>(1.2),

                    T frequency =
                    static_cast<T>(3.0),

                    T damping =
                    static_cast<T>(4.0))
                {
                    m_wells.push_back(
                        {
                            x,
                            y,
                            radius,
                            strength,
                            currentTime,
                            duration,
                            frequency,
                            damping
                        });
                }


                // ------------------------------------------------------------
                // Wells loeschen
                // ------------------------------------------------------------

                void ClearWells()
                {
                    m_wells.clear();
                }


                size_t ActiveWellCount() const
                {
                    return m_wells.size();
                }


                // ------------------------------------------------------------
                // Effekt anwenden
                // ------------------------------------------------------------

                void Apply(
                    GBuffer<T>& gbuffer,
                    T time) override
                {
                    if (!gbuffer.HasColor())
                        return;

                    const int width = gbuffer.width;
                    const int height = gbuffer.height;

                    if (width <= 0 || height <= 0)
                        return;


                    // --------------------------------------------------------
                    // Abgelaufene Wells entfernen
                    // --------------------------------------------------------

                    m_wells.erase(
                        std::remove_if(
                            m_wells.begin(),
                            m_wells.end(),

                            [&](const GravityWell<T>& w)
                            {
                                return
                                    (time - w.startTime) >
                                    w.duration;
                            }),

                        m_wells.end());


                    if (m_wells.empty())
                        return;


                    // --------------------------------------------------------
                    // Gemeinsame Bounding Box
                    // --------------------------------------------------------

                    int minX = width;
                    int minY = height;

                    int maxX = 0;
                    int maxY = 0;


                    for (const auto& w : m_wells)
                    {
                        minX =
                            std::min(
                                minX,
                                static_cast<int>(
                                    std::floor(
                                        w.x - w.radius)));

                        minY =
                            std::min(
                                minY,
                                static_cast<int>(
                                    std::floor(
                                        w.y - w.radius)));

                        maxX =
                            std::max(
                                maxX,
                                static_cast<int>(
                                    std::ceil(
                                        w.x + w.radius)));

                        maxY =
                            std::max(
                                maxY,
                                static_cast<int>(
                                    std::ceil(
                                        w.y + w.radius)));
                    }


                    minX = std::max(minX, 0);
                    minY = std::max(minY, 0);

                    maxX = std::min(maxX, width);
                    maxY = std::min(maxY, height);


                    if (minX >= maxX ||
                        minY >= maxY)
                    {
                        return;
                    }


                    // --------------------------------------------------------
                    // Originalbild kopieren
                    // --------------------------------------------------------

                    m_sourceCopy.assign(
                        gbuffer.color,
                        gbuffer.color + gbuffer.PixelCount());


                    // --------------------------------------------------------
                    // Effekt berechnen
                    // --------------------------------------------------------

                    constexpr T PI =
                        static_cast<T>(
                            3.14159265358979323846);


                    for (int y = minY;
                        y < maxY;
                        ++y)
                    {
                        uint32_t* destRow =
                            gbuffer.color +
                            static_cast<size_t>(y) * width;


                        for (int x = minX;
                            x < maxX;
                            ++x)
                        {
                            T dx =
                                static_cast<T>(0);

                            T dy =
                                static_cast<T>(0);


                            for (const auto& w : m_wells)
                            {
                                const T ox =
                                    static_cast<T>(x) - w.x;

                                const T oy =
                                    static_cast<T>(y) - w.y;


                                const T dist =
                                    std::sqrt(
                                        ox * ox +
                                        oy * oy);


                                if (dist >= w.radius)
                                    continue;


                                // ------------------------------------------------
                                // Radialer Falloff
                                // ------------------------------------------------

                                const T t =
                                    static_cast<T>(1) -
                                    dist / w.radius;


                                const T falloff =
                                    t * t *
                                    (
                                        static_cast<T>(3) -
                                        static_cast<T>(2) * t
                                        );


                                // ------------------------------------------------
                                // Zeit innerhalb des Effekts
                                // ------------------------------------------------

                                const T lt =
                                    std::clamp(
                                        (time - w.startTime) /
                                        w.duration,

                                        static_cast<T>(0),
                                        static_cast<T>(1));


                                // ------------------------------------------------
                                // Gedaempfte Schwingung
                                // ------------------------------------------------

                                const T envelope =
                                    std::exp(
                                        -w.damping * lt);


                                const T amplitude =
                                    std::sin(
                                        w.frequency *
                                        lt *
                                        PI)
                                    * envelope;


                                // ------------------------------------------------
                                // Normalisierte Richtung
                                // ------------------------------------------------

                                const T invDist =
                                    static_cast<T>(1) /
                                    std::max(
                                        dist,
                                        static_cast<T>(0.0001));


                                const T nx =
                                    ox * invDist;

                                const T ny =
                                    oy * invDist;


                                // ------------------------------------------------
                                // Verschiebung
                                // ------------------------------------------------

                                const T mag =
                                    amplitude *
                                    falloff *
                                    w.strength;


                                dx += nx * mag;
                                dy += ny * mag;
                            }


                            const int32_t su =
                                static_cast<int32_t>(
                                    (
                                        static_cast<T>(x) +
                                        dx
                                        ) *
                                    static_cast<T>(65536));


                            const int32_t sv =
                                static_cast<int32_t>(
                                    (
                                        static_cast<T>(y) +
                                        dy
                                        ) *
                                    static_cast<T>(65536));


                            destRow[x] =
                                detail::SampleBilinearFP(
                                    m_sourceCopy.data(),
                                    width,
                                    height,
                                    su,
                                    sv);
                        }
                    }
                }


                const char* GetName() const override
                {
                    return "GravityWell";
                }


            private:

                std::vector<GravityWell<T>> m_wells;

                std::vector<uint32_t> m_sourceCopy;
            };


            namespace detail
            {
                inline int Ch(uint32_t c, int shift)
                {
                    return static_cast<int>((c >> shift) & 0xff);
                }

                // Horizontaler Box-Blur mit gebrochenem Radius (ri + f)
                inline void BoxBlurH(const uint32_t* src, uint32_t* dst,
                    int w, int h, int ri, float f)
                {
                    const float inv = 1.0f / (2.0f * ri + 1.0f + 2.0f * f);

                    for (int y = 0; y < h; ++y)
                    {
                        const uint32_t* row = src + static_cast<size_t>(y) * w;
                        uint32_t* out = dst + static_cast<size_t>(y) * w;

                        auto at = [&](int x) -> uint32_t
                            {
                                return row[std::clamp(x, 0, w - 1)];
                            };

                        int sr = 0, sg = 0, sb = 0;
                        for (int i = -ri; i <= ri; ++i)
                        {
                            const uint32_t c = at(i);
                            sr += Ch(c, 16); sg += Ch(c, 8); sb += Ch(c, 0);
                        }

                        for (int x = 0; x < w; ++x)
                        {
                            const uint32_t cl = at(x - ri - 1);
                            const uint32_t cr = at(x + ri + 1);

                            const float R = (sr + f * (Ch(cl, 16) + Ch(cr, 16))) * inv;
                            const float G = (sg + f * (Ch(cl, 8) + Ch(cr, 8))) * inv;
                            const float B = (sb + f * (Ch(cl, 0) + Ch(cr, 0))) * inv;

                            out[x] = 0xff000000u
                                | (static_cast<uint32_t>(R + 0.5f) << 16)
                                | (static_cast<uint32_t>(G + 0.5f) << 8)
                                | static_cast<uint32_t>(B + 0.5f);

                            // Fenster verschieben
                            sr += Ch(cr, 16); sg += Ch(cr, 8); sb += Ch(cr, 0);
                            const uint32_t co = at(x - ri);
                            sr -= Ch(co, 16); sg -= Ch(co, 8); sb -= Ch(co, 0);
                        }
                    }
                }

                // Vertikaler Box-Blur, zeilenweise (cache-freundlich)
                inline void BoxBlurV(const uint32_t* src, uint32_t* dst,
                    int w, int h, int ri, float f,
                    std::vector<int32_t>& sums)
                {
                    const float inv = 1.0f / (2.0f * ri + 1.0f + 2.0f * f);
                    sums.assign(static_cast<size_t>(w) * 3, 0);

                    auto rowPtr = [&](int y) -> const uint32_t*
                        {
                            return src + static_cast<size_t>(std::clamp(y, 0, h - 1)) * w;
                        };

                    for (int i = -ri; i <= ri; ++i)
                    {
                        const uint32_t* r = rowPtr(i);
                        for (int x = 0; x < w; ++x)
                        {
                            sums[x * 3 + 0] += Ch(r[x], 16);
                            sums[x * 3 + 1] += Ch(r[x], 8);
                            sums[x * 3 + 2] += Ch(r[x], 0);
                        }
                    }

                    for (int y = 0; y < h; ++y)
                    {
                        const uint32_t* rl = rowPtr(y - ri - 1);
                        const uint32_t* rr = rowPtr(y + ri + 1);
                        const uint32_t* ro = rowPtr(y - ri);
                        uint32_t* out = dst + static_cast<size_t>(y) * w;

                        for (int x = 0; x < w; ++x)
                        {
                            int32_t* s = &sums[x * 3];

                            const float R = (s[0] + f * (Ch(rl[x], 16) + Ch(rr[x], 16))) * inv;
                            const float G = (s[1] + f * (Ch(rl[x], 8) + Ch(rr[x], 8))) * inv;
                            const float B = (s[2] + f * (Ch(rl[x], 0) + Ch(rr[x], 0))) * inv;

                            out[x] = 0xff000000u
                                | (static_cast<uint32_t>(R + 0.5f) << 16)
                                | (static_cast<uint32_t>(G + 0.5f) << 8)
                                | static_cast<uint32_t>(B + 0.5f);

                            s[0] += Ch(rr[x], 16) - Ch(ro[x], 16);
                            s[1] += Ch(rr[x], 8) - Ch(ro[x], 8);
                            s[2] += Ch(rr[x], 0) - Ch(ro[x], 0);
                        }
                    }
                }

                inline uint32_t BlendToColor(uint32_t dst, uint32_t src, int a /*0..255*/)
                {
                    const int ia = 255 - a;
                    const uint32_t r = (Ch(dst, 16) * ia + Ch(src, 16) * a) / 255;
                    const uint32_t g = (Ch(dst, 8) * ia + Ch(src, 8) * a) / 255;
                    const uint32_t b = (Ch(dst, 0) * ia + Ch(src, 0) * a) / 255;
                    return 0xff000000u | (r << 16) | (g << 8) | b;
                }
            } // namespace detail


            // ================================================================
            // BlurEffect
            //
            // strength = Radius in Pixeln (0 = aus, Bruchteile erlaubt,
            //            daher flackerfrei animierbar).
            // passes   = 1..4. Mehr Durchgaenge -> weicher (naehert Gauss an).
            // ================================================================
            template <class T>
            class BlurEffect : public IPostFXEffect<T>
            {
            public:

                explicit BlurEffect(T strength = static_cast<T>(2.0), int passes = 2)
                    : m_strength(strength), m_passes(std::clamp(passes, 1, 4))
                {
                }

                void SetStrength(T s) { m_strength = std::max(static_cast<T>(0), s); }
                T    GetStrength() const { return m_strength; }

                void SetPasses(int p) { m_passes = std::clamp(p, 1, 4); }
                int  GetPasses() const { return m_passes; }

                void Apply(GBuffer<T>& gb, T /*time*/) override
                {
                    if (!gb.HasColor() || gb.width <= 0 || gb.height <= 0)
                        return;

                    const float s = static_cast<float>(m_strength);
                    if (s < 0.01f)
                        return;

                    const int w = gb.width;
                    const int h = gb.height;

                    const int ri = std::min(static_cast<int>(std::floor(s)),
                        std::max(w, h));
                    const float f = s - std::floor(s);

                    const size_t n = gb.PixelCount();
                    m_a.resize(n);
                    m_b.resize(n);

                    // Pass 0 liest direkt aus dem Color-Buffer
                    detail::BoxBlurH(gb.color, m_b.data(), w, h, ri, f);
                    detail::BoxBlurV(m_b.data(), m_a.data(), w, h, ri, f, m_sums);

                    for (int p = 1; p < m_passes; ++p)
                    {
                        detail::BoxBlurH(m_a.data(), m_b.data(), w, h, ri, f);
                        detail::BoxBlurV(m_b.data(), m_a.data(), w, h, ri, f, m_sums);
                    }

                    std::memcpy(gb.color, m_a.data(), n * sizeof(uint32_t));
                }

                const char* GetName() const override { return "Blur"; }

            private:
                T   m_strength;
                int m_passes;

                std::vector<uint32_t> m_a, m_b;
                std::vector<int32_t>  m_sums;
            };


            // ================================================================
            // SnowEffect
            // ================================================================
            template <class T>
            struct SnowParams
            {
                int  count = 1500;                       // Anzahl Flocken
                T    volumeRadius = static_cast<T>(40);  // halbe Breite/Tiefe der Box um die Kamera
                T    volumeHeight = static_cast<T>(25);  // halbe Hoehe der Box um die Kamera

                T    minSize = static_cast<T>(0.04);     // Flockengroesse (Welteinheiten)
                T    maxSize = static_cast<T>(0.12);

                T    fallSpeedMin = static_cast<T>(1.5); // Einheiten/Sek.
                T    fallSpeedMax = static_cast<T>(3.5);

                T    windX = static_cast<T>(0.8);
                T    windZ = static_cast<T>(0.0);
                T    sway = static_cast<T>(0.6);         // seitliches Taumeln

                T    nearClip = static_cast<T>(0.2);
                T    maxDistance = static_cast<T>(45);   // darueber voll ausgeblendet

                T    brightness = static_cast<T>(0.95);  // max. Deckkraft
                uint32_t color = 0x00ffffffu;            // RGB der Flocken

                bool collide = true;                     // am Depth-Buffer liegenbleiben
                T    stickTime = static_cast<T>(4.0);    // Sek. bis eine liegende Flocke schmilzt
                T    depthBias = static_cast<T>(0.02);   // relative Toleranz beim Depth-Test
            };

            template <class T>
            class SnowEffect : public IPostFXEffect<T>
            {
            public:

                explicit SnowEffect(const SnowParams<T>& p = SnowParams<T>(),
                    uint32_t seed = 1337u)
                    : m_p(p), m_rng(seed)
                {
                }

                void SetParams(const SnowParams<T>& p)
                {
                    const bool resize = (p.count != m_p.count);
                    m_p = p;
                    if (resize) m_flakes.clear();   // wird neu initialisiert
                }
                const SnowParams<T>& GetParams() const { return m_p; }

                // Kamera-Orientierung (Bogenmass)
                void SetCamera(T yaw, T pitch) { m_yaw = yaw; m_pitch = pitch; }

                // Kamera-Position (optional, fuer Laufen/Fahren)
                void SetCameraPosition(T x, T y, T z) { m_cx = x; m_cy = y; m_cz = z; }

                // Vertikales Field-of-View (Bogenmass)
                void SetFov(T fovY) { m_fovY = fovY; }

                std::vector<BufferSlot> GetRequiredInputs() const override
                {
                    return { BufferSlot::Color, BufferSlot::Depth };
                }

                const char* GetName() const override { return "Snow"; }


                void Apply(GBuffer<T>& gb, T time) override
                {
                    if (!gb.HasColor() || !gb.HasDepth())
                        return;
                    if (gb.width <= 0 || gb.height <= 0 || m_p.count <= 0)
                        return;

                    // --- dt ------------------------------------------------
                    T dt = static_cast<T>(0);
                    if (m_hasTime)
                        dt = std::clamp(time - m_lastTime, static_cast<T>(0), static_cast<T>(0.1));
                    m_lastTime = time;
                    m_hasTime = true;

                    // --- Kamera-Matrix (sin/cos, einmal pro Frame) ---------
                    m_sy = std::sin(m_yaw);   m_cyaw = std::cos(m_yaw);
                    m_sp = std::sin(m_pitch); m_cp = std::cos(m_pitch);
                    m_focal = (static_cast<T>(gb.height) * static_cast<T>(0.5)) /
                        std::tan(m_fovY * static_cast<T>(0.5));

                    if (static_cast<int>(m_flakes.size()) != m_p.count)
                        InitFlakes();

                    Simulate(gb, time, dt);
                    Render(gb);
                }


            private:

                struct Flake
                {
                    T x, y, z;
                    T size;
                    T fall;
                    T phase;
                    T stuck;       // < 0: faellt, sonst Restzeit des Liegenbleibens
                };

                // ---------------------------------------------------------
                T Rand(T a, T b)
                {
                    std::uniform_real_distribution<float> d(0.0f, 1.0f);
                    return a + (b - a) * static_cast<T>(d(m_rng));
                }

                void Respawn(Flake& f, bool anywhereInHeight)
                {
                    f.x = m_cx + Rand(-m_p.volumeRadius, m_p.volumeRadius);
                    f.z = m_cz + Rand(-m_p.volumeRadius, m_p.volumeRadius);
                    f.y = anywhereInHeight
                        ? m_cy + Rand(-m_p.volumeHeight, m_p.volumeHeight)
                        : m_cy + m_p.volumeHeight;
                    f.size = Rand(m_p.minSize, m_p.maxSize);
                    f.fall = Rand(m_p.fallSpeedMin, m_p.fallSpeedMax);
                    f.phase = Rand(static_cast<T>(0), static_cast<T>(6.2831853));
                    f.stuck = static_cast<T>(-1);
                }

                void InitFlakes()
                {
                    m_flakes.resize(static_cast<size_t>(m_p.count));
                    for (auto& f : m_flakes)
                        Respawn(f, true);
                }

                // Welt -> Bildschirm. false = hinter der Kamera.
                bool Project(T wx, T wy, T wz, const GBuffer<T>& gb,
                    T& sx, T& sy, T& z) const
                {
                    const T dx = wx - m_cx;
                    const T dy = wy - m_cy;
                    const T dz = wz - m_cz;

                    // Yaw
                    const T x1 = m_cyaw * dx - m_sy * dz;
                    const T z1 = m_sy * dx + m_cyaw * dz;
                    // Pitch
                    const T y2 = m_cp * dy - m_sp * z1;
                    const T z2 = m_sp * dy + m_cp * z1;

                    if (z2 < m_p.nearClip)
                        return false;

                    const T k = m_focal / z2;
                    sx = static_cast<T>(gb.width) * static_cast<T>(0.5) + x1 * k;
                    sy = static_cast<T>(gb.height) * static_cast<T>(0.5) - y2 * k;
                    z = z2;
                    return true;
                }

                // true, wenn Punkt auf dem Schirm UND hinter Geometrie
                bool IsOccluded(T sx, T sy, T z, const GBuffer<T>& gb) const
                {
                    const int px = static_cast<int>(sx);
                    const int py = static_cast<int>(sy);
                    if (sx < 0 || sy < 0 || px >= gb.width || py >= gb.height)
                        return false;

                    const T d = gb.depth[static_cast<size_t>(py) * gb.width + px];
                    if (d <= static_cast<T>(1e-8))
                        return false;   // nichts gezeichnet -> Himmel

                    return d * z > (static_cast<T>(1) + m_p.depthBias);
                }

                // ---------------------------------------------------------
                void Simulate(const GBuffer<T>& gb, T time, T dt)
                {
                    const T R = m_p.volumeRadius;
                    const T H = m_p.volumeHeight;
                    const T twoR = R * static_cast<T>(2);

                    for (auto& f : m_flakes)
                    {
                        // ---- liegende Flocke: Timer ---------------------
                        if (f.stuck >= static_cast<T>(0))
                        {
                            f.stuck -= dt;
                            if (f.stuck <= static_cast<T>(0))
                                Respawn(f, false);
                            continue;
                        }

                        const T px = f.x, py = f.y, pz = f.z;

                        // ---- Bewegung ----------------------------------
                        f.x += (m_p.windX + std::sin(time * static_cast<T>(0.8) + f.phase) * m_p.sway) * dt;
                        f.z += (m_p.windZ + std::cos(time * static_cast<T>(0.7) + f.phase * static_cast<T>(1.3)) * m_p.sway) * dt;
                        f.y -= f.fall * dt;

                        // ---- Box um die Kamera (horizontal wrap) ---------
                        f.x -= twoR * std::floor((f.x - m_cx + R) / twoR);
                        f.z -= twoR * std::floor((f.z - m_cz + R) / twoR);

                        // ---- unten raus / zu hoch -> oben neu ------------
                        if (f.y < m_cy - H || f.y > m_cy + H)
                        {
                            Respawn(f, false);
                            continue;
                        }

                        // ---- Kollision mit Depth-Buffer ------------------
                        if (m_p.collide && dt > static_cast<T>(0))
                        {
                            T sx, sy, z;
                            if (Project(f.x, f.y, f.z, gb, sx, sy, z) &&
                                IsOccluded(sx, sy, z, gb))
                            {
                                // Vorherige Position war sichtbar davor?
                                T psx, psy, pz2;
                                if (Project(px, py, pz, gb, psx, psy, pz2) &&
                                    !IsOccluded(psx, psy, pz2, gb))
                                {
                                    f.x = px; f.y = py; f.z = pz;   // an Oberflaeche halten
                                    f.stuck = m_p.stickTime;
                                }
                                // sonst: war schon verdeckt -> ignorieren
                            }
                        }
                    }
                }

                // ---------------------------------------------------------
                void Render(GBuffer<T>& gb) const
                {
                    const int W = gb.width;
                    const int H = gb.height;

                    for (const auto& f : m_flakes)
                    {
                        T sx, sy, z;
                        if (!Project(f.x, f.y, f.z, gb, sx, sy, z))
                            continue;

                        // Entfernungs-Fade
                        T alpha = m_p.brightness *
                            (static_cast<T>(1) - z / m_p.maxDistance);
                        if (alpha <= static_cast<T>(0.01))
                            continue;

                        // Schmelz-Fade bei liegenden Flocken
                        if (f.stuck >= static_cast<T>(0))
                            alpha *= std::min(static_cast<T>(1), f.stuck * static_cast<T>(2));

                        const T r = f.size * m_focal / z;      // Radius in Pixeln

                        if (r < static_cast<T>(0.75))
                        {
                            // Subpixel-Flocke: ein Pixel, Deckkraft ~ Groesse
                            const int px = static_cast<int>(sx);
                            const int py = static_cast<int>(sy);
                            if (px < 0 || py < 0 || px >= W || py >= H)
                                continue;

                            const size_t idx = static_cast<size_t>(py) * W + px;
                            if (!Visible(gb, idx, z))
                                continue;

                            const T a = alpha * std::max(r / static_cast<T>(0.75), static_cast<T>(0.35));
                            gb.color[idx] = detail::BlendToColor(
                                gb.color[idx], m_p.color,
                                static_cast<int>(a * static_cast<T>(255)));
                        }
                        else
                        {
                            // Weiche Scheibe
                            const int x0 = std::max(0, static_cast<int>(std::floor(sx - r)));
                            const int x1 = std::min(W - 1, static_cast<int>(std::ceil(sx + r)));
                            const int y0 = std::max(0, static_cast<int>(std::floor(sy - r)));
                            const int y1 = std::min(H - 1, static_cast<int>(std::ceil(sy + r)));

                            for (int y = y0; y <= y1; ++y)
                            {
                                for (int x = x0; x <= x1; ++x)
                                {
                                    const T ddx = (static_cast<T>(x) + static_cast<T>(0.5)) - sx;
                                    const T ddy = (static_cast<T>(y) + static_cast<T>(0.5)) - sy;
                                    const T dist = std::sqrt(ddx * ddx + ddy * ddy);
                                    if (dist >= r) continue;

                                    const size_t idx = static_cast<size_t>(y) * W + x;
                                    if (!Visible(gb, idx, z)) continue;

                                    const T t = static_cast<T>(1) - dist / r;
                                    const int a = static_cast<int>(alpha * t * static_cast<T>(255));
                                    if (a <= 0) continue;

                                    gb.color[idx] = detail::BlendToColor(gb.color[idx], m_p.color, a);
                                }
                            }
                        }
                    }
                }

                bool Visible(const GBuffer<T>& gb, size_t idx, T z) const
                {
                    const T d = gb.depth[idx];
                    if (d <= static_cast<T>(1e-8))
                        return true;   // Himmel
                    return d * z <= (static_cast<T>(1) + m_p.depthBias);
                }

            private:

                SnowParams<T> m_p;
                std::mt19937  m_rng;

                std::vector<Flake> m_flakes;

                // Kamera
                T m_yaw = static_cast<T>(0);
                T m_pitch = static_cast<T>(0);
                T m_cx = static_cast<T>(0);
                T m_cy = static_cast<T>(0);
                T m_cz = static_cast<T>(0);
                T m_fovY = static_cast<T>(1.2217);   // ~70 Grad

                // pro Frame gecacht
                T m_sy = static_cast<T>(0), m_cyaw = static_cast<T>(1);
                T m_sp = static_cast<T>(0), m_cp = static_cast<T>(1);
                T m_focal = static_cast<T>(1);

                T    m_lastTime = static_cast<T>(0);
                bool m_hasTime = false;
            };









            template <class T>
            class DynamicLightFX : public IPostFXEffect<T>
            {
            public:
                using Vec3 = ng::math::Matrix<3, 1, T>;

                struct Light
                {
                    Vec3 pos;
                    Vec3 color{ 1, 1, 1 };
                    T intensity = T(1);
                    T radius = T(300);
                };

                struct View
                {
                    Vec3 pos, right, up, forward;
                    T p00 = 1, p11 = 1;     // NDC-Skalen aus GetViewBasis()
                };

                // Pro Frame VOR mgr.Apply() setzen
                void SetView(const View& v) { m_view = v; }
                void SetLights(const std::vector<Light>& l) { m_lights = l; }

                const char* GetName() const override { return "DynamicLight"; }

                std::vector<BufferSlot> GetRequiredInputs() const override
                {
                    return { BufferSlot::Color, BufferSlot::Depth, BufferSlot::Normal };
                }

                void Apply(GBuffer<T>& gb, T /*time*/) override
                {
                    if (!gb.color || !gb.depth || !gb.normal || gb.width <= 0 || gb.height <= 0) return;
                    if (m_lights.empty()) return;

                    const int W = gb.width, H = gb.height;
                    const T cx = T(W) * T(0.5), cy = T(H) * T(0.5);
                    const T kx = T(1) / (cx * m_view.p00);
                    const T ky = T(1) / (cy * m_view.p11);
                    const Vec3& pos = m_view.pos, & R = m_view.right, & U = m_view.up, & F = m_view.forward;

                    for (const auto& L : m_lights)
                    {
                        const T r2 = L.radius * L.radius;
                        const T invR2 = T(1) / r2;

                        // Screen-Rechteck der Lichtkugel
                        int x0 = 0, y0 = 0, x1 = W - 1, y1 = H - 1;
                        const Vec3 rel = L.pos - pos;
                        const T zc = rel ^ F;
                        if ((rel ^ rel) > r2)
                        {
                            if (zc + L.radius < T(0.01)) continue;
                            if (zc > L.radius)
                            {
                                const T pxc = cx + ((rel ^ R) / zc) * (cx * m_view.p00);
                                const T pyc = cy - ((rel ^ U) / zc) * (cy * m_view.p11);
                                const T rpx = L.radius / (zc - L.radius) * std::abs(cx * m_view.p00);
                                const T rpy = L.radius / (zc - L.radius) * std::abs(cy * m_view.p11);
                                x0 = std::max(0, (int)std::floor(pxc - rpx));
                                x1 = std::min(W - 1, (int)std::ceil(pxc + rpx));
                                y0 = std::max(0, (int)std::floor(pyc - rpy));
                                y1 = std::min(H - 1, (int)std::ceil(pyc + rpy));
                                if (x0 > x1 || y0 > y1) continue;
                            }
                        }

                        for (int y = y0; y <= y1; ++y)
                        {
                            const T yv = -((T(y) + T(0.5)) - cy) * ky;
                            for (int x = x0; x <= x1; ++x)
                            {
                                const size_t i = (size_t)y * W + x;
                                const T d = gb.depth[i];
                                if (!(d > T(1e-9))) continue;
                                const T z = T(1) / d;

                                const T xv = ((T(x) + T(0.5)) - cx) * kx;
                                const Vec3 P = pos + R * (xv * z) + U * (yv * z) + F * z;

                                Vec3 toL = L.pos - P;
                                const T d2 = toL ^ toL;
                                if (d2 >= r2) continue;

                                const uint32_t np = gb.normal[i];
                                if ((np >> 24) == 0) continue;               // nie beschrieben
                                Vec3 N(T((np >> 16) & 255) / T(255) * T(2) - T(1),
                                    T((np >> 8) & 255) / T(255) * T(2) - T(1),
                                    T(np & 255) / T(255) * T(2) - T(1));
                                const T nl = N.length();
                                if (nl < T(1e-6)) continue;
                                N = N * (T(1) / nl);

                                toL = toL * (T(1) / std::max(std::sqrt(d2), T(1e-4)));
                                const T ndl = N ^ toL;
                                if (ndl <= T(0)) continue;

                                T att = T(1) - d2 * invR2;
                                att *= att;
                                const T k = ndl * att * L.intensity;
                                if (k < T(1.0 / 512)) continue;

                                const uint32_t c = gb.color[i];
                                const int r = (c >> 16) & 255, g = (c >> 8) & 255, b = c & 255;
                                const int nr = std::min(255, r + int(r * k * L.color(0) + k * L.color(0) * 24));
                                const int ng_ = std::min(255, g + int(g * k * L.color(1) + k * L.color(1) * 24));
                                const int nb = std::min(255, b + int(b * k * L.color(2) + k * L.color(2) * 24));
                                gb.color[i] = (c & 0xFF000000u) | (nr << 16) | (ng_ << 8) | nb;
                            }
                        }
                    }
                }

            private:
                View m_view;
                std::vector<Light> m_lights;
            };













        } // namespace postfx
    } // namespace renderer
} // namespace ng

