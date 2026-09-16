#pragma once
#include "Global.h"
#include "MathUtils.h"
#include "Matrix.h"
#include "Quaternion.h"
#include "RayPlaneLine.h"
#include "BoundingSphere.h"
#include "AABBox.h"
#include "Bounding.h"
#include "Vertex.h"
//scene
#include "Camera.h"
//controller
#include "CameraController.h"

//buffer
#include "PixelBuffer.h"
#include "ZBuffer.h"
#include "HierarchicalZBuffer.h"
//gfx2d

#include "Canvas2D.h"
#include "Object2D.h"
#include "Object3D.h"
#include "Sprite.h"

// Spatial
#include "KDTree2D.h"
#include "KDTree3D.h"

#include "Visualize.h"

#include "FPSCounter.h"
#include "fxlib.h"

#include "ResourceManager.h"
#include "Image.h"
#include "Texture.h"

#include "Rasterizer3D.h"
#include "RenderPipeline.h"

#include "Clipping.h"

#include "Mesh.h"
#include "MeshGenerator.h"

#include "VirtualFileSystem.h"
#include "VFSFileManager.h"
#include "VFSDirectoryDrive.h"
#include "VFSZipFile.h"
#include "VFSZipDrive.h"
#include "BspBuilder.h"
#include "SkyBoxGen.h"
#include "Portalgen.h"
#include "Floodfill.h"
#include "Bsprenderer.h"


#include "WaveformObj.h"
#ifndef NG_BIND_MANAGER
#define NG_BIND_MANAGER ng::resource::Manager m_static_main_manager;\
                        ng::resource::ImageResourceManager m_static_image_resource_manager; \
                        ng::resource::TextureResourceManager m_static_texture_resource_manager; \
                        ng::resource::WaveformObjResourceManager m_static_waveform_ob_resource_manager;
                        
#endif


