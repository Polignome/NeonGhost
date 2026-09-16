
#pragma once
// ============================================================================
// MaskedOcclusionBuffer<T>
//
// Selbst hergeleitete, SKALARE Variante von "Masked Occlusion Culling",
// KEIN Port von Intels Library. Grundidee (wie bei Intel): der Bildschirm
// wird in kleine Zellen unterteilt; jede Zelle speichert NICHT den vollen
// Tiefenwert pro Pixel, sondern nur zwei komprimierte Tiefenwerte (zA, zB)
// plus eine 32-Bit-Maske, die festlegt, welches Pixel zu welcher Gruppe
// gehoert.
//
// Warum ueberhaupt komprimieren (statt einfach ein per-Pixel-Array)?
// Ein einzelner Skalar pro Zelle (z.B. "Minimum ueber alle Pixel") waere
// bei GEMISCHTEN Zellen (halb Occluder, halb Luecke) sofort auf 0
// ("nichts garantiert verdeckt"), sobald AUCH NUR EIN Pixel der Zelle noch
// leer ist - das ist exakt die Schwaeche, die euer bestehender AABB-basierter
// HZB-Test bei verstreuter/duenner Geometrie schon gezeigt hat. Zwei Werte
// + Maske erlauben es, "innerhalb des ersten Occluders" und "ausserhalb
// davon" getrennt zu behandeln, ohne gleich ein volles Pixel-Array
// vorzuhalten.
//
// KORREKTHEITS-INVARIANTE (das ist der Teil, den ich explizit hergeleitet
// und nicht nur "nachgebaut" habe):
//
//   Fuer jedes Pixel p einer Zelle gilt jederzeit:
//       gespeicherterWert(p) <= wahrer_Occluder_Tiefenwert(p)
//
//   d.h. der gespeicherte Wert ist eine SICHERE UNTERSCHAETZUNG der
//   tatsaechlichen Naehe. Ein Occlusion-Test darf also nur "verdeckt"
//   melden, wenn selbst diese Unterschaetzung schon naeher/gleich der
//   Anfrage ist - das ist per Konstruktion niemals falsch, kann hoechstens
//   zu VORSICHTIG sein (weniger Culling als theoretisch moeglich, aber
//   NIE fehlerhaftes Culling).
//
// MERGE-HERLEITUNG (pro Zelle, beim Einschreiben eines neuen Dreiecks):
//
//   Sei mask die aktuelle Partition (0 = Gruppe A, 1 = Gruppe B), zA/zB die
//   aktuellen (sicheren) Werte je Gruppe. Ein neues Dreieck deckt eine
//   Teilmenge newMask der 32 Pixel mit echten Tiefenwerten newZ[p] ab.
//
//   1. ERSTBERUEHRUNG einer noch "jungfraeulichen" Zelle:
//      Die Maske wird NEU aus der Abdeckung DIESES ersten Dreiecks
//      gebildet (mask = newMask). Das haelt die Silhouettenkante des
//      ersten Occluders in der Zelle fest. Gruppe A ("ausserhalb")
//      bleibt bei 0 (nichts bekannt), Gruppe B ("innerhalb") bekommt das
//      Minimum der abgedeckten newZ-Werte (= sichere Unterschaetzung).
//
//   2. WEITERE Dreiecke (Maske/Partition bleibt danach FEST):
//      Fuer jedes Pixel p gilt: alterWert(p) <= wahrerWert_alt(p) (Invariante).
//      Neuer wahrer Wert = max(wahrerWert_alt(p), newZ[p]) falls p von
//      newMask abgedeckt, sonst unveraendert. Da max() monoton ist, gilt:
//          max(wahrerWert_alt(p), newZ[p]) >= max(gespeichert(p), newZ[p])
//      Also ist max(gespeichert(p), newZ[p]) weiterhin eine SICHERE
//      Unterschaetzung des neuen wahren Werts - noch bevor wir irgendwas
//      komprimieren. Um das wieder auf (zA, zB) zu komprimieren, nehmen
//      wir je Gruppe das MINIMUM dieser Kandidatenwerte (Minimum einer
//      Menge ist <= jedes Mitglied -> bleibt sicher).
//
//   Ergebnis: das Verfahren ist bei JEDER Anzahl Merges korrekt (nie
//   truegerisch optimistisch). Einziger Kompromiss: je MEHR als zwei
//   klar getrennte Dreiecke dieselbe Zelle treffen, desto mehr naehert
//   sich die Schaetzung dem konservativen Minimum an (Culling wird
//   vorsichtiger, aber NIE falsch). Das ist der Preis dafuer, dass wir
//   nur 2 Werte statt 32 Werte pro Zelle speichern.
//
// Was hier NICHT nachgebaut wird (bewusste Vereinfachung ggue. Intels
// Library):
//   - Kein SIMD (SSE/AVX) - reines Skalar-C++, als korrekte Referenz.
//     SIMD waere ein spaeterer, rein performance-getriebener Umbau NACH
//     Validierung dieser Logik.
//   - Keine Nahebenen-Clipping fuer Occluder-Dreiecke - ein Dreieck mit
//     einer Ecke hinter der Kamera wird komplett verworfen (sicher, aber
//     verschenkt etwas Occluder-Potenzial nahe der Kamera).
//   - Keine SIMD-freundliche 8x2-Zellen-Gruppierung ("Tile") wie bei
//     Intel - jede 8x4-Zelle wird einzeln gehalten.
// ============================================================================
#include <vector>
#include <cstdint>
#include <cmath>
#include <limits>
#include <algorithm>
#include "Matrix.h"
#include "AABBox.h"
#include "HZBOcclusion.h" // wiederverwendet occlusion::ProjectedPoint<T>
#include "ScreenVertex.h"
#include <immintrin.h>
#include <type_traits>


