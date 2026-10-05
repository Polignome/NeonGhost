#pragma once
#include "Global.h"
#include "Matrix.h"
#include "Rasterizer3D.h"
#include "Camera.h"
#include "Primitive.h"
#include "ScreenVertex.h"
#include "Frustum.h"
#include "CameraFrustum.h"
#include "AABBox.h"
#include "Bounding.h"
#include "Clipping.h"
#include <vector>
#include <algorithm>
#include <numeric>
#include <cstring>
#include <cstdio>
#include "HZBOcclusion.h"
#include "Object3D.h"
#include "Mesh.h"
#include "BSP.h"
#include "PVS.h"
#include "VisCells.h"
#include "Texture.h"

namespace ng::renderer::rasterizer {

    enum CULLING
    {
        CULL_NONE = 0,
        CULL_FRONTFACE = 1 << 0,
        CULL_BACKFACE = 1 << 1,
        CULL_FRUSTUM = 1 << 2,
        CULL_HZB = 1 << 3,
        CULL_TRANSFORM = 1 << 4,
        CULL_MOC = 1 << 5,
        CULL_STANDARD = CULL_BACKFACE | CULL_FRUSTUM | CULL_HZB | CULL_TRANSFORM | CULL_MOC
    };

    struct RenderStatistics {
        uint32_t polys_in = 0;
        uint32_t polys_face_culled = 0;
        uint32_t polys_frustum_culled = 0;
        uint32_t objs_in = 0;
        uint32_t objs_frustum_culled = 0;
        uint32_t objs_culled_by_hzb = 0;
        uint32_t polys_culled_by_hzb = 0;
        uint32_t polys_rendered = 0;
        uint32_t polys_culled_by_transform = 0;
        uint32_t objs_culled_by_moc = 0;

        // ---- Welt (RenderWorld: VisCells + PVS) ----
        uint32_t cells_in_pvs = 0;          // vom PVS als moeglicherweise sichtbar geliefert
        uint32_t cells_with_geometry = 0;   // davon Zellen, die Polygone enthalten
        uint32_t cells_frustum_culled = 0;
        uint32_t cells_culled_by_moc = 0;
        uint32_t cells_culled_by_hzb = 0;
        uint32_t cells_rendered = 0;        // tatsaechlich gezeichnete Zellen
    };

    template <class T>
    class RenderPipeline
    {
    public:
        using Rasterizer = ng::renderer::rasterizer::Rasterizer3D<T>;
    public:
        RenderPipeline(Rasterizer& rasterizer) :m_rasterizer(rasterizer)
        {
        }


        void Begin() {

            m_rasterizer.Begin();

            std::memset(&m_statistics, 0, sizeof(m_statistics));
        }

        void End() {
            m_rasterizer.End();
            m_worldTexViewCache.clear();
            m_worldLastDirectTexture = ng::renderer::gfx::Texture();
        }

        void PreSortObjs(bool enable)
        {
            m_pre_zsort_objs = enable;
        }
        inline bool IsPreSortObjsEnabled()
        {
            return m_pre_zsort_objs;
        }


        void RenderLine(const geometry::Vertex<T>& v0, const geometry::Vertex<T>& v1) {
            //std::vector < ng::renderer::rasterizer::ScreenVertex<T> > verts2d;
            std::vector<geometry::Vertex<T>> verts = { v0,v1 };
            std::vector < ng::renderer::rasterizer::ScreenVertex<T> > verts2d = BringToScreen(verts);
            if (verts2d.empty()) return;
            m_rasterizer.DrawLine(verts2d[0], verts2d[1]);

        }

        RenderStatistics GetStatistics() {
            return m_statistics;
        }

        void RenderPrimitive(ng::geometry::Primitive<3, T> prim) {
            m_statistics.polys_in++;
            std::vector < ng::renderer::rasterizer::ScreenVertex<T> > verts2d = BringToScreen(prim.Vertices(), prim.Plane());
            if (verts2d.empty()) return;
            if (verts2d.size() < 3) return;



            m_rasterizer.DrawPolygon(verts2d);


        }

        void RenderPrimitiveW(ng::geometry::Primitive<3, T> prim) {
            m_statistics.polys_in++;
            std::vector < ng::renderer::rasterizer::ScreenVertex<T> > verts2d = BringToScreen(prim.Vertices(), prim.Plane());
            if (verts2d.empty()) return;
            if (verts2d.size() < 3) return;
            m_rasterizer.DrawPolygonW(verts2d);
            m_statistics.polys_rendered++;
        }

        inline void RenderMesh(objects::Mesh<T> object) {
            m_statistics.objs_in++;
            m_statistics.polys_in += (uint32_t)object.TriangleCount();

            ng::bounding::AABBox<3, T> aabb = object.Bounds();
            if (m_culling_mode & CULL_FRUSTUM) {
                if (!m_frustum.intersectsAABB(aabb)) {
                    m_statistics.objs_frustum_culled++;
                    return;
                }
            }

            // NEU: nur noch EIN MOC-Test - gegen den Stand des Buffers, der
            // von allen BISHER in diesem Frame verarbeiteten (front-to-back
            // sortierten!) Objekten aufgebaut wurde. Ersetzt den separaten
            // Occluder-Pass komplett - kein m_occluder_pass mehr noetig.
            if ((m_culling_mode & CULL_MOC))
            {
                if (!MOCTestAABBoxVisibility(aabb)) {
                    m_statistics.objs_culled_by_moc++;
                    return;
                }
            }

            if (m_culling_mode & CULL_HZB) {
                if (!HZBTestAABBoxVisibility(aabb)) {
                    m_statistics.objs_culled_by_hzb++;
                    return;
                }
            }

            const auto& indices = object.GetRenderIndices(m_camera_pos);
            size_t triangleCount = object.RenderTriangleCount(m_camera_pos);
            for (size_t i = 0; i < triangleCount; i++)
            {
                uint32_t a, b, c;
                object.GetRenderTriangleIndices(m_camera_pos, i, a, b, c);
                auto v0 = object.GetVertex(a);
                v0.world() = object.TransformPoint(v0.world());
                auto v1 = object.GetVertex(b);
                auto v2 = object.GetVertex(c);
                v1.world() = object.TransformPoint(v1.world());
                v2.world() = object.TransformPoint(v2.world());

                math::RayPlaneLine<3, T> ray(v0.world(), v1.world(), v2.world());
                auto res = ray.classify(m_camera_pos);
                if ((res == math::RayPlaneLine<3, T>::BACK && m_culling_mode & CULL_BACKFACE) ||
                    (res == math::RayPlaneLine<3, T>::FRONT && m_culling_mode & CULL_FRONTFACE)) {
                    m_statistics.polys_face_culled++;
                    continue;
                }
                std::vector<ng::geometry::Vertex<T>> verts = { v0, v1, v2 };
                if (object.HasUVs())
                {
                    verts[0].texture() = object.GetUV(a);
                    verts[1].texture() = object.GetUV(b);
                    verts[2].texture() = object.GetUV(c);


                }
                {
                    std::vector<ng::renderer::rasterizer::ScreenVertex<T>> verts2d = BringToScreen(verts);
                    


                    if (verts2d.empty()) continue;
                    if (verts2d.size() < 3) continue;

                    // Sichtbares Dreieck rastern ...
                    m_rasterizer.DrawPolygon(verts2d);

                  
                    if ((m_culling_mode & CULL_MOC) && m_rasterizer.IsMocUsed() &&
                        !(m_rasterizer.State().flags & RASTER_TRANSPARENT))
                    {
                        m_rasterizer.DrawOcclusionPolygon(verts2d);
                    }

                    m_statistics.polys_rendered++;
                }
            }
        }

