#pragma once

#include <vector>
#include <cassert>
#include <cstdio>
#include <utility>
#include "Global.h"
#include "RayPlaneLine.h"
#include "AABBox.h"
#include "Vertex.h"
namespace ng::renderer::occlusion
{


    enum class ContainResult
    {
        OUTSIDE = 0,
        INTERSECTING = 1,
        INSIDE = 2
    };


    template <class T>
    class Frustum
    {
    private:

        std::vector<math::RayPlaneLine<3, T>> _frustum;
        std::vector<math::Matrix<3, 1, T>>    _vertices;


    public:

        // ============================================================
        // Construction / Destruction
        // ============================================================

        Frustum()
        {
        }

        Frustum(
            const std::vector<math::RayPlaneLine<3, T>>& inplanes)
            : _frustum(inplanes)
        {
            computeVerticesFromPlanes();
        }

        ~Frustum()
        {
        }


        // ============================================================
        // Clear
        // ============================================================

        inline void clear()
        {
            _frustum.clear();
            _vertices.clear();
        }

        inline void invert() {
            for (auto& p : _frustum) p.invert();
        }

        // ============================================================
        // Build Frustum from View / Projection
        //
        // RayPlaneLine erwartet:
        //
        //     RayPlaneLine(normal, point)
        //
        // und NICHT:
        //
        //     RayPlaneLine(normal, D)
        //
        // ============================================================
        
        static Frustum<T> buildFrustumFromViewProj(
            const ng::math::Matrix<4, 4, T>& view,
            const ng::math::Matrix<4, 4, T>& proj,
            bool zeroToOneDepth = false)
        {
            using Vec4 = ng::math::Matrix<4, 1, T>;
            using Vec3 = ng::math::Matrix<3, 1, T>;

            /*
                Deine Matrix-Konvention:

                    View >> Projection

                ergibt die kombinierte ViewProjection-Matrix.
            */

            ng::math::Matrix<4, 4, T> VP = view >> proj;

            Vec4 rowX = VP.extractXVector();
            Vec4 rowY = VP.extractYVector();
            Vec4 rowZ = VP.extractZVector();
            Vec4 rowW = VP.extractWVector();
            auto makePlane = [](const Vec4& v)
                {
                    Vec3 n(
                        v.x(),
                        v.y(),
                        v.z()
                    );

                    T len = n.length();

                    if (len <= math::EPSILON<T>)
                    {
                        /*
                            Sollte bei einer gültigen ViewProjection-Matrix
                            nicht auftreten.

                            Da RayPlaneLine keinen (normal, D)-Konstruktor
                            besitzt, können wir hier nicht einfach eine
                            ungültige Ebene erzeugen.
                        */

                        return ng::math::RayPlaneLine<3, T>(
                            Vec3((T)0, (T)0, (T)1),
                            Vec3((T)0, (T)0, (T)0)
                        );
                    }

                    n /= len;

                    T d = v.w() / len;

                    Vec3 point = -d * n;

                    return ng::math::RayPlaneLine<3, T>(point,n);
                };


            std::vector<ng::math::RayPlaneLine<3, T>> planes;

            planes.reserve(6);


            // --------------------------------------------------------
            // Near
            // --------------------------------------------------------

            if (zeroToOneDepth)
            {
                // D3D / Vulkan-artiger Depth-Bereich [0,1]
                planes.push_back(
                    makePlane(rowZ)
                );
            }
            else
            {
                // OpenGL-artiger Depth-Bereich [-1,1]
                planes.push_back(
                    makePlane(rowW + rowZ)
                );
            }


            // --------------------------------------------------------
            // Far
            // --------------------------------------------------------

            planes.push_back(
                makePlane(rowW - rowZ)
            );


            // --------------------------------------------------------
            // Top
            // --------------------------------------------------------

            planes.push_back(
                makePlane(rowW - rowY)
            );


            // --------------------------------------------------------
            // Bottom
            // --------------------------------------------------------

            planes.push_back(
                makePlane(rowW + rowY)
            );


            // --------------------------------------------------------
            // Left
            // --------------------------------------------------------

            planes.push_back(
                makePlane(rowW + rowX)
            );


            // --------------------------------------------------------
            // Right
            // --------------------------------------------------------

            planes.push_back(
                makePlane(rowW - rowX)
            );


            return Frustum<T>(planes);
        }