namespace ng {
    namespace renderer {
        namespace occlusion {



















            template <class T>
            class MaskedOcclusionBuffer
            {
            public:
                // Zellgroesse: 8x4 = 32 Pixel = exakt ein uint32_t-Bit pro Pixel.
                static constexpr int CELL_W = 8;
                static constexpr int CELL_H = 4;
                static constexpr int CELL_PIXELS = CELL_W * CELL_H; // 32

                using ScreenVertex = ng::renderer::rasterizer::ScreenVertex<T>;

                struct Cell
                {
                    uint32_t mask = 0;      // 0 = Gruppe A, 1 = Gruppe B (siehe Herleitung oben)
                    T zA = 0;
                    T zB = 0;
                    bool virgin = true;     // true = noch nie von einem Dreieck getroffen
                };

                MaskedOcclusionBuffer() = default;

                void Resize(int width, int height)
                {
                    m_width = width;
                    m_height = height;
                    m_cellsX = (width + CELL_W - 1) / CELL_W;
                    m_cellsY = (height + CELL_H - 1) / CELL_H;
                    m_cells.assign(static_cast<size_t>(m_cellsX) * m_cellsY, Cell{});
                }

                // Einmal pro Frame VOR dem Occluder-Pass aufrufen.
                void Clear()
                {
                    std::fill(m_cells.begin(), m_cells.end(), Cell{});
                }

                int Width() const { return m_width; }
                int Height() const { return m_height; }

                // ------------------------------------------------------------
                // PASS 1: Statische Occluder-Geometrie einschreiben.
                //
                // 'project' hat exakt dieselbe Signatur/Semantik wie bei eurem
                // bestehenden occlusion::TestAABBoxVisibility(): nimmt einen
                // Weltpunkt entgegen, liefert ProjectedPoint<T> (x, y in
                // Bildschirmkoordinaten, oneOverZ = 1/w, groesser = naeher).
                // ------------------------------------------------------------
                template <class ProjectFn>
                void RenderOccluder(
                    const std::vector<ng::math::Matrix<3, 1, T>>& worldVerts,
                    const uint32_t* indices,
                    size_t triCount,
                    ProjectFn project)
                {
                    for (size_t t = 0; t < triCount; ++t)
                    {
                        const uint32_t i0 = indices[t * 3 + 0];
                        const uint32_t i1 = indices[t * 3 + 1];
                        const uint32_t i2 = indices[t * 3 + 2];
                        const ProjectedPoint<T> p0 = project(worldVerts[i0]);
                        const ProjectedPoint<T> p1 = project(worldVerts[i1]);
                        const ProjectedPoint<T> p2 = project(worldVerts[i2]);
                        // Vereinfachung: Dreiecke mit einer Ecke hinter der
                        // Kamera werden verworfen statt geclippt (siehe
                        // Kommentar am Dateianfang).
                        if (p0.behindCamera || p1.behindCamera || p2.behindCamera)
                            continue;
                        RasterizeTriangle(
                            p0.x, p0.y, p0.oneOverZ,
                            p1.x, p1.y, p1.oneOverZ,
                            p2.x, p2.y, p2.oneOverZ);
                    }
                }

                template <class ProjectFn>
                void RenderOccluder(
                    const ng::math::Matrix<3, 1, T>& w0, const ng::math::Matrix<3, 1, T>& w1, const ng::math::Matrix<3, 1, T>& w2,
                    ProjectFn project)
                {
                    const ProjectedPoint<T> p0 = project(w0);
                    const ProjectedPoint<T> p1 = project(w1);
                    const ProjectedPoint<T> p2 = project(w2);
                    // Vereinfachung: Dreiecke mit einer Ecke hinter der
                    // Kamera werden verworfen statt geclippt (siehe
                    // Kommentar am Dateianfang).
                    if (p0.behindCamera || p1.behindCamera || p2.behindCamera) return;

                    RasterizeTriangle(
                        p0.x, p0.y, p0.oneOverZ,
                        p1.x, p1.y, p1.oneOverZ,
                        p2.x, p2.y, p2.oneOverZ);
                }


