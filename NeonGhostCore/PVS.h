#pragma once

#pragma once

// ----------------------------------------------------------------------
// PVS.h - Potentially Visible Set fuer den BSPTree
//
// Verfahren: klassischer Portal-Flow (wie Quake "vis")
//   1. Jedes Portal wird in zwei gerichtete Portale zerlegt
//      (Leaf A -> Leaf B und Leaf B -> Leaf A).
//   2. BasePortalVis: grobe, konservative Vorauswahl "mightsee"
//      pro gerichtetem Portal (reine Ebenentests + Flood).
//   3. PortalFlow: pro Portal wird ueber die Nachbar-Leaves geflutet.
//      Portale werden dabei gegen Separator-Ebenen (Kante des einen +
//      Punkt des anderen Portals) geclippt. Was komplett wegfaellt, ist
//      nicht sichtbar. Ergebnis: portalvis + leafvis.
//   4. LeafFlow: Sichtbarkeit eines Leaves = ODER ueber die leafvis
//      aller Portale, die das Leaf verlassen (+ das Leaf selbst).
//   5. Jede Zeile wird wie in Quake lauflaengen-komprimiert gespeichert.
//
// Voraussetzung (Reihenfolge):
//   tree.Build(...); tree.BuildPortals();
//   auto reached = tree.FloodFromFlag();
//   tree.RemoveReachedPolygons(reached);     // markiert Void-Leaves INTOTHEVOID
//   pvs.Build(tree);
//
// Alles ist iterativ (kein Rekursions-Stack), Portal-Flow laeuft optional
// auf mehreren Threads.
// ----------------------------------------------------------------------

#include "BSP.h"

#include <vector>
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <atomic>
#include <thread>
#include <chrono>
#include <memory>
#include <numeric>

namespace ng
{
    namespace bsp
    {
        template <class T>
        struct PVSOptions
        {
            // Toleranz in Weltunits (wie ON_EPSILON in Quake vis).
            T epsilon = T(0.1);

            // 0 = hardware_concurrency()
            unsigned threads = 0;

            // Fortschritt per printf
            bool verbose = true;

            // Leaves mit INTOTHEVOID (vom Flood erreicht) werden ignoriert.
            bool skipVoidLeaves = true;

            // Nur Zusammenhangskomponenten des Portalgraphen, in denen mindestens
            // ein Leaf Polygone enthaelt, sind "begehbar". Filtert das Innere von
            // Brushes (Zellen ohne jede Geometrie, vom Rest abgeschlossen) heraus.
            bool requirePolygonsInComponent = true;

            // Portale kleiner als diese Flaeche werden ignoriert.
            T minPortalArea = T(0.01);
        };

        namespace pvs_detail
        {
            using Bits = std::vector<uint64_t>;

            inline bool BitGet(const Bits& b, size_t i)
            {
                return ((b[i >> 6] >> (i & 63)) & 1ull) != 0;
            }

            inline void BitSetOn(Bits& b, size_t i)
            {
                b[i >> 6] |= (1ull << (i & 63));
            }

            inline size_t BitCount(const Bits& b)
            {
                size_t c = 0;
                for (uint64_t w : b)
                {
                    while (w) { w &= (w - 1); ++c; }
                }
                return c;
            }

            template <class T>
            inline T Dot3(const ng::math::Matrix<3, 1, T>& a, const ng::math::Matrix<3, 1, T>& b)
            {
                return a(0) * b(0) + a(1) * b(1) + a(2) * b(2);
            }

            template <class T>
            inline ng::math::Matrix<3, 1, T> Cross3(const ng::math::Matrix<3, 1, T>& a,
                const ng::math::Matrix<3, 1, T>& b)
            {
                return ng::math::Matrix<3, 1, T>(
                    a(1) * b(2) - a(2) * b(1),
                    a(2) * b(0) - a(0) * b(2),
                    a(0) * b(1) - a(1) * b(0));
            }

            // Behaelt den Teil vor der Ebene (dot(n,p) - dist > 0).
            // Liegt kein Punkt echt davor, ist das Ergebnis leer.
            template <class T>
            std::vector<ng::math::Matrix<3, 1, T>> ClipFront(
                const std::vector<ng::math::Matrix<3, 1, T>>& in,
                const ng::math::Matrix<3, 1, T>& n, T dist, T eps)
            {
                using Vec3 = ng::math::Matrix<3, 1, T>;

                const size_t cnt = in.size();
                if (cnt < 3) return std::vector<Vec3>();

                std::vector<T> dists(cnt + 1);
                std::vector<int> sides(cnt + 1);   // 0 = front, 1 = back, 2 = on
                size_t counts[3] = { 0, 0, 0 };

                for (size_t i = 0; i < cnt; ++i)
                {
                    const T d = Dot3(in[i], n) - dist;
                    dists[i] = d;
                    sides[i] = (d > eps) ? 0 : ((d < -eps) ? 1 : 2);
                    ++counts[sides[i]];
                }
                dists[cnt] = dists[0];
                sides[cnt] = sides[0];

                if (!counts[0]) return std::vector<Vec3>();
                if (!counts[1]) return in;

                std::vector<Vec3> out;
                out.reserve(cnt + 4);

                for (size_t i = 0; i < cnt; ++i)
                {
                    const Vec3& p1 = in[i];

                    if (sides[i] == 2)
                    {
                        out.push_back(p1);
                        continue;
                    }

                    if (sides[i] == 0)
                        out.push_back(p1);

                    if (sides[i + 1] == 2 || sides[i + 1] == sides[i])
                        continue;

                    const Vec3& p2 = in[(i + 1) % cnt];
                    const T t = dists[i] / (dists[i] - dists[i + 1]);
                    out.push_back(p1 + (p2 - p1) * t);
                }

                return out;
            }

            // ----------------------------------------------------------
            // ClipToSeparators (wie Quake)
            //
            // Erzeugt Trennebenen aus einer Kante von "source" und einem
            // Punkt von "pass". Liegt "pass" komplett auf der einen und
            // "source" auf der anderen Seite, ist das eine Trennebene und
            // "target" wird auf die Seite von "pass" beschnitten.
            // Ist target danach leer, ist es durch diese Oeffnungen nicht
            // sichtbar. flipclip = Reihenfolge (pass, source, target).
            // ----------------------------------------------------------
            template <class T>
            std::vector<ng::math::Matrix<3, 1, T>> ClipToSeparators(
                const std::vector<ng::math::Matrix<3, 1, T>>& source,
                const std::vector<ng::math::Matrix<3, 1, T>>& pass,
                std::vector<ng::math::Matrix<3, 1, T>> target,
                bool flipclip, T eps)
            {
                using Vec3 = ng::math::Matrix<3, 1, T>;

                const size_t sn = source.size();
                const size_t pn = pass.size();

                for (size_t i = 0; i < sn; ++i)
                {
                    const size_t l = (i + 1) % sn;
                    const Vec3 v1 = source[l] - source[i];

                    for (size_t j = 0; j < pn; ++j)
                    {
                        const Vec3 v2 = pass[j] - source[i];

                        Vec3 normal = Cross3(v1, v2);
                        const T length = Dot3(normal, normal);

                        // Kein gueltige Ebene
                        if (length < eps)
                            continue;

                        normal = normal * (T(1) / std::sqrt(length));
                        T dist = Dot3(pass[j], normal);

                        // Auf welcher Seite liegt source?
                        bool fliptest = false;
                        size_t k = 0;
                        for (k = 0; k < sn; ++k)
                        {
                            if (k == i || k == l) continue;

                            const T d = Dot3(source[k], normal) - dist;
                            if (d < -eps) { fliptest = false; break; }
                            if (d > eps) { fliptest = true;  break; }
                        }
                        if (k == sn)
                            continue;   // planar mit source

                        if (fliptest)
                        {
                            normal = normal * T(-1);
                            dist = -dist;
                        }

                        // Alle Punkte von pass muessen auf der positiven Seite liegen
                        int front = 0;
                        size_t kk = 0;
                        for (kk = 0; kk < pn; ++kk)
                        {
                            if (kk == j) continue;

                            const T d = Dot3(pass[kk], normal) - dist;
                            if (d < -eps) break;
                            if (d > eps) ++front;
                        }
                        if (kk != pn) continue;   // keine Trennebene
                        if (!front)   continue;   // planar mit Trennebene

                        if (flipclip)
                        {
                            normal = normal * T(-1);
                            dist = -dist;
                        }

                        target = ClipFront(target, normal, dist, eps);
                        if (target.size() < 3)
                            return std::vector<Vec3>();
                    }
                }

                return target;
            }

