#pragma once
#include <cstdint>
#include <cmath>
#include <algorithm>
#include "Frustum.h"
#include "Texture.h"


namespace ng::renderer::rasterizer
{
    // ================================================================
    // Depth compare function pointer
    // Signatur: bool(T newZ, T storedZ)
    //   -> true  = Test besteht, Pixel wird geschrieben
    //   -> false = Test schlägt fehl, Pixel wird verworfen
    // ================================================================
    template<class T>
    using DepthFuncPtr = bool(*)(T newZ, T storedZ);

    // ----------------------------------------------------------------
    // Vordefinierte Standardfunktionen (als freie Funktionen, damit
    // sie sich zu Funktionszeigern decayen lassen)
    // ----------------------------------------------------------------
    template<class T> inline bool DepthFunc_Never(T, T) { return false; }
    template<class T> inline bool DepthFunc_Less(T newZ, T storedZ) { return newZ < storedZ; }
    template<class T> inline bool DepthFunc_Equal(T newZ, T storedZ) { return newZ == storedZ; }
    template<class T> inline bool DepthFunc_LEqual(T newZ, T storedZ) { return newZ <= storedZ; }
    template<class T> inline bool DepthFunc_Greater(T newZ, T storedZ) { return newZ > storedZ; }
    template<class T> inline bool DepthFunc_NotEqual(T newZ, T storedZ) { return newZ != storedZ; }
    template<class T> inline bool DepthFunc_GEqual(T newZ, T storedZ) { return newZ >= storedZ; }
    template<class T> inline bool DepthFunc_Always(T, T) { return true; }

    enum DepthFunc : uint32_t
    {
        DEPTHFUNC_NEVER,
        DEPTHFUNC_LESS,
        DEPTHFUNC_EQUAL,
        DEPTHFUNC_LESSEQUAL,
        DEPTHFUNC_GREATER,
        DEPTHFUNC_NOTEQUAL,
        DEPTHFUNC_GREATEREQUAL,
        DEPTHFUNC_ALWWAYS
    };


    template <class T>
    inline DepthFuncPtr<T> SelectDepthFn_Impl(uint32_t func)
    {
        switch (func)
        {
        case DEPTHFUNC_NEVER:
            return DepthFunc_Never<T>;

        case DEPTHFUNC_LESS:
            return DepthFunc_Less<T>;

        case DEPTHFUNC_EQUAL:
            return DepthFunc_Equal<T>;

        case DEPTHFUNC_LESSEQUAL:
            return DepthFunc_LEqual<T>;

        case DEPTHFUNC_GREATER:
            return DepthFunc_Greater<T>;

        case DEPTHFUNC_NOTEQUAL:
            return DepthFunc_NotEqual<T>;

        case DEPTHFUNC_GREATEREQUAL:
            return DepthFunc_GEqual<T>;

        case DEPTHFUNC_ALWWAYS:
            return DepthFunc_Always<T>;
        }

        return DepthFunc_Less<T>;
    }


}



// ---------------------------------------------------------------------
// Erwartete Umgebung (bei dir bereits vorhanden):
//   - math::matrix<N,1,T> mit .x()/.y()/.z()/.w() Zugriff
//   - primitive::SVertex<T>  (screen-space Vertex nach Project())
//   - RGBA(r,g,b,a) -> uint32_t   (wie in PixelBufferRGBA benutzt)
//   - ZBuffer / IndexBuffer / NormalBuffer: gleiche Grundstruktur wie
//     PixelBufferRGBA (Resize/Clear/GetRawBuffer/PutPixel/GetPixel),
//     nur mit float bzw. uint32_t als Elementtyp.
//
// Alle rechnenden Typen (Fragment, DrawState, Gradients, Edge, die
// ColorFn/WriteFn-Signaturen) sind auf T templatisiert - float oder
// double, je nachdem was deine SVertex<T>-Instanz nutzt.
// RenderTarget bleibt bewusst NICHT templatisiert: das sind reine
// Speicherformate (uint32_t Farbe, float Tiefe), unabhaengig von der
// Rechenpraezision.
// ---------------------------------------------------------------------

