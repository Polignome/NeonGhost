#pragma once
#include <memory>
#include <thread>
#include <algorithm>

#include "Maskedocclusionbuffer.h"
#include "TileRasterizer.h"   // NEU: TileGrid, SubmitTriangle, RenderFrameThreaded, RenderFrameSingleThreaded
namespace ng::renderer::rasterizer {
    
    
    template<class T>
    class Rasterizer3D
    {
    public:
        using Target = RenderTarget<T>;
        using DrawState = DrawState<T>;
        using SVertex = ScreenVertex<T>;
        using Vertex = ng::geometry::Vertex<T>;
        using ColorBuffer = ng::renderer::buffers::PixelBufferRGBA;
        using NormalBuffer = ng::renderer::buffers::PixelBufferRGBA;
        using IndexBuffer = ng::renderer::buffers::PixelBufferRGBA;
        using DepthBuffer = ng::renderer::buffers::HierarchicalZBuffer<T>;
        using MOCBuffer = ng::renderer::occlusion::MaskedOcclusionBuffer<float>;
    public:
        // ============================================================
        // Construction
        // ============================================================
        Rasterizer3D(
            int width = 0,
            int height = 0)
        {
            m_key = reinterpret_cast<uint64_t>(this);
            m_state.detph = DEPTHFUNC_LESS;
            m_state.flatColor = RGB(255, 255, 0);
            m_state.flags = RASTER_DEPTH_TEST | RASTER_DEPTH_WRITE;
            ResetState();
            Resize(width, height);
        }
        ~Rasterizer3D()
        {
            ReleaseBuffers();
        }
        inline T ScreenCenterX() {
            return m_center_x;
        }
        inline T ScreenCenterY() {
            return m_center_y;
        }
    public:
        // ============================================================
        // Resize
        // ============================================================
        void Resize(
            int width,
            int height)
        {
            if (width == m_width &&
                height == m_height)
            {
                return;
            }
            ReleaseBuffers();
            m_width = width;
            m_height = height;
            m_center_x = (T)0.0;
            m_center_y = (T)0.0;
            if (m_width <= 0 ||
                m_height <= 0)
            {
                return;
            }
            m_center_x = (T)m_width / (T)2;
            m_center_y = (T)m_height / (T)2;
            // --------------------------------------------------------
            // Allocate buffers
            // --------------------------------------------------------
            m_depth.Resize(
                m_width,
                m_height,
                m_mipmaplevel);
            m_color.Resize(
                m_width,
                m_height);
            m_normal.Resize(
                m_width,
                m_height);
            m_index.Resize(
                m_width,
                m_height);
            
            
                //	m_spanRenderer->Resize(m_width, m_height);  
                    // --------------------------------------------------------
                    // Lock buffers
                    // --------------------------------------------------------
                m_depth.Lock(m_key);
            m_color.Lock(m_key);
            m_normal.Lock(m_key);
            m_index.Lock(m_key);
            UpdateRenderTarget();
            // NEU: SpanRenderer braucht dieselbe Aufloesung. Wird nur
            // (re-)erzeugt, wenn der Span-Pfad ueberhaupt aktiv ist -
            // kostet also nichts, solange niemand SetSpanRendererEnabled(true)
            // aufruft.
            
            m_moc.Resize(m_width, m_height);
            // NEU: Tile-Grid fuer das Bin -> Rasterize Multithreading
            // mit resizen. Kostet nichts Nennenswertes, solange
            // m_use_tiled == false - die Bin-Listen bleiben einfach leer.
            m_tileGrid.Resize(m_width, m_height);
        }
        int Width() const
        {
            return m_width;
        }
        int Height() const
        {
            return m_height;
        }
    private:
        // ============================================================
        // Buffer handling
        // ============================================================
        void ReleaseBuffers()
        {
            if (m_depth.IsLocked())
                m_depth.UnLock(m_key);
            if (m_color.IsLocked())
                m_color.UnLock(m_key);
            if (m_normal.IsLocked())
                m_normal.UnLock(m_key);
            if (m_index.IsLocked())
                m_index.UnLock(m_key);
        }
        void UpdateRenderTarget()
        {
            m_target.color =
                m_color.GetRawBuffer();
            m_target.normal =
                m_normal.GetRawBuffer();
            m_target.index =
                m_index.GetRawBuffer();
            m_target.depth =
                m_depth.GetRawBuffer();
            m_target.width =
                m_width;
            m_target.height =
                m_height;
        }
    public:
        // ============================================================
        // Frame
        // ============================================================
        bool IsMocUsed() const
        {
            return m_use_moc;
        }
        void Begin()
        {
            UpdateRenderTarget();
            if (m_use_hzb)
            {
                m_depth.Clear();
            }
            else m_depth.Clear(0, false);
           
            // NEU: Bin-Listen des Tile-Grids fuer den neuen Frame leeren.
            // Kapazitaet der einzelnen Bins bleibt erhalten (siehe
            // TileGrid::Clear() in TileRasterizer.h) - ab dem zweiten
            // Frame entstehen dadurch kaum noch Allokationen.
            if (m_use_tiled)
                m_tileGrid.Clear();
        }
        void End()
        {
            // NEU: Rasterize-Phase des Tile-based Renderings. Alle
            // waehrend des Frames per DrawTriangle()/DrawPolygon()
            // gesammelten Dreiecke sind jetzt vollstaendig gebinnt -
            // erst jetzt darf tatsaechlich gerastert werden (siehe
            // Begruendung im Chat: waehrend des Objekt-Traversierens
            // steht die Bin-Liste einer Kachel noch nicht fest).
            if (m_use_tiled)
            {
                UpdateRenderTarget();
                if (m_tiled_single_threaded_debug)
                    RenderFrameSingleThreaded(m_target, m_tileGrid);
                else
                    RenderFrameThreaded(m_target, m_tileGrid, m_thread_count);
            }
        }
    public:
        // ============================================================
        // Draw State
        // ============================================================
        DrawState& State()
        {
            return m_state;
        }
        const DrawState& State() const
        {
            return m_state;
        }
        void SetState(
            const DrawState& state)
        {
            m_state = state;
        }
        void ResetState()
        {
            m_state = DrawState{};
        }
        // ------------------------------------------------------------
        // State Stack
        // ------------------------------------------------------------
        void PushState()
        {
            m_stateStack.push_back(m_state);
        }
        void PopState()
        {
            assert(!m_stateStack.empty());
            if (m_stateStack.empty())
                return;
            m_state =
                m_stateStack.back();
            m_stateStack.pop_back();
        }
        void ClearStateStack()
        {
            m_stateStack.clear();
        }
    public:
        // ============================================================
        // NEU: Tile-based Threaded Rendering
        //
        // Wenn aktiviert, werden DrawTriangle()/DrawPolygon() nicht
        // mehr sofort gerastert, sondern nur gegen das Tile-Grid
        // gebinnt (siehe TileRasterizer.h). Die eigentliche Rasterung
        // passiert gesammelt in End(), parallel ueber mehrere Threads.
        //
        // Default: aus - bestehender Code, der Rasterizer3D nutzt,
        // ohne diese Methode aufzurufen, verhaelt sich unveraendert.
        //
        // m_tiled_single_threaded_debug: erzwingt in End() den
        // single-threaded Referenzpfad (RenderFrameSingleThreaded)
        // statt RenderFrameThreaded - zum Verifizieren des Binnings/
        // Tile-Clippings gegen den Immediate-Pfad, BEVOR Threading
        // mit ins Spiel kommt (siehe Testreihenfolge im Chat).
        // ============================================================
        void SetTiledRenderingEnabled(bool enabled)
        {
            m_use_tiled = enabled;
        }
        bool TiledRenderingEnabled() const
        {
            return m_use_tiled;
        }
        void SetThreadCount(int count)
        {
            m_thread_count = std::max(1, count);
        }
        int ThreadCount() const
        {
            return m_thread_count;
        }
        void SetTiledSingleThreadedDebug(bool enabled)
        {
            m_tiled_single_threaded_debug = enabled;
        }
        bool TiledSingleThreadedDebug() const
        {
            return m_tiled_single_threaded_debug;
        }
    public:
        // ============================================================
        // Triangle
        // ============================================================
        inline void DrawTriangle(
            const SVertex& v0,
            const SVertex& v1,
            const SVertex& v2)
        {
            UpdateRenderTarget();
            if (m_use_tiled)
            {
                // SelectDrawFunctions() passiert normalerweise als
                // erster Schritt IN DrawTriangleFast() (siehe
                // Rasterizer.h). Da SubmitTriangle() nicht rastert,
                // sondern nur bint, muessen colorFn/writeFn/depthFn
                // hier explizit VOR dem Submit gesetzt werden - sonst
                // sind sie beim spaeteren Rastern in End() noch
                // nullptr.
                SelectDrawFunctions(m_state);
                SubmitTriangle(m_tileGrid, m_state, v0, v1, v2);
                return;
            }
            DrawTriangleFast(
                m_target,
                m_state,
                v0,
                v1,
                v2);
        }
        inline void DrawLine(
            const SVertex& v0,
            const SVertex& v1)
        {
            UpdateRenderTarget();
            DrawLine3D(
                m_target,
                m_state,
                v0,
                v1
            );
        }
        inline void DrawTriangleW(
            const SVertex& v0,
            const SVertex& v1,
            const SVertex& v2)
        {
            UpdateRenderTarget();
            DrawLine3D(m_target, m_state, v0, v1);
            DrawLine3D(m_target, m_state, v1, v2);
            DrawLine3D(m_target, m_state, v2, v0);
        }
        inline void DrawPolygonW(const std::vector< SVertex>& verts)
        {
            UpdateRenderTarget();
            for (size_t i = 0; i < verts.size(); i++) {
                SVertex v0 = verts[i];
                SVertex v1 = verts[(i + 1) % verts.size()];
                DrawLine3D(m_target, m_state, verts[i], verts[(i + 1) % verts.size()]);
            }
        }
       