            template <class F>
            void ParallelFor(size_t count, unsigned threads, F&& fn)
            {
                if (threads <= 1 || count < 2)
                {
                    for (size_t i = 0; i < count; ++i) fn(i);
                    return;
                }

                std::atomic<size_t> next(0);

                auto worker = [&]()
                    {
                        for (;;)
                        {
                            const size_t i = next.fetch_add(1);
                            if (i >= count) break;
                            fn(i);
                        }
                    };

                std::vector<std::thread> pool;
                for (unsigned t = 1; t < threads; ++t)
                    pool.emplace_back(worker);

                worker();

                for (auto& th : pool) th.join();
            }

            inline double SecondsSince(const std::chrono::steady_clock::time_point& t0)
            {
                return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
            }
        }

        // ------------------------------------------------------------
        // PVS
        // ------------------------------------------------------------

        template <class T>
        class PVS
        {
        public:
            using Vec3 = ng::math::Matrix<3, 1, T>;

            PVS() {}

            // Baut das PVS. Rueckgabe false, wenn keine nutzbaren Portale existieren.
            bool Build(const BSPTree<T>& tree, const PVSOptions<T>& opt = PVSOptions<T>())
            {
                using namespace pvs_detail;

                Clear();

                const auto t0 = std::chrono::steady_clock::now();

                const auto& leaves = tree.GetLeaves();
                const auto& tportals = tree.GetPortals();

                m_leafCount = leaves.size();
                m_active.assign(m_leafCount, 0);
                m_rows.assign(m_leafCount, std::vector<uint8_t>());

                if (m_leafCount == 0)
                    return false;

                const T eps = opt.epsilon;

                // ------------------------------------------------------
                // 1. Aktive (begehbare) Leaves bestimmen
                // ------------------------------------------------------

                std::vector<uint8_t> notVoid(m_leafCount, 1);
                if (opt.skipVoidLeaves)
                {
                    for (size_t i = 0; i < m_leafCount; ++i)
                        if (leaves[i]->State() & INTOTHEVOID)
                            notVoid[i] = 0;
                }

                std::vector<int32_t> parent(m_leafCount);
                std::iota(parent.begin(), parent.end(), 0);

                auto findRoot = [&](int32_t x)
                    {
                        while (parent[x] != x)
                        {
                            parent[x] = parent[parent[x]];
                            x = parent[x];
                        }
                        return x;
                    };

                const int32_t lc = (int32_t)m_leafCount;

                for (const auto& tp : tportals)
                {
                    const int32_t a = tp.leafFront();
                    const int32_t b = tp.leafBack();
                    if (a < 0 || b < 0 || a >= lc || b >= lc || a == b) continue;
                    if (!notVoid[a] || !notVoid[b]) continue;

                    const int32_t ra = findRoot(a);
                    const int32_t rb = findRoot(b);
                    if (ra != rb) parent[ra] = rb;
                }

                std::vector<uint8_t> compHasPolys(m_leafCount, 0);
                for (size_t i = 0; i < m_leafCount; ++i)
                {
                    if (notVoid[i] && !leaves[i]->GetPolygons().empty())
                        compHasPolys[findRoot((int32_t)i)] = 1;
                }

                size_t activeCount = 0;
                for (size_t i = 0; i < m_leafCount; ++i)
                {
                    bool act = notVoid[i] != 0;
                    if (act && opt.requirePolygonsInComponent)
                        act = compHasPolys[findRoot((int32_t)i)] != 0;

                    m_active[i] = act ? 1 : 0;
                    if (act) ++activeCount;
                }

                // ------------------------------------------------------
                // 2. Gerichtete Portale erzeugen
                // ------------------------------------------------------

                m_leafPortals.assign(m_leafCount, std::vector<size_t>());

                for (const auto& tp : tportals)
                {
                    const int32_t a = tp.leafFront();
                    const int32_t b = tp.leafBack();
                    if (a < 0 || b < 0 || a >= lc || b >= lc || a == b) continue;
                    if (!m_active[a] || !m_active[b]) continue;
                    if (tp.verts().size() < 3) continue;
                    if (PortalArea(tp.verts()) < opt.minPortalArea) continue;

                    Vec3 n = tp.plane().normal();
                    const T nl = n.length();
                    if (nl <= T(1e-12)) continue;
                    n = n * (T(1) / nl);

                    // BSP-Ebene: Normale zeigt zum FRONT-Leaf.
                    AddDirected(tp.verts(), n, b, a);                 // Back  -> Front
                    AddDirected(tp.verts(), n * T(-1), a, b);         // Front -> Back
                }

                const size_t P = m_portals.size();

                if (P == 0)
                {
                    if (opt.verbose)
                        printf("[PVS] keine nutzbaren Portale.\n");
                    // Jedes aktive Leaf sieht zumindest sich selbst.
                    FinishRowsWithoutPortals();
                    return false;
                }

                m_portalWords = (P + 63) / 64;
                m_leafWords = (m_leafCount + 63) / 64;

                for (auto& fp : m_portals)
                {
                    fp.mightsee.assign(m_portalWords, 0);
                    fp.portalvis.assign(m_portalWords, 0);
                    fp.leafvis.assign(m_leafWords, 0);
                }

                unsigned threads = opt.threads;
                if (threads == 0)
                {
                    threads = std::thread::hardware_concurrency();
                    if (threads == 0) threads = 1;
                }

                if (opt.verbose)
                    printf("[PVS] aktive Leaves=%zu von %zu, gerichtete Portale=%zu, Threads=%u\n",
                        activeCount, m_leafCount, P, threads);

                // ------------------------------------------------------
                // 3. Base-Vis
                // ------------------------------------------------------

                ParallelFor(P, threads, [&](size_t i) { BasePortalVis(i, eps); });

                if (opt.verbose)
                    printf("[PVS] BasePortalVis fertig (%.1fs)\n", SecondsSince(t0));

                // ------------------------------------------------------
                // 4. Portal-Flow, Portale mit wenig Kandidaten zuerst
                // ------------------------------------------------------

                std::vector<size_t> order(P);
                std::iota(order.begin(), order.end(), size_t(0));
                std::sort(order.begin(), order.end(),
                    [&](size_t a, size_t b) { return m_portals[a].numMightSee < m_portals[b].numMightSee; });

                m_status.reset(new std::atomic<int>[P]);
                for (size_t i = 0; i < P; ++i) m_status[i].store(0);

                std::atomic<size_t> done(0);

                ParallelFor(P, threads, [&](size_t k)
                    {
                        const size_t pi = order[k];
                        PortalFlow(pi, eps);
                        m_status[pi].store(2, std::memory_order_release);

                        const size_t d = done.fetch_add(1) + 1;
                        if (opt.verbose && (d % 200) == 0)
                            printf("[PVS] Portal-Flow %zu / %zu  (%.1fs)\n", d, P, SecondsSince(t0));
                    });

                // ------------------------------------------------------
                // 5. Leaf-Flow + Kompression
                // ------------------------------------------------------

                const size_t rowBytes = (m_leafCount + 7) >> 3;
                size_t visibleSum = 0;
                size_t compressedBytes = 0;

                for (size_t leaf = 0; leaf < m_leafCount; ++leaf)
                {
                    if (!m_active[leaf]) continue;

                    Bits acc(m_leafWords, 0);
                    for (size_t pi : m_leafPortals[leaf])
                    {
                        const Bits& lv = m_portals[pi].leafvis;
                        for (size_t w = 0; w < m_leafWords; ++w)
                            acc[w] |= lv[w];
                    }
                    BitSetOn(acc, leaf);   // jedes Leaf sieht sich selbst

                    std::vector<uint8_t> row(rowBytes, 0);
                    for (size_t i = 0; i < m_leafCount; ++i)
                    {
                        if (BitGet(acc, i))
                        {
                            row[i >> 3] |= (uint8_t)(1u << (i & 7));
                            ++visibleSum;
                        }
                    }

                    CompressRow(row, m_rows[leaf]);
                    compressedBytes += m_rows[leaf].size();
                }

                if (opt.verbose)
                {
                    printf("[PVS] fertig (%.1fs): im Schnitt %.1f sichtbare Leaves pro Leaf, %zu Bytes komprimiert\n",
                        SecondsSince(t0),
                        activeCount ? double(visibleSum) / double(activeCount) : 0.0,
                        compressedBytes);
                }

                // Rechendaten freigeben, nur die Zeilen bleiben.
                m_portals.clear();
                m_portals.shrink_to_fit();
                m_leafPortals.clear();
                m_leafPortals.shrink_to_fit();
                m_status.reset();

                m_built = true;
                return true;
            }