namespace ng::renderer::rasterizer
{
    // -------------------------------------------------------------
    // RenderTarget: buendelt alle Ausgabebuffer, die der Rasterizer
    // beschreibt. Bewusst als reiner Pointer-Container gehalten -
    // exakt dasselbe Muster wie der postfx::GBuffer, den wir schon
    // fuer das PostFX-System gebaut haben. Wenn du magst, kannst du
    // spaeter beide zusammenlegen (der Rasterizer fuellt denselben
    // GBuffer, den PostFX danach weiterverarbeitet).
    // -------------------------------------------------------------
    template<class T>
    struct RenderTarget
    {
        uint32_t* color = nullptr; // ARGB8888
        T*        depth = nullptr; // 1/w, groesser = naeher an der Kamera
        uint32_t* normal = nullptr; // gepackte Normale (siehe PackNormal)
        uint32_t* index = nullptr; // Objekt-/Primitiv-ID pro Pixel

        int width = 0;
        int height = 0;


        inline size_t Offset(int x, int y) const { return (size_t)y * width + x; }
    };

    // Normale in ein uint32 packen (je Kanal 8 Bit, -1..1 -> 0..255)
    template<class T>
    inline uint32_t PackNormal(T x, T y, T z)
    {
        auto enc = [](T v) -> uint32_t
            {
                v = std::clamp(v, (T)-1, (T)1);
                return (uint32_t)((v * (T)0.5 + (T)0.5) * (T)255);
            };
        return (0xffu << 24) | (enc(x) << 16) | (enc(y) << 8) | enc(z);
    }

    // -------------------------------------------------------------
    // Textur-View: keine eigene Ressourcenverwaltung, nur ein
    // schreibgeschuetzter Blick auf existierende Pixeldaten
    // (z.B. aus deinem _Image / PixelBufferRGBA::GetRawBuffer()).
    // -------------------------------------------------------------
    struct TextureView
    {
        ng::renderer::gfx::Texture texture; // Resource-Handle - billig zu kopieren/halten

        template<class T>
        inline uint32_t SampleNearestLevel(T u, T v, int level) const
        {
            
            
            if (!texture) return 0xffff00ffu; // Magenta = fehlende Textur
            const auto& mip = texture->GetMipLevel(level);
            int w = mip.Width(), h = mip.Height();
            if (w <= 0 || h <= 0) return 0xffff00ffu;
            int x = (int)(u * (T)w) % w;
            int y = (int)(v * (T)h) % h;
            if (x < 0) x += w;
            if (y < 0) y += h;
            return mip.GetRawBuffer()[(size_t)y * w + x];
            
        }
        // Mip-Level aus den Bildschirmraum-Ableitungen von u/v waehlen.
        // rho = Texel-pro-Pixel-Verhaeltnis, bezogen auf die Originalgroesse
        // (Level 0) - die Mip-Kette selbst halbiert sich pro Stufe, das
        // muss hier nicht nochmal nachgerechnet werden.
        template<class T>
        inline int SelectMipLevel(T dudx, T dvdx, T dudy, T dvdy) const
        {
            
            
            if (!texture) return 0;
            int w = texture->Width(), h = texture->Height();
            T ddx = dudx * (T)w, ddy = dvdx * (T)h;
            T edx = dudy * (T)w, edy = dvdy * (T)h;
            T rho = std::sqrt(std::max(ddx * ddx + ddy * ddy, edx * edx + edy * edy));
            if (rho <= (T)1) return 0;
            int level = (int)std::log2((double)rho);
            return std::clamp(level, 0, (int)texture->GetMipLevels());
            
        }

        // Rueckwaertskompatibel: samplet immer Level 0 (Original).
      
   /*     template<class T>
        inline uint32_t SampleNearest(T u, T v) const
        {
            return SampleNearestLevel(u, v, 0);
        }
     */   
    };

    
    
    