        inline void DrawPolygon(const std::vector< SVertex>& verts)
        {
            UpdateRenderTarget();
            if (verts.empty()) return;
            if (verts.size() < 3) return;

            if (m_use_tiled)
            {
                // Einmal pro Polygon, nicht pro Dreieck - dieselbe
                // Semantik wie zuvor DrawTriangleFast() intern pro
                // Dreieck aufgerufen hat, hier bewusst vorgezogen,
                // weil ALLE Fan-Dreiecke desselben Polygons denselben
                // DrawState benutzen.
                SelectDrawFunctions(m_state);
               
                for (size_t i = 1; i < verts.size() - 1; ++i)
                {
                    SubmitTriangle(m_tileGrid, m_state, verts[0], verts[i], verts[i + 1]);
                }
                return;
            }

            for (size_t i = 1; i < verts.size() - 1; ++i) {
                DrawTriangleFast(
                    m_target,
                    m_state,
                    verts[0],
                    verts[i],
                    verts[i + 1]);
            }
        }
        template <class ProjectFn>
        inline void DrawOcclusionPolygon(const std::vector<Vertex>& verts, ProjectFn project)
        {
            for (size_t i = 1; i < verts.size() - 1; ++i) {
                const auto& v0 = verts[0].world();
                const auto& v1 = verts[i].world();
                const auto& v2 = verts[i + 1].world();
                m_moc.RenderOccluder(v0, v1, v2, project);
            }
        }