        // ============================================================
        // Plane access
        // ============================================================

        inline std::vector<math::RayPlaneLine<3, T>>& planes()
        {
            return _frustum;
        }

        inline const std::vector<math::RayPlaneLine<3, T>>& planes() const
        {
            return _frustum;
        }


        // ============================================================
        // Vertex access
        // ============================================================

        inline const std::vector<math::Matrix<3, 1, T>>& vertices() const
        {
            return _vertices;
        }

        inline void setVertices(
            std::vector<math::Matrix<3, 1, T>> v)
        {
            _vertices = std::move(v);
        }

        inline void addVertex(
            const math::Matrix<3, 1, T>& v)
        {
            _vertices.push_back(v);
        }


        // ============================================================
        // AABB intersection
        // ============================================================

        inline bool intersectsAABB(
            const bounding::AABBox<3, T>& aabb) const
        {
            for (const auto& plane : _frustum)
            {
                T px =
                    (plane.normal().x() >= (T)0)
                    ? aabb.Max().x()
                    : aabb.Min().x();

                T py =
                    (plane.normal().y() >= (T)0)
                    ? aabb.Max().y()
                    : aabb.Min().y();

                T pz =
                    (plane.normal().z() >= (T)0)
                    ? aabb.Max().z()
                    : aabb.Min().z();


                T distance =
                    plane.normal().x() * px +
                    plane.normal().y() * py +
                    plane.normal().z() * pz +
                    plane.D();


                if (distance < (T)0)
                {
                    return false;
                }
            }

            return true;
        }


        // ============================================================
        // Polygon classification
        // ============================================================

        inline int classify(
            const std::vector<ng::geometry::Vertex<T>>& verts) const
        {
            for (const auto& f : _frustum)
            {
                int res = Classify(f, verts);

                if (res ==
                    math::RayPlaneLine<3, T>::BACK ||
                    res ==
                    math::RayPlaneLine<3, T>::SPANNING)
                {
                    return res;
                }
            }

            return math::RayPlaneLine<3, T>::FRONT;
        }


        // ============================================================
        // Polygon classification debug
        // ============================================================

        inline int classifyDebug(
            const std::vector<ng::geometry::Vertex<T>>& verts) const
        {

            int rres = math::RayPlaneLine<3, T>::FRONT;
            int i = 0;

            printf("\nFrustum Classify Debug:\n");

            for (const auto& f : _frustum)
            {
                int res = Classify(f, verts);


                switch (i)
                {
                case 0:
                    printf("NEAR PLANE: ");
                    break;

                case 1:
                    printf("FAR PLANE: ");
                    break;

                case 2:
                    printf("TOP PLANE: ");
                    break;

                case 3:
                    printf("BOTTOM PLANE: ");
                    break;

                case 4:
                    printf("LEFT PLANE: ");
                    break;

                case 5:
                    printf("RIGHT PLANE: ");
                    break;

                default:
                    break;
                }


                switch (res)
                {
                case math::RayPlaneLine<3, T>::BACK:
                    printf("BACK\n");
                    break;

                case math::RayPlaneLine<3, T>::FRONT:
                    printf("FRONT\n");
                    break;

                case math::RayPlaneLine<3, T>::SPANNING:
                    printf("SPANNING\n");
                    break;

                case math::RayPlaneLine<3, T>::PLANAR:
                    printf("PLANAR\n");
                    break;
                }


                if (res ==
                    math::RayPlaneLine<3, T>::BACK ||
                    res ==
                    math::RayPlaneLine<3, T>::SPANNING)
                {

                    rres = res;   //  return res;
                }

                ++i;
            }

            return rres;
        }