        inline void RenderPolygon(const std::vector<ng::geometry::Vertex<T>>& verts)
        {
            m_statistics.polys_in++;
            std::vector<ng::renderer::rasterizer::ScreenVertex<T>> verts2d = BringToScreen(verts, ng::math::RayPlaneLine<3, T>(verts[0].world(), verts[1].world(), verts[2].world()));
            if (verts2d.size() < 3) return;
            m_rasterizer.DrawPolygon(verts2d);
            m_statistics.polys_rendered++;

        }

        inline void RenderMesh(const std::vector<objects::Mesh<T>>& objects) {
            if (m_pre_zsort_objs) {
                // Nur (Distanz, Index)-Paare sortieren - die Mesh-Objekte
                // selbst werden NICHT kopiert. Distanz wird pro Objekt genau
                // EINMAL berechnet (statt O(n log n) mal im Komparator).
                m_sortScratch.clear();
                m_sortScratch.reserve(objects.size());
                for (size_t i = 0; i < objects.size(); ++i)
                {
                    const auto& box = objects[i].Bounds();
                    auto center = (box.Min() + box.Max()) * static_cast<T>(0.5);
                    auto d = center - m_camera_pos;
                    m_sortScratch.emplace_back(d ^ d, i);
                }
                std::sort(m_sortScratch.begin(), m_sortScratch.end(),
                    [](const auto& a, const auto& b) { return a.first < b.first; });

                for (const auto& [dist, idx] : m_sortScratch)
                    RenderMesh(objects[idx]);
                return;
            }
            for (auto& obj : objects) {
                RenderMesh(obj);
            }
        }
        


        
        template <class T>
        inline void DebugPvsMisses(const math::Matrix<3, 1, T>& camPos,
            const math::Matrix<3, 1, T>& camDir,
            const ng::bsp::VisCells<T>& cells,
            const ng::bsp::PVS<T>& pvs,
            std::type_identity_t<T> cosHalfFov = T(0.5))
        {
            const auto& tree = cells.Tree();
            const int camLeaf = tree.FindLeafId(camPos);
            int missed = 0;
            
            printf("[PVS-MISS] cam=(%.0f,%.0f,%.0f) dir=(%.2f,%.2f,%.2f) leaf=%d\n",
                (double)camPos(0), (double)camPos(1), (double)camPos(2),
                (double)camDir(0), (double)camDir(1), (double)camDir(2), camLeaf);

            for (const auto& leaf : tree.GetLeaves())
            {
                if (!leaf) continue;
                if (pvs.CanSee(camLeaf, leaf->GetID())) continue;   // nur Leaves, die das PVS ausschliesst

                for (const auto& poly : leaf->GetPolygons())
                {
                    if (poly.verts.empty()) continue;

                    // Mittelpunkt des Polygons
                    math::Matrix<3, 1, T> c(0, 0, 0);
                    for (const auto& v : poly.verts) c = c + v.world();
                    c = c * (T(1) / T(poly.verts.size()));

                    math::Matrix<3, 1, T> d = c - camPos;
                    const T dist = d.length();
                    if (dist < T(1)) continue;
                    d = d * (T(1) / dist);

                    if ((d ^ camDir) < cosHalfFov) continue;          // NEU: nur Polygone im Blickfeld
                    if ((d ^ poly.plane.normal()) >= T(0)) continue;  // Rueckseite

                    if (!ng::bsp::BSPRayOccluded(tree, camPos, d, dist - T(0.5),
                        T(1e-3), &poly))
                    {
                        printf("[PVS-MISS] Leaf %d poly sichtbar, aber nicht im PVS (cam=%d) mitte=(%.0f,%.0f,%.0f) dist=%.0f\n",
                            leaf->GetID(), camLeaf,
                            (double)c(0), (double)c(1), (double)c(2), (double)dist);
                        ++missed;
                        break;
                    }
                }
            }
            printf("[PVS-MISS] %d Leaves zu Unrecht ausgeschlossen\n", missed);
        }



        // ----------------------------------------------------------------------
// Ersetzt deine RenderWorld(...) in der Renderer-Klasse.
//
// Neu gegenueber vorher:
//   * Parameter "warps"
//   * Schritt 2-4 sind in die rekursive Lambda "drawView" gewandert
//   * Portal-Polygone werden nicht gezeichnet, sondern gesammelt; danach pro Portal:
//       Tiefe im sichtbaren Portalbereich zuruecksetzen + Gegenseite rendern
//
// Zwei Dinge, die DU liefern musst (mit "HOOK" markiert):
//   HOOK 1: CameraFrustumPlanes()  - Ebenen des Kamera-Frustums (Normalen nach innen, Weltraum)
//   HOOK 2: RASTER_DEPTH_CLEAR     - neues DrawState-Flag im Rasterizer:
//           Pixel, die den Tiefentest bestehen, bekommen Tiefe = "fern" und
//           KEINE Farbe geschrieben.
// ----------------------------------------------------------------------