            void Clear()
            {
                m_built = false;
                m_leafCount = 0;
                m_active.clear();
                m_rows.clear();
                m_portals.clear();
                m_leafPortals.clear();
                m_status.reset();
            }

            // ------------------------------------------------------
            // Abfragen
            // ------------------------------------------------------

            inline bool IsBuilt() const { return m_built; }
            inline size_t LeafCount() const { return m_leafCount; }

            // Ist das Leaf Teil des begehbaren Bereichs?
            // Steht die Kamera in einem inaktiven Leaf, sollte alles gezeichnet werden.
            inline bool IsActiveLeaf(int32_t leaf) const
            {
                return leaf >= 0 && (size_t)leaf < m_leafCount && m_active[leaf] != 0;
            }

            // Sieht "from" das Leaf "to"? (false bei ungueltigen/inaktiven Leaves)
            bool CanSee(int32_t from, int32_t to) const
            {
                if (!IsActiveLeaf(from) || to < 0 || (size_t)to >= m_leafCount)
                    return false;

                const auto& row = m_rows[from];
                const size_t target = (size_t)to >> 3;

                size_t pos = 0;
                size_t i = 0;

                while (i < row.size() && pos <= target)
                {
                    if (row[i])
                    {
                        if (pos == target)
                            return ((row[i] >> (to & 7)) & 1u) != 0;
                        ++pos;
                        ++i;
                    }
                    else
                    {
                        if (i + 1 >= row.size()) return false;
                        const size_t run = row[i + 1];
                        if (target < pos + run) return false;
                        pos += run;
                        i += 2;
                    }
                }

                return false;
            }

            // Unkomprimierte Zeile (Bit i in Byte i>>3, Bit i&7).
            std::vector<uint8_t> DecompressRow(int32_t leaf) const
            {
                const size_t rowBytes = (m_leafCount + 7) >> 3;
                std::vector<uint8_t> out(rowBytes, 0);

                if (!IsActiveLeaf(leaf)) return out;

                const auto& in = m_rows[leaf];
                size_t i = 0, o = 0;

                while (o < rowBytes && i < in.size())
                {
                    if (in[i])
                    {
                        out[o++] = in[i++];
                        continue;
                    }

                    if (i + 1 >= in.size()) break;
                    o += in[i + 1];
                    i += 2;
                }

                return out;
            }

            // Alle sichtbaren Leaf-IDs.
            std::vector<int32_t> VisibleLeaves(int32_t leaf) const
            {
                std::vector<int32_t> result;
                if (!IsActiveLeaf(leaf)) return result;

                const auto row = DecompressRow(leaf);
                for (size_t i = 0; i < m_leafCount; ++i)
                    if ((row[i >> 3] >> (i & 7)) & 1u)
                        result.push_back((int32_t)i);

                return result;
            }

            // Komprimierte Zeile (zum Speichern in eine Datei).
            inline const std::vector<uint8_t>& CompressedRow(int32_t leaf) const { return m_rows[leaf]; }

        private:
            using Bits = pvs_detail::Bits;

            struct FlowPortal
            {
                std::vector<Vec3> verts;
                Vec3 normal;           // zeigt in Flussrichtung (in Leaf "to")
                T dist = T(0);         // dot(normal, p) == dist auf dem Portal
                Vec3 origin;           // Schwerpunkt
                T radius = T(0);       // Huellkugel
                int32_t from = -1;
                int32_t to = -1;

                Bits mightsee;         // Portale (Base-Vis)
                Bits portalvis;        // Portale (Ergebnis)
                Bits leafvis;          // Leaves  (Ergebnis)
                size_t numMightSee = 0;
            };

            struct Frame
            {
                int32_t leaf = -1;
                int32_t from = -1;
                size_t next = 0;
                std::vector<Vec3> source;
                std::vector<Vec3> pass;
                bool hasPass = false;
                Vec3 entryN;
                T entryD = T(0);
                Bits mightsee;
            };

            bool m_built = false;
            size_t m_leafCount = 0;
            size_t m_portalWords = 0;
            size_t m_leafWords = 0;

            std::vector<uint8_t> m_active;
            std::vector<std::vector<uint8_t>> m_rows;

            // Nur waehrend Build()
            std::vector<FlowPortal> m_portals;
            std::vector<std::vector<size_t>> m_leafPortals;   // Portale, die das Leaf VERLASSEN
            std::unique_ptr<std::atomic<int>[]> m_status;     // 0 offen, 2 fertig

            void AddDirected(const std::vector<Vec3>& verts, const Vec3& n, int32_t from, int32_t to)
            {
                using namespace pvs_detail;

                FlowPortal fp;
                fp.verts = verts;
                fp.normal = n;
                fp.from = from;
                fp.to = to;

                Vec3 c(0, 0, 0);
                T d = T(0);
                for (const auto& v : verts)
                {
                    c = c + v;
                    d += Dot3(n, v);
                }
                const T inv = T(1) / T(verts.size());
                c = c * inv;
                fp.origin = c;
                fp.dist = d * inv;

                T r = T(0);
                for (const auto& v : verts)
                    r = std::max(r, (v - c).length());
                fp.radius = r;

                m_leafPortals[from].push_back(m_portals.size());
                m_portals.push_back(std::move(fp));
            }