        // ============================================================
        // Point inside frustum
        // ============================================================

        inline bool posInside(
            const math::Matrix<3, 1, T>& pos) const
        {
            for (const auto& plane : _frustum)
            {
                T distance =
                    plane.normal().x() * pos.x() +
                    plane.normal().y() * pos.y() +
                    plane.normal().z() * pos.z() +
                    plane.D();


                if (distance < (T)0)
                    return false;
            }

            return true;
        }


        // ============================================================
        // AABB containment
        // ============================================================

        inline ContainResult aabbInside(
            const bounding::AABBox<3, T>& aabb) const
        {
            bool intersecting = false;


            for (const auto& plane : _frustum)
            {
                T pxFar =
                    (plane.normal().x() >= (T)0)
                    ? aabb.Max().x()
                    : aabb.Min().x();

                T pyFar =
                    (plane.normal().y() >= (T)0)
                    ? aabb.Max().y()
                    : aabb.Min().y();

                T pzFar =
                    (plane.normal().z() >= (T)0)
                    ? aabb.Max().z()
                    : aabb.Min().z();


                T pxNear =
                    (plane.normal().x() >= (T)0)
                    ? aabb.Min().x()
                    : aabb.Max().x();

                T pyNear =
                    (plane.normal().y() >= (T)0)
                    ? aabb.Min().y()
                    : aabb.Max().y();

                T pzNear =
                    (plane.normal().z() >= (T)0)
                    ? aabb.Min().z()
                    : aabb.Max().z();


                T distFar =
                    plane.normal().x() * pxFar +
                    plane.normal().y() * pyFar +
                    plane.normal().z() * pzFar +
                    plane.D();


                T distNear =
                    plane.normal().x() * pxNear +
                    plane.normal().y() * pyNear +
                    plane.normal().z() * pzNear +
                    plane.D();


                if (distFar < (T)0)
                    return ContainResult::OUTSIDE;


                if (distNear < (T)0)
                    intersecting = true;
            }


            return intersecting
                ? ContainResult::INTERSECTING
                : ContainResult::INSIDE;
        }


        // ============================================================
        // Sphere containment
        // ============================================================

        inline ContainResult sphereInside(
            const math::Matrix<3, 1, T>& center,
            T radius) const
        {
            bool intersecting = false;


            for (const auto& plane : _frustum)
            {
                T distance =
                    plane.normal().x() * center.x() +
                    plane.normal().y() * center.y() +
                    plane.normal().z() * center.z() +
                    plane.D();


                if (distance < -radius)
                    return ContainResult::OUTSIDE;


                if (distance < radius)
                    intersecting = true;
            }


            return intersecting
                ? ContainResult::INTERSECTING
                : ContainResult::INSIDE;
        }


        // ============================================================
        // Frustum collision
        // ============================================================

        inline bool collide(
            const occlusion::Frustum<T>& other) const
        {
            assert(
                !_vertices.empty() &&
                !other._vertices.empty() &&
                "collide() braucht Vertices auf beiden Seiten - "
                "setVertices() aufrufen"
            );


            // --------------------------------------------------------
            // Test planes of this frustum against other vertices
            // --------------------------------------------------------

            for (const auto& plane : _frustum)
            {
                bool allOutside = true;


                for (const auto& v : other._vertices)
                {
                    T d =
                        plane.normal().x() * v.x() +
                        plane.normal().y() * v.y() +
                        plane.normal().z() * v.z() +
                        plane.D();


                    if (d >= (T)0)
                    {
                        allOutside = false;
                        break;
                    }
                }


                if (allOutside)
                    return false;
            }


            // --------------------------------------------------------
            // Test planes of other frustum against our vertices
            // --------------------------------------------------------

            for (const auto& plane : other._frustum)
            {
                bool allOutside = true;


                for (const auto& v : _vertices)
                {
                    T d =
                        plane.normal().x() * v.x() +
                        plane.normal().y() * v.y() +
                        plane.normal().z() * v.z() +
                        plane.D();


                    if (d >= (T)0)
                    {
                        allOutside = false;
                        break;
                    }
                }


                if (allOutside)
                    return false;
            }


            return true;
        }


