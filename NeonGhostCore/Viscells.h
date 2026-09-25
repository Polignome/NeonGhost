#pragma once

// ----------------------------------------------------------------------
// VisCells.h - grobe Sichtbarkeitszellen fuer das PVS
//
// Idee: Der Haupt-BSP nutzt JEDE Polygon-Ebene als Splitter. Das ist fuer
// Flood Fill / Geometrie-Entfernung richtig, fuer das PVS aber viel zu fein
// (winzige Leaves, riesige Portalzahl, O(P^2) Speicher).
//
// Hier wird ein zweiter BSP nur aus STRUKTURELLEN Ebenen gebaut:
//   - Polygone werden nach Ebene gruppiert (auch wenn die CSG-/BSP-Splits
//     eine Wand in viele Fragmente zerlegt haben).
//   - Nur Ebenen mit genug Gesamtflaeche (Waende, Boeden, Decken) werden
//     Splitter. Alles andere ist "Detail" und blockiert keine Sicht.
// Das ist konservativ (es kann nur MEHR sichtbar werden, nie weniger) und
// liefert echte konvexe Zellen, wie sie PVS.h voraussetzt.
//
// Danach werden ALLE Polygone (auch Details) den Zellen zugeordnet, die sie
// beruehren. Ein Polygon kann in mehreren Zellen stehen (kein Splitten).
//
// Ablauf:
//   auto visible = tree.CollectRemainingPolygons();  // nach dem Flood/Remove
//   VisCells<T> cells;   cells.Build(visible);
//   PVS<T> pvs;          pvs.Build(cells.Tree());
//
//   // Extra-Polygone (Wasser, Glas, ... - separat gerendert, blockieren
//   // keine Sicht, tauchen nicht in den strukturellen Ebenen auf):
//   cells.InsertExtraPolygons(waterAndGlassPolys);   // NACH Build()
//
//   // Rendering
//   int32_t cell = cells.FindCell(cameraPos);
//   std::vector<uint32_t> list;
//   cells.CollectVisiblePolygons(pvs, cell, list);         // Indizes in cells.Polygons()
//   std::vector<uint32_t> extraList;
//   cells.CollectVisibleExtraPolygons(pvs, cell, extraList); // Indizes in cells.ExtraPolygons()
// ----------------------------------------------------------------------

#include "BSP.h"
#include "PVS.h"

#include <vector>
#include <unordered_map>
#include <algorithm>
#include <numeric>
#include <cstdint>
#include <cstdio>

namespace ng
{
    namespace bsp
    {
        template <class T>
        struct VisCellOptions
        {
            // Ebenen mit weniger Gesamtflaeche (Summe aller Polygone auf dieser
            // Ebene) gelten als Detail. Groesse in Weltunits^2.
            // 4096 = 64 x 64. Erhoehen = groebere Zellen, schnelleres PVS,
            // weniger Culling. Senken = feinere Zellen.
            T minPlaneArea = T(4096);

            // 0 = unbegrenzt. Sonst nur die N flaechenmaessig groessten Ebenen.
            size_t maxStructuralPlanes = 0;

            // Teilbaeume, in denen weniger als maxOccluderArea Innenwand steckt
            // (Flaeche der Polygone, die INNERHALB der Zelle liegen, nicht auf ihrem
            // Rand), werden zu EINER Zelle zusammengefasst. Weniger Zellen und Portale,
            // schnelleres PVS - dafuer etwas weniger Culling und groebere Zellen.
            // 0 = nichts zusammenfassen.
            bool mergeSmallCells = true;
            T maxOccluderArea = T(1024);

            // Werden an den Vis-BSP weitergereicht.
            T epsilon = T(0.01);
            T minPortalWidth = T(0.05);

            bool verbose = true;
        };

        template <class T>
        class VisCells
        {
        public:
            using Vec3 = ng::math::Matrix<3, 1, T>;