            void FinishRowsWithoutPortals()
            {
                const size_t rowBytes = (m_leafCount + 7) >> 3;
                for (size_t leaf = 0; leaf < m_leafCount; ++leaf)
                {
                    if (!m_active[leaf]) continue;
                    std::vector<uint8_t> row(rowBytes, 0);
                    row[leaf >> 3] |= (uint8_t)(1u << (leaf & 7));
                    CompressRow(row, m_rows[leaf]);
                }
                m_built = true;
            }

            static void CompressRow(const std::vector<uint8_t>& src, std::vector<uint8_t>& dst)
            {
                dst.clear();
                const size_t n = src.size();

                for (size_t j = 0; j < n; ++j)
                {
                    dst.push_back(src[j]);
                    if (src[j]) continue;

                    size_t c = 1;
                    for (++j; j < n; ++j)
                    {
                        if (src[j] || c == 255) break;
                        ++c;
                    }
                    --j;

                    dst.push_back(static_cast<uint8_t>(c));
                }
            }

            // ------------------------------------------------------
            // BasePortalVis
            //
            // Portal j ist fuer Portal i "moeglicherweise sichtbar", wenn
            //  - j mindestens einen Punkt vor der Ebene von i hat und
            //  - i mindestens einen Punkt hinter der Ebene von j hat
            // und j ueber eine Kette solcher Portale erreichbar ist.
            // ------------------------------------------------------
            void BasePortalVis(size_t i, T eps)
            {
                using namespace pvs_detail;

                FlowPortal& p = m_portals[i];
                const size_t P = m_portals.size();

                std::vector<uint8_t> portalsee(P, 0);

                for (size_t j = 0; j < P; ++j)
                {
                    if (j == i) continue;

                    const FlowPortal& tp = m_portals[j];

                    // tp muss einen Punkt vor p haben
                    T d = Dot3(tp.origin, p.normal) - p.dist;
                    if (d < -tp.radius)
                        continue;
                    if (d <= tp.radius)
                    {
                        bool any = false;
                        for (const auto& v : tp.verts)
                        {
                            if (Dot3(v, p.normal) - p.dist > eps) { any = true; break; }
                        }
                        if (!any) continue;
                    }

                    // p muss einen Punkt hinter tp haben
                    d = Dot3(p.origin, tp.normal) - tp.dist;
                    if (d > p.radius)
                        continue;
                    if (d >= -p.radius)
                    {
                        bool any = false;
                        for (const auto& v : p.verts)
                        {
                            if (Dot3(v, tp.normal) - tp.dist < -eps) { any = true; break; }
                        }
                        if (!any) continue;
                    }

                    portalsee[j] = 1;
                }

                // SimpleFlood (iterativ)
                std::vector<int32_t> stack;
                stack.push_back(p.to);

                while (!stack.empty())
                {
                    const int32_t leaf = stack.back();
                    stack.pop_back();

                    for (size_t q : m_leafPortals[leaf])
                    {
                        if (!portalsee[q]) continue;
                        if (BitGet(p.mightsee, q)) continue;

                        BitSetOn(p.mightsee, q);
                        stack.push_back(m_portals[q].to);
                    }
                }

                p.numMightSee = BitCount(p.mightsee);
            }

            // ------------------------------------------------------
            // PortalFlow (iterativ, expliziter Stack)
            // ------------------------------------------------------
            void PortalFlow(size_t si, T eps)
            {
                using namespace pvs_detail;

                FlowPortal& S = m_portals[si];

                std::vector<Frame> stack;
                Bits tmp(m_portalWords, 0);

                {
                    Frame f;
                    f.leaf = S.to;
                    f.from = S.from;
                    f.next = 0;
                    f.source = S.verts;
                    f.hasPass = false;
                    f.entryN = S.normal;
                    f.entryD = S.dist;
                    f.mightsee = S.mightsee;

                    BitSetOn(S.leafvis, (size_t)S.to);
                    stack.push_back(std::move(f));
                }

                while (!stack.empty())
                {
                    Frame& f = stack.back();
                    const auto& lp = m_leafPortals[f.leaf];

                    if (f.next >= lp.size())
                    {
                        stack.pop_back();
                        continue;
                    }

                    const size_t ti = lp[f.next++];
                    if (ti == si) continue;

                    const FlowPortal& Tp = m_portals[ti];

                    if (!BitGet(f.mightsee, ti)) continue;

                    // nicht zurueck in das Leaf, aus dem wir kommen
                    if (Tp.to == f.from) continue;

                    // Portal, das koplanar in die Gegenrichtung zeigt
                    if (Dot3(Tp.normal, f.entryN) < T(-0.9999) &&
                        std::abs(Dot3(Tp.origin, f.entryN) - f.entryD) < eps)
                        continue;

                    // Ein Strahl durchquert ein konvexes Leaf hoechstens einmal.
                    if (stack.size() > m_leafCount) continue;

                    const bool tDone = m_status[ti].load(std::memory_order_acquire) == 2;
                    const Bits& test = tDone ? Tp.portalvis : Tp.mightsee;

                    uint64_t more = 0;
                    for (size_t w = 0; w < m_portalWords; ++w)
                    {
                        tmp[w] = f.mightsee[w] & test[w];
                        more |= (tmp[w] & ~S.portalvis[w]);
                    }

                    // Bringt nichts Neues
                    if (!more && BitGet(S.portalvis, ti)) continue;

                    // Ziel-Portal auf den Bereich VOR dem Quell-Portal beschneiden
                    std::vector<Vec3> pass;
                    T d = Dot3(Tp.origin, S.normal) - S.dist;
                    if (d < -Tp.radius)
                        continue;
                    else if (d > Tp.radius)
                        pass = Tp.verts;
                    else
                    {
                        pass = ClipFront(Tp.verts, S.normal, S.dist, eps);
                        if (pass.size() < 3) continue;
                    }

                    // Quelle auf den Bereich HINTER dem Ziel-Portal beschneiden
                    std::vector<Vec3> source;
                    d = Dot3(S.origin, Tp.normal) - Tp.dist;
                    if (d > S.radius)
                        continue;
                    else if (d < -S.radius)
                        source = f.source;
                    else
                    {
                        source = ClipFront(f.source, Tp.normal * T(-1), -Tp.dist, eps);
                        if (source.size() < 3) continue;
                    }

                    if (f.hasPass)
                    {
                        pass = ClipToSeparators(source, f.pass, std::move(pass), false, eps);
                        if (pass.size() < 3) continue;

                        pass = ClipToSeparators(f.pass, source, std::move(pass), true, eps);
                        if (pass.size() < 3) continue;
                    }

                    // sichtbar
                    BitSetOn(S.portalvis, ti);
                    BitSetOn(S.leafvis, (size_t)Tp.to);

                    Frame c;
                    c.leaf = Tp.to;
                    c.from = f.leaf;
                    c.next = 0;
                    c.source = std::move(source);
                    c.pass = std::move(pass);
                    c.hasPass = true;
                    c.entryN = Tp.normal;
                    c.entryD = Tp.dist;
                    c.mightsee = tmp;

                    stack.push_back(std::move(c));   // "f" ist ab hier ungueltig
                }
            }
        };

    } // namespace bsp
} // namespace ng