        template <class MaterialBinder>
        inline uint32_t RenderWorld(const ng::bsp::VisCells<T>& cells, const ng::bsp::PVS<T>& pvs,
            ng::renderer::gfx::Texture lightmapAtlas,
            const std::vector<ng::bsp::WarpPortal<T>>& warps,
            MaterialBinder&& bindMaterial)
        {
            using Vec3 = ng::math::Matrix<3, 1, T>;
            using Plane = ng::math::RayPlaneLine<3, T>;
            using Vertex = ng::geometry::Vertex<T>;
            using ScreenVerts = std::vector<ng::renderer::rasterizer::ScreenVertex<T>>;
            constexpr int kMaxPortalDepth = 4;
            const T kClipEps = T(0.01);

            UpdateCameraPos();

            const auto& polys = cells.Polygons();
            const auto& extrapolys = cells.ExtraPolygons();
            const size_t cellCount = cells.CellCount();

            if (cellCount == 0 || polys.empty())
                return 0;

            // ---------------- Prolog: unveraendert ----------------
            const bool pvsMatches = pvs.IsBuilt() && pvs.LeafCount() == cellCount;

            if (!pvsMatches)
            {
                if (!m_worldWarned)
                {
                    printf("[RenderWorld] PVS passt nicht zu den Zellen (PVS-Leaves=%zu, Zellen=%zu). "
                        "Alle Zellen werden gezeichnet. PVS mit pvs.Build(cells.Tree()) bauen!\n",
                        (size_t)pvs.LeafCount(), cellCount);
                    m_worldWarned = true;
                }
            }
            else
            {
                m_worldWarned = false;
            }

            uint32_t medium = cells.Tree().MediumAtPoint(m_camera_pos);
            const int32_t cell = pvsMatches ? cells.FindCell(m_camera_pos) : -1;

            const bool dataChanged =
                m_worldCellsPtr != &cells ||
                m_worldPvsPtr != &pvs ||
                m_worldCellCount != cellCount ||
                m_worldPolyCount != polys.size() ||
                m_worldPvsMatches != pvsMatches;

            if (dataChanged || m_worldCell != cell)
            {
                if (pvsMatches && pvs.IsActiveLeaf(cell))
                    m_worldVisCells = pvs.VisibleLeaves(cell);
                else
                {
                    m_worldVisCells.resize(cellCount);
                    std::iota(m_worldVisCells.begin(), m_worldVisCells.end(), int32_t(0));
                }

                m_worldCellsPtr = &cells;
                m_worldPvsPtr = &pvs;
                m_worldCell = cell;
                m_worldCellCount = cellCount;
                m_worldPolyCount = polys.size();
                m_worldPvsMatches = pvsMatches;
            }

            if (dataChanged ||
                m_worldBoundsOwner != &cells ||
                m_worldCellBounds.size() != cellCount ||
                m_worldPolyStamp.size() != polys.size())
            {
                BuildWorldCellBounds(cells);
            }

            m_statistics.cells_in_pvs += (uint32_t)m_worldVisCells.size();

            if (m_culling_mode & CULL_FRUSTUM)
                (void)Frustum();

            // ---------------- Lightmap-Bindung (unveraendert) ----------------
            bool haveMaterial = false;
            int lastMaterial = 0;
            m_worldHaveDirectTexture = false;

            const bool haveLightmap = (static_cast<bool>(lightmapAtlas) &&
                (m_rasterizer.State().flags & ng::renderer::rasterizer::RASTER_LIGHTMAP));
            if (haveLightmap)
            {
                m_worldLightmapView.texture = lightmapAtlas;

                ng::renderer::rasterizer::DrawState<T> state = m_rasterizer.State();
                state.lightmapTex = &m_worldLightmapView;
                state.flags |= ng::renderer::rasterizer::RASTER_LIGHTMAP;
                m_rasterizer.SetState(state);
            }

            auto bindPolygonTexture = [&](const auto& poly)
                {
                    if (poly.texture)
                    {
                        const void* key = static_cast<const void*>(poly.texture.get());

                        if (!m_worldHaveDirectTexture || m_worldLastDirectTextureKey != key)
                        {
                            auto it = m_worldTexViewCache.find(key);
                            if (it == m_worldTexViewCache.end())
                            {
                                ng::renderer::rasterizer::TextureView view;
                                view.texture = poly.texture;
                                it = m_worldTexViewCache.emplace(key, std::move(view)).first;
                            }

                            ng::renderer::rasterizer::DrawState<T> state = m_rasterizer.State();
                            state.diffuseTex = &it->second;
                            state.flags |= ng::renderer::rasterizer::RASTER_TEXTURE;
                            m_rasterizer.SetState(state);

                            m_worldLastDirectTextureKey = key;
                            m_worldHaveDirectTexture = true;
                        }
                        haveMaterial = false;
                    }
                    else
                    {
                        if (!haveMaterial || poly.materialId != lastMaterial)
                        {
                            bindMaterial(poly.materialId);
                            lastMaterial = poly.materialId;
                            haveMaterial = true;
                        }

                        if (m_worldHaveDirectTexture)
                        {
                            ng::renderer::rasterizer::DrawState<T> state = m_rasterizer.State();
                            state.flags &= ~ng::renderer::rasterizer::RASTER_TEXTURE;
                            state.diffuseTex = nullptr;
                            m_rasterizer.SetState(state);
                            m_worldHaveDirectTexture = false;
                        }
                    }
                };

            // ---------------- Sicht (Root oder durch ein Portal) ----------------
            struct View
            {
                Vec3 eye;                                   // Auge im nativen Raum dieser Sicht
                std::vector<Plane> planes;                  // Frustum im nativen Raum (Root: leer, nutzt m_frustum)
                const std::vector<int32_t>* visCells = nullptr;
                ng::bsp::RigidXform<T> toScreen;            // nativer Raum -> reale Kamera
            };

            View root;
            root.eye = m_camera_pos;
            root.visCells = &m_worldVisCells;

            // Polygon -> Bildschirm. Root: wie bisher. Tiefer: erst gegen das Portal-Frustum
            // klippen (nativer Raum), dann in den Raum der realen Kamera transformieren.
            auto projectPoly = [&](const auto& poly, const View& v, bool isRoot) -> ScreenVerts
                {
                    if (isRoot)
                        return BringToScreen(poly.verts, poly.plane);

                    std::vector<Vertex> cv;
                    if (!ng::bsp::ClipVertsToPlanes(poly.verts, v.planes, kClipEps, cv))
                        return ScreenVerts();

                    for (auto& x : cv)
                    {
                        x.world() = v.toScreen.Point(x.world());
                        x.normal() = v.toScreen.Dir(x.normal());
                    }

                    Plane pl = poly.plane;
                    if (!ng::bsp::BuildOrientedPlane(cv, v.toScreen.Dir(poly.plane.normal()), pl))
                        return ScreenVerts();

                    return BringToScreen(cv, pl);
                };

            auto drawView = [&](auto&& self, const View& view, int depth) -> void
                {
                    const bool isRoot = (depth == 0);

                    if (++m_worldStampValue == 0)
                    {
                        std::fill(m_worldPolyStamp.begin(), m_worldPolyStamp.end(), 0u);
                        m_worldStampValue = 1;
                    }

                    // ---- Schritt 2: Culling pro Zelle ----
                    std::vector<std::pair<T, int32_t>> order;

                    for (int32_t c : *view.visCells)
                    {
                        if (c < 0 || (size_t)c >= cellCount) continue;
                        if (cells.CellPolygons(c).empty() && cells.CellExtraPolygons(c).empty())
                            continue;

                        if (isRoot) m_statistics.cells_with_geometry++;

                        const ng::bounding::AABBox<3, T> box = m_worldCellBounds[c];

                        if (isRoot)
                        {
                            if ((m_culling_mode & CULL_FRUSTUM) && !m_frustum.intersectsAABB(box))
                            {
                                m_statistics.cells_frustum_culled++;
                                continue;
                            }
                        }
                        else if (ng::bsp::AABBOutsidePlanes(box, view.planes))
                            continue;

                        const auto center = (box.Min() + box.Max()) * static_cast<T>(0.5);
                        const auto d = center - view.eye;
                        order.emplace_back(d ^ d, c);
                    }

                    // ---- Schritt 3: von vorn nach hinten ----
                    std::sort(order.begin(), order.end(),
                        [](const auto& a, const auto& b) { return a.first < b.first; });

                    // ---- Schritt 4: zeichnen ----
                    std::vector<int32_t> hits;                    // sichtbare Portale dieser Sicht
                    std::vector<int32_t> postRender;              // Extra-Polygone (Wasser ...)

                    for (const auto& entry : order)
                    {
                        const int32_t c = entry.second;
                        const ng::bounding::AABBox<3, T> box = m_worldCellBounds[c];

                        // MOC/HZB gelten nur fuer die echte Kamera
                        if (isRoot && (m_culling_mode & CULL_MOC))
                        {
                            if (!MOCTestAABBoxVisibility(box)) { m_statistics.cells_culled_by_moc++; continue; }
                        }
                        if (isRoot && (m_culling_mode & CULL_HZB))
                        {
                            if (!HZBTestAABBoxVisibility(box)) { m_statistics.cells_culled_by_hzb++; continue; }
                        }

                        m_statistics.cells_rendered++;

                        for (uint32_t pi : cells.CellPolygons(c))
                        {
                            if (pi >= polys.size())
                            {
                                printf("[RenderWorld] UNGUELTIGER Polygon-Index: pi=%u, polys.size()=%zu (Zelle %d)\n",
                                    pi, polys.size(), c);
                                continue;
                            }

                            if (m_worldPolyStamp[pi] == m_worldStampValue) continue;
                            m_worldPolyStamp[pi] = m_worldStampValue;

                            const auto& poly = polys[pi];

                            // ---- Portal-Face: nicht zeichnen, sondern merken ----
                            if ((poly.flags & ng::bsp::POLY_FLAG_PORTAL) &&
                                depth < kMaxPortalDepth &&
                                poly.portalId >= 0 && (size_t)poly.portalId < warps.size())
                            {
                                const auto& w = warps[poly.portalId];
                                if ((w.flags & ng::bsp::WARP_RENDER) && w.partner >= 0 &&
                                    w.plane.distance(view.eye) > T(0))
                                {
                                    if (std::find(hits.begin(), hits.end(), poly.portalId) == hits.end())
                                        hits.push_back(poly.portalId);
                                    continue;
                                }
                                // sonst: normal zeichnen (z.B. Maximaltiefe -> Portal-Textur als Fuellung)
                            }

                            m_statistics.polys_in++;

                            ScreenVerts verts2d = projectPoly(poly, view, isRoot);
                            if (verts2d.size() < 3) continue;

                            bindPolygonTexture(poly);
                            m_rasterizer.DrawPolygon(verts2d);

                            if (isRoot && (m_culling_mode & CULL_MOC) && m_rasterizer.IsMocUsed() &&
                                !(m_rasterizer.State().flags & RASTER_TRANSPARENT))
                            {
                                m_rasterizer.DrawOcclusionPolygon(verts2d);
                            }

                            m_statistics.polys_rendered++;
                        }

                        postRender.insert(postRender.end(),
                            cells.CellExtraPolygons(c).begin(), cells.CellExtraPolygons(c).end());
                    }

                    for (uint32_t pi : postRender)
                    {
                        if (pi >= extrapolys.size())
                        {
                            printf("[RenderWorld] UNGUELTIGER Extra-Polygon-Index: pi=%u, extrapolys.size()=%zu\n",
                                pi, extrapolys.size());
                            continue;
                        }

                        const auto& poly = extrapolys[pi];
                        m_statistics.polys_in++;

                        ScreenVerts verts2d = projectPoly(poly, view, isRoot);
                        if (verts2d.size() < 3) continue;

                        bindPolygonTexture(poly);

                        if (poly.materialId == 0)
                        {
                            ng::renderer::rasterizer::DrawState state = m_rasterizer.State();
                            state.flags |= ng::renderer::rasterizer::RASTER_DEPTH_TEST;
                            state.flags &= ~ng::renderer::rasterizer::RASTER_DEPTH_WRITE;
                            state.flags |= ng::renderer::rasterizer::RASTER_TRANSPARENT;
                            state.materialAlpha = 0.8;
                            m_rasterizer.PushState();
                            m_rasterizer.SetState(state);
                            m_rasterizer.DrawPolygon(verts2d);
                            m_rasterizer.PopState();
                        }
                        else m_rasterizer.DrawPolygon(verts2d);

                        if (isRoot && (m_culling_mode & CULL_MOC) && m_rasterizer.IsMocUsed() &&
                            !(m_rasterizer.State().flags & RASTER_TRANSPARENT))
                        {
                            m_rasterizer.DrawOcclusionPolygon(verts2d);
                        }

                        m_statistics.polys_rendered++;
                    }

                    // ---- Portale dieser Sicht: Gegenseite rendern ----
                    if (hits.empty()) return;

                    // Frustum dieser Sicht als Ebenen (Root: von der Kamera, sonst die gespeicherten)
                    std::vector<Plane> curPlanes = isRoot ? CameraFrustumPlanes() /* HOOK 1 */ : view.planes;

                    for (int32_t id : hits)
                    {
                        const auto& A = warps[id];
                        const auto& B = warps[A.partner];

                        Vec3 nextEye;
                        std::vector<Plane> nextPlanes;
                        std::vector<Vec3> entry;

                        if (!ng::bsp::BuildNextFrustum(A, B, view.eye, curPlanes, nextEye, nextPlanes,
                            T(1), &entry))
                            continue;

                        // (a) Sichtbaren Teil des Portals: Tiefe auf "fern" setzen (HOOK 2)
                        {
                            std::vector<Vertex> ev;
                            ev.reserve(entry.size());
                            for (const auto& p : entry)
                            {
                                Vertex v;
                                v.world() = view.toScreen.Point(p);
                                ev.push_back(v);
                            }

                            Plane planeReal = A.plane;
                            if (ng::bsp::BuildOrientedPlane(ev, view.toScreen.Dir(A.normal), planeReal))
                            {
                                ScreenVerts pv = BringToScreen(ev, planeReal);
                                if (pv.size() >= 3)
                                {
                                    ng::renderer::rasterizer::DrawState<T> st = m_rasterizer.State();
                                    st.flags |= ng::renderer::rasterizer::RASTER_DEPTH_TEST;
                                    st.flags |= ng::renderer::rasterizer::RASTER_DEPTH_CLEAR;   // HOOK 2
                                    st.flags &= ~ng::renderer::rasterizer::RASTER_TEXTURE;
                                    st.flags &= ~ng::renderer::rasterizer::RASTER_LIGHTMAP;
                                    m_rasterizer.PushState();
                                    m_rasterizer.SetState(st);
                                    m_rasterizer.DrawPolygon(pv);
                                    m_rasterizer.PopState();
                                    m_worldHaveDirectTexture = false;   // Textur-State wurde gewechselt
                                }
                            }
                        }

                        // (b) Gegenseite: Sicht von B mit B's Ziel-PVS
                        View nv;
                        nv.eye = nextEye;
                        nv.planes = std::move(nextPlanes);
                        nv.visCells = &B.visCells;
                        nv.toScreen = view.toScreen.After(
                            ng::bsp::RigidXform<T>::FromPortals(B, A));   // B-Raum -> A-Raum -> real

                        self(self, nv, depth + 1);
                    }
                };

            drawView(drawView, root, 0);

            // ---------------- Lightmap-State entfernen (unveraendert) ----------------
            if (haveLightmap)
            {
                ng::renderer::rasterizer::DrawState<T> state = m_rasterizer.State();
                state.flags &= ~ng::renderer::rasterizer::RASTER_LIGHTMAP;
                state.lightmapTex = nullptr;
                m_rasterizer.SetState(state);
            }
            return medium;
        }