            // Baut die groben Zellen aus den (fertigen, bereinigten) Polygonen.
            // Rueckgabe false, wenn keine strukturellen Ebenen gefunden wurden.
            bool Build(const std::vector<BSPPolygon<T>>& polygons,
                const VisCellOptions<T>& opt = VisCellOptions<T>())
            {
                m_opt = opt;
                m_polys.clear();
                m_cellPolys.clear();
                m_stamp.clear();
                m_stampValue = 0;

                // Extra-Polygone (Wasser, Glas, ...) gehoeren zu EINEM Build() und
                // muessen nach einem Neubau erneut per InsertExtraPolygons() gesetzt
                // werden, da sich Zellen/IDs geaendert haben koennen.
                m_extraPolys.clear();
                m_cellExtraPolys.clear();
                m_extraStamp.clear();
                m_extraStampValue = 0;

                m_polys.reserve(polygons.size());
                for (const auto& p : polygons)
                {
                    if (p.verts.size() < 3) continue;
                    m_polys.push_back(p);
                    m_polys.back().flags &= ~static_cast<uint32_t>(POLY_FLAG_SPLITTER_USED);
                }

                if (m_polys.empty())
                    return false;

                // ------------------------------------------------------
                // 1. Polygone nach Ebene gruppieren, Flaeche summieren
                // ------------------------------------------------------

                std::unordered_map<PlaneKey, size_t, PlaneKeyHash> planeIndex;
                std::vector<T> planeArea;
                std::vector<size_t> polyPlane(m_polys.size(), 0);

                planeIndex.reserve(m_polys.size());

                for (size_t i = 0; i < m_polys.size(); ++i)
                {
                    const PlaneKey key = MakePlaneKey(m_polys[i].plane);

                    auto it = planeIndex.find(key);
                    size_t idx;
                    if (it == planeIndex.end())
                    {
                        idx = planeArea.size();
                        planeIndex.emplace(key, idx);
                        planeArea.push_back(T(0));
                    }
                    else
                    {
                        idx = it->second;
                    }

                    planeArea[idx] += VertsArea(m_polys[i].verts);
                    polyPlane[i] = idx;
                }

                // ------------------------------------------------------
                // 2. Strukturelle Ebenen auswaehlen
                // ------------------------------------------------------

                std::vector<uint8_t> keepPlane(planeArea.size(), 0);
                size_t keptPlanes = 0;

                for (size_t i = 0; i < planeArea.size(); ++i)
                {
                    if (planeArea[i] >= opt.minPlaneArea)
                    {
                        keepPlane[i] = 1;
                        ++keptPlanes;
                    }
                }

                if (opt.maxStructuralPlanes > 0 && keptPlanes > opt.maxStructuralPlanes)
                {
                    std::vector<size_t> idx;
                    for (size_t i = 0; i < keepPlane.size(); ++i)
                        if (keepPlane[i]) idx.push_back(i);

                    std::sort(idx.begin(), idx.end(),
                        [&](size_t a, size_t b) { return planeArea[a] > planeArea[b]; });

                    std::fill(keepPlane.begin(), keepPlane.end(), uint8_t(0));
                    for (size_t k = 0; k < opt.maxStructuralPlanes; ++k)
                        keepPlane[idx[k]] = 1;

                    keptPlanes = opt.maxStructuralPlanes;
                }

                std::vector<BSPPolygon<T>> structural;
                double totalArea = 0.0, keptArea = 0.0;

                for (size_t i = 0; i < m_polys.size(); ++i)
                {
                    const double a = (double)VertsArea(m_polys[i].verts);
                    totalArea += a;

                    if (keepPlane[polyPlane[i]])
                    {
                        structural.push_back(m_polys[i]);
                        keptArea += a;
                    }
                }

                if (opt.verbose)
                {
                    printf("[VisCells] Ebenen: %zu strukturell von %zu, Polygone: %zu von %zu, Flaeche: %.1f%%\n",
                        keptPlanes, planeArea.size(), structural.size(), m_polys.size(),
                        totalArea > 0.0 ? 100.0 * keptArea / totalArea : 0.0);
                }

                if (structural.empty())
                {
                    if (opt.verbose)
                        printf("[VisCells] keine strukturellen Ebenen - minPlaneArea zu gross?\n");
                    return false;
                }

                // ------------------------------------------------------
                // 3. Grober BSP + Portale
                // ------------------------------------------------------

                m_tree.SetVerbose(opt.verbose);
                m_tree.SetEpsilon(opt.epsilon);
                m_tree.SetMinPortalWidth(opt.minPortalWidth);

                m_tree.Build(structural);

                if (opt.mergeSmallCells && opt.maxOccluderArea > T(0))
                {
                    const size_t before = m_tree.GetLeaves().size();
                    const size_t merged = m_tree.CollapseSmallCells(opt.maxOccluderArea);

                    if (opt.verbose)
                        printf("[VisCells] Zellen zusammengefasst: %zu Teilbaeume, Zellen %zu -> %zu\n",
                            merged, before, m_tree.GetLeaves().size());
                }

                m_tree.BuildPortals();

                if (opt.verbose)
                    printf("[VisCells] Zellen=%zu Portale=%zu (gerichtet fuer das PVS: %zu)\n",
                        m_tree.GetLeaves().size(), m_tree.GetPortals().size(),
                        m_tree.GetPortals().size() * 2);

                // ------------------------------------------------------
                // 4. Alle Polygone (auch Details) den Zellen zuordnen
                // ------------------------------------------------------

                AssignPolygons();
                m_cellExtraPolys.assign(m_tree.GetLeaves().size(), std::vector<uint32_t>());

                return true;
            }