    template<class T>
    struct Fragment
    {
        int x, y;

        T invW;

        T u, v;
        T fxu, fxv;

        // Ableitungen der perspektivisch korrigierten UVs
        T dudx, dvdx;
        T dudy, dvdy;

        T r, g, b, a;

        T nx, ny, nz;
    };
    // -------------------------------------------------------------
    // DrawState: alles, was fuer einen kompletten Draw-Call konstant
    // ist (Material/Flags). ColorFn/WriteFn werden einmal pro Call
    // ausgewaehlt, nicht pro Pixel.
    // -------------------------------------------------------------
    enum RasterFlags : uint32_t
    {
        RASTER_NONE = 0,
        RASTER_VERTEXCOLOR = 1u << 0,
        RASTER_TEXTURE = 1u << 1,
        RASTER_LIGHTMAP = 1u << 2, // nur zusammen mit RASTER_TEXTURE sinnvoll
        RASTER_TRANSPARENT = 1u << 3,
        RASTER_DEPTH_TEST = 1u << 4,
        RASTER_DEPTH_WRITE = 1u << 5,
        RASTER_WRITE_NORMAL = 1u << 6,
        RASTER_WRITE_INDEX = 1u << 7,
        RASTER_WOBBLE = 1 << 1u << 8 // Nur beim spanrenderer
    };

    
    template<class T>
    struct DrawState
    {

        // ... bestehende Felder (z.B. Blend, Texture, etc.) ...

        // ------------------------------------------------------------
        // Depth
        // ------------------------------------------------------------
        bool      depthTestEnabled = true;
        bool      depthWriteEnabled = true;

        // ------------------------------------------------------------
        // Clipping (Frustum-Ebenen einzeln togglebar)
        // ------------------------------------------------------------
          
        uint32_t flags = RASTER_DEPTH_TEST | RASTER_DEPTH_WRITE;
        uint32_t detph = DEPTHFUNC_LESS;
        uint32_t flatColor = 0xffffffffu; // genutzt wenn weder VERTEXCOLOR noch TEXTURE
        uint32_t objectIndex = 0;
        T materialAlpha = (T)1;            // zusaetzlich zur Vertex-/Textur-Alpha

        const TextureView* diffuseTex = nullptr;
        const TextureView* lightmapTex = nullptr;

        // ColorFn/WriteFn werden von SelectDrawFunctions() befuellt -
        // koennen aber auch manuell ueberschrieben werden (eigene Shader).
        uint32_t(*colorFn)(const Fragment<T>&, const DrawState<T>&) = nullptr;
        void     (*writeFn)(RenderTarget<T>&, size_t offset, uint32_t color, const DrawState<T>&) = nullptr;
        DepthFuncPtr<T> depthFn = nullptr;

        T dU_dx = 0;
        T dU_dy = 0;
        T dV_dx = 0;
        T dV_dy = 0;


    };

    // ===============================================================
    // ColorFn-Implementierungen
    // ===============================================================
    template<class T>
    inline uint32_t ColorFlat(const Fragment<T>&, const DrawState<T>& st)
    {
        return st.flatColor;
    }

    template<class T>
    inline uint32_t ColorVertex(const Fragment<T>& f, const DrawState<T>&)
    {
        
        uint8_t r = (uint8_t)std::clamp(f.r * (T)255, (T)0, (T)255);
        uint8_t g = (uint8_t)std::clamp(f.g * (T)255, (T)0, (T)255);
        uint8_t b = (uint8_t)std::clamp(f.b * (T)255, (T)0, (T)255);
        uint8_t a = (uint8_t)std::clamp(f.a * (T)255, (T)0, (T)255);
       
        return  RGBA(r, g, b, 255);//(uint32_t(a) << 24) | (uint32_t(r) << 16) | (uint32_t(g) << 8) | uint32_t(b);
    }
  
    template<class T>
    inline T PerspectiveDerivative(
        T value,
        T invW,
        T dValue,
        T dInvW)
    {
        T invW2 = invW * invW;

        return (dValue * invW - value * dInvW) * invW2;
    }

