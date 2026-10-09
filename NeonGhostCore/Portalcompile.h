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
#include <unordered_map>
#include "BSP.h"
#include "WarpPortal.h"
#include <vector>
#include <map>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "Map.h"
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
        template <class T, class F>
        std::vector<ng::bsp::WarpPortal<T>> BuildWarpPortals(
            std::vector<ng::map::Brush<T>>& portalBrushes, F isTeleportTex,
            std::vector<ng::bsp::BSPPolygon<T>>& hullPolys,
            const ng::math::Matrix<3, 1, T>& worldUp)
        {
            using namespace ng::bsp;
            using Vec3 = ng::math::Matrix<3, 1, T>;

            std::vector<WarpPortal<T>> portals;
            std::unordered_map<uint32_t, int32_t> byId;
            std::vector<uint32_t> linked;

            for (auto& b : portalBrushes)
            {
                const int fi = b.PortalFaceIndex();
                if (fi < 0) continue;

                const auto& cfg = b.PortalSetting();
                if (byId.count(cfg.id))
                {
                    printf("[Portal] doppelte Portal-ID %u (Brush %u)\n", cfg.id, b.id()); continue;
                }

                const auto prims = b.primitives_org_copy();
                const auto& face = prims[fi];

                Vec3 bc(0, 0, 0); size_t cnt = 0;
                for (const auto& p : prims) for (const auto& v : p.verts()) { bc = bc + v.world(); ++cnt; }
                bc = bc * (T(1) / T(cnt));

                WarpPortal<T> w;
                w.id = (int32_t)cfg.id;
                w.flags = (cfg.teleport ? WARP_TELEPORT : 0u)
                    | ((cfg.visible && cfg.translucent) ? WARP_RENDER : 0u);
                w.plane = face.plane();
                for (const auto& v : face.verts()) w.verts.push_back(v.world());

                Vec3 fc(0, 0, 0);
                for (const auto& v : w.verts) fc = fc + v;
                fc = fc * (T(1) / T(w.verts.size()));

                Vec3 n = w.plane.normal(); n.normalize();
                const T s = (((fc - bc) ^ n) >= T(0)) ? T(1) : T(-1);   // Normale nach aussen
                
                
                const auto& vax = b.faces()[fi].vAxis();                 // Valve-220, V zeigt nach "unten"
                Vec3 upHint(-vax(0), -vax(1), -vax(2));
                if (upHint.length() < T(1e-3)) upHint = worldUp;         // keine Mapping-Daten
                w.Finalize(fc + n * s, upHint);
                
                
                //w.Finalize(fc + n * s, worldUp);





                byId[cfg.id] = (int32_t)portals.size();
                linked.push_back(cfg.linked_id);
                portals.push_back(std::move(w));

                if (cfg.visible)
                {
                    for (size_t i = 0; i < prims.size(); ++i)
                    {
                        
                        BSPPolygon<T> bp = BSPPolygon<T>::FromBrushPoly(prims[i]);
                        bp.objectId = b.id();                         // Brush-ID
                        if ((int)i == fi)
                        {
                            bp.flags |= POLY_FLAG_PORTAL;
                            bp.portalId = (int32_t)cfg.id;            // Portal-ID
                        }
                        hullPolys.push_back(std::move(bp));
                    }
                }
            }

            for (size_t i = 0; i < portals.size(); ++i)
            {
                auto it = byId.find(linked[i]);
                if (it == byId.end())
                {
                    printf("[Portal] %d: linked_id %u existiert nicht\n", portals[i].id, linked[i]); continue;
                }
                portals[i].partner = it->second;
                if (portals[it->second].id != (int32_t)linked[i] ||
                    linked[it->second] != (uint32_t)portals[i].id)
                    printf("[Portal] %d <-> %u ist nicht wechselseitig\n", portals[i].id, linked[i]);
            }
            return portals;
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