            // --------------------------------------------------------
            // InsertExtraPolygons
            //
            // Fuegt Polygone hinzu, die NICHT Teil der strukturellen Geometrie sind
            // (Wasser, Glas, ...): sie werden den Zellen zugeordnet wie normale
            // Polygone (auf mehrere Zellen verteilt, nicht geteilt), erscheinen aber
            // NIE in CollectVisiblePolygons()/Polygons(), sondern nur in
            // ExtraPolygons()/CollectVisibleExtraPolygons(). So kannst du sie separat
            // rendern (eigene Blend-/Sortierlogik fuer Transparenz).
            //
            // Muss NACH Build() aufgerufen werden. Mehrfacher Aufruf ist erlaubt
            // (fuegt weitere Polygone hinzu); ein Build() verwirft alle bisherigen.
            // --------------------------------------------------------
            void InsertExtraPolygons(const std::vector<BSPPolygon<T>>& extra)
            {
                if (m_tree.GetLeaves().empty() || extra.empty())
                    return;

                if (m_cellExtraPolys.size() != m_tree.GetLeaves().size())
                    m_cellExtraPolys.assign(m_tree.GetLeaves().size(), std::vector<uint32_t>());

                const size_t base = m_extraPolys.size();

                m_extraPolys.reserve(base + extra.size());
                for (const auto& p : extra)
                {
                    if (p.verts.size() < 3) continue;
                    m_extraPolys.push_back(p);
                    m_extraPolys.back().flags &= ~static_cast<uint32_t>(POLY_FLAG_SPLITTER_USED);
                }

                AssignRange(m_extraPolys, base, m_extraPolys.size(), m_cellExtraPolys);

                m_extraStamp.assign(m_extraPolys.size(), 0);
                m_extraStampValue = 0;
            }

            // Der grobe BSP mit Portalen - das wird ins PVS gegeben.
            inline BSPTree<T>& Tree() { return m_tree; }
            inline const BSPTree<T>& Tree() const { return m_tree; }

            // Alle Eingabepolygone (Indizes aus CollectVisiblePolygons zeigen hierher).
            inline const std::vector<BSPPolygon<T>>& Polygons() const { return m_polys; }

            // Polygone, die eine Zelle beruehren.
            inline const std::vector<uint32_t>& CellPolygons(int32_t cell) const { return m_cellPolys[cell]; }