        // Ohne Material-Callback
        inline uint32_t RenderWorld(const ng::bsp::VisCells<T>& cells, const ng::bsp::PVS<T>& pvs,
            ng::renderer::gfx::Texture lightmapAtlas)
        {
            static const std::vector<ng::bsp::WarpPortal<T>> none;
            return RenderWorld(cells, pvs, lightmapAtlas, none, [](int) {});
        }
        
        // Mit Portalen, ohne Material-Callback
        inline uint32_t RenderWorld(const ng::bsp::VisCells<T>& cells, const ng::bsp::PVS<T>& pvs,
            ng::renderer::gfx::Texture lightmapAtlas,
            const std::vector<ng::bsp::WarpPortal<T>>& warps)
        {
            return RenderWorld(cells, pvs, lightmapAtlas, warps, [](int) {});
        }

        // Muss aufgerufen werden, wenn VisCells/PVS neu gebaut wurden, ohne dass
        // sich ihre Adresse geaendert hat.
        inline void InvalidateWorldCache()
        {
            m_worldCellsPtr = nullptr;
            m_worldPvsPtr = nullptr;
            m_worldBoundsOwner = nullptr;
            m_worldCell = -2;
        }

        inline const ng::math::Matrix<4, 4, T>& ProjectionMatrix() const {
            return m_projection_matrix;

        }