    template<class T>
        inline uint32_t ColorTexture(const Fragment<T>& f, const DrawState<T>& st)
    {
        if (!st.diffuseTex)
            return 0xffff00ffu;

        int mip = st.diffuseTex->SelectMipLevel(f.dudx, f.dvdx, f.dudy, f.dvdy);
        uint32_t tex = st.diffuseTex->SampleNearestLevel(f.u, f.v, mip);

        uint32_t tr = RGBToRed(tex);
        uint32_t tg = RGBToGreen(tex);
        uint32_t tb = RGBToBlue(tex);
        uint32_t ta = RGBToAlpha(tex);

        // NEU: nur modulieren, wenn explizit gewuenscht. Ohne
        // RASTER_VERTEXCOLOR soll die reine Texturfarbe durchkommen -
        // vorher wurde IMMER mit f.r/f.g/f.b multipliziert, unabhaengig
        // vom Flag, was zu einem Farbstich durch die (fuer Debug-Zwecke
        // gesetzte) normalenbasierte Vertexfarbe fuehrte.
        if (st.flags & RASTER_VERTEXCOLOR)
        {
            uint32_t r = (uint32_t)(((T)tr / (T)255) * f.r * (T)255);
            uint32_t g = (uint32_t)(((T)tg / (T)255) * f.g * (T)255);
            uint32_t b = (uint32_t)(((T)tb / (T)255) * f.b * (T)255);
            return RGBA(r, g, b, ta);
        }

        return RGBA(tr, tg, tb, ta);
    }

    template<class T>
    inline uint32_t ColorTextureLightmap(const Fragment<T>& f, const DrawState<T>& st)
    {
        uint32_t base = ColorTexture(f, st);
        auto mipl = st.lightmapTex->SelectMipLevel(f.dudx, f.dvdx, f.dudy, f.dvdy);
        uint32_t lm = st.lightmapTex->SampleNearestLevel(f.fxu, f.fxv, mipl);
        //uint32_t lm = st.lightmapTex->SampleNearest(f.fxu, f.fxv);
        
        uint32_t lr = (lm >> 16) & 0xff, lg = (lm >> 8) & 0xff, lb = lm & 0xff;
        uint32_t br = (base >> 16) & 0xff, bg = (base >> 8) & 0xff, bb = base & 0xff, ba = (base >> 24) & 0xff;
        uint32_t r = std::min(255u, (br * lr) / 255u);
        uint32_t g = std::min(255u, (bg * lg) / 255u);
        uint32_t b = std::min(255u, (bb * lb) / 255u);
        return (ba << 24) | (r << 16) | (g << 8) | b;
    }

    template<class T>
    inline void SelectColorFn_Impl(uint32_t flags, uint32_t(**out)(const Fragment<T>&, const DrawState<T>&))
    {
        if (flags & RASTER_TEXTURE)
        {
            
            *out = (flags & RASTER_LIGHTMAP) ? ColorTextureLightmap<T> : ColorTexture<T>;
        }
        else if (flags & RASTER_VERTEXCOLOR)
        {
            
            *out = ColorVertex<T>;
        }
        else
        {
            
            *out = ColorFlat<T>;
        }
    
    }

    // ===============================================================
    // WriteFn-Implementierungen
    // ===============================================================
    template<class T>
    inline void WriteOpaque(RenderTarget<T>& target, size_t offset, uint32_t color, const DrawState<T>&)
    {
        target.color[offset] = color;
    }