            // Extra-Polygone (Wasser, Glas, ...), die eine Zelle beruehren.
            // Indizes zeigen in ExtraPolygons().
            inline const std::vector<uint32_t>& CellExtraPolygons(int32_t cell) const { return m_cellExtraPolys[cell]; }

            // Alle per InsertExtraPolygons() eingefuegten Polygone.
            inline const std::vector<BSPPolygon<T>>& ExtraPolygons() const { return m_extraPolys; }

            inline size_t CellCount() const { return m_cellPolys.size(); }

            // Zelle zu einem Punkt (z.B. Kameraposition).
            inline int32_t FindCell(const Vec3& p) const { return m_tree.FindLeafId(p); }

            // Sammelt alle Polygonindizes, die von "fromCell" aus sichtbar sein koennen.
            // Ist die Zelle nicht Teil des begehbaren Bereichs (z.B. Kamera ausserhalb),
            // werden ALLE Polygone geliefert.
            // Nicht thread-sicher (interner Zeitstempel zur Duplikat-Vermeidung).
            void CollectVisiblePolygons(const PVS<T>& pvs, int32_t fromCell,
                std::vector<uint32_t>& out) const
            {
                out.clear();

                if (!pvs.IsActiveLeaf(fromCell))
                {
                    out.resize(m_polys.size());
                    std::iota(out.begin(), out.end(), uint32_t(0));
                    return;
                }

                if (m_stamp.size() != m_polys.size())
                {
                    m_stamp.assign(m_polys.size(), 0);
                    m_stampValue = 0;
                }

                if (++m_stampValue == 0)
                {
                    std::fill(m_stamp.begin(), m_stamp.end(), 0u);
                    m_stampValue = 1;
                }

                const std::vector<uint8_t> row = pvs.DecompressRow(fromCell);

                for (size_t leaf = 0; leaf < m_cellPolys.size(); ++leaf)
                {
                    if (!((row[leaf >> 3] >> (leaf & 7)) & 1u))
                        continue;

                    for (uint32_t pi : m_cellPolys[leaf])
                    {
                        if (m_stamp[pi] == m_stampValue) continue;
                        m_stamp[pi] = m_stampValue;
                        out.push_back(pi);
                    }
                }
            }

            // Wie CollectVisiblePolygons(), aber fuer die per InsertExtraPolygons()
            // eingefuegten Polygone (Wasser, Glas, ...). Eigener Zeitstempel-Puffer,
            // beide Methoden koennen also unabhaengig voneinander aufgerufen werden.
            // Nicht thread-sicher.
            void CollectVisibleExtraPolygons(const PVS<T>& pvs, int32_t fromCell,
                std::vector<uint32_t>& out) const
            {
                out.clear();

                if (m_extraPolys.empty())
                    return;

                if (!pvs.IsActiveLeaf(fromCell))
                {
                    out.resize(m_extraPolys.size());
                    std::iota(out.begin(), out.end(), uint32_t(0));
                    return;
                }

                if (m_extraStamp.size() != m_extraPolys.size())
                {
                    m_extraStamp.assign(m_extraPolys.size(), 0);
                    m_extraStampValue = 0;
                }

                if (++m_extraStampValue == 0)
                {
                    std::fill(m_extraStamp.begin(), m_extraStamp.end(), 0u);
                    m_extraStampValue = 1;
                }

                const std::vector<uint8_t> row = pvs.DecompressRow(fromCell);

                for (size_t leaf = 0; leaf < m_cellExtraPolys.size(); ++leaf)
                {
                    if (!((row[leaf >> 3] >> (leaf & 7)) & 1u))
                        continue;

                    for (uint32_t pi : m_cellExtraPolys[leaf])
                    {
                        if (m_extraStamp[pi] == m_extraStampValue) continue;
                        m_extraStamp[pi] = m_extraStampValue;
                        out.push_back(pi);
                    }
                }
            }

        private:
            VisCellOptions<T> m_opt;
            BSPTree<T> m_tree;
            std::vector<BSPPolygon<T>> m_polys;
            std::vector<std::vector<uint32_t>> m_cellPolys;