                /*
                void RenderOccluder(const ng::math::Matrix<3, 1, T>& w0, const ng::math::Matrix<3, 1, T>& w1, const ng::math::Matrix<3, 1, T>& w2)
                {

                    RasterizeTriangle(
                        w0.x(), w0.y(), w0.z(),
                        w1.x(), w1.y(), w1.z(),
                        w2.x(), w2.y(), w2.z());
                }
                */

                void RenderOccluder(const ng::math::Matrix<3, 1, float>& w0, const ng::math::Matrix<3, 1, float>& w1, const ng::math::Matrix<3, 1, float>& w2)
                {

                    RasterizeTriangle(
                        (T)w0.x(), (T)w0.y(), (T)w0.z(),
                        (T)w1.x(), (T)w1.y(), (T)w1.z(),
                        (T)w2.x(), (T)w2.y(), (T)w2.z());
                }


                void RenderOccluder(const ng::math::Matrix<3, 1, double>& w0, const ng::math::Matrix<3, 1, double>& w1, const ng::math::Matrix<3, 1, double>& w2)
                {

                    RasterizeTriangle(
                        (T)w0.x(), (T)w0.y(), (T)w0.z(),
                        (T)w1.x(), (T)w1.y(), (T)w1.z(),
                        (T)w2.x(), (T)w2.y(), (T)w2.z());
                }

                /*


                void RenderOccluder2(const ScreenVertex& w0, const ScreenVertex& w1, const ScreenVertex& w2)
                {

                    RasterizeTriangle(
                        w0.x(), w0.y(), w0.w(),
                        w1.x(), w1.y(), w1.w(),
                        w2.x(), w2.y(), w2.w());
                }
                */




                // ------------------------------------------------------------
                // PASS 2: Ersetzt occlusion::IsAABBoxVisible().
                //
                // Gleiches Prinzip wie euer HZB-Test: alle 8 Ecken projizieren,
                // Screen-Rect + naechstes 1/w bilden, dann gegen die
                // ueberlappenden Zellen pruefen. Rueckgabe: true = sichtbar
                // (nicht bewiesen verdeckt), false = bewiesen verdeckt.
                // ------------------------------------------------------------
                template <class AABBoxT, class ProjectFn>
                bool IsAABBoxVisible(AABBoxT& box, ProjectFn project) const
                {
                    auto mn = box.Min();
                    auto mx = box.Max();
                    const T corners[8][3] =
                    {
                        { (T)mn.x(), (T)mn.y(), (T)mn.z() }, { (T)mx.x(), (T)mn.y(), (T)mn.z() },
                        { (T)mn.x(), (T)mx.y(), (T)mn.z() }, { (T)mx.x(), (T)mx.y(), (T)mn.z() },
                        { (T)mn.x(), (T)mn.y(), (T)mx.z() }, { (T)mx.x(), (T)mn.y(), (T)mx.z() },
                        { (T)mn.x(), (T)mx.y(), (T)mx.z() }, { (T)mx.x(), (T)mx.y(), (T)mx.z() }
                    };

                    T minX = std::numeric_limits<T>::max();
                    T minY = std::numeric_limits<T>::max();
                    T maxX = std::numeric_limits<T>::lowest();
                    T maxY = std::numeric_limits<T>::lowest();
                    T nearestOneOverZ = std::numeric_limits<T>::lowest();

                    for (int i = 0; i < 8; ++i)
                    {
                        ng::math::Matrix<3, 1, T> p3(corners[i][0], corners[i][1], corners[i][2]);
                        ProjectedPoint<T> p = project(p3);
                        if (p.behindCamera)
                            return true; // Kamera-Sonderfall: nie culen, wie bei eurem HZB-Test
                        minX = std::min(minX, static_cast<T>(p.x));
                        maxX = std::max(maxX, static_cast<T>(p.x));
                        minY = std::min(minY, static_cast<T>(p.y));
                        maxY = std::max(maxY, static_cast<T>(p.y));
                        nearestOneOverZ = std::max(nearestOneOverZ, static_cast<T>(p.oneOverZ));
                    }

                    minX = std::max(minX, static_cast<T>(0));
                    minY = std::max(minY, static_cast<T>(0));
                    maxX = std::min(maxX, static_cast<T>(m_width - 1));
                    maxY = std::min(maxY, static_cast<T>(m_height - 1));
                    if (minX > maxX || minY > maxY)
                        return true; // ausserhalb des Bildschirms - Frustum-Culling kuemmert sich darum

                    int cellMinX = std::clamp(static_cast<int>(minX) / CELL_W, 0, m_cellsX - 1);
                    int cellMaxX = std::clamp(static_cast<int>(maxX) / CELL_W, 0, m_cellsX - 1);
                    int cellMinY = std::clamp(static_cast<int>(minY) / CELL_H, 0, m_cellsY - 1);
                    int cellMaxY = std::clamp(static_cast<int>(maxY) / CELL_H, 0, m_cellsY - 1);

                    for (int cy = cellMinY; cy <= cellMaxY; ++cy)
                    {
                        for (int cx = cellMinX; cx <= cellMaxX; ++cx)
                        {
                            const Cell& cell = m_cells[static_cast<size_t>(cy) * m_cellsX + cx];
                            // Nur Gruppen pruefen, die ueberhaupt Pixel enthalten
                            // (siehe Query-Fallstrick in der Herleitung oben -
                            // eine ungenutzte Gruppe darf den Test nicht
                            // spurios scheitern lassen).
                            const bool groupAUsed = (cell.mask != 0xFFFFFFFFu);
                            const bool groupBUsed = (cell.mask != 0u);
                            if (groupAUsed && cell.zA < nearestOneOverZ)
                                return true; // nicht bewiesen verdeckt
                            if (groupBUsed && cell.zB < nearestOneOverZ)
                                return true;
                        }
                    }
                    return false; // jede ueberlappende Zelle ist sicher naeher/gleich -> verdeckt
                }


