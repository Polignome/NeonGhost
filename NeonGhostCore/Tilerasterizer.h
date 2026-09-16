#pragma once
// =====================================================================
// TileRasterizer.h
//
// Erweitert den bestehenden Base Rasterizer (siehe Rasterizer.h) um
// ein Bin -> Rasterize Schema fuer Multithreading:
//
//   1) Submit-Phase (single-threaded, billig):
//      Jedes Dreieck wird nur gegen die Screen-BBox der Kacheln (Tiles)
//      getestet und sein Index/Kopie in die Bin-Liste jeder ueberlappten
//      Kachel eingetragen.
//
//   2) Rasterize-Phase (multi-threaded):
//      Jeder Worker-Thread uebernimmt eine oder mehrere Kacheln und
//      rastert NUR die Dreiecke aus deren Bin-Liste, zusaetzlich auf
//      die Kachelgrenzen geclippt. Da Kacheln disjunkte Speicher-
//      bereiche im RenderTarget sind, gibt es keine Data Races und
//      man braucht kein Locking.
//
// Abhaengigkeit: Rasterizer.h muss VOR dieser Datei inkludiert werden
// (RenderTarget<T>, DrawState<T>, Gradients<T>, Edge<T>, ShadePixel,
// SelectDrawFunctions werden wiederverwendet).
//
// Empfohlene Einfuehrungsreihenfolge (siehe Chatverlauf):
//   1. TileGrid/SubmitTriangle/RasterizeTile SINGLE-THREADED einbauen
//      und Pixel-fuer-Pixel gegen DrawTriangleFast() verifizieren.
//   2. Erst danach den Thread-Pool aktivieren (RenderFrameThreaded).
//   3. Tile-Groesse (TILE_SIZE) je nach Dreiecksdichte/Kernanzahl tunen.
//   4. Spaeter: Thread-Pool persistent machen statt pro Frame neu zu
//      erzeugen (Create/Join-Overhead vermeiden).
// =====================================================================

#include <vector>
#include <atomic>
#include <thread>
#include <algorithm>
#include <cmath>
#include <cstdint>

#include "Rasterizer3dBase.h"

namespace ng::renderer::rasterizer
{
    // ------------------------------------------------------------
    // TriangleJob: das, was in die Bin-Listen wandert. Haelt eine
    // Kopie der drei (bereits projizierten) Vertices UND eine Kopie
    // des DrawState.
    //
    // WICHTIG: state wird bewusst per WERT gespeichert, nicht per
    // Zeiger. In der echten Nutzung (Rasterizer3D) ist m_state ein
    // einzelnes, mutable Member, das zwischen Draw-Calls staendig
    // ueberschrieben wird (SetState()/PushState()/PopState()). Ein
    // Zeiger auf m_state waere zum Zeitpunkt der Rasterize-Phase
    // (End()) fuer praktisch alle Jobs bereits veraltet/falsch. Die
    // Kopie ist unproblematisch, da DrawState<T> nur Funktionszeiger
    // und rohe, extern gehaltene const TextureView* enthaelt - keine
    // Owned-Resources.
    // ------------------------------------------------------------
    template<class T, class SVertexT>
    struct TriangleJob
    {
        SVertexT v0, v1, v2;
        DrawState<T> state;
    };

    template<class T, class SVertexT>
    struct TileBin
    {
        std::vector<TriangleJob<T, SVertexT>> triangles;
    };

    // ------------------------------------------------------------
    // TileGrid: unterteilt den Screen in TILE_SIZE x TILE_SIZE
    // grosse Kacheln. Jede Kachel besitzt ihre eigene Bin-Liste.
    // ------------------------------------------------------------
    template<class T, class SVertexT>
    struct TileGrid
    {
        static constexpr int TILE_SIZE = 64; // ggf. 32/128 tunen und messen

        int tilesX = 0;
        int tilesY = 0;
        int targetWidth = 0;
        int targetHeight = 0;
        std::vector<TileBin<T, SVertexT>> bins;