#ifndef USE_OLD

#else

// ----------------------------------------------------------------------
// PVS.h - Potentially Visible Set fuer den BSPTree
//
// Verfahren: klassischer Portal-Flow (wie Quake "vis")
//   1. Jedes Portal wird in zwei gerichtete Portale zerlegt
//      (Leaf A -> Leaf B und Leaf B -> Leaf A).
//   2. BasePortalVis: grobe, konservative Vorauswahl "mightsee"
//      pro gerichtetem Portal (reine Ebenentests + Flood).
//   3. PortalFlow: pro Portal wird ueber die Nachbar-Leaves geflutet.
//      Portale werden dabei gegen Separator-Ebenen (Kante des einen +
//      Punkt des anderen Portals) geclippt. Was komplett wegfaellt, ist
//      nicht sichtbar. Ergebnis: portalvis + leafvis.
//   4. LeafFlow: Sichtbarkeit eines Leaves = ODER ueber die leafvis
//      aller Portale, die das Leaf verlassen (+ das Leaf selbst).
//   5. Jede Zeile wird wie in Quake lauflaengen-komprimiert gespeichert.
//
// Voraussetzung (Reihenfolge):
//   tree.Build(...); tree.BuildPortals();
//   auto reached = tree.FloodFromFlag();
//   tree.RemoveReachedPolygons(reached);     // markiert Void-Leaves INTOTHEVOID
//   pvs.Build(tree);
//
// Alles ist iterativ (kein Rekursions-Stack), Portal-Flow laeuft optional
// auf mehreren Threads.
// ----------------------------------------------------------------------

#include "BSPBuilder.h"

#include <vector>
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <atomic>
#include <thread>
#include <chrono>
#include <memory>
#include <numeric>

namespace ng
{
    namespace bsp
    {
        template <class T>
        struct PVSOptions
        {
            // Toleranz in Weltunits (wie ON_EPSILON in Quake vis).
            T epsilon = T(0.1);

            // 0 = hardware_concurrency()
            unsigned threads = 0;

            // Fortschritt per printf
            bool verbose = true;

            // Leaves mit INTOTHEVOID (vom Flood erreicht) werden ignoriert.
            bool skipVoidLeaves = true;

            // Nur Zusammenhangskomponenten des Portalgraphen, in denen mindestens
            // ein Leaf Polygone enthaelt, sind "begehbar". Filtert das Innere von
            // Brushes (Zellen ohne jede Geometrie, vom Rest abgeschlossen) heraus.
            bool requirePolygonsInComponent = true;

            // Portale kleiner als diese Flaeche werden ignoriert.
            T minPortalArea = T(0.01);
        };

        namespace pvs_detail
        {
            using Bits = std::vector<uint64_t>;

            inline bool BitGet(const Bits& b, size_t i)
            {
                return ((b[i >> 6] >> (i & 63)) & 1ull) != 0;
            }

            inline void BitSetOn(Bits& b, size_t i)
            {
                b[i >> 6] |= (1ull << (i & 63));
            }

            inline size_t BitCount(const Bits& b)
            {
                size_t c = 0;
                for (uint64_t w : b)
                {
                    while (w) { w &= (w - 1); ++c; }
                }
                return c;
            }

            template <class T>
            inline T Dot3(const ng::math::Matrix<3, 1, T>& a, const ng::math::Matrix<3, 1, T>& b)
            {
                return a(0) * b(0) + a(1) * b(1) + a(2) * b(2);
            }

            template <class T>
            inline ng::math::Matrix<3, 1, T> Cross3(const ng::math::Matrix<3, 1, T>& a,
                const ng::math::Matrix<3, 1, T>& b)
            {
                return ng::math::Matrix<3, 1, T>(
                    a(1) * b(2) - a(2) * b(1),
                    a(2) * b(0) - a(0) * b(2),
                    a(0) * b(1) - a(1) * b(0));
            }

            // Behaelt den Teil vor der Ebene (dot(n,p) - dist > 0).
            // Liegt kein Punkt echt davor, ist das Ergebnis leer.
            template <class T>
            std::vector<ng::math::Matrix<3, 1, T>> ClipFront(
                const std::vector<ng::math::Matrix<3, 1, T>>& in,
                const ng::math::Matrix<3, 1, T>& n, T dist, T eps)
            {
                using Vec3 = ng::math::Matrix<3, 1, T>;

                const size_t cnt = in.size();
                if (cnt < 3) return std::vector<Vec3>();

                std::vector<T> dists(cnt + 1);
                std::vector<int> sides(cnt + 1);   // 0 = front, 1 = back, 2 = on
                size_t counts[3] = { 0, 0, 0 };

                for (size_t i = 0; i < cnt; ++i)
                {
                    const T d = Dot3(in[i], n) - dist;
                    dists[i] = d;
                    sides[i] = (d > eps) ? 0 : ((d < -eps) ? 1 : 2);
                    ++counts[sides[i]];
                }
                dists[cnt] = dists[0];
                sides[cnt] = sides[0];

                if (!counts[0]) return std::vector<Vec3>();
                if (!counts[1]) return in;

                std::vector<Vec3> out;
                out.reserve(cnt + 4);

                for (size_t i = 0; i < cnt; ++i)
                {
                    const Vec3& p1 = in[i];

                    if (sides[i] == 2)
                    {
                        out.push_back(p1);
                        continue;
                    }

                    if (sides[i] == 0)
                        out.push_back(p1);

                    if (sides[i + 1] == 2 || sides[i + 1] == sides[i])
                        continue;

                    const Vec3& p2 = in[(i + 1) % cnt];
                    const T t = dists[i] / (dists[i] - dists[i + 1]);
                    out.push_back(p1 + (p2 - p1) * t);
                }

                return out;
            }

            // ----------------------------------------------------------
            // ClipToSeparators (wie Quake)
            //
            // Erzeugt Trennebenen aus einer Kante von "source" und einem
            // Punkt von "pass". Liegt "pass" komplett auf der einen und
            // "source" auf der anderen Seite, ist das eine Trennebene und
            // "target" wird auf die Seite von "pass" beschnitten.
            // Ist target danach leer, ist es durch diese Oeffnungen nicht
            // sichtbar. flipclip = Reihenfolge (pass, source, target).
            // ----------------------------------------------------------
            template <class T>
            std::vector<ng::math::Matrix<3, 1, T>> ClipToSeparators(
                const std::vector<ng::math::Matrix<3, 1, T>>& source,
                const std::vector<ng::math::Matrix<3, 1, T>>& pass,
                std::vector<ng::math::Matrix<3, 1, T>> target,
                bool flipclip, T eps)
            {
                using Vec3 = ng::math::Matrix<3, 1, T>;

                const size_t sn = source.size();
                const size_t pn = pass.size();

                for (size_t i = 0; i < sn; ++i)
                {
                    const size_t l = (i + 1) % sn;
                    const Vec3 v1 = source[l] - source[i];

                    for (size_t j = 0; j < pn; ++j)
                    {
                        const Vec3 v2 = pass[j] - source[i];

                        Vec3 normal = Cross3(v1, v2);
                        const T length = Dot3(normal, normal);

                        // Kein gueltige Ebene
                        if (length < eps)
                            continue;

                        normal = normal * (T(1) / std::sqrt(length));
                        T dist = Dot3(pass[j], normal);

                        // Auf welcher Seite liegt source?
                        bool fliptest = false;
                        size_t k = 0;
                        for (k = 0; k < sn; ++k)
                        {
                            if (k == i || k == l) continue;

                            const T d = Dot3(source[k], normal) - dist;
                            if (d < -eps) { fliptest = false; break; }
                            if (d > eps) { fliptest = true;  break; }
                        }
                        if (k == sn)
                            continue;   // planar mit source

                        if (fliptest)
                        {
                            normal = normal * T(-1);
                            dist = -dist;
                        }

                        // Alle Punkte von pass muessen auf der positiven Seite liegen
                        int front = 0;
                        size_t kk = 0;
                        for (kk = 0; kk < pn; ++kk)
                        {
                            if (kk == j) continue;

                            const T d = Dot3(pass[kk], normal) - dist;
                            if (d < -eps) break;
                            if (d > eps) ++front;
                        }
                        if (kk != pn) continue;   // keine Trennebene
                        if (!front)   continue;   // planar mit Trennebene

                        if (flipclip)
                        {
                            normal = normal * T(-1);
                            dist = -dist;
                        }

                        target = ClipFront(target, normal, dist, eps);
                        if (target.size() < 3)
                            return std::vector<Vec3>();
                    }
                }

                return target;
            }