        inline ng::math::Matrix<4, 4, T>& ProjectionMatrix() {
            m_update_combo_matrix = true;;
            m_update_frustum = true;
            return m_projection_matrix;

        }

        inline const ng::math::Matrix<4, 4, T>& ModelViewMatrix() const {
            return m_model_view_matrix;

        }

        inline ng::math::Matrix<4, 4, T>& ModelViewMatrix() {
            m_update_combo_matrix = true;;
            m_update_frustum = true;
            m_update_camera_pos = true;
            return m_model_view_matrix;
        }

        inline const math::Matrix<3, 1, T>& CameraPos() const {
            //    m_camera_pos = GetCameraPositionFromView(m_model_view_matrix);
            m_update_camera_pos = false;
            return m_camera_pos;
        }

        inline math::Matrix<3, 1, T>& CameraPos() {
            m_camera_pos = GetCameraPositionFromView(m_model_view_matrix);
            m_update_frustum = true;
            return m_camera_pos;
        }

        inline const ng::math::Matrix<4, 4, T>& ComboMatrix() const {
            if (!m_update_combo_matrix) return m_combo_matrix;
            m_combo_matrix = m_model_view_matrix >> m_projection_matrix;
            m_update_combo_matrix = false;
            return m_combo_matrix;
        }

        inline const occlusion::Frustum<T> Frustum() {
            if (!m_update_frustum) return m_frustum;
            m_frustum = occlusion::Frustum<T>::buildFrustumFromViewProj(m_model_view_matrix, m_projection_matrix);
            m_update_frustum = false;
            return m_frustum;
        }