        void Resize(int width, int height)
        {
            targetWidth = width;
            targetHeight = height;
            tilesX = (width + TILE_SIZE - 1) / TILE_SIZE;
            tilesY = (height + TILE_SIZE - 1) / TILE_SIZE;
            bins.assign(static_cast<size_t>(tilesX) * tilesY, TileBin<T, SVertexT>{});
        }

        // Einmal pro Frame VOR der Submit-Phase aufrufen. Die
        // Kapazitaet der einzelnen std::vector bleibt erhalten -
        // dadurch entstehen ab dem zweiten Frame kaum noch
        // Allokationen.
        void Clear()
        {
            for (auto& b : bins)
                b.triangles.clear();
        }

        inline TileBin<T, SVertexT>& At(int tx, int ty)
        {
            return bins[static_cast<size_t>(ty) * tilesX + tx];
        }

        inline int TileX0(int tx) const { return tx * TILE_SIZE; }
        inline int TileY0(int ty) const { return ty * TILE_SIZE; }
        inline int TileX1(int tx) const { return std::min(TileX0(tx) + TILE_SIZE, targetWidth); }
        inline int TileY1(int ty) const { return std::min(TileY0(ty) + TILE_SIZE, targetHeight); }
    };

    // ------------------------------------------------------------
    // Submit-Phase: statt sofort zu rastern (wie DrawTriangleFast)
    // wird das Dreieck nur gebinnt. SelectDrawFunctions(st) muss
    // VORHER (einmal pro Draw-Call) aufgerufen worden sein - nicht
    // mehr hier pro Dreieck, wie es DrawTriangleFast bisher tut.
    //
    // st wird per const-Referenz uebergeben und in den Job KOPIERT
    // (siehe Kommentar an TriangleJob) - der Aufrufer kann sein
    // DrawState-Objekt also direkt danach wieder veraendern.
    // ------------------------------------------------------------
    template<class T, class SVertexT>
    inline void SubmitTriangle(
        TileGrid<T, SVertexT>& grid,
        const DrawState<T>& st,
        SVertexT v0, SVertexT v1, SVertexT v2)
    {
        T minXf = std::min({ v0.x(), v1.x(), v2.x() });
        T maxXf = std::max({ v0.x(), v1.x(), v2.x() });
        T minYf = std::min({ v0.y(), v1.y(), v2.y() });
        T maxYf = std::max({ v0.y(), v1.y(), v2.y() });

        if (std::abs(v0.y() - v2.y()) < (T)1e-6 &&
            std::abs(v0.y() - v1.y()) < (T)1e-6)
            return; // komplett degeneriert (Flaeche 0 in Y)

        int tileMinX = std::max(0, (int)std::floor(minXf) / TileGrid<T, SVertexT>::TILE_SIZE);
        int tileMaxX = std::min(grid.tilesX - 1, (int)std::floor(maxXf) / TileGrid<T, SVertexT>::TILE_SIZE);
        int tileMinY = std::max(0, (int)std::floor(minYf) / TileGrid<T, SVertexT>::TILE_SIZE);
        int tileMaxY = std::min(grid.tilesY - 1, (int)std::floor(maxYf) / TileGrid<T, SVertexT>::TILE_SIZE);
        if (tileMinX > tileMaxX || tileMinY > tileMaxY)
            return; // komplett ausserhalb des Bildschirms

        TriangleJob<T, SVertexT> job{ v0, v1, v2, st }; // st wird hier kopiert
        for (int ty = tileMinY; ty <= tileMaxY; ++ty)
            for (int tx = tileMinX; tx <= tileMaxX; ++tx)
                grid.At(tx, ty).triangles.push_back(job);
    }