namespace ng::renderer::occlusion
{
    // ================================================================
    // BuildPrimitivesFromFrustum
    //
    // Baut aus einem generischen Frustum<T> (beliebig viele Ebenen,
    // keine Near/Far/Top/Bottom-Semantik) ein Primitive<3,T> PRO
    // SEITENFLAECHE. Nuetzlich zum Testen/Visualisieren, z.B. um
    // Primitive::Area(), Primitive::Center() oder eigene
    // Primitive-basierte Renderpfade gegen ein Frustum zu pruefen.
    //
    // Vorgehen (analog zum generischen Wireframe-Widget):
    //   1. Fuer jede Plane: alle Vertices einsammeln, die
    //      (innerhalb einer Toleranz) auf dieser Plane liegen
    //      -> das sind die Eckpunkte der zugehoerigen Flaeche.
    //   2. Punkte um ihren Schwerpunkt herum winkelsortieren
    //      (2D-Basis in der Ebene aufspannen), damit ein konvexes
    //      Polygon entsteht statt einer wirren Punktwolke.
    //   3. Primitive<3,T> aus den sortierten Punkten bauen.
    //   4. Winding-Check: Primitive::UpdatePlane() berechnet die
    //      Normale nach SEINER EIGENEN Konvention (e1 % e0). Damit
    //      das erzeugte Primitive dieselbe Normalenrichtung wie die
    //      Frustum-Ebene hat (nach INNEN, siehe deine Konvention),
    //      wird bei Bedarf Reverse() aufgerufen.
    //
    // Voraussetzung: frustum.vertices() ist befuellt, d.h.
    // computeVerticesFromPlanes() wurde bereits aufgerufen
    // (passiert automatisch im Frustum(planes)-Konstruktor).
    // ================================================================
    template<class T>
    inline std::vector<ng::geometry::Primitive<3, T>> BuildPrimitivesFromFrustum(
         Frustum<T>& frustum)
    {
        using Vec3 = math::Matrix<3, 1, T>;
        std::vector<ng::geometry::Primitive<3, T>> result;
        // Bereits vorhandene (z.B. von CameraFrustum exakt berechnete) Vertices
        // NICHT überschreiben - nur als Fallback für generische Frustums nutzen.
        if (frustum.vertices().empty())
            frustum.computeVerticesFromPlanes();


        const auto& verts = frustum.vertices();
        const auto& planes = frustum.planes();

        if (verts.size() < 4 || planes.empty())
            return result;

        // Etwas toleranter als exakte Ebenengleichheit, da die
        // Vertices selbst schon aus 3-Ebenen-Schnitten mit
        // Rundungsfehlern berechnet wurden (computeVerticesFromPlanes).
        const T eps = math::EPSILON<T> *(T)10;
        int i = 0;
        int j = 0;
        for (const auto& plane : planes)
        {    
            //   printf("Plane %i \n", (i++));
            // --------------------------------------------------
            // 1) Vertices sammeln, die auf dieser Plane liegen
            // --------------------------------------------------
            std::vector<Vec3> face;
            face.reserve(verts.size());

            const T nlen = plane.normal().length();
            if (nlen <= math::EPSILON<T>)
                continue;

            for (const auto& v : verts)
            {
                T d = (plane.normal().x() * v.x() +
                    plane.normal().y() * v.y() +
                    plane.normal().z() * v.z() +
                    plane.D()) / nlen;

                // Toleranz an die Größenordnung des Punktes koppeln,
                // da der numerische Fehler aus der 3-Ebenen-Schnittberechnung
                // mit den Koordinatenbeträgen mitwächst.
                T scale = std::max((T)1, v.length());
                T localEps = eps * scale;

                if (std::abs(d) <= localEps)
                    face.push_back(v);
            }
            // Degenerierte/nicht beitragende Ebene ueberspringen
            if (face.size() < 3)
                continue;

            // --------------------------------------------------
            // 2) Schwerpunkt + 2D-Basis in der Ebene aufspannen
            // --------------------------------------------------
            Vec3 centroid((T)0, (T)0, (T)0);
            for (const auto& p : face)
                centroid += p;
            centroid = centroid * ((T)1 / (T)face.size());

            Vec3 n = plane.normal();
            Vec3 arbitrary =
                (std::abs(n.x()) < (T)0.9)
                ? Vec3((T)1, (T)0, (T)0)
                : Vec3((T)0, (T)1, (T)0);

            Vec3 tangent1 = n % arbitrary;
            T tlen = tangent1.length();
            if (tlen <= math::EPSILON<T>)
                continue; // sollte praktisch nie passieren
            tangent1 = tangent1 * ((T)1 / tlen);
            Vec3 tangent2 = n % tangent1;

            // --------------------------------------------------
            // Punkte winkelsortieren -> konvexes Polygon
            // --------------------------------------------------
            std::sort(
                face.begin(),
                face.end(),
                [&](const Vec3& a, const Vec3& b)
                {
                    Vec3 ra = a - centroid;
                    Vec3 rb = b - centroid;
                    T au = ra.x() * tangent1.x() + ra.y() * tangent1.y() + ra.z() * tangent1.z();
                    T av = ra.x() * tangent2.x() + ra.y() * tangent2.y() + ra.z() * tangent2.z();
                    T bu = rb.x() * tangent1.x() + rb.y() * tangent1.y() + rb.z() * tangent1.z();
                    T bv = rb.x() * tangent2.x() + rb.y() * tangent2.y() + rb.z() * tangent2.z();
                    return std::atan2((float)av, (float)au) < std::atan2((float)bv, (float)bu);
                });

            // --------------------------------------------------
            // 3) Primitive bauen (Konstruktor ruft UpdatePlane()
            //    automatisch auf)
            // --------------------------------------------------
            ng::geometry::Primitive<3, T> prim(face);
            int kk = 0;
           // for (auto v : face) printf("%i %f %f %f\n",(kk++), v.x(), v.y(), v.z());
            // --------------------------------------------------
            // 4) Winding-Check: Normale des Primitive mit der
            //    Frustum-Ebenen-Normale abgleichen. Zeigen sie in
            //    unterschiedliche Richtungen, Vertexreihenfolge
            //    umdrehen, damit prim.Plane().normal() konsistent
            //    zur Frustum-Konvention (nach innen) zeigt.
            // --------------------------------------------------
            T dot =
                prim.Plane().normal().x() * n.x() +
                prim.Plane().normal().y() * n.y() +
                prim.Plane().normal().z() * n.z();

            if (dot < (T)0)
                prim.Reverse();
            
            result.push_back(std::move(prim));
        }

        return result;
    }
}