        // ----------------------------------------------------------------------
   // In die Klasse RenderPipeline einfügen (z.B. direkt unter Frustum()).
   //
   // Liefert die 5 Sichtebenen der echten Kamera im Weltraum
   // (links, rechts, unten, oben, near), Normalen zeigen nach INNEN.
   //
   // Grundlage: dieselben Sichtbarkeitsbedingungen wie in TransformToScreen():
   //     -w <= x <= w,  -w <= y <= w,  z >= 0
   // Welt->Clip ist affin; die Koeffizienten werden numerisch aus
   // ComboMatrix() gelesen (unabhaengig von der Matrix-Konvention).
   // ----------------------------------------------------------------------
        inline std::vector<ng::math::RayPlaneLine<3, T>> CameraFrustumPlanes()
        {
            using Vec3 = ng::math::Matrix<3, 1, T>;
            using Plane = ng::math::RayPlaneLine<3, T>;

            const auto& M = ComboMatrix();

            const auto c0 = M >> Vec3(0, 0, 0);
            const auto cx = M >> Vec3(1, 0, 0);
            const auto cy = M >> Vec3(0, 1, 0);
            const auto cz = M >> Vec3(0, 0, 1);

            // f(p) = ax*px + ay*py + az*pz + b
            struct Lin { T ax, ay, az, b; };

            auto lin = [&](auto get) -> Lin
                {
                    const T b = (T)get(c0);
                    return Lin{ (T)get(cx) - b, (T)get(cy) - b, (T)get(cz) - b, b };
                };

            const Lin X = lin([](const auto& c) { return c.x(); });
            const Lin Y = lin([](const auto& c) { return c.y(); });
            const Lin Z = lin([](const auto& c) { return c.z(); });
            const Lin W = lin([](const auto& c) { return c.w(); });

            auto comb = [](const Lin& a, const Lin& b, T s) -> Lin
                {
                    return Lin{ a.ax + s * b.ax, a.ay + s * b.ay, a.az + s * b.az, a.b + s * b.b };
                };

            // Ebene "f(p) >= 0 ist innen" als RayPlaneLine mit Normale nach innen
            auto toPlane = [&](const Lin& f, std::vector<Plane>& out)
                {
                    Vec3 n(f.ax, f.ay, f.az);
                    const T len = n.length();
                    if (len < T(1e-12)) return;

                    n = n * (T(1) / len);
                    const T d = f.b / len;                  // n.p + d = 0 auf der Ebene

                    const Vec3 p0 = n * (-d);               // Punkt auf der Ebene

                    Vec3 axis(0, 0, 0);
                    if (std::abs(n(0)) <= std::abs(n(1)) && std::abs(n(0)) <= std::abs(n(2))) axis(0) = T(1);
                    else if (std::abs(n(1)) <= std::abs(n(2)))                                axis(1) = T(1);
                    else                                                                      axis(2) = T(1);

                    Vec3 u = axis % n; u.normalize();
                    Vec3 v = n % u;    v.normalize();

                    Plane pl(p0, p0 + u, p0 + v);
                    if (pl.distance(p0 + n) < T(0))         // Orientierung pruefen, ggf. drehen
                        pl = Plane(p0, p0 + v, p0 + u);

                    out.push_back(pl);
                };

            std::vector<Plane> planes;
            planes.reserve(5);
            toPlane(comb(X, W, T(1)), planes);    // links : x + w >= 0
            toPlane(comb(W, X, T(-1)), planes);   // rechts: w - x >= 0
            toPlane(comb(Y, W, T(1)), planes);    // unten : y + w >= 0
            toPlane(comb(W, Y, T(-1)), planes);   // oben  : w - y >= 0
            toPlane(Z, planes);                   // near  : z >= 0
            return planes;
        }


        inline void SetCamera(const scene::Camera<T>& camera) {

            ModelViewMatrix() = camera.ViewMatrix();

            ProjectionMatrix() = camera.ProjectionMatrix();
            m_camera_pos = camera.position();
            m_camera = camera;






            // Frustum konsistent aus der (jetzt korrigierten) VP-Matrix ableiten,
            // statt aus einer eventuell separat/anders berechneten CameraFrustum.
            m_update_frustum = true;
            m_update_camera_pos = false;

        }
        inline void SetCullingMode(uint8_t  mode) {
            m_culling_mode = mode;
        }

        inline uint8_t GetCullingMode() {
            return m_culling_mode;
        }

        bool HZBTestAABBoxVisibility(ng::bounding::AABBox<3, T> box)
        {

            auto project = [&](math::Matrix<3, 1, T>& world) -> occlusion::ProjectedPoint<T>
                {
                    occlusion::ProjectedPoint<T> p;
                    // Welt -> Clip-Raum (identisch zur normalen Render-Pipeline)
                    auto clip = ComboMatrix() >> world;
                    // Gleiche Near-Clip-Konvention wie in TransformToScreen
                    // (dort: "s.z() < 0.0" => vor der Near-Plane, DirectX-Style
                    // Clip-Range [0, w]).
                    if (clip.w() <= static_cast<T>(1e-5) || clip.z() < static_cast<T>(0))
                    {
                        p.behindCamera = true;
                        return p;
                    }
                    T ow = static_cast<T>(1.0) / clip.w();   // <-- FIX: w statt z
                    p.x = static_cast<T>(
                        m_rasterizer.ScreenCenterX()
                        + clip.x() * ow * m_rasterizer.ScreenCenterX());
                    p.y = static_cast<T>(
                        m_rasterizer.ScreenCenterY()
                        - clip.y() * ow * m_rasterizer.ScreenCenterY());
                    p.oneOverZ = ow;
                    return p;
                };

            return ng::renderer::occlusion::IsAABBoxVisible<T>(
                box,
                m_rasterizer.GetDepthBufferRef(),
                m_rasterizer.GetDepthBuffer().Width(),
                m_rasterizer.GetDepthBuffer().Height(),
                project);
        }

        bool MOCTestAABBoxVisibility(ng::bounding::AABBox<3, T> box)
        {
            if (!(m_culling_mode & CULL_MOC)) return true;
            auto project = [&](math::Matrix<3, 1, T>& world) -> occlusion::ProjectedPoint<T>
                {
                    occlusion::ProjectedPoint<T> p;
                    // Welt -> Clip-Raum (identisch zur normalen Render-Pipeline)
                    auto clip = ComboMatrix() >> world;
                    // Gleiche Near-Clip-Konvention wie in TransformToScreen
                    // (dort: "s.z() < 0.0" => vor der Near-Plane, DirectX-Style
                    // Clip-Range [0, w]).
                    if (clip.w() <= static_cast<T>(1e-5) || clip.z() < static_cast<T>(0))
                    {
                        p.behindCamera = true;
                        return p;
                    }
                    T ow = static_cast<T>(1.0) / clip.w();   // <-- FIX: w statt z
                    p.x = static_cast<T>(
                        m_rasterizer.ScreenCenterX()
                        + clip.x() * ow * m_rasterizer.ScreenCenterX());
                    p.y = static_cast<T>(
                        m_rasterizer.ScreenCenterY()
                        - clip.y() * ow * m_rasterizer.ScreenCenterY());
                    p.oneOverZ = ow;
                    return p;
                };


            auto projectf = [&](math::Matrix<3, 1, float>& world) -> occlusion::ProjectedPoint<float>
                {
                    occlusion::ProjectedPoint<float> p;
                    math::Matrix<3, 1, double> worldh((float)world.x(), (float)world.y(), (float)world.z());
                    // Welt -> Clip-Raum (identisch zur normalen Render-Pipeline)
                    auto clip = ComboMatrix() >> worldh;
                    // Gleiche Near-Clip-Konvention wie in TransformToScreen
                    // (dort: "s.z() < 0.0" => vor der Near-Plane, DirectX-Style
                    // Clip-Range [0, w]).
                    if (clip.w() <= static_cast<float>(1e-5) || clip.z() < static_cast<float>(0))
                    {
                        p.behindCamera = true;
                        return p;
                    }
                    T ow = static_cast<float>(1.0) / (float)clip.w();   // <-- FIX: w statt z
                    p.x = static_cast<float>(
                        m_rasterizer.ScreenCenterX()
                        + (float)clip.x() * ow * m_rasterizer.ScreenCenterX());
                    p.y = static_cast<float>(
                        m_rasterizer.ScreenCenterY()
                        - (float)clip.y() * ow * m_rasterizer.ScreenCenterY());
                    p.oneOverZ = (float)ow;
                    return p;
                };

            return m_rasterizer.GetMOCBufferRef().IsAABBoxVisible(box, projectf);
        }