    // ------------------------------------------------------------
    // DrawSpanClamped: identisch zu DrawSpan aus Rasterizer.h, nur
    // dass xStart/xEnd zusaetzlich auf die Kachelgrenzen [tileX0,
    // tileX1) geclampt werden, damit ein Thread garantiert nur in
    // seinen eigenen Speicherbereich des RenderTarget schreibt.
    // ------------------------------------------------------------
    template<class T>
    inline void DrawSpanClamped(
        RenderTarget<T>& target, const DrawState<T>& st, const Gradients<T>& grad,
        int y, const Edge<T>& left, const Edge<T>& right,
        int tileX0, int tileX1)
    {
        int xStart = std::max((int)std::ceil(left.x - (T)0.5), tileX0);
        int xEnd = std::min((int)std::ceil(right.x - (T)0.5), tileX1);
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

        for (int x = xStart; x < xEnd; x++)
        {
            f.x = x;
            T w = (T)1 / f.invW;
            Fragment<T> shaded = f;
            shaded.u = f.u * w;
            shaded.v = f.v * w;
            shaded.fxu = f.fxu * w;
            shaded.fxv = f.fxv * w;

            // NEU: exakt derselbe Fix wie in DrawSpan() (Rasterizer.h) -
            // ohne diese Zeilen bleiben shaded.dudx/dvdx/dudy/dvdy
            // uninitialisiert (zufaelliger Stack-Muell), was zu einer
            // zufaelligen Mip-Level-Auswahl pro Pixel fuehrt (sichtbar
            // als diagonales Streifen-/Moiré-Muster).
            shaded.dudx = PerspectiveDerivative(f.u, f.invW, grad.dU_dx, grad.dInvW_dx);
            shaded.dvdx = PerspectiveDerivative(f.v, f.invW, grad.dV_dx, grad.dInvW_dx);
            shaded.dudy = PerspectiveDerivative(f.u, f.invW, grad.dU_dy, grad.dInvW_dy);
            shaded.dvdy = PerspectiveDerivative(f.v, f.invW, grad.dV_dy, grad.dInvW_dy);

            ShadePixel(target, st, shaded);

            f.invW += grad.dInvW_dx;
            f.u += grad.dU_dx; f.v += grad.dV_dx;
            f.fxu += grad.dFxu_dx; f.fxv += grad.dFxv_dx;
            f.r += grad.dR_dx; f.g += grad.dG_dx; f.b += grad.dB_dx; f.a += grad.dA_dx;
            f.nx += grad.dNx_dx; f.ny += grad.dNy_dx; f.nz += grad.dNz_dx;
        }
    }

    // ------------------------------------------------------------
    // AdvanceEdgeTo: springt eine Edge<T> direkt auf eine Zielzeile,
    // OHNE iterativ Step() aufzurufen. Nutzt aus, dass alle Attribute
    // einer Edge affin-linear in y sind (wert(y) = wert(y0) + (y-y0)*step).
    //
    // WICHTIG (Bugfix): die urspruengliche Implementierung hat pro
    // Dreieckshaelfte eine eigene iterative Katch-up-Schleife genutzt,
    // um longEdge bis zum Kachelanfang vorzuspulen. Wenn eine Kachel
    // komplett unterhalb der oberen Haelfte lag, wurde longEdge im
    // oberen Block bereits bis zum Kachelanfang vorgespult UND im
    // unteren Block nochmal ab v1.y() bis zum Kachelanfang - also fuer
    // denselben Zeilenbereich DOPPELT. Das fuehrte zu den beobachteten
    // Verschiebungen/Loechern an Kachelgrenzen. Mit AdvanceEdgeTo plus
    // einem explizit mitgefuehrten "aktuelle Zeile"-Tracker (currentRow)
    // ist das nicht mehr moeglich: jede Zeile wird immer genau einmal
    // gezaehlt, unabhaengig davon, wie oft/wo AdvanceEdgeTo aufgerufen
    // wird.
    // ------------------------------------------------------------
    template<class T>
    inline void AdvanceEdgeTo(Edge<T>& e, int& currentRow, int targetRow)
    {
        if (targetRow == currentRow) return;
        const T delta = (T)(targetRow - currentRow);
        e.x += e.xStep * delta;
        e.invW += e.invW_step * delta;
        e.u += e.u_step * delta;
        e.v += e.v_step * delta;
        e.fxu += e.fxu_step * delta;
        e.fxv += e.fxv_step * delta;
        e.r += e.r_step * delta;
        e.g += e.g_step * delta;
        e.b += e.b_step * delta;
        e.a += e.a_step * delta;
        e.nx += e.nx_step * delta;
        e.ny += e.ny_step * delta;
        e.nz += e.nz_step * delta;
        currentRow = targetRow;
    }