                void VisualizeDepth(uint32_t* bufferRGBA) const
                {
                    if (!bufferRGBA)
                        return;

                    for (int y = 0; y < m_height; ++y)
                    {
                        for (int x = 0; x < m_width; ++x)
                        {
                            const int cx = x / CELL_W;
                            const int cy = y / CELL_H;

                            const int lx = x % CELL_W;
                            const int ly = y % CELL_H;

                            const Cell& cell =
                                m_cells[static_cast<size_t>(cy) * m_cellsX + cx];

                            const int bit = ly * CELL_W + lx;

                            const bool groupB =
                                (cell.mask & (1u << bit)) != 0;

                            const T z =
                                groupB ? cell.zB : cell.zA;

                            // 1/Z:
                            // größer = näher
                            //
                            // Nur zur Visualisierung.
                            const T v = std::clamp(
                                z * static_cast<T>(255),
                                static_cast<T>(0),
                                static_cast<T>(255));

                            const uint32_t c =
                                static_cast<uint32_t>(v);

                            bufferRGBA[y * m_width + x] =
                                0xFF000000u |
                                (c << 16) |
                                (c << 8) |
                                c;
                        }
                    }
                }

                void VisualizeMask(uint32_t* bufferRGBA) const
                {
                    if (!bufferRGBA)
                        return;

                    // Erstmal alles schwarz/transparent setzen
                    const size_t pixelCount =
                        static_cast<size_t>(m_width) * m_height;

                    std::fill(
                        bufferRGBA,
                        bufferRGBA + pixelCount,
                        0xFF000000u
                    );

                    for (int cy = 0; cy < m_cellsY; ++cy)
                    {
                        for (int cx = 0; cx < m_cellsX; ++cx)
                        {
                            const Cell& cell =
                                m_cells[cy * m_cellsX + cx];

                            for (int ly = 0; ly < CELL_H; ++ly)
                            {
                                for (int lx = 0; lx < CELL_W; ++lx)
                                {
                                    const int x = cx * CELL_W + lx;
                                    const int y = cy * CELL_H + ly;

                                    if (x >= m_width || y >= m_height)
                                        continue;

                                    const int bit = ly * CELL_W + lx;

                                    const bool set =
                                        (cell.mask & (1u << bit)) != 0;

                                    if (set)
                                    {
                                        // Bit = 1 -> Rot
                                        bufferRGBA[y * m_width + x] = 0xFFFF0000u;
                                    }
                                    else
                                    {
                                        // Bit = 0 -> Grün
                                        bufferRGBA[y * m_width + x] = 0xFF00FF00u;
                                    }
                                }
                            }
                        }
                    }
                }
            private:
                int m_width = 0;
                int m_height = 0;
                int m_cellsX = 0;
                int m_cellsY = 0;
                std::vector<Cell> m_cells;

                static inline T EdgeFn(T ax, T ay, T bx, T by, T px, T py)
                {
                    return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
                }