        inline void DrawOcclusionPolygon(const std::vector<SVertex>& verts)
        {
            for (size_t i = 1; i < verts.size() - 1; ++i) {
                const auto& v0 = verts[0];
                const auto& v1 = verts[i];
                const auto& v2 = verts[i + 1];
                math::Matrix<3, 1, T> p0(v0.x(), v0.y(), v0.w());
                math::Matrix<3, 1, T> p1(v1.x(), v1.y(), v1.w());
                math::Matrix<3, 1, T > p2(v2.x(), v2.y(), v2.w());
                m_moc.RenderOccluder(p0, p1, p2);
            }
        }

      
    public:
    public:
        // ============================================================
        // HZB
        // ============================================================
        void SetHZBEnabled(
            bool enabled)
        {
            m_use_hzb = enabled;
        }
        bool HZBEnabled() const
        {
            return m_use_hzb;
        }
        void SetHZBMipLevel(
            int level)
        {
            m_mipmaplevel = level;
            if (m_width > 0 &&
                m_height > 0)
            {
                Resize(
                    m_width,
                    m_height);
            }
        }
        int HZBMipLevel() const
        {
            return m_mipmaplevel;
        }
        // NEU: Baut die HZB-Mip-Pyramide aus dem AKTUELLEN Tiefenbild neu,
        // OHNE den Depth-Buffer zu loeschen. Fuer den Aufruf nach
        // FlushSpanRenderer() gedacht (frische HZB fuer die dynamischen
        // Objekte im selben Frame - siehe Ablauf-Kommentar oben).
        //
        // Voraussetzung: HierarchicalZBuffer<T> braucht dafuer eine
        // RefreshHZB()-Methode (ruft intern nur BuildMipMaps() auf, ohne
        // Clear) - siehe vorheriger Vorschlag dazu.
        void RefreshHZB()
        {
            m_depth.RefreshHZB();
        }
    public:
        // ============================================================
        // Buffers
        // ============================================================
        ColorBuffer GetColorBuffer()
        {
            return m_color;
        }
        NormalBuffer GetNormalBuffer()
        {
            return m_normal;
        }
        IndexBuffer GetIndexBuffer()
        {
            return m_index;
        }
        DepthBuffer GetDepthBuffer()
        {
            return m_depth;
        }
        DepthBuffer& GetDepthBufferRef()
        {
            return m_depth;
        }
        inline MOCBuffer& GetMOCBufferRef()
        {
            return m_moc;
        }
    private:
        T m_center_y = 0;
        T m_center_x = 0;
        // ============================================================
        // Render Target
        // ============================================================
        Target m_target;
        // ============================================================
        // Buffers
        // ============================================================
        DepthBuffer m_depth;
        ColorBuffer m_color;
        NormalBuffer m_normal;
        IndexBuffer m_index;
        // ============================================================
        // State
        // ============================================================
        DrawState m_state;
        std::vector<DrawState>
            m_stateStack;
        // ============================================================
        // Size
        // ============================================================
        int m_width = 0;
        int m_height = 0;
        // ============================================================
        // Buffer locking
        // ============================================================
        uint64_t m_key = 0;
        // ============================================================
        // HZB
        // ============================================================
        int m_mipmaplevel =
            buffers::MAX_HZB_LEVEL;
        bool m_use_hzb = true;
        bool m_use_moc = true;
        // ============================================================
        // NEU: Span-Renderer (statische BSP/PVS-Geometrie)
        // ============================================================
        MOCBuffer m_moc;
        // ============================================================
        // NEU: Tile-based Multithreading fuer DrawTriangle/DrawPolygon
        // ============================================================
        bool m_use_tiled = false;
        bool m_tiled_single_threaded_debug = false;
        int  m_thread_count = (int)std::thread::hardware_concurrency();
        TileGrid<T, SVertex> m_tileGrid;
        inline void Noise(ng::renderer::buffers::PixelBufferRGBA& pixelbuff)
        {
            static uint32_t seed = 123456789;
            uint32_t* buf = pixelbuff.GetRawBuffer();
            for (int i = 0; i < pixelbuff.Width() * pixelbuff.Height(); i++)
            {
                seed = seed * 1664525u + 1013904223u;
                uint8_t c = seed >> 24;
                buf[i] =
                    0xFF000000 |
                    (c << 16) |
                    (c << 8) |
                    c;
            }
        }
    };
}