            template <class F>
            void ParallelFor(size_t count, unsigned threads, F&& fn)
            {
                if (threads <= 1 || count < 2)
                {
                    for (size_t i = 0; i < count; ++i) fn(i);
                    return;
                }

                std::atomic<size_t> next(0);

                auto worker = [&]()
                    {
                        for (;;)
                        {
                            const size_t i = next.fetch_add(1);
                            if (i >= count) break;
                            fn(i);
                        }
                    };

                std::vector<std::thread> pool;
                for (unsigned t = 1; t < threads; ++t)
                    pool.emplace_back(worker);

                worker();

                for (auto& th : pool) th.join();
            }

            inline double SecondsSince(const std::chrono::steady_clock::time_point& t0)
            {
                return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
            }
        }

        // ------------------------------------------------------------
        // PVS
        // ------------------------------------------------------------

        template <class T>
        class PVS
        {
        public:
            using Vec3 = ng::math::Matrix<3, 1, T>;

            PVS() {}

            // Baut das PVS. Rueckgabe false, wenn keine nutzbaren Portale existieren.
            bool Build(const BSPTree<T>& tree, const PVSOptions<T>& opt = PVSOptions<T>())
            {
                using namespace pvs_detail;

                Clear();

                const auto t0 = std::chrono::steady_clock::now();

                const auto& leaves = tree.GetLeaves();
                const auto& tportals = tree.GetPortals();

                m_leafCount = leaves.size();
                m_active.assign(m_leafCount, 0);
                m_rows.assign(m_leafCount, std::vector<uint8_t>());

                if (m_leafCount == 0)
                    return false;

                const T eps = opt.epsilon;

                // ------------------------------------------------------
                // 1. Aktive (begehbare) Leaves bestimmen
                // ------------------------------------------------------

                std::vector<uint8_t> notVoid(m_leafCount, 1);
                if (opt.skipVoidLeaves)
                {
                    for (size_t i = 0; i < m_leafCount; ++i)
                        if (leaves[i]->State() & INTOTHEVOID)
                            notVoid[i] = 0;
                }

                std::vector<int32_t> parent(m_leafCount);
                std::iota(parent.begin(), parent.end(), 0);

                auto findRoot = [&](int32_t x)
                    {
                        while (parent[x] != x)
                        {
                            parent[x] = parent[parent[x]];
                            x = parent[x];
                        }
                        return x;
                    };

                const int32_t lc = (int32_t)m_leafCount;

                for (const auto& tp : tportals)
                {
                    const int32_t a = tp.leafFront();
                    const int32_t b = tp.leafBack();
                    if (a < 0 || b < 0 || a >= lc || b >= lc || a == b) continue;
                    if (!notVoid[a] || !notVoid[b]) continue;

                    const int32_t ra = findRoot(a);
                    const int32_t rb = findRoot(b);
                    if (ra != rb) parent[ra] = rb;
                }

                std::vector<uint8_t> compHasPolys(m_leafCount, 0);
                for (size_t i = 0; i < m_leafCount; ++i)
                {
                    if (notVoid[i] && !leaves[i]->GetPolygons().empty())
                        compHasPolys[findRoot((int32_t)i)] = 1;
                }

                size_t activeCount = 0;
                for (size_t i = 0; i < m_leafCount; ++i)
                {
                    bool act = notVoid[i] != 0;
                    if (act && opt.requirePolygonsInComponent)
                        act = compHasPolys[findRoot((int32_t)i)] != 0;

                    m_active[i] = act ? 1 : 0;
                    if (act) ++activeCount;
                }

                // ------------------------------------------------------
                // 2. Gerichtete Portale erzeugen
                // ------------------------------------------------------

                m_leafPortals.assign(m_leafCount, std::vector<size_t>());

                for (const auto& tp : tportals)
                {
                    const int32_t a = tp.leafFront();
                    const int32_t b = tp.leafBack();
                    if (a < 0 || b < 0 || a >= lc || b >= lc || a == b) continue;
                    if (!m_active[a] || !m_active[b]) continue;
                    if (tp.verts().size() < 3) continue;
                    if (PortalArea(tp.verts()) < opt.minPortalArea) continue;

                    Vec3 n = tp.plane().normal();
                    const T nl = n.length();
                    if (nl <= T(1e-12)) continue;
                    n = n * (T(1) / nl);

                    // BSP-Ebene: Normale zeigt zum FRONT-Leaf.
                    AddDirected(tp.verts(), n, b, a);                 // Back  -> Front
                    AddDirected(tp.verts(), n * T(-1), a, b);         // Front -> Back
                }

                const size_t P = m_portals.size();

                if (P == 0)
                {
                    if (opt.verbose)
                        printf("[PVS] keine nutzbaren Portale.\n");
                    // Jedes aktive Leaf sieht zumindest sich selbst.
                    FinishRowsWithoutPortals();
                    return false;
                }

                m_portalWords = (P + 63) / 64;
                m_leafWords = (m_leafCount + 63) / 64;

                for (auto& fp : m_portals)
                {
                    fp.mightsee.assign(m_portalWords, 0);
                    fp.portalvis.assign(m_portalWords, 0);
                    fp.leafvis.assign(m_leafWords, 0);
                }

                unsigned threads = opt.threads;
                if (threads == 0)
                {
                    threads = std::thread::hardware_concurrency();
                    if (threads == 0) threads = 1;
                }

                if (opt.verbose)
                    printf("[PVS] aktive Leaves=%zu von %zu, gerichtete Portale=%zu, Threads=%u\n",
                        activeCount, m_leafCount, P, threads);

                // ------------------------------------------------------
                // 3. Base-Vis
                // ------------------------------------------------------

                ParallelFor(P, threads, [&](size_t i) { BasePortalVis(i, eps); });

                if (opt.verbose)
                    printf("[PVS] BasePortalVis fertig (%.1fs)\n", SecondsSince(t0));

                // ------------------------------------------------------
                // 4. Portal-Flow, Portale mit wenig Kandidaten zuerst
                // ------------------------------------------------------

                std::vector<size_t> order(P);
                std::iota(order.begin(), order.end(), size_t(0));
                std::sort(order.begin(), order.end(),
                    [&](size_t a, size_t b) { return m_portals[a].numMightSee < m_portals[b].numMightSee; });

                m_status.reset(new std::atomic<int>[P]);
                for (size_t i = 0; i < P; ++i) m_status[i].store(0);

                std::atomic<size_t> done(0);

                ParallelFor(P, threads, [&](size_t k)
                    {
                        const size_t pi = order[k];
                        PortalFlow(pi, eps);
                        m_status[pi].store(2, std::memory_order_release);

                        const size_t d = done.fetch_add(1) + 1;
                        if (opt.verbose && (d % 200) == 0)
                            printf("[PVS] Portal-Flow %zu / %zu  (%.1fs)\n", d, P, SecondsSince(t0));
                    });

                // ------------------------------------------------------
                // 5. Leaf-Flow + Kompression
                // ------------------------------------------------------

                const size_t rowBytes = (m_leafCount + 7) >> 3;
                size_t visibleSum = 0;
                size_t compressedBytes = 0;

                for (size_t leaf = 0; leaf < m_leafCount; ++leaf)
                {
                    if (!m_active[leaf]) continue;

                    Bits acc(m_leafWords, 0);
                    for (size_t pi : m_leafPortals[leaf])
                    {
                        const Bits& lv = m_portals[pi].leafvis;
                        for (size_t w = 0; w < m_leafWords; ++w)
                            acc[w] |= lv[w];
                    }
                    BitSetOn(acc, leaf);   // jedes Leaf sieht sich selbst

                    std::vector<uint8_t> row(rowBytes, 0);
                    for (size_t i = 0; i < m_leafCount; ++i)
                    {
                        if (BitGet(acc, i))
                        {
                            row[i >> 3] |= (uint8_t)(1u << (i & 7));
                            ++visibleSum;
                        }
                    }

                    CompressRow(row, m_rows[leaf]);
                    compressedBytes += m_rows[leaf].size();
                }

                if (opt.verbose)
                {
                    printf("[PVS] fertig (%.1fs): im Schnitt %.1f sichtbare Leaves pro Leaf, %zu Bytes komprimiert\n",
                        SecondsSince(t0),
                        activeCount ? double(visibleSum) / double(activeCount) : 0.0,
                        compressedBytes);
                }

                // Rechendaten freigeben, nur die Zeilen bleiben.
                m_portals.clear();
                m_portals.shrink_to_fit();
                m_leafPortals.clear();
                m_leafPortals.shrink_to_fit();
                m_status.reset();

                m_built = true;
                return true;
            }

