#pragma once
// ----------------------------------------------------------------------
// PortalCompile.h - Compile-Seite der Warp-Portale
//
// Ablauf in Compile():
//   1. VOR MakeUnion: Kopie der Original-Brushes behalten
//   2. BuildWarpPortals(origBrushes, entities, worldUp)  -> scene.m_warps
//   3. Beim Erzeugen der Solid-Polygone: FindWarpForFace() -> poly.flags/portalId setzen
//   4. NACH pvs.Build(): LinkWarpCells(cells, pvs, warps)
// ----------------------------------------------------------------------

#include "BSP.h"
#include "WarpPortal.h"
#include <vector>
#include <map>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace ng
{
    namespace bsp
    {
        // Das, was dein Entity-Parser pro "info_portal" liefern muss.
        template <class T>
        struct PortalEntity
        {
            int32_t id = -1;          // eigene ID
            int32_t target = -1;      // ID des Gegenportals (-1 = keins)
            uint32_t flags = WARP_TELEPORT | WARP_RENDER;
            math::Matrix<3, 1, T> origin;   // steht "genau vor dem Face"
        };

        // Liegt p (auf/nahe der Polygonebene) innerhalb des konvexen Polygons?
        template <class T>
        bool PointInConvexPoly(const std::vector<math::Matrix<3, 1, T>>& v,
            const math::Matrix<3, 1, T>& n, const math::Matrix<3, 1, T>& p, T tol)
        {
            const size_t cnt = v.size();
            bool pos = false, neg = false;
            for (size_t i = 0; i < cnt; ++i)
            {
                const auto& a = v[i];
                const auto& b = v[(i + 1) % cnt];
                const auto e = b - a;
                const T len = e.length();
                if (len < T(1e-9)) continue;
                const T d = (((e % (p - a)) ^ n)) / len;   // vorzeichenbehafteter Abstand zur Kante
                if (d > tol)  pos = true;
                if (d < -tol) neg = true;
                if (pos && neg) return false;
            }
            return true;
        }

        // ------------------------------------------------------------
        // Portale bauen: pro Entity das naechste Brush-Face, das direkt dahinter liegt.
        // "Brushes" = ORIGINAL-Brushes (vor MakeUnion), "prims" ueber primitives_org_copy().
        // Warp-Index = Position im Ergebnis-Vektor; partner = Index des Zielportals.
        // ------------------------------------------------------------
        template <class T, class BrushT>
        std::vector<WarpPortal<T>> BuildWarpPortals(
            const std::vector<BrushT>& origBrushes,
            const std::vector<PortalEntity<T>>& ents,
            const math::Matrix<3, 1, T>& worldUp,
            T maxDist = T(16))
        {
            using Vec3 = math::Matrix<3, 1, T>;
            using Plane = math::RayPlaneLine<3, T>;

            std::vector<WarpPortal<T>> out;
            std::map<int32_t, int32_t> idToIndex;
            std::vector<int32_t> targetOf;

            for (const auto& e : ents)
            {
                T bestDist = maxDist;
                std::vector<Vec3> bestVerts;

                for (const auto& b : origBrushes)
                {
                    auto faces = b.primitives_org_copy();
                    for (const auto& f : faces)
                    {
                        const auto& fv = f.verts();
                        if (fv.size() < 3) continue;

                        std::vector<Vec3> pts;
                        pts.reserve(fv.size());
                        for (const auto& v : fv) pts.push_back(v.world());

                        Plane pl(pts[0], pts[1], pts[2]);
                        const T d = std::abs(pl.distance(e.origin));
                        if (d >= bestDist) continue;

                        // Entity-Position auf die Ebene projizieren und gegen das Polygon testen
                        Vec3 n = pl.normal(); n.normalize();
                        const Vec3 proj = e.origin - n * pl.distance(e.origin);
                        if (!PointInConvexPoly(pts, n, proj, T(0.5))) continue;

                        bestDist = d;
                        bestVerts = pts;
                    }
                }

                if (bestVerts.size() < 3)
                {
                    printf("[Portal] Entity id=%d: kein Face in %.1f Units gefunden\n", e.id, (double)maxDist);
                    continue;
                }

                WarpPortal<T> w;
                w.id = (int32_t)out.size();
                w.flags = e.flags;
                w.verts = bestVerts;
                w.plane = Plane(bestVerts[0], bestVerts[1], bestVerts[2]);
                w.Finalize(e.origin, worldUp);

                idToIndex[e.id] = w.id;
                targetOf.push_back(e.target);
                out.push_back(std::move(w));
            }

            // Partner aufloesen
            for (size_t i = 0; i < out.size(); ++i)
            {
                auto it = idToIndex.find(targetOf[i]);
                if (targetOf[i] >= 0 && it != idToIndex.end())
                    out[i].partner = it->second;
                else
                {
                    out[i].partner = -1;
                    out[i].flags = 0;   // ohne Gegenstueck weder teleportieren noch rendern
                    printf("[Portal] id=%d hat kein gueltiges Ziel -> deaktiviert\n", (int)i);
                }
            }

            printf("[Portal] %zu Portale gebaut\n", out.size());
            return out;
        }

        // ------------------------------------------------------------
        // Zu einem Solid-Face (nach MakeUnion) das passende Portal finden.
        // -1 = normales Face.
        // ------------------------------------------------------------
        template <class T, class VertexVec>
        int32_t FindWarpForFace(const std::vector<WarpPortal<T>>& warps,
            const VertexVec& faceVerts, T eps = T(0.5))
        {
            using Vec3 = math::Matrix<3, 1, T>;

            if (faceVerts.size() < 3 || warps.empty()) return -1;

            Vec3 c(0, 0, 0);
            for (const auto& v : faceVerts) c = c + v.world();
            c = c * (T(1) / T(faceVerts.size()));

            math::RayPlaneLine<3, T> fp(faceVerts[0].world(), faceVerts[1].world(), faceVerts[2].world());
            Vec3 fn = fp.normal(); fn.normalize();

            for (const auto& w : warps)
            {
                if ((fn ^ w.normal) < T(0.99)) continue;           // gleiche Orientierung
                if (std::abs(w.plane.distance(c)) > eps) continue; // gleiche Ebene
                if (!PointInConvexPoly(w.verts, w.normal, c, eps)) continue;
                return w.id;
            }
            return -1;
        }

        // ------------------------------------------------------------
        // Nach pvs.Build(): Zellen pro Portal finden und das Ziel-PVS vorberechnen.
        // Zellen-Index == PVS-Leaf-Index (das PVS wurde auf cells.Tree() gebaut).
        // ------------------------------------------------------------
        template <class T, class CellsT, class PvsT>
        void LinkWarpCells(const CellsT& cells, const PvsT& pvs, std::vector<WarpPortal<T>>& warps)
        {
            const auto& polys = cells.Polygons();
            const size_t cellCount = cells.CellCount();

            for (auto& w : warps) { w.leaves.clear(); w.visCells.clear(); }

            for (size_t c = 0; c < cellCount; ++c)
            {
                for (uint32_t pi : cells.CellPolygons((int32_t)c))
                {
                    if (pi >= polys.size()) continue;
                    const auto& p = polys[pi];
                    if (!(p.flags & POLY_FLAG_PORTAL)) continue;
                    if (p.portalId < 0 || (size_t)p.portalId >= warps.size()) continue;

                    auto& l = warps[p.portalId].leaves;
                    if (l.empty() || l.back() != (int32_t)c) l.push_back((int32_t)c);
                }
            }

            for (auto& w : warps)
            {
                std::vector<uint8_t> mark(cellCount, 0);
                bool all = w.leaves.empty();

                for (int32_t leaf : w.leaves)
                {
                    if (!pvs.IsActiveLeaf(leaf)) { all = true; break; }   // sicher: alles zeigen
                    for (int32_t v : pvs.VisibleLeaves(leaf)) mark[(size_t)v] = 1;
                }

                for (size_t i = 0; i < cellCount; ++i)
                    if (all || mark[i]) w.visCells.push_back((int32_t)i);

                printf("[Portal] id=%d: %zu Zellen mit Portal, Ziel sieht %zu Zellen\n",
                    w.id, w.leaves.size(), w.visCells.size());
            }
        }
    } // namespace bsp
} // namespace ng