    template<class T>
    inline void WriteBlend(RenderTarget<T>& target, size_t offset, uint32_t color, const DrawState<T>& st)
    {
        uint32_t srcA = (uint32_t)std::clamp(((color >> 24) & 0xff) * st.materialAlpha, (T)0, (T)255);
        if (srcA == 0) return;
        if (srcA >= 255) { target.color[offset] = color; return; }

        uint32_t dst = target.color[offset];
        uint32_t sr = (color >> 16) & 0xff, sg = (color >> 8) & 0xff, sb = color & 0xff;
        uint32_t dr = (dst >> 16) & 0xff, dg = (dst >> 8) & 0xff, db = dst & 0xff;

        uint32_t r = (sr * srcA + dr * (255 - srcA)) / 255;
        uint32_t g = (sg * srcA + dg * (255 - srcA)) / 255;
        uint32_t b = (sb * srcA + db * (255 - srcA)) / 255;
        target.color[offset] = (0xffu << 24) | (r << 16) | (g << 8) | b;
    }

    template<class T>
    inline void SelectWriteFn_Impl(uint32_t flags, void (**out)(RenderTarget<T>&, size_t, uint32_t, const DrawState<T>&))
    {
        *out = (flags & RASTER_TRANSPARENT) ? WriteBlend<T> : WriteOpaque<T>;
    }

    // Einmal pro Draw-Call aufrufen, bevor gerastert wird.
    template<class T>
    inline void SelectDrawFunctions(ng::renderer::rasterizer::DrawState<T>& st)
    {
        
 
        
        st.depthFn = SelectDepthFn_Impl<T>(st.detph);
        SelectColorFn_Impl<T>(st.flags, &st.colorFn);
        SelectWriteFn_Impl<T>(st.flags, &st.writeFn);
    }

    // -------------------------------------------------------------
    // Ein Pixel durch die komplette Pipeline schicken: Tiefentest,
    // ColorFn, WriteFn, optional Normal-/Index-Buffer.
    // -------------------------------------------------------------
    template<class T>
    inline void ShadePixel(RenderTarget<T>& target, const DrawState<T>& st, const Fragment<T>& f)
    {
        
        if ((unsigned)f.x >= (unsigned)target.width || (unsigned)f.y >= (unsigned)target.height) return;
        size_t offset = target.Offset(f.x, f.y);

        
        if (st.flags & RASTER_DEPTH_TEST)
        {
            if (st.depthFn(f.invW, target.depth[offset])) return;
            //  if ((float)f.invW <= target.depth[offset]) return; // schon etwas Naeheres da
        }

        uint32_t color = st.colorFn(f, st);
        st.writeFn(target, offset, color, st);

        if (st.flags & RASTER_DEPTH_WRITE)
            target.depth[offset] = (float)f.invW;
        if (st.flags & RASTER_WRITE_NORMAL)
            target.normal[offset] = PackNormal<T>(f.nx, f.ny, f.nz);
        if (st.flags & RASTER_WRITE_INDEX)
            target.index[offset] = st.objectIndex;
    }

    // ===============================================================
    // Line3D
    // ===============================================================
    // Einfache DDA-Linie im Screenspace, interpoliert alle Attribute
    // linear ueber die Anzahl Schritte. Fuer Wireframe/Debug-Zwecke -
    // nutzt dieselbe Shading-Pipeline wie die Dreiecke.
    template<class T, class SVertexT>
    inline void DrawLine3D(RenderTarget<T>& target, DrawState<T>& st, const SVertexT& v0, const SVertexT& v1)
    {
        SelectDrawFunctions(st);
        
       

        T dx = v1.x() - v0.x();
        T dy = v1.y() - v0.y();
        int steps = (int)std::max(std::abs(dx), std::abs(dy));
        if (steps <= 0)
        {
            Fragment<T> f{ (int)v0.x(), (int)v0.y(), v0.w(),
                v0.u(), v0.v(), v0.fxu(), v0.fxv(),
                v0.color().x(), v0.color().y(), v0.color().z(), v0.alpha(),
                v0.normal().x(), v0.normal().y(), v0.normal().z() };



            ShadePixel(target, st, f);
            return;
        }

        T invSteps = (T)1 / (T)steps;
        for (int i = 0; i <= steps; i++)
        {
            T t = i * invSteps;
            Fragment<T> f{};

            f.x = (int)v0.x();
            f.y = (int)v0.y();

            f.invW = v0.w();

            f.u = v0.u();
            f.v = v0.v();
            f.fxu = v0.fxu();
            f.fxv = v0.fxv();

            // Bei einer Linie verwenden wir kein Mipmapping.
            // Daher Derivate auf 0 setzen.
            f.dudx = 0;
            f.dvdx = 0;
            f.dudy = 0;
            f.dvdy = 0;

            f.r = v0.color().x();
            f.g = v0.color().y();
            f.b = v0.color().z();
            f.a = v0.alpha();

            f.nx = v0.normal().x();
            f.ny = v0.normal().y();
            f.nz = v0.normal().z();
            
            


            ShadePixel(target, st, f);
        }
    }