        // ============================================================
        // Compute frustum vertices
        // ============================================================


        inline void computeVerticesFromPlanes()
        {
            _vertices.clear();
            const size_t n = _frustum.size();
            if (n < 4)
                return;
            const T epsAbs = math::EPSILON<T>;
            for (size_t i = 0; i < n; ++i)
                for (size_t j = i + 1; j < n; ++j)
                    for (size_t k = j + 1; k < n; ++k)
                    {
                        math::RayPlaneLine<3, T> a = _frustum[i];
                        math::RayPlaneLine<3, T> b = _frustum[j];
                        math::RayPlaneLine<3, T> c = _frustum[k];
                        math::Matrix<3, 1, T> p;
                        if (!a.intersection(b, c, p))
                            continue;

                        // Skalierte Toleranz - analog zu BuildPrimitivesFromFrustum
                        T scale = std::max((T)1, p.length());
                        T eps = epsAbs * scale * (T)10;

                        bool inside = true;
                        for (size_t m = 0; m < n; ++m)
                        {
                            if (m == i || m == j || m == k) continue;
                            const auto& pl = _frustum[m];
                            T d = pl.normal().x() * p.x() +
                                pl.normal().y() * p.y() +
                                pl.normal().z() * p.z() +
                                pl.D();
                            if (d < -eps) { inside = false; break; }
                        }
                        if (!inside) continue;

                        bool duplicate = false;
                        for (const auto& v : _vertices)
                        {
                            if ((v - p).length() < eps) { duplicate = true; break; }
                        }
                        if (duplicate) continue;

                        _vertices.push_back(p);
                    }
        }



    private:

        // ============================================================
        // Classify Matrix vertices against plane
        // ============================================================

        inline int Classify(
            const math::RayPlaneLine<3, T>& plane,
            const std::vector<
            math::Matrix<3, 1, T>>&verts) const
        {
            bool front = false;
            bool back = false;


            for (const auto& v : verts)
            {
                T d =
                    (v ^ plane.normal()) +
                    plane.D();


                if (d > math::EPSILON<T>)
                {
                    front = true;
                }
                else if (d < -math::EPSILON<T>)
                {
                    back = true;
                }


                if (front && back)
                    return math::RayPlaneLine<3, T>::SPANNING;
            }


            if (front)
                return math::RayPlaneLine<3, T>::FRONT;


            if (back)
                return math::RayPlaneLine<3, T>::BACK;


            return math::RayPlaneLine<3, T>::PLANAR;
        }


        // ============================================================
        // Classify geometry vertices against plane
        // ============================================================

        inline int Classify(
            const math::RayPlaneLine<3, T>& plane,
            const std::vector<
            geometry::Vertex<T>>&verts) const
        {
            bool front = false;
            bool back = false;


            for (const auto& v : verts)
            {
                T d =
                    (v.world() ^ plane.normal()) +
                    plane.D();


                if (d > math::EPSILON<T>)
                {
                    front = true;
                }
                else if (d < -math::EPSILON<T>)
                {
                    back = true;
                }


                if (front && back)
                    return math::RayPlaneLine<3, T>::SPANNING;
            }


            if (front)
                return math::RayPlaneLine<3, T>::FRONT;


            if (back)
                return math::RayPlaneLine<3, T>::BACK;


            return math::RayPlaneLine<3, T>::PLANAR;
        }
    };


}