    // ------------------------------------------------------------
    // Ein einzelnes Dreieck, aber geclippt auf eine Kachel gerastert.
    // Struktur folgt DrawTriangleFast() aus Rasterizer.h - Unterschied:
    // die Y-Schleifen werden auf [tileY0, tileY1) begrenzt, und die
    // Kanten werden per AdvanceEdgeTo() DIREKT (nicht iterativ) auf
    // den Kachelanfang synchronisiert.
    //
    // longRow trackt, auf welcher Zeile sich longEdge gerade befindet -
    // wird ueber BEIDE Haelften hinweg gemeinsam fortgeschrieben, damit
    // niemals derselbe Zeilenbereich zweimal vorgespult wird (siehe
    // Bugfix-Kommentar an AdvanceEdgeTo).
    // ------------------------------------------------------------
    template<class T, class SVertexT>
    inline void RasterizeTriangleInTile(
        RenderTarget<T>& target, DrawState<T>& st,
        SVertexT v0, SVertexT v1, SVertexT v2,
        int tileX0, int tileY0, int tileX1, int tileY1)
    {
        if (v0.y() > v1.y()) std::swap(v0, v1);
        if (v1.y() > v2.y()) std::swap(v1, v2);
        if (v0.y() > v1.y()) std::swap(v0, v1);
        if (std::abs(v0.y() - v2.y()) < (T)1e-6) return;

        Gradients<T> grad(v0, v1, v2);
        T area = (v2.x() - v0.x()) * (v1.y() - v0.y()) - (v2.y() - v0.y()) * (v1.x() - v0.x());
        bool middleIsLeft = area >= (T)0;
        Edge<T> longEdge(v0, v2, grad);
        int longRow = longEdge.yStart; // Zeile, auf der longEdge aktuell "steht" (0 Steps bisher)

        auto clampedSpan = [&](int y, const Edge<T>& left, const Edge<T>& right)
            {
                if (y < tileY0 || y >= tileY1) return;
                if (middleIsLeft) DrawSpanClamped(target, st, grad, y, left, right, tileX0, tileX1);
                else               DrawSpanClamped(target, st, grad, y, right, left, tileX0, tileX1);
            };

        if (v0.y() < v1.y())
        {
            Edge<T> shortEdge(v0, v1, grad);
            int shortRow = shortEdge.yStart;
            const int rawStart = shortEdge.yStart;
            const int rawEnd = shortEdge.yEnd;
            const int yStart = std::max(rawStart, tileY0);
            const int yEnd = std::min(rawEnd, tileY1);

            if (yStart < yEnd)
            {
                AdvanceEdgeTo(shortEdge, shortRow, yStart);
                AdvanceEdgeTo(longEdge, longRow, yStart);
                for (int y = yStart; y < yEnd; ++y)
                {
                    clampedSpan(y, shortEdge, longEdge);
                    shortEdge.Step(); ++shortRow;
                    longEdge.Step();  ++longRow;
                }
            }
            // longEdge MUSS exakt auf Zeile rawEnd (= v1.y()) stehen,
            // bevor es in die untere Haelfte weitergeht - unabhaengig
            // davon, ob/wie weit oben tatsaechlich gezeichnet wurde.
            // Das ist der entscheidende Punkt gegenueber der alten,
            // fehlerhaften Implementierung.
            AdvanceEdgeTo(longEdge, longRow, rawEnd);
        }

        if (v1.y() < v2.y())
        {
            Edge<T> shortEdge(v1, v2, grad);
            int shortRow = shortEdge.yStart;
            const int rawStart = shortEdge.yStart;
            const int rawEnd = shortEdge.yEnd;
            const int yStart = std::max(rawStart, tileY0);
            const int yEnd = std::min(rawEnd, tileY1);

            if (yStart < yEnd)
            {
                AdvanceEdgeTo(shortEdge, shortRow, yStart);
                AdvanceEdgeTo(longEdge, longRow, yStart);
                for (int y = yStart; y < yEnd; ++y)
                {
                    clampedSpan(y, shortEdge, longEdge);
                    shortEdge.Step(); ++shortRow;
                    longEdge.Step();  ++longRow;
                }
            }
        }
    }