    // ===============================================================
    // Fast Triangle (Gradienten-Rasterizer)
    // ===============================================================
    // Alle dA/dx, dA/dy einmal pro Dreieck berechnen (klassische
    // Technik nach Chris Hecker / Fabian Giesen). Danach kostet jedes
    // Pixel nur noch Additionen - keine Multiplikation, keine Division
    // (bis auf die eine, unvermeidbare fuer perspektivisch-korrektes UV).
    template<class T>
    struct Gradients
    {
        T dInvW_dx, dInvW_dy;
        T dU_dx, dU_dy, dV_dx, dV_dy;
        T dFxu_dx, dFxu_dy, dFxv_dx, dFxv_dy;
        T dR_dx, dR_dy, dG_dx, dG_dy, dB_dx, dB_dy, dA_dx, dA_dy;
        T dNx_dx, dNx_dy, dNy_dx, dNy_dy, dNz_dx, dNz_dy;

        template<class SVertexT>
        Gradients(const SVertexT& v0, const SVertexT& v1, const SVertexT& v2)
        {
            T denom = (v1.x() - v2.x()) * (v0.y() - v2.y()) - (v0.x() - v2.x()) * (v1.y() - v2.y());
            if (std::abs(denom) < (T)1e-8) denom = (T)1e-8; // entartetes Dreieck absichern
            T oneOverDx = (T)1 / denom;
            T oneOverDy = -oneOverDx;

            auto grad = [&](T a0, T a1, T a2, T& outDx, T& outDy)
                {
                    outDx = ((a1 - a2) * (v0.y() - v2.y()) - (a0 - a2) * (v1.y() - v2.y())) * oneOverDx;
                    outDy = ((a1 - a2) * (v0.x() - v2.x()) - (a0 - a2) * (v1.x() - v2.x())) * oneOverDy;
                };

            grad(v0.w(), v1.w(), v2.w(), dInvW_dx, dInvW_dy);
            grad(v0.u(), v1.u(), v2.u(), dU_dx, dU_dy);
            grad(v0.v(), v1.v(), v2.v(), dV_dx, dV_dy);
            grad(v0.fxu(), v1.fxu(), v2.fxu(), dFxu_dx, dFxu_dy);
            grad(v0.fxv(), v1.fxv(), v2.fxv(), dFxv_dx, dFxv_dy);
            grad(v0.color().x(), v1.color().x(), v2.color().x(), dR_dx, dR_dy);
            grad(v0.color().y(), v1.color().y(), v2.color().y(), dG_dx, dG_dy);
            grad(v0.color().z(), v1.color().z(), v2.color().z(), dB_dx, dB_dy);
            grad(v0.alpha(), v1.alpha(), v2.alpha(), dA_dx, dA_dy);
            grad(v0.normal().x(), v1.normal().x(), v2.normal().x(), dNx_dx, dNx_dy);
            grad(v0.normal().y(), v1.normal().y(), v2.normal().y(), dNy_dx, dNy_dy);
            grad(v0.normal().z(), v1.normal().z(), v2.normal().z(), dNz_dx, dNz_dy);
        }
    };

    
    template<class T>
    struct Edge
    {
        T x, xStep;
        int yStart, yEnd;

        T invW, invW_step;
        T u, u_step, v, v_step;
        T fxu, fxu_step, fxv, fxv_step;
        T r, r_step, g, g_step, b, b_step, a, a_step;
        T nx, nx_step, ny, ny_step, nz, nz_step;