        template <class MeshT>
        inline void SortObjectsByDistanceToCamera(std::vector<MeshT>& objects) const
        {
            const auto camPos = m_camera_pos; // Kopie, da CameraPos()/m_camera_pos evtl. mutable-Zugriffe hat

            auto sqDistToCamera = [&camPos](const MeshT& obj) -> T
                {
                    // Bounds() ist bereits gecacht (m_boundsDirty-Mechanismus in
                    // Object3D), also faktisch kostenlos, solange sich das Objekt
                    // seit dem letzten Update() nicht mehr verändert hat.
                    const auto& box = obj.Bounds();
                    auto center = (box.Min() + box.Max()) * static_cast<T>(0.5);
                    auto d = center - camPos;
                    return (d ^ d); // Dot-Produkt mit sich selbst = quadrierte Länge
                };

            std::sort(objects.begin(), objects.end(),
                [&](const MeshT& a, const MeshT& b)
                {
                    return sqDistToCamera(a) < sqDistToCamera(b);
                });
        }

        // ------------------------------------------------------------
        // Pointer-/Handle-Variante:
        //
        // Falls die eigentlichen Mesh-Objekte woanders liegen (Szenegraph,
        // Objekt-Pool o.ä.) und nur eine Reihenfolge zum Rendern gebraucht
        // wird, statt die (potenziell großen) Mesh-Objekte selbst zu
        // verschieben. Sortiert nur die Pointer - billige Swaps.
        // ------------------------------------------------------------
        template <class MeshT>
        inline void SortObjectsByDistanceToCamera(std::vector<MeshT*>& objects) const
        {
            const auto camPos = m_camera_pos;

            auto sqDistToCamera = [&camPos](const MeshT* obj) -> T
                {
                    const auto& box = obj->Bounds();
                    auto center = (box.Min() + box.Max()) * static_cast<T>(0.5);
                    auto d = center - camPos;
                    return (d ^ d);
                };

            std::sort(objects.begin(), objects.end(),
                [&](const MeshT* a, const MeshT* b)
                {
                    return sqDistToCamera(a) < sqDistToCamera(b);
                });
        }

    protected:
    private:
        Rasterizer& m_rasterizer;
        mutable std::vector<std::pair<T, size_t>> m_sortScratch;
        mutable ng::math::Matrix<4, 4, T> m_model_view_matrix;
        mutable ng::math::Matrix<4, 4, T> m_projection_matrix;
        mutable ng::math::Matrix<3, 1, T> m_camera_pos;
        mutable ng::math::Matrix<4, 4, T> m_combo_matrix;
        mutable occlusion::Frustum<T> m_frustum;
        mutable bool m_update_camera_pos = true;
        mutable bool m_update_combo_matrix = true;
        mutable bool m_update_frustum = true;
        mutable bool m_pre_zsort_objs = false;
        mutable bool m_occluder_pass = true;


        ng::renderer::rasterizer::TextureView m_worldTexView;
        ng::renderer::rasterizer::TextureView m_worldLightmapView;
        ng::renderer::gfx::Texture m_worldLastDirectTexture;

        std::vector<uint32_t> m_worldExtraPolyStamp;
        uint32_t m_worldExtraStampValue = 0;

        std::unordered_map<const void*, ng::renderer::rasterizer::TextureView> m_worldTexViewCache;
        bool m_worldHaveDirectTexture = false;
        const void* m_worldLastDirectTextureKey = nullptr;


        RenderStatistics m_statistics;
        uint8_t m_culling_mode = CULL_STANDARD;

        scene::Camera<T> m_camera;

        // ---- Cache fuer RenderWorld ----
        const void* m_worldCellsPtr = nullptr;
        const void* m_worldPvsPtr = nullptr;
        int32_t m_worldCell = -2;
        std::vector<int32_t> m_worldVisCells;                       // PVS-Zellen der aktuellen Kamerazelle

        const void* m_worldBoundsOwner = nullptr;
        std::vector<ng::bounding::AABBox<3, T>> m_worldCellBounds;  // AABB pro Zelle
        std::vector<uint32_t> m_worldPolyStamp;                     // Duplikat-Vermeidung
        uint32_t m_worldStampValue = 0;
        size_t m_worldCellCount = 0;
        size_t m_worldPolyCount = 0;
        bool m_worldPvsMatches = false;
        bool m_worldWarned = false;
        std::vector<std::pair<T, int32_t>> m_worldCellOrder;        // (Distanz^2, Zelle) pro Frame


        // Berechnet die Kameraposition nur neu, wenn sich die View-Matrix geaendert hat
        // (die nicht-konstante CameraPos() invertiert bei JEDEM Aufruf die Matrix).
        inline void UpdateCameraPos()
        {
            if (m_update_camera_pos)
            {
                m_camera_pos = GetCameraPositionFromView(m_model_view_matrix);
                m_update_camera_pos = false;
            }
        }

        void BuildWorldCellBounds(const ng::bsp::VisCells<T>& cells)
        {
            const auto& polys = cells.Polygons();

            m_worldCellBounds.assign(cells.CellCount(), ng::bounding::AABBox<3, T>());

            for (size_t c = 0; c < cells.CellCount(); ++c)
            {
                auto& b = m_worldCellBounds[c];
                b.Reset();

                for (uint32_t pi : cells.CellPolygons((int32_t)c))
                {
                    auto verts = polys[pi].verts;   // Kopie: operator+= nimmt evtl. keine const&
                    b += verts;
                }
            }

            m_worldPolyStamp.assign(polys.size(), 0u);
            m_worldStampValue = 0;
            m_worldBoundsOwner = &cells;
        }


