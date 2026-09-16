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
#include "HZBOcclusion.h"
#include "Object3D.h"
#include "Mesh.h"

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
                    //printf("Poly3d________________________\n");
                    //for (auto v : verts) {
                    //    printf("%f %f %f  ->   %f %f \n", v.world().x(), v.world().y(), v.world().z(), v.texture().u(), v.texture().v());
                    //}


                    if (verts2d.empty()) continue;
                    if (verts2d.size() < 3) continue;

                    // Sichtbares Dreieck rastern ...
                    m_rasterizer.DrawPolygon(verts2d);
                    
                    //   printf("Poly2d________________________\n");
                    //for (auto v : verts2d) {
                     //   printf("%f %f %f  ->   %f %f \n", v.x(), v.y(), v.z(), v.u(), v.v());
                  //  }
                    // ... UND gleichzeitig in den MOC-Buffer schreiben, damit
                // WEITER ENTFERNTE Objekte im selben Durchlauf dagegen
                // testen koennen. Transparente Objekte sollen NICHT
                // occludieren - sie lassen ja etwas durchscheinen.
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
        /*
        inline void RenderMesh(const std::vector<objects::Mesh<T>>&object) {
            if (m_pre_zsort_objs) {
                std::vector<objects::Mesh<T>> objs = object;
                SortObjectsByDistanceToCamera<objects::Mesh<T>>(objs);
                for (auto& obj : objs) {
                    RenderMesh(obj);
                }
                return;
            }
            for (auto& obj : object) {
                RenderMesh(obj);
            }
        }
        */
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

        /*
        inline void SetCamera(const scene::Camera<T>& camera) {

            ModelViewMatrix() = camera.ViewMatrix();
            ProjectionMatrix() = camera.ProjectionMatrix();
            m_update_frustum = false;
            m_update_camera_pos = false;
            m_camera_pos = camera.position();
            m_frustum = scene::CameraFrustum<T>(camera);
            m_camera = camera;
        }
        */

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
                    // seit dem letzten Update() nicht mehr ver‰ndert hat.
                    const auto& box = obj.Bounds();
                    auto center = (box.Min() + box.Max()) * static_cast<T>(0.5);
                    auto d = center - camPos;
                    return (d ^ d); // Dot-Produkt mit sich selbst = quadrierte L‰nge
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
        // Objekt-Pool o.‰.) und nur eine Reihenfolge zum Rendern gebraucht
        // wird, statt die (potenziell groﬂen) Mesh-Objekte selbst zu
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

        RenderStatistics m_statistics;
        uint8_t m_culling_mode = CULL_STANDARD;

        scene::Camera<T> m_camera;


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

                verts2d.push_back(vs);
                code = (s.x() > s.w() ? 1 : 0) | (s.x() < -s.w() ? 2 : 0) |
                    (s.y() > s.w() ? 4 : 0) | (s.y() < -s.w() ? 8 : 0) |
                    (s.z() < 0.0 ? 16 : 0) | (s.z() > s.w() ? 32 : 0);
                codeOff &= code;
                codeOn |= code;
            }


            if (!(m_culling_mode & CULL_TRANSFORM)) return true;
            if (codeOff != 0)
                return false;      // vollst‰ndig auﬂerhalb


            return true;     // vollst‰ndig innerhalb

            //    return codeOff !=0;

        }

        inline math::Matrix<3, 1, T> GetCameraPositionFromView(const math::Matrix<4, 4, T>& viewMatrix)
        {
            math::Matrix<4, 4, T> worldMatrix = viewMatrix; // Kopie, da invert() in-place arbeitet
            T det = worldMatrix.invert();
            assert(det != (T)0 && "View-Matrix ist singul‰r, kann nicht invertiert werden");
            return worldMatrix.extractTranslation();
        }

        inline std::vector <ng::renderer::rasterizer::ScreenVertex<T>> BringToScreen(const std::vector <ng::geometry::Vertex<T>>& verts, math::RayPlaneLine<3, T> plane) {
            std::vector <ng::renderer::rasterizer::ScreenVertex<T>> verts2d;
            if (m_culling_mode & CULL_BACKFACE || m_culling_mode & CULL_FRONTFACE)
            {
                int res = plane.classify(CameraPos());
                if (m_culling_mode & CULL_BACKFACE && res == math::RayPlaneLine<3, T>::BACK) return std::vector <ng::renderer::rasterizer::ScreenVertex<T>>(); else
                    if (m_culling_mode & CULL_FRONTFACE && res == math::RayPlaneLine<3, T>::FRONT) return std::vector <ng::renderer::rasterizer::ScreenVertex<T>>();
                m_statistics.polys_face_culled++;
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

            //   printf("Vor Frustum Clip: ");
            //for (auto& v : vv) printf("[%f %f] ", v.texture().x(), v.texture().y());
            //printf("\n");

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

            //printf("nach Frustum Clip: ");
            //for (auto& v : vv) printf("[%f %f] ", v.texture().x(), v.texture().y());
            //printf("\n");


            if (!TransformToScreen(vv, verts2d) && (m_culling_mode & CULL_TRANSFORM)) {
                m_statistics.polys_culled_by_transform++;
                return std::vector <ng::renderer::rasterizer::ScreenVertex<T>>();
            }

            //printf("nach transform Clip: ");
            //for (auto& v : verts2d) printf("[%f %f] ", v.u(), v.v());
            //printf("\n");

            Project(verts2d);

            return verts2d;
        }





    };
}