        template<class SVertexT>
        Edge(
            const SVertexT& top,
            const SVertexT& bottom,
            const Gradients<T>& grad)
        {
            yStart = (int)std::ceil(top.y() - (T)0.5);
            yEnd = (int)std::ceil(bottom.y() - (T)0.5);

            T yDist = bottom.y() - top.y();
            T xDist = bottom.x() - top.x();

            if (std::abs(yDist) < (T)1e-6)
                yDist = (T)1e-6;

            xStep = xDist / yDist;

            T yPreStep = (T)yStart + (T)0.5 - top.y();

            x = top.x() + yPreStep * xStep;

            T xPreStep = x - top.x();

            auto init =
                [&](T topVal,
                    T dx,
                    T dy,
                    T& val,
                    T& step)
                {
                    val = topVal
                        + xPreStep * dx
                        + yPreStep * dy;

                    step = xStep * dx + dy;
                };

            init(top.w(),
                grad.dInvW_dx,
                grad.dInvW_dy,
                invW,
                invW_step);

            init(top.u(),
                grad.dU_dx,
                grad.dU_dy,
                u,
                u_step);

            init(top.v(),
                grad.dV_dx,
                grad.dV_dy,
                v,
                v_step);

            init(top.fxu(),
                grad.dFxu_dx,
                grad.dFxu_dy,
                fxu,
                fxu_step);

            init(top.fxv(),
                grad.dFxv_dx,
                grad.dFxv_dy,
                fxv,
                fxv_step);

            init(top.color().x(),
                grad.dR_dx,
                grad.dR_dy,
                r,
                r_step);

            init(top.color().y(),
                grad.dG_dx,
                grad.dG_dy,
                g,
                g_step);

            init(top.color().z(),
                grad.dB_dx,
                grad.dB_dy,
                b,
                b_step);

            init(top.alpha(),
                grad.dA_dx,
                grad.dA_dy,
                a,
                a_step);

            init(top.normal().x(),
                grad.dNx_dx,
                grad.dNx_dy,
                nx,
                nx_step);

            init(top.normal().y(),
                grad.dNy_dx,
                grad.dNy_dy,
                ny,
                ny_step);

            init(top.normal().z(),
                grad.dNz_dx,
                grad.dNz_dy,
                nz,
                nz_step);
        }

        inline void Step()
        {
            x += xStep;

            invW += invW_step;

            u += u_step;
            v += v_step;

            fxu += fxu_step;
            fxv += fxv_step;

            r += r_step;
            g += g_step;
            b += b_step;
            a += a_step;

            nx += nx_step;
            ny += ny_step;
            nz += nz_step;
        }
    };