            void Clear()
            {
                m_built = false;
                m_leafCount = 0;
                m_active.clear();
                m_rows.clear();
                m_portals.clear();
                m_leafPortals.clear();
                m_status.reset();
            }

            // ------------------------------------------------------
            // Abfragen
            // ------------------------------------------------------

            inline bool IsBuilt() const { return m_built; }
            inline size_t LeafCount() const { return m_leafCount; }

            // Ist das Leaf Teil des begehbaren Bereichs?
            // Steht die Kamera in einem inaktiven Leaf, sollte alles gezeichnet werden.
            inline bool IsActiveLeaf(int32_t leaf) const
            {
                return leaf >= 0 && (size_t)leaf < m_leafCount && m_active[leaf] != 0;
            }

            // Sieht "from" das Leaf "to"? (false bei ungueltigen/inaktiven Leaves)
            bool CanSee(int32_t from, int32_t to) const
            {
                if (!IsActiveLeaf(from) || to < 0 || (size_t)to >= m_leafCount)
                    return false;

                const auto& row = m_rows[from];
                const size_t target = (size_t)to >> 3;

                size_t pos = 0;
                size_t i = 0;

                while (i < row.size() && pos <= target)
                {
                    if (row[i])
                    {
                        if (pos == target)
                            return ((row[i] >> (to & 7)) & 1u) != 0;
                        ++pos;
                        ++i;
                    }
                    else
                    {
                        if (i + 1 >= row.size()) return false;
                        const size_t run = row[i + 1];
                        if (target < pos + run) return false;
                        pos += run;
                        i += 2;
                    }
                }

                return false;
            }

            // Unkomprimierte Zeile (Bit i in Byte i>>3, Bit i&7).
            std::vector<uint8_t> DecompressRow(int32_t leaf) const
            {
                const size_t rowBytes = (m_leafCount + 7) >> 3;
                std::vector<uint8_t> out(rowBytes, 0);

                if (!IsActiveLeaf(leaf)) return out;

                const auto& in = m_rows[leaf];
                size_t i = 0, o = 0;

                while (o < rowBytes && i < in.size())
                {
                    if (in[i])
                    {
                        out[o++] = in[i++];
                        continue;
                    }

                    if (i + 1 >= in.size()) break;
                    o += in[i + 1];
                    i += 2;
                }

                return out;
            }

            // Alle sichtbaren Leaf-IDs.
            std::vector<int32_t> VisibleLeaves(int32_t leaf) const
            {
                std::vector<int32_t> result;
                if (!IsActiveLeaf(leaf)) return result;

                const auto row = DecompressRow(leaf);
                for (size_t i = 0; i < m_leafCount; ++i)
                    if ((row[i >> 3] >> (i & 7)) & 1u)
                        result.push_back((int32_t)i);

                return result;
            }

            // Komprimierte Zeile (zum Speichern in eine Datei).
            inline const std::vector<uint8_t>& CompressedRow(int32_t leaf) const { return m_rows[leaf]; }

        private:
            using Bits = pvs_detail::Bits;

            struct FlowPortal
            {
                std::vector<Vec3> verts;
                Vec3 normal;           // zeigt in Flussrichtung (in Leaf "to")
                T dist = T(0);         // dot(normal, p) == dist auf dem Portal
                Vec3 origin;           // Schwerpunkt
                T radius = T(0);       // Huellkugel
                int32_t from = -1;
                int32_t to = -1;

                Bits mightsee;         // Portale (Base-Vis)
                Bits portalvis;        // Portale (Ergebnis)
                Bits leafvis;          // Leaves  (Ergebnis)
                size_t numMightSee = 0;
            };

            struct Frame
            {
                int32_t leaf = -1;
                int32_t from = -1;
                size_t next = 0;
                std::vector<Vec3> source;
                std::vector<Vec3> pass;
                bool hasPass = false;
                Vec3 entryN;
                T entryD = T(0);
                Bits mightsee;
            };

            bool m_built = false;
            size_t m_leafCount = 0;
            size_t m_portalWords = 0;
            size_t m_leafWords = 0;

            std::vector<uint8_t> m_active;
            std::vector<std::vector<uint8_t>> m_rows;

            // Nur waehrend Build()
            std::vector<FlowPortal> m_portals;
            std::vector<std::vector<size_t>> m_leafPortals;   // Portale, die das Leaf VERLASSEN
            std::unique_ptr<std::atomic<int>[]> m_status;     // 0 offen, 2 fertig

            void AddDirected(const std::vector<Vec3>& verts, const Vec3& n, int32_t from, int32_t to)
            {
                using namespace pvs_detail;

                FlowPortal fp;
                fp.verts = verts;
                fp.normal = n;
                fp.from = from;
                fp.to = to;

                Vec3 c(0, 0, 0);
                T d = T(0);
                for (const auto& v : verts)
                {
                    c = c + v;
                    d += Dot3(n, v);
                }
                const T inv = T(1) / T(verts.size());
                c = c * inv;
                fp.origin = c;
                fp.dist = d * inv;

                T r = T(0);
                for (const auto& v : verts)
                    r = std::max(r, (v - c).length());
                fp.radius = r;

                m_leafPortals[from].push_back(m_portals.size());
                m_portals.push_back(std::move(fp));
            }