    // ------------------------------------------------------------
    // Rasterize-Phase fuer eine komplette Kachel: iteriert nur ueber
    // die Dreiecke, die in dieser Kachel gebinnt wurden.
    // ------------------------------------------------------------
    template<class T, class SVertexT>
    inline void RasterizeTile(
        RenderTarget<T>& target,
        TileBin<T, SVertexT>& bin,
        int tileX0, int tileY0, int tileX1, int tileY1)
    {
        for (auto& job : bin.triangles)
        {
            RasterizeTriangleInTile(
                target, job.state,
                job.v0, job.v1, job.v2,
                tileX0, tileY0, tileX1, tileY1);
        }
    }

    // ------------------------------------------------------------
    // Single-threaded Referenzpfad: alle Kacheln nacheinander
    // abarbeiten. NUR zum Verifizieren gegen DrawTriangleFast()
    // gedacht (Schritt 1 der empfohlenen Einfuehrung) - erzeugt
    // exakt dasselbe Bild wie RenderFrameThreaded(), nur ohne
    // Threads.
    // ------------------------------------------------------------
    template<class T, class SVertexT>
    inline void RenderFrameSingleThreaded(RenderTarget<T>& target, TileGrid<T, SVertexT>& grid)
    {
        for (int ty = 0; ty < grid.tilesY; ++ty)
        {
            for (int tx = 0; tx < grid.tilesX; ++tx)
            {
                RasterizeTile(
                    target, grid.At(tx, ty),
                    grid.TileX0(tx), grid.TileY0(ty),
                    grid.TileX1(tx), grid.TileY1(ty));
            }
        }
    }

    // ------------------------------------------------------------
    // Multi-threaded Rasterize-Phase. Nutzt eine simple work-stealing
    // Queue (std::atomic<int> nextTile): jeder Thread holt sich die
    // naechste freie Kachel, statt starr eine feste Teilmenge
    // zugewiesen zu bekommen. Das balanciert automatisch aus, wenn
    // manche Kacheln deutlich mehr Dreiecke enthalten als andere.
    //
    // Hinweis: erzeugt/joint pro Aufruf einen neuen Thread-Pool.
    // Fuer produktiven Einsatz spaeter auf einen persistenten Pool
    // umstellen (Threads leben ueber Frames hinweg, warten per
    // Condition-Variable auf neue Arbeit) - der Create/Join-Overhead
    // pro Frame ist bei hoher Framerate spuerbar.
    // ------------------------------------------------------------
    template<class T, class SVertexT>
    inline void RenderFrameThreaded(
        RenderTarget<T>& target,
        TileGrid<T, SVertexT>& grid,
        int numThreads)
    {
        if (numThreads <= 1)
        {
            RenderFrameSingleThreaded(target, grid);
            return;
        }

        std::atomic<int> nextTile{ 0 };
        const int totalTiles = grid.tilesX * grid.tilesY;

        auto worker = [&]()
            {
                int idx;
                while ((idx = nextTile.fetch_add(1, std::memory_order_relaxed)) < totalTiles)
                {
                    int tx = idx % grid.tilesX;
                    int ty = idx / grid.tilesX;
                    RasterizeTile(
                        target, grid.At(tx, ty),
                        grid.TileX0(tx), grid.TileY0(ty),
                        grid.TileX1(tx), grid.TileY1(ty));
                }
            };

        std::vector<std::thread> pool;
        pool.reserve(numThreads);
        for (int i = 0; i < numThreads; ++i)
            pool.emplace_back(worker);
        for (auto& t : pool)
            t.join();
    }

} // namespace ng::renderer::rasterizer