            mutable std::vector<uint32_t> m_stamp;
            mutable uint32_t m_stampValue = 0;

            std::vector<BSPPolygon<T>> m_extraPolys;
            std::vector<std::vector<uint32_t>> m_cellExtraPolys;
            mutable std::vector<uint32_t> m_extraStamp;
            mutable uint32_t m_extraStampValue = 0;

            // Schickt jedes Polygon durch den groben BSP (iterativ). Spannt es
            // ueber eine Ebene, kommt es in beide Seiten - ohne es zu teilen.
            void AssignPolygons()
            {
                m_cellPolys.assign(m_tree.GetLeaves().size(), std::vector<uint32_t>());
                AssignRange(m_polys, 0, m_polys.size(), m_cellPolys);
            }

            // Wie AssignPolygons(), aber fuer einen Indexbereich [first, last) eines
            // beliebigen Polygon-Vektors - gemeinsam genutzt von AssignPolygons() und
            // InsertExtraPolygons(), damit die Zuordnungslogik nur einmal existiert.
            void AssignRange(const std::vector<BSPPolygon<T>>& src, size_t first, size_t last,
                std::vector<std::vector<uint32_t>>& cellLists) const
            {
                using Plane = ng::math::RayPlaneLine<3, T>;

                const T eps = m_opt.epsilon;
                std::vector<BSPNode<T>*> stack;
                
                for (size_t pi = first; pi < last; ++pi)
                {
                    const auto& poly = src[pi];
                    stack.push_back(m_tree.GetRoot().get());

                    while (!stack.empty())
                    {
                        BSPNode<T>* n = stack.back();
                        stack.pop_back();

                        if (!n) continue;

                        if (n->isLeaf())
                        {
                            const int32_t id = n->GetID();
                            if (id >= 0 && (size_t)id < cellLists.size())
                                cellLists[id].push_back((uint32_t)pi);
                            continue;
                        }

                        const Plane plane = n->GetPlane();
                        BSPNode<T>* f = n->GetFrontNode().get();
                        BSPNode<T>* b = n->GetBackNode().get();
                        /*
                        switch (ClassifyVerts(plane, poly.verts, eps))
                        {
                        case Plane::FRONT:
                            stack.push_back(f);
                            break;

                        case Plane::BACK:
                            stack.push_back(b);
                            break;

                        case Plane::PLANAR:
                        {
                            // Wie im BSP: das Polygon gehoert auf die Seite, in die
                            // seine Normale zeigt.
                            const Vec3 a = plane.normal();
                            const Vec3 c = poly.plane.normal();
                            const T dot = a(0) * c(0) + a(1) * c(1) + a(2) * c(2);
                            stack.push_back(dot >= T(0) ? f : b);
                            break;
                        }

                        default: // SPANNING
                            stack.push_back(f);
                            stack.push_back(b);
                            break;
                        }
                        */

                        switch (ClassifyVerts(plane, poly.verts, eps))
                        {
                        case Plane::FRONT:
                            stack.push_back(f);
                            break;

                        case Plane::BACK:
                            stack.push_back(b);
                            break;

                        case Plane::PLANAR:
                            // Wie bei BSPTree::InsertExtraPolygons: ein Polygon exakt in der
                            // Splitter-Ebene ist von BEIDEN Seiten aus sichtbar (z.B. eine
                            // Wasseroberflaeche, die zufaellig koplanar zu einer strukturellen
                            // Zellgrenze liegt). Nur EINE Seite zu waehlen fuehrt dazu, dass
                            // das Polygon aus der jeweils anderen Zelle komplett unsichtbar
                            // wird - das genaue Symptom "Extra-Polygone fehlen teilweise".
                            stack.push_back(f);
                            stack.push_back(b);
                            break;

                        default: // SPANNING
                            stack.push_back(f);
                            stack.push_back(b);
                            break;
                        }


                    }
                }
            }
        };

    } // namespace bsp
} // namespace ng