                // Fuegt die Kandidatenwerte eines neuen Dreiecks in eine Zelle
                // ein - implementiert exakt die oben hergeleitete Merge-Logik.
                void MergeCell(Cell& cell, uint32_t newMask, const T* newZ)
                {
                    if (newMask == 0)
                        return; // Dreieck beruehrt diese Zelle gar nicht

                    if (cell.virgin)
                    {
                        // Erstberuehrung: Partition NEU aus der Abdeckung dieses
                        // Dreiecks bilden (siehe Herleitung, Schritt 1).
                        cell.mask = newMask;
                        cell.virgin = false;
                        cell.zA = 0; // Gruppe A ("ausserhalb") - noch nichts bekannt
                        T minB = std::numeric_limits<T>::max();
                        for (int p = 0; p < CELL_PIXELS; ++p)
                            if ((newMask >> p) & 1u)
                                minB = std::min(minB, newZ[p]);
                        cell.zB = minB;
                        return;
                    }

                    // Weiteres Dreieck: Partition (mask) bleibt FEST, nur die
                    // beiden Gruppenwerte werden gemaess Herleitung Schritt 2
                    // aktualisiert.
                    T candAMin = std::numeric_limits<T>::max();
                    T candBMin = std::numeric_limits<T>::max();
                    bool anyA = false;
                    bool anyB = false;
                    for (int p = 0; p < CELL_PIXELS; ++p)
                    {
                        const bool inGroupB = (cell.mask >> p) & 1u;
                        const T stored = inGroupB ? cell.zB : cell.zA;
                        const bool touchedByNew = (newMask >> p) & 1u;
                        const T candidate = touchedByNew ? std::max(stored, newZ[p]) : stored;
                        if (inGroupB) { candBMin = std::min(candBMin, candidate); anyB = true; }
                        else { candAMin = std::min(candAMin, candidate); anyA = true; }
                    }
                    if (anyA) cell.zA = candAMin;
                    if (anyB) cell.zB = candBMin;
                }


                // -----------------------------------------------------------------
                // SSE-Pfad, wird nur fuer T == float instanziiert (siehe if constexpr
                // in RasterizeTriangle). Verarbeitet eine Zelle (CELL_W x CELL_H =
                // 8x4 Pixel) in 4er-Bloecken - 2 SSE-Gruppen pro Zeile.
                // -----------------------------------------------------------------
                void RasterizeCellSSE(
                    int cellPixelX0, int cellPixelY0,
                    int minX, int maxX, int minY, int maxY,
                    T A0, T B0, T C0,
                    T A1, T B1, T C1,
                    T A2, T B2, T C2,
                    T area, bool signPositive,
                    T w0, T w1, T w2,
                    uint32_t& newMask, T* newZ)
                {
                    const __m128 vA0 = _mm_set1_ps(A0), vA1 = _mm_set1_ps(A1), vA2 = _mm_set1_ps(A2);
                    const __m128 vB0 = _mm_set1_ps(B0), vB1 = _mm_set1_ps(B1), vB2 = _mm_set1_ps(B2);
                    const __m128 vC0 = _mm_set1_ps(C0), vC1 = _mm_set1_ps(C1), vC2 = _mm_set1_ps(C2);
                    const __m128 vW0 = _mm_set1_ps(w0), vW1 = _mm_set1_ps(w1), vW2 = _mm_set1_ps(w2);
                    const __m128 vInvArea = _mm_set1_ps((T)1 / area);
                    const __m128 vZero = _mm_setzero_ps();
                    const __m128 vHalf = _mm_set1_ps((T)0.5);
                    const __m128 lane = _mm_set_ps(3.0f, 2.0f, 1.0f, 0.0f); // Pixel-Offsets in der 4er-Gruppe
                    const __m128 vMinX = _mm_set1_ps((T)minX);
                    const __m128 vMaxX = _mm_set1_ps((T)maxX);

                    for (int ly = 0; ly < CELL_H; ++ly)
                    {
                        const int py = cellPixelY0 + ly;
                        if (py < minY || py > maxY) continue;
                        const T sy = (T)py + (T)0.5;
                        const __m128 vSy = _mm_set1_ps(sy);

                        // Zeilenanteil (B*sy + C) - pro Pixel bleibt nur noch A*sx uebrig.
                        const __m128 rowC0 = _mm_add_ps(_mm_mul_ps(vB0, vSy), vC0);
                        const __m128 rowC1 = _mm_add_ps(_mm_mul_ps(vB1, vSy), vC1);
                        const __m128 rowC2 = _mm_add_ps(_mm_mul_ps(vB2, vSy), vC2);

                        // CELL_W = 8 -> zwei 4er-Bloecke pro Zeile
                        for (int groupX = 0; groupX < CELL_W; groupX += 4)
                        {
                            const int px0 = cellPixelX0 + groupX;
                            const __m128 vPx = _mm_add_ps(_mm_set1_ps((T)px0), lane);
                            const __m128 vSx = _mm_add_ps(vPx, vHalf);

                            const __m128 e0 = _mm_add_ps(_mm_mul_ps(vA0, vSx), rowC0);
                            const __m128 e1 = _mm_add_ps(_mm_mul_ps(vA1, vSx), rowC1);
                            const __m128 e2 = _mm_add_ps(_mm_mul_ps(vA2, vSx), rowC2);

                            __m128 insideMask = signPositive
                                ? _mm_and_ps(_mm_and_ps(_mm_cmpge_ps(e0, vZero), _mm_cmpge_ps(e1, vZero)), _mm_cmpge_ps(e2, vZero))
                                : _mm_and_ps(_mm_and_ps(_mm_cmple_ps(e0, vZero), _mm_cmple_ps(e1, vZero)), _mm_cmple_ps(e2, vZero));

                            // Scissor/Bounds in X (Y ist oben schon geprueft)
                            const __m128 boundMask = _mm_and_ps(_mm_cmpge_ps(vPx, vMinX), _mm_cmple_ps(vPx, vMaxX));
                            insideMask = _mm_and_ps(insideMask, boundMask);

                            const int laneMask = _mm_movemask_ps(insideMask); // Bit k = Pixel groupX+k
                            if (laneMask == 0)
                                continue;

                            // invW = (e0*w0 + e1*w1 + e2*w2) / area, fuer alle 4 Pixel gleichzeitig
                            const __m128 invW = _mm_mul_ps(
                                _mm_add_ps(_mm_add_ps(_mm_mul_ps(e0, vW0), _mm_mul_ps(e1, vW1)), _mm_mul_ps(e2, vW2)),
                                vInvArea);

                            alignas(16) T tmp[4];
                            _mm_store_ps(tmp, invW);

                            const int bitBase = ly * CELL_W + groupX;
                            for (int k = 0; k < 4; ++k)
                            {
                                if (laneMask & (1 << k))
                                {
                                    const int bit = bitBase + k;
                                    newMask |= (1u << bit);
                                    newZ[bit] = tmp[k];
                                }
                            }
                        }
                    }
                }