    template<class T>
    inline void DrawSpan(RenderTarget<T>& target, const DrawState<T>& st, const Gradients<T>& grad, int y, const Edge<T>& left, const Edge<T>& right)
    {
        int xStart = (int)std::ceil(left.x - (T)0.5);
        int xEnd = (int)std::ceil(right.x - (T)0.5);
        if (xStart >= xEnd) return;

        T xPreStep = ((T)xStart + (T)0.5) - left.x;

        Fragment<T> f;
        f.y = y;
        f.invW = left.invW + xPreStep * grad.dInvW_dx;
        f.u = left.u + xPreStep * grad.dU_dx;
        f.v = left.v + xPreStep * grad.dV_dx;
        f.fxu = left.fxu + xPreStep * grad.dFxu_dx;
        f.fxv = left.fxv + xPreStep * grad.dFxv_dx;
        f.r = left.r + xPreStep * grad.dR_dx;
        f.g = left.g + xPreStep * grad.dG_dx;
        f.b = left.b + xPreStep * grad.dB_dx;
        f.a = left.a + xPreStep * grad.dA_dx;
        f.nx = left.nx + xPreStep * grad.dNx_dx;
        f.ny = left.ny + xPreStep * grad.dNy_dx;
        f.nz = left.nz + xPreStep * grad.dNz_dx;

        // NEU: perspektivisch korrekte Bildschirmraum-Ableitungen von u/v,
    // fuer die Mip-Level-Auswahl in ColorTexture(). f.u/f.v/f.invW sind
    // hier noch die ROHEN (nicht durch invW geteilten) Werte an diesem
    // Pixel - genau das braucht PerspectiveDerivative als Eingabe.
        
        f.dudx = PerspectiveDerivative(f.u, f.invW, grad.dU_dx, grad.dInvW_dx);
        f.dvdx = PerspectiveDerivative(f.v, f.invW, grad.dV_dx, grad.dInvW_dx);
        f.dudy = PerspectiveDerivative(f.u, f.invW, grad.dU_dy, grad.dInvW_dy);
        f.dvdy = PerspectiveDerivative(f.v, f.invW, grad.dV_dy, grad.dInvW_dy);

        for (int x = xStart; x < xEnd; x++)
        {
            f.x = x;

            // Perspektivisch korrektes UV: die eine Division pro Pixel,
            // die sich nicht wegoptimieren laesst.
            T w = (T)1 / f.invW;
            Fragment<T> shaded = f;
            shaded.u = f.u * w;
            shaded.v = f.v * w;
            shaded.fxu = f.fxu * w;
            shaded.fxv = f.fxv * w;

            ShadePixel(target, st, shaded);

            f.invW += grad.dInvW_dx;
            f.u += grad.dU_dx; f.v += grad.dV_dx;
            f.fxu += grad.dFxu_dx; f.fxv += grad.dFxv_dx;
            f.r += grad.dR_dx; f.g += grad.dG_dx; f.b += grad.dB_dx; f.a += grad.dA_dx;
            f.nx += grad.dNx_dx; f.ny += grad.dNy_dx; f.nz += grad.dNz_dx;
        }
    }

    // Haupteinstieg: Dreieck aus bereits projizierten SVertex (nach
    // Project()) rastern. Sortiert nach Y, teilt in Ober-/Unterhaelfte,
    // laeuft scanline-weise ueber DrawSpan().
    template<class T, class SVertexT>
    inline void DrawTriangleFast(RenderTarget<T>& target, DrawState<T>& st, SVertexT v0, SVertexT v1, SVertexT v2)
    {

        SelectDrawFunctions(st);

       
        if (v0.y() > v1.y()) std::swap(v0, v1);
        if (v1.y() > v2.y()) std::swap(v1, v2);
        if (v0.y() > v1.y()) std::swap(v0, v1);

        if (std::abs(v0.y() - v2.y()) < (T)1e-6) return;
        Gradients<T> grad(v0, v1, v2);

        // Handedness bestimmen: liegt v1 links oder rechts der langen Kante v0->v2?
        T area = (v2.x() - v0.x()) * (v1.y() - v0.y()) - (v2.y() - v0.y()) * (v1.x() - v0.x());
        bool middleIsLeft = area >= (T)0;

        Edge<T> longEdge(v0, v2, grad);

        if (v0.y() < v1.y())
        {

            Edge<T> shortEdge(v0, v1, grad);
            for (int y = shortEdge.yStart; y < shortEdge.yEnd; y++)
            {
                if (middleIsLeft) DrawSpan(target, st, grad, y, shortEdge, longEdge);
                else               DrawSpan(target, st, grad, y, longEdge, shortEdge);
                shortEdge.Step();
                longEdge.Step();
            }
        }
        if (v1.y() < v2.y())
        {

            Edge<T> shortEdge(v1, v2, grad);

            for (int y = shortEdge.yStart; y < shortEdge.yEnd; y++)
            {
                if (middleIsLeft) DrawSpan(target, st, grad, y, shortEdge, longEdge);
                else               DrawSpan(target, st, grad, y, longEdge, shortEdge);
                shortEdge.Step();
                longEdge.Step();
            }
        }
    }

} // namespace ng::renderer::raster