        inline void Project(std::vector<ng::renderer::rasterizer::ScreenVertex<T>>& verts2d) {
            for (auto& v : verts2d)
            {
                T ow = (T)1.0 / v.w();
                v.x() = (m_rasterizer.ScreenCenterX() + v.x() * ow * m_rasterizer.ScreenCenterX());
                v.y() = (m_rasterizer.ScreenCenterY() - v.y() * ow * m_rasterizer.ScreenCenterY());

                // NEU: u/v (und fxu/fxv, falls Lightmap genutzt wird) VOR der
                // Rasterisierung durch w teilen. Das ist die Voraussetzung
                // dafuer, dass Gradients/Edge/DrawSpan ueberhaupt korrekt
                // perspektivisch interpolieren koennen - u/w ist ueber die
                // Bildschirmflaeche linear, die rohe UV NICHT.
                v.u() *= ow;
                v.v() *= ow;
                v.fxu() *= ow;
                v.fxv() *= ow;

                v.w() = ow;
            }
        }

        inline bool TransformToScreen(const std::vector<geometry::Vertex<T>>& verts, std::vector < ng::renderer::rasterizer::ScreenVertex<T> >& verts2d) {

            int codeOff = 0xFFFFFFFF;
            int codeOn = 0;
            int code = 0;
            size_t go = 0;


            for (auto v : verts)
            {
                math::Matrix<4, 1, NG_MATH_FLOAT> s = ComboMatrix() >> v.world();
                ng::renderer::rasterizer::ScreenVertex<T> vs;

                vs.x() = s.x();
                vs.y() = s.y();
                vs.z() = s.z();
                vs.w() = s.w();
                vs.color() = v.color();
                vs.normal() = v.normal();
                vs.u() = v.texture().x();
                vs.v() = v.texture().y();

                // NEU: Lightmap-UV mit uebernehmen. Ohne das bleiben fxu()/fxv()
                // uninitialisiert (ScreenVertex setzt sie im Konstruktor nicht),
                // wodurch die Lightmap ueberall denselben (zufaelligen) Punkt des
                // Atlas sampelt statt der tatsaechlich gebackenen Werte.
                vs.fxu() = v.fxtexture().x();
                vs.fxv() = v.fxtexture().y();

                verts2d.push_back(vs);
                code = (s.x() > s.w() ? 1 : 0) | (s.x() < -s.w() ? 2 : 0) |
                    (s.y() > s.w() ? 4 : 0) | (s.y() < -s.w() ? 8 : 0) |
                    (s.z() < 0.0 ? 16 : 0) | (s.z() > s.w() ? 32 : 0);
                codeOff &= code;
                codeOn |= code;
            }


            if (!(m_culling_mode & CULL_TRANSFORM)) return true;
            if (codeOff != 0)
                return false;      // vollständig außerhalb


            return true;     // vollständig innerhalb
        }

        inline math::Matrix<3, 1, T> GetCameraPositionFromView(const math::Matrix<4, 4, T>& viewMatrix)
        {
            math::Matrix<4, 4, T> worldMatrix = viewMatrix; // Kopie, da invert() in-place arbeitet
            T det = worldMatrix.invert();
            assert(det != (T)0 && "View-Matrix ist singulär, kann nicht invertiert werden");
            return worldMatrix.extractTranslation();
        }

        inline std::vector <ng::renderer::rasterizer::ScreenVertex<T>> BringToScreen(const std::vector <ng::geometry::Vertex<T>>& verts, math::RayPlaneLine<3, T> plane) {
            std::vector <ng::renderer::rasterizer::ScreenVertex<T>> verts2d;
            if (m_culling_mode & CULL_BACKFACE || m_culling_mode & CULL_FRONTFACE)
            {
                // GEAENDERT: kein CameraPos() mehr (das invertiert bei jedem Aufruf die
                // View-Matrix), sondern die gecachte Position.
                UpdateCameraPos();
                int res = plane.classify(m_camera_pos);

                // GEAENDERT: der Zaehler stand vorher ausserhalb der Bedingungen und
                // zaehlte jedes Polygon als "face culled".
                if (m_culling_mode & CULL_BACKFACE && res == math::RayPlaneLine<3, T>::BACK) {
                    m_statistics.polys_face_culled++;
                    return std::vector <ng::renderer::rasterizer::ScreenVertex<T>>();
                }
                if (m_culling_mode & CULL_FRONTFACE && res == math::RayPlaneLine<3, T>::FRONT) {
                    m_statistics.polys_face_culled++;
                    return std::vector <ng::renderer::rasterizer::ScreenVertex<T>>();
                }
            }



            return BringToScreen(verts);



        }

        std::vector <ng::renderer::rasterizer::ScreenVertex<T>> BringToScreen(const  std::vector <ng::geometry::Vertex<T>>& verts) {


            auto project = [&](math::Matrix<3, 1, T> world) -> occlusion::ProjectedPoint<T>
                {
                    occlusion::ProjectedPoint<T> p;
                    // Welt -> Clip-Raum (identisch zur normalen Render-Pipeline)
                    auto clip = ComboMatrix() >> world;
                    // Gleiche Near-Clip-Konvention wie in TransformToScreen
                    // (dort: "s.z() < 0.0" => vor der Near-Plane, DirectX-Style
                    // Clip-Range [0, w]).
                    if (clip.w() <= static_cast<T>(1e-5) || clip.z() < static_cast<T>(0))
                    {
                        p.behindCamera = true;
                        return p;
                    }
                    T ow = static_cast<T>(1.0) / clip.w();   // <-- FIX: w statt z
                    p.x = static_cast<T>(
                        m_rasterizer.ScreenCenterX()
                        + clip.x() * ow * m_rasterizer.ScreenCenterX());
                    p.y = static_cast<T>(
                        m_rasterizer.ScreenCenterY()
                        - clip.y() * ow * m_rasterizer.ScreenCenterY());
                    p.oneOverZ = ow;
                    return p;
                };





            std::vector <ng::renderer::rasterizer::ScreenVertex<T>> verts2d;
            std::vector <ng::geometry::Vertex<T>> vv = verts;

          

            if (m_culling_mode & CULL_FRUSTUM) {

                m_frustum = Frustum();

                int res = m_frustum.classify(vv);


                if (res == ng::math::RayPlaneLine<3, T>::SPANNING) {
                    if (vv.size() == 2)
                    {
                        if (!ng::geometry::ClipLineByFrustum(vv[0], vv[1], m_frustum, vv[0], vv[1])) {
                            return std::vector <ng::renderer::rasterizer::ScreenVertex<T>>();
                        }
                    }
                    else {

                        vv = ng::geometry::ClipByFrustum(vv, m_frustum);


                        m_statistics.polys_frustum_culled++;
                    }
                }


            }

           

            if (!TransformToScreen(vv, verts2d) && (m_culling_mode & CULL_TRANSFORM)) {
                m_statistics.polys_culled_by_transform++;
                return std::vector <ng::renderer::rasterizer::ScreenVertex<T>>();
            }


            Project(verts2d);

            return verts2d;
        }





    };
}