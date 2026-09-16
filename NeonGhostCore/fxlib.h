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



            template<class T>
            class SnowEffect
                : public IPostFXEffect<T>
            {
            public:

                using Vector3 = ng::math::Matrix<3, 1, T>;

                struct SnowFlake
                {
                    T x = 0;
                    T y = 0;
                    T z = 0;

                    T speed = 0;
                    T size = 1;
                    T phase = 0;
                };


            public:

                SnowEffect(
                    size_t count = 1200,
                    T speed = (T)8,
                    T wind = (T)2,
                    T size = (T)1.5)
                    :
                    m_count(count),
                    m_speed(speed),
                    m_wind(wind),
                    m_size(size)
                {
                    m_flakes.resize(m_count);

                    std::mt19937 rng(1337);

                    std::uniform_real_distribution<T> rnd(
                        (T)0,
                        (T)1);

                    for (auto& f : m_flakes)
                    {
                        f.x =
                            (rnd(rng) - (T)0.5) *
                            m_fieldWidth;

                        f.y =
                            (rnd(rng) - (T)0.5) *
                            m_fieldHeight;

                        f.z =
                            rnd(rng) *
                            m_fieldDepth +
                            (T)1;

                        f.speed =
                            m_speed *
                            ((T)0.5 + rnd(rng));

                        f.size =
                            m_size *
                            ((T)0.5 + rnd(rng));

                        f.phase =
                            rnd(rng) *
                            (T)6.283185307179586;
                    }
                }


                // ============================================================
                // IPostFXEffect
                // ============================================================

                void Apply(
                    GBuffer<T>& gbuffer,
                    T time) override
                {
                    if (!gbuffer.color)
                        return;

                    if (gbuffer.width <= 0 ||
                        gbuffer.height <= 0)
                        return;

                    Update(time);

                    for (const auto& f : m_flakes)
                    {
                        RenderFlake(
                            gbuffer,
                            f,
                            time);
                    }
                }


                std::vector<BufferSlot>
                    GetRequiredInputs() const override
                {
                    return
                    {
                        BufferSlot::Color
                    };
                }


                const char*
                    GetName() const override
                {
                    return "Snow";
                }


                // ============================================================
                // Camera
                // ============================================================

                void SetCamera(
                    const Vector3& forward,
                    const Vector3& up)
                {
                    m_forward = forward;
                    m_up = up;

                    m_forward.normalize();
                    m_up.normalize();

                    //
                    // Deine Konvention:
                    //
                    // Forward = -Z
                    // Up      = +Y
                    //
                    // Forward x Up = Right
                    //

                    m_right =
                        m_forward % m_up;

                    m_right.normalize();
                }


                void SetCameraDelta(
                    const Vector3& delta)
                {
                    m_cameraDelta = delta;
                }


                void SetCameraInfluence(T value)
                {
                    m_cameraInfluence =
                        std::max(
                            (T)0,
                            value);
                }


                // ============================================================
                // Projection
                // ============================================================

                void SetFOV(T fov)
                {
                    m_fov = fov;
                }


                void SetAspect(T aspect)
                {
                    m_aspect = aspect;
                }


                // ============================================================
                // Snow parameters
                // ============================================================

                void SetSpeed(T value)
                {
                    m_speed = value;

                    for (auto& f : m_flakes)
                        f.speed =
                        m_speed;
                }


                void SetWind(T value)
                {
                    m_wind = value;
                }


                void SetSize(T value)
                {
                    m_size = value;
                }


                void SetFieldSize(
                    T width,
                    T height,
                    T depth)
                {
                    m_fieldWidth = width;
                    m_fieldHeight = height;
                    m_fieldDepth = depth;
                }


            private:

                // ============================================================
                // Update
                // ============================================================

                void Update(T time)
                {
                    T dt = (T)0;

                    if (m_lastTime >= (T)0)
                    {
                        dt =
                            time -
                            m_lastTime;
                    }

                    m_lastTime = time;

                    dt =
                        std::clamp(
                            dt,
                            (T)0,
                            (T)0.05);


                    //
                    // Kamerabewegung in Kameraachsen
                    //

                    const T cameraX =
                        m_cameraDelta.dot(
                            m_right);

                    const T cameraY =
                        m_cameraDelta.dot(
                            m_up);

                    const T cameraZ =
                        m_cameraDelta.dot(
                            m_forward);


                    //
                    // Weltgravitation
                    //

                    const T gravityX =
                        m_worldDown.dot(
                            m_right);

                    const T gravityY =
                        m_worldDown.dot(
                            m_up);

                    const T gravityZ =
                        m_worldDown.dot(
                            m_forward);


                    for (auto& f : m_flakes)
                    {
                        const T fall =
                            f.speed * dt;


                        //
                        // Schnee fällt nach Welt-Y
                        //

                        f.x +=
                            gravityX * fall;

                        f.y +=
                            gravityY * fall;

                        f.z +=
                            gravityZ * fall;


                        //
                        // Wind
                        //

                        f.x +=
                            m_wind * dt;


                        //
                        // leichte Bewegung
                        //

                        f.x +=
                            std::sin(
                                time * m_wobbleSpeed +
                                f.phase) *
                            m_wobbleAmount *
                            dt;


                        //
                        // Kamerabewegung
                        //
                        // Forward = -Z
                        //

                        f.x -=
                            cameraX *
                            m_cameraInfluence;

                        f.y -=
                            cameraY *
                            m_cameraInfluence;

                        f.z +=
                            cameraZ *
                            m_cameraInfluence;


                        //
                        // Feld wrap
                        //

                        const T halfWidth =
                            m_fieldWidth * (T)0.5;

                        const T halfHeight =
                            m_fieldHeight * (T)0.5;


                        if (f.x < -halfWidth)
                            f.x += m_fieldWidth;

                        if (f.x > halfWidth)
                            f.x -= m_fieldWidth;


                        if (f.y < -halfHeight)
                            f.y += m_fieldHeight;

                        if (f.y > halfHeight)
                            f.y -= m_fieldHeight;


                        if (f.z < (T)1)
                            f.z += m_fieldDepth;

                        if (f.z > m_fieldDepth + (T)1)
                            f.z -= m_fieldDepth;
                    }


                    //
                    // Delta nur einmal anwenden
                    //

                    m_cameraDelta =
                        Vector3(
                            (T)0,
                            (T)0,
                            (T)0);
                }


            private:

                // ============================================================
                // Render flake
                // ============================================================

                void RenderFlake(
                    GBuffer<T>& gbuffer,
                    const SnowFlake& flake,
                    T time)
                {
                    //
                    // Kamera-relative Position
                    //

                    Vector3 position =
                        m_right * flake.x
                        + m_up * flake.y
                        + m_forward * flake.z;


                    //
                    // Tiefe in Blickrichtung
                    //

                    const T depth =
                        position.dot(
                            m_forward);


                    if (depth <= (T)0.1)
                        return;


                    //
                    // FOV in Grad -> Radiant
                    //

                    constexpr T PI =
                        static_cast<T>(
                            3.14159265358979323846);

                    const T fovRad =
                        m_fov *
                        (PI / (T)180);


                    const T tanHalfFov =
                        std::tan(
                            fovRad * (T)0.5);


                    if (tanHalfFov <= (T)0)
                        return;


                    //
                    // Projektionsfaktor
                    //

                    const T focalY =
                        ((T)gbuffer.height * (T)0.5) /
                        tanHalfFov;

                    const T focalX =
                        focalY /
                        m_aspect;


                    const T cameraX =
                        position.dot(
                            m_right);

                    const T cameraY =
                        position.dot(
                            m_up);


                    const T sx =
                        ((T)gbuffer.width * (T)0.5) +
                        cameraX *
                        focalX /
                        depth;


                    const T sy =
                        ((T)gbuffer.height * (T)0.5) -
                        cameraY *
                        focalY /
                        depth;


                    //
                    // Größe
                    //

                    T pixelSize =
                        flake.size *
                        focalY /
                        depth;

                    pixelSize =
                        std::max(
                            (T)1,
                            pixelSize);


                    const int ix =
                        static_cast<int>(sx);

                    const int iy =
                        static_cast<int>(sy);

                    const int radius =
                        std::max(
                            0,
                            static_cast<int>(
                                pixelSize * (T)0.5));


                    if (ix + radius < 0 ||
                        ix - radius >= gbuffer.width ||
                        iy + radius < 0 ||
                        iy - radius >= gbuffer.height)
                    {
                        return;
                    }


                    //
                    // Schnee:
                    //
                    // 0xAABBGGRR
                    //

                    constexpr uint32_t snowColor =
                        0xFFFFFFFF;


                    const T radius2 =
                        static_cast<T>(
                            radius * radius);


                    for (int y = -radius;
                        y <= radius;
                        ++y)
                    {
                        const int py =
                            iy + y;

                        if (py < 0 ||
                            py >= gbuffer.height)
                            continue;


                        for (int x = -radius;
                            x <= radius;
                            ++x)
                        {
                            const int px =
                                ix + x;

                            if (px < 0 ||
                                px >= gbuffer.width)
                                continue;


                            const T d2 =
                                static_cast<T>(
                                    x * x +
                                    y * y);


                            if (d2 > radius2)
                                continue;


                            T alpha = (T)1;

                            if (radius > 0)
                            {
                                alpha =
                                    (T)1 -
                                    std::sqrt(d2) /
                                    static_cast<T>(
                                        radius);

                                alpha =
                                    std::clamp(
                                        alpha,
                                        (T)0,
                                        (T)1);
                            }


                            uint32_t& dst =
                                gbuffer.color[
                                    py * gbuffer.width +
                                        px];


                            BlendPixel(
                                dst,
                                snowColor,
                                alpha);
                        }
                    }
                }


                // ============================================================
                // Blend
                // ============================================================

                static void BlendPixel(
                    uint32_t& dst,
                    uint32_t src,
                    T alpha)
                {
                    if (alpha <= (T)0)
                        return;

                    if (alpha >= (T)1)
                    {
                        dst = src;
                        return;
                    }


                    //
                    // 0xAABBGGRR
                    //

                    const int sr =
                        (src >> 0) & 0xff;

                    const int sg =
                        (src >> 8) & 0xff;

                    const int sb =
                        (src >> 16) & 0xff;


                    const int dr =
                        (dst >> 0) & 0xff;

                    const int dg =
                        (dst >> 8) & 0xff;

                    const int db =
                        (dst >> 16) & 0xff;


                    const int r =
                        static_cast<int>(
                            dr +
                            (sr - dr) * alpha);

                    const int g =
                        static_cast<int>(
                            dg +
                            (sg - dg) * alpha);

                    const int b =
                        static_cast<int>(
                            db +
                            (sb - db) * alpha);


                    dst =
                        ((uint32_t)255 << 24) |
                        ((uint32_t)b << 16) |
                        ((uint32_t)g << 8) |
                        ((uint32_t)r);
                }


            private:

                std::vector<SnowFlake>
                    m_flakes;

                size_t m_count = 0;


                T m_speed = (T)8;
                T m_wind = (T)2;
                T m_size = (T)1.5;


                T m_wobbleAmount = (T)0.5;
                T m_wobbleSpeed = (T)1.5;


                T m_fieldWidth = (T)100;
                T m_fieldHeight = (T)60;
                T m_fieldDepth = (T)100;


                Vector3 m_forward =
                    Vector3(
                        (T)0,
                        (T)0,
                        (T)-1);

                Vector3 m_right =
                    Vector3(
                        (T)1,
                        (T)0,
                        (T)0);

                Vector3 m_up =
                    Vector3(
                        (T)0,
                        (T)1,
                        (T)0);


                Vector3 m_cameraDelta =
                    Vector3(
                        (T)0,
                        (T)0,
                        (T)0);


                Vector3 m_worldDown =
                    Vector3(
                        (T)0,
                        (T)-1,
                        (T)0);


                T m_cameraInfluence =
                    (T)0.15;


                T m_fov =
                    (T)90;

                T m_aspect =
                    (T)1;


                T m_lastTime =
                    (T)-1;
            };




        } // namespace postfx
    } // namespace renderer
} // namespace ng