                // -----------------------------------------------------------------
                // AVX-Pfad, nur fuer T == float. Verarbeitet eine ganze Zeile der
                // Zelle (CELL_W = 8 Pixel) pro Iteration - eine __m256 pro Zeile,
                // 4 Zeilen pro Zelle. Kein innerer 4er-Block mehr noetig.
                // -----------------------------------------------------------------
                void RasterizeCellAVX(
                    int cellPixelX0, int cellPixelY0,
                    int minX, int maxX, int minY, int maxY,
                    T A0, T B0, T C0,
                    T A1, T B1, T C1,
                    T A2, T B2, T C2,
                    T area, bool signPositive,
                    T w0, T w1, T w2,
                    uint32_t& newMask, T* newZ)
                {
                    const __m256 vA0 = _mm256_set1_ps(A0), vA1 = _mm256_set1_ps(A1), vA2 = _mm256_set1_ps(A2);
                    const __m256 vB0 = _mm256_set1_ps(B0), vB1 = _mm256_set1_ps(B1), vB2 = _mm256_set1_ps(B2);
                    const __m256 vC0 = _mm256_set1_ps(C0), vC1 = _mm256_set1_ps(C1), vC2 = _mm256_set1_ps(C2);
                    const __m256 vW0 = _mm256_set1_ps(w0), vW1 = _mm256_set1_ps(w1), vW2 = _mm256_set1_ps(w2);
                    const __m256 vInvArea = _mm256_set1_ps((T)1 / area);
                    const __m256 vZero = _mm256_setzero_ps();
                    const __m256 vHalf = _mm256_set1_ps((T)0.5);
                    // Pixel-Offsets 0..7 innerhalb der Zeile (CELL_W = 8)
                    const __m256 lane = _mm256_set_ps(7.0f, 6.0f, 5.0f, 4.0f, 3.0f, 2.0f, 1.0f, 0.0f);
                    const __m256 vMinX = _mm256_set1_ps((T)minX);
                    const __m256 vMaxX = _mm256_set1_ps((T)maxX);
                    const __m256 vPx0 = _mm256_add_ps(_mm256_set1_ps((T)cellPixelX0), lane);
                    const __m256 vSx = _mm256_add_ps(vPx0, vHalf);
                    const __m256 boundMaskX = _mm256_and_ps(_mm256_cmp_ps(vPx0, vMinX, _CMP_GE_OQ),
                        _mm256_cmp_ps(vPx0, vMaxX, _CMP_LE_OQ));

                    for (int ly = 0; ly < CELL_H; ++ly)
                    {
                        const int py = cellPixelY0 + ly;
                        if (py < minY || py > maxY) continue;
                        const T sy = (T)py + (T)0.5;
                        const __m256 vSy = _mm256_set1_ps(sy);

                        const __m256 rowC0 = _mm256_add_ps(_mm256_mul_ps(vB0, vSy), vC0);
                        const __m256 rowC1 = _mm256_add_ps(_mm256_mul_ps(vB1, vSy), vC1);
                        const __m256 rowC2 = _mm256_add_ps(_mm256_mul_ps(vB2, vSy), vC2);

                        const __m256 e0 = _mm256_add_ps(_mm256_mul_ps(vA0, vSx), rowC0);
                        const __m256 e1 = _mm256_add_ps(_mm256_mul_ps(vA1, vSx), rowC1);
                        const __m256 e2 = _mm256_add_ps(_mm256_mul_ps(vA2, vSx), rowC2);

                        __m256 insideMask = signPositive
                            ? _mm256_and_ps(_mm256_and_ps(_mm256_cmp_ps(e0, vZero, _CMP_GE_OQ), _mm256_cmp_ps(e1, vZero, _CMP_GE_OQ)), _mm256_cmp_ps(e2, vZero, _CMP_GE_OQ))
                            : _mm256_and_ps(_mm256_and_ps(_mm256_cmp_ps(e0, vZero, _CMP_LE_OQ), _mm256_cmp_ps(e1, vZero, _CMP_LE_OQ)), _mm256_cmp_ps(e2, vZero, _CMP_LE_OQ));

                        insideMask = _mm256_and_ps(insideMask, boundMaskX);

                        const int laneMask = _mm256_movemask_ps(insideMask); // Bit k = Pixel k dieser Zeile
                        if (laneMask == 0)
                            continue;

                        const __m256 invW = _mm256_mul_ps(
                            _mm256_add_ps(_mm256_add_ps(_mm256_mul_ps(e0, vW0), _mm256_mul_ps(e1, vW1)), _mm256_mul_ps(e2, vW2)),
                            vInvArea);

                        alignas(32) T tmp[8];
                        _mm256_store_ps(tmp, invW);

                        const int bitBase = ly * CELL_W;
                        for (int k = 0; k < 8; ++k)
                        {
                            if (laneMask & (1 << k))
                            {
                                const int bit = bitBase + k;
                                newMask |= (1u << bit);
                                newZ[bit] = tmp[k];
                            }
                        }
                    }
                }
                void RasterizeTriangle(
                    T x0, T y0, T w0,
                    T x1, T y1, T w1,
                    T x2, T y2, T w2)
                {
                    const T minXf = std::min({ x0, x1, x2 });
                    const T maxXf = std::max({ x0, x1, x2 });
                    const T minYf = std::min({ y0, y1, y2 });
                    const T maxYf = std::max({ y0, y1, y2 });
                    int minX = std::max(0, static_cast<int>(std::floor(minXf)));
                    int maxX = std::min(m_width - 1, static_cast<int>(std::ceil(maxXf)));
                    int minY = std::max(0, static_cast<int>(std::floor(minYf)));
                    int maxY = std::min(m_height - 1, static_cast<int>(std::ceil(maxYf)));
                    if (minX > maxX || minY > maxY)
                        return;

                    const T area = EdgeFn(x0, y0, x1, y1, x2, y2);
                    if (std::abs(area) < static_cast<T>(1e-8))
                        return;

                    const T A0 = y1 - y2, B0 = x2 - x1, C0 = (y2 - y1) * x1 - (x2 - x1) * y1;
                    const T A1 = y2 - y0, B1 = x0 - x2, C1 = (y0 - y2) * x2 - (x0 - x2) * y2;
                    const T A2 = y0 - y1, B2 = x1 - x0, C2 = (y1 - y0) * x0 - (x1 - x0) * y0;
                    const bool signPositive = area >= (T)0;

                    const int cellMinX = minX / CELL_W;
                    const int cellMaxX = maxX / CELL_W;
                    const int cellMinY = minY / CELL_H;
                    const int cellMaxY = maxY / CELL_H;

                    T newZ[CELL_PIXELS];
                    for (int cy = cellMinY; cy <= cellMaxY; ++cy)
                    {
                        for (int cx = cellMinX; cx <= cellMaxX; ++cx)
                        {
                            const int cellPixelX0 = cx * CELL_W;
                            const int cellPixelY0 = cy * CELL_H;
                            uint32_t newMask = 0;

                            if constexpr (std::is_same_v<T, float>)
                            {
#if defined(NG_USE_AVX)
                                RasterizeCellAVX(cellPixelX0, cellPixelY0, minX, maxX, minY, maxY,
                                    A0, B0, C0, A1, B1, C1, A2, B2, C2,
                                    area, signPositive, w0, w1, w2, newMask, newZ);
#else
                                RasterizeCellSSE(cellPixelX0, cellPixelY0, minX, maxX, minY, maxY,
                                    A0, B0, C0, A1, B1, C1, A2, B2, C2,
                                    area, signPositive, w0, w1, w2, newMask, newZ);
#endif
                            }
                            else
                            {
                                // Skalarer Fallback, unveraendert - fuer T = double.
                                for (int ly = 0; ly < CELL_H; ++ly)
                                {
                                    const int py = cellPixelY0 + ly;
                                    if (py < minY || py > maxY) continue;
                                    for (int lx = 0; lx < CELL_W; ++lx)
                                    {
                                        const int px = cellPixelX0 + lx;
                                        if (px < minX || px > maxX) continue;
                                        const T sx = (T)px + (T)0.5;
                                        const T sy = (T)py + (T)0.5;
                                        const T e0 = A0 * sx + B0 * sy + C0;
                                        const T e1 = A1 * sx + B1 * sy + C1;
                                        const T e2 = A2 * sx + B2 * sy + C2;
                                        const bool inside = signPositive
                                            ? (e0 >= 0 && e1 >= 0 && e2 >= 0)
                                            : (e0 <= 0 && e1 <= 0 && e2 <= 0);
                                        if (!inside) continue;
                                        const T invW = (e0 * w0 + e1 * w1 + e2 * w2) / area;
                                        const int bit = ly * CELL_W + lx;
                                        newMask |= (1u << bit);
                                        newZ[bit] = invW;
                                    }
                                }
                            }

                            if (newMask == 0) continue;
                            Cell& cell = m_cells[static_cast<size_t>(cy) * m_cellsX + cx];
                            MergeCell(cell, newMask, newZ);
                        }
                    }
                }
                // Einfacher Edge-Function-Rasterizer. 1/w ist affin/linear im
                // Bildschirmraum (Standard-Grafik-Fakt), deshalb reicht simple
                // baryzentrische Interpolation - KEINE perspektivische Korrektur
                // noetig, weil wir genau diesen Wert selbst interpolieren.
                void RasterizeTriangleSimpel(
                    T x0, T y0, T w0,
                    T x1, T y1, T w1,
                    T x2, T y2, T w2)
                {
                    const T minXf = std::min({ x0, x1, x2 });
                    const T maxXf = std::max({ x0, x1, x2 });
                    const T minYf = std::min({ y0, y1, y2 });
                    const T maxYf = std::max({ y0, y1, y2 });

                    int minX = std::max(0, static_cast<int>(std::floor(minXf)));
                    int maxX = std::min(m_width - 1, static_cast<int>(std::ceil(maxXf)));
                    int minY = std::max(0, static_cast<int>(std::floor(minYf)));
                    int maxY = std::min(m_height - 1, static_cast<int>(std::ceil(maxYf)));
                    if (minX > maxX || minY > maxY)
                        return;

                    const T area = EdgeFn(x0, y0, x1, y1, x2, y2);
                    if (std::abs(area) < static_cast<T>(1e-8))
                        return; // entartetes Dreieck

                    const int cellMinX = minX / CELL_W;
                    const int cellMaxX = maxX / CELL_W;
                    const int cellMinY = minY / CELL_H;
                    const int cellMaxY = maxY / CELL_H;

                    T newZ[CELL_PIXELS];
                    for (int cy = cellMinY; cy <= cellMaxY; ++cy)
                    {
                        for (int cx = cellMinX; cx <= cellMaxX; ++cx)
                        {
                            const int cellPixelX0 = cx * CELL_W;
                            const int cellPixelY0 = cy * CELL_H;
                            uint32_t newMask = 0;
                            for (int ly = 0; ly < CELL_H; ++ly)
                            {
                                const int py = cellPixelY0 + ly;
                                if (py < minY || py > maxY) continue;
                                for (int lx = 0; lx < CELL_W; ++lx)
                                {
                                    const int px = cellPixelX0 + lx;
                                    if (px < minX || px > maxX) continue;
                                    const T sx = static_cast<T>(px) + static_cast<T>(0.5);
                                    const T sy = static_cast<T>(py) + static_cast<T>(0.5);
                                    const T e0 = EdgeFn(x1, y1, x2, y2, sx, sy);
                                    const T e1 = EdgeFn(x2, y2, x0, y0, sx, sy);
                                    const T e2 = EdgeFn(x0, y0, x1, y1, sx, sy);
                                    const bool inside =
                                        (e0 >= 0 && e1 >= 0 && e2 >= 0) ||
                                        (e0 <= 0 && e1 <= 0 && e2 <= 0);
                                    if (!inside) continue;
                                    const T b0 = e0 / area;
                                    const T b1 = e1 / area;
                                    const T b2 = e2 / area;
                                    const T invW = b0 * w0 + b1 * w1 + b2 * w2;
                                    const int bit = ly * CELL_W + lx;
                                    newMask |= (1u << bit);
                                    newZ[bit] = invW;
                                }
                            }
                            if (newMask == 0)
                                continue;
                            Cell& cell = m_cells[static_cast<size_t>(cy) * m_cellsX + cx];
                            MergeCell(cell, newMask, newZ);
                        }
                    }
                }
            };

        } // namespace occlusion
    } // namespace renderer
} // namespace ng