            void FinishRowsWithoutPortals()
            {
                const size_t rowBytes = (m_leafCount + 7) >> 3;
                for (size_t leaf = 0; leaf < m_leafCount; ++leaf)
                {
                    if (!m_active[leaf]) continue;
                    std::vector<uint8_t> row(rowBytes, 0);
                    row[leaf >> 3] |= (uint8_t)(1u << (leaf & 7));
                    CompressRow(row, m_rows[leaf]);
                }
                m_built = true;
            }

            static void CompressRow(const std::vector<uint8_t>& src, std::vector<uint8_t>& dst)
            {
                dst.clear();
                const size_t n = src.size();

                for (size_t j = 0; j < n; ++j)
                {
                    dst.push_back(src[j]);
                    if (src[j]) continue;

                    size_t c = 1;
                    for (++j; j < n; ++j)
                    {
                        if (src[j] || c == 255) break;
                        ++c;
                    }
                    --j;

                    dst.push_back(static_cast<uint8_t>(c));
                }
            }

            // ------------------------------------------------------
            // BasePortalVis
            //
            // Portal j ist fuer Portal i "moeglicherweise sichtbar", wenn
            //  - j mindestens einen Punkt vor der Ebene von i hat und
            //  - i mindestens einen Punkt hinter der Ebene von j hat
            // und j ueber eine Kette solcher Portale erreichbar ist.
            // ------------------------------------------------------
            void BasePortalVis(size_t i, T eps)
            {
                using namespace pvs_detail;

                FlowPortal& p = m_portals[i];
                const size_t P = m_portals.size();

                std::vector<uint8_t> portalsee(P, 0);

                for (size_t j = 0; j < P; ++j)
                {
                    if (j == i) continue;

                    const FlowPortal& tp = m_portals[j];

                    // tp muss einen Punkt vor p haben
                    T d = Dot3(tp.origin, p.normal) - p.dist;
                    if (d < -tp.radius)
                        continue;
                    if (d <= tp.radius)
                    {
                        bool any = false;
                        for (const auto& v : tp.verts)
                        {
                            if (Dot3(v, p.normal) - p.dist > eps) { any = true; break; }
                        }
                        if (!any) continue;
                    }

                    // p muss einen Punkt hinter tp haben
                    d = Dot3(p.origin, tp.normal) - tp.dist;
                    if (d > p.radius)
                        continue;
                    if (d >= -p.radius)
                    {
                        bool any = false;
                        for (const auto& v : p.verts)
                        {
                            if (Dot3(v, tp.normal) - tp.dist < -eps) { any = true; break; }
                        }
                        if (!any) continue;
                    }

                    portalsee[j] = 1;
                }

                // SimpleFlood (iterativ)
                std::vector<int32_t> stack;
                stack.push_back(p.to);

                while (!stack.empty())
                {
                    const int32_t leaf = stack.back();
                    stack.pop_back();

                    for (size_t q : m_leafPortals[leaf])
                    {
                        if (!portalsee[q]) continue;
                        if (BitGet(p.mightsee, q)) continue;

                        BitSetOn(p.mightsee, q);
                        stack.push_back(m_portals[q].to);
                    }
                }

                p.numMightSee = BitCount(p.mightsee);
            }

            // ------------------------------------------------------
            // PortalFlow (iterativ, expliziter Stack)
            // ------------------------------------------------------
            void PortalFlow(size_t si, T eps)
            {
                using namespace pvs_detail;

                FlowPortal& S = m_portals[si];

                std::vector<Frame> stack;
                Bits tmp(m_portalWords, 0);

                {
                    Frame f;
                    f.leaf = S.to;
                    f.from = S.from;
                    f.next = 0;
                    f.source = S.verts;
                    f.hasPass = false;
                    f.entryN = S.normal;
                    f.entryD = S.dist;
                    f.mightsee = S.mightsee;

                    BitSetOn(S.leafvis, (size_t)S.to);
                    stack.push_back(std::move(f));
                }

                while (!stack.empty())
                {
                    Frame& f = stack.back();
                    const auto& lp = m_leafPortals[f.leaf];

                    if (f.next >= lp.size())
                    {
                        stack.pop_back();
                        continue;
                    }

                    const size_t ti = lp[f.next++];
                    if (ti == si) continue;

                    const FlowPortal& Tp = m_portals[ti];

                    if (!BitGet(f.mightsee, ti)) continue;

                    // nicht zurueck in das Leaf, aus dem wir kommen
                    if (Tp.to == f.from) continue;

                    // Portal, das koplanar in die Gegenrichtung zeigt
                    if (Dot3(Tp.normal, f.entryN) < T(-0.9999) &&
                        std::abs(Dot3(Tp.origin, f.entryN) - f.entryD) < eps)
                        continue;

                    // Ein Strahl durchquert ein konvexes Leaf hoechstens einmal.
                    if (stack.size() > m_leafCount) continue;

                    const bool tDone = m_status[ti].load(std::memory_order_acquire) == 2;
                    const Bits& test = tDone ? Tp.portalvis : Tp.mightsee;

                    uint64_t more = 0;
                    for (size_t w = 0; w < m_portalWords; ++w)
                    {
                        tmp[w] = f.mightsee[w] & test[w];
                        more |= (tmp[w] & ~S.portalvis[w]);
                    }

                    // Bringt nichts Neues
                    if (!more && BitGet(S.portalvis, ti)) continue;

                    // Ziel-Portal auf den Bereich VOR dem Quell-Portal beschneiden
                    std::vector<Vec3> pass;
                    T d = Dot3(Tp.origin, S.normal) - S.dist;
                    if (d < -Tp.radius)
                        continue;
                    else if (d > Tp.radius)
                        pass = Tp.verts;
                    else
                    {
                        pass = ClipFront(Tp.verts, S.normal, S.dist, eps);
                        if (pass.size() < 3) continue;
                    }

                    // Quelle auf den Bereich HINTER dem Ziel-Portal beschneiden
                    std::vector<Vec3> source;
                    d = Dot3(S.origin, Tp.normal) - Tp.dist;
                    if (d > S.radius)
                        continue;
                    else if (d < -S.radius)
                        source = f.source;
                    else
                    {
                        source = ClipFront(f.source, Tp.normal * T(-1), -Tp.dist, eps);
                        if (source.size() < 3) continue;
                    }

                    if (f.hasPass)
                    {
                        pass = ClipToSeparators(source, f.pass, std::move(pass), false, eps);
                        if (pass.size() < 3) continue;

                        pass = ClipToSeparators(f.pass, source, std::move(pass), true, eps);
                        if (pass.size() < 3) continue;
                    }

                    // sichtbar
                    BitSetOn(S.portalvis, ti);
                    BitSetOn(S.leafvis, (size_t)Tp.to);

                    Frame c;
                    c.leaf = Tp.to;
                    c.from = f.leaf;
                    c.next = 0;
                    c.source = std::move(source);
                    c.pass = std::move(pass);
                    c.hasPass = true;
                    c.entryN = Tp.normal;
                    c.entryD = Tp.dist;
                    c.mightsee = tmp;

                    stack.push_back(std::move(c));   // "f" ist ab hier ungueltig
                }
            }
        };

    } // namespace bsp
} // namespace ng
#endif