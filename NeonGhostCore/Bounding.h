#pragma once

#include <vector>
#include <cmath>
#include "AABBox.h"
#include "BoundingSphere.h"
#include "AABBox2D.h"
#include "BoundingCircle2D.h"
#include "Vertex.h"
#include "Primitive.h"
#include <algorithm>
#include "Global.h"

namespace ng {
    namespace bounding {

        // ================================================================
        // Sphere / AABB
        // ================================================================

        template<typename T>
        inline  bool Intersects(const BoundingSphere<T>& sphere, const AABBox<3, T>& box)
        {

            const auto& center = sphere.Center();

            T x = std::max(
                box.Min().x(),
                std::min(center.x(), box.Max().x()));

            T y = std::max(
                box.Min().y(),
                std::min(center.y(), box.Max().y()));

            T z = std::max(
                box.Min().z(),
                std::min(center.z(), box.Max().z()));

            T dx = center.x() - x;
            T dy = center.y() - y;
            T dz = center.z() - z;

            T distanceSquared =
                dx * dx +
                dy * dy +
                dz * dz;

            T radius = sphere.Radius();

            return distanceSquared <= radius * radius;
        }

        template<typename T>
        inline  bool Intersects(const AABBox<3, T>& box, const BoundingSphere<T>& sphere)
        {
            return Intersects(sphere,box);
        }


        template<typename T>
        inline  bool Intersects(const AABBox<3, T>& boxa, const AABBox<3, T>& boxb)
        {
            return boxa.IntersectedByBounds(boxb);
        }

        template<typename T>
        inline  bool Intersects(const BoundingSphere<T>& spherea, const BoundingSphere<T>& sphereb)
        {
            return spherea.Intersects(sphereb);
        }



    // ============================== Vector3 ==============================

        template<class T>
        inline AABBox<3, T>& operator+=(AABBox<3, T>& box, const std::vector<ng::math::Matrix<3, 1, T>>& vertices)
        {
            for (const auto& vertex : vertices) box += vertex;
            return box;
        }

        template<class T>
        inline AABBox<3, T> operator+(AABBox<3, T> box, const std::vector<ng::math::Matrix<3, 1, T>>& vertices)
        {
            box += vertices;
            return box;
        }

        // ============================== Vertex3 ==============================

        template<class T>
        inline AABBox<3, T>& operator+=(AABBox<3, T>& box, const std::vector<ng::geometry::Vertex<T>>& vertices)
        {
            for (const auto& vertex : vertices) box += vertex.world();
            return box;
        }

        template<class T>
        inline AABBox<3, T> operator+(AABBox<3, T> box, const std::vector<ng::geometry::Vertex<T>>& vertices)
        {
            box += vertices;
            return box;
        }

        template<class T>
        inline AABBox<3, T>& operator+=(AABBox<3, T>& box, const ng::geometry::Vertex<T>& vertex)
        {
            box += vertex.world();
            return box;
        }

        template<class T>
        inline AABBox<3, T> operator+(AABBox<3, T> box, const ng::geometry::Vertex<T>& vertex)
        {
            box += vertex;
            return box;
        }
        ///////////////////////////////////////////////// Primitive \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\

        template<class T>
        inline AABBox<3, T>& operator+=(AABBox<3, T>& box, const ng::geometry::Primitive<3, T>& prim)
        {
            box += prim.Vertices();
            return box;
        }

        template<class T>
        inline AABBox<3, T> operator+(AABBox<3, T> box, const ng::geometry::Primitive<3, T>& prim)
        {
            box += prim;
            return box;
        }

        template<class T>
        inline AABBox<3, T>& operator+=(AABBox<3, T>& box, const std::vector<ng::geometry::Primitive<3, T>>& prims)
        {
            for (const auto& prim : prims) box += prim.Vertices();
            return box;
        }

        template<class T>
        inline AABBox<3, T> operator+(AABBox<3, T> box, const std::vector<ng::geometry::Primitive<3, T>>& prims)
        {
            box += prims;
            return box;
        }

        ///////////////////////////////////////////////// Boundings \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\

        template<class T>
        inline AABBox<3, T>& operator+=(AABBox<3, T>& box, const BoundingSphere<T>& sphere)
        {
            ng::math::Matrix<3, 1, T> mm = sphere.Center() - sphere.Radius();
            ng::math::Matrix<3, 1, T> mx = sphere.Center() + sphere.Radius();
            box += mm;
            box += mx;
            return box;
        }

        template<class T>
        inline AABBox<3, T> operator+(AABBox<3, T> box, const BoundingSphere<T>& sphere)
        {
            box += sphere;
            return box;
        }


        ///////////////////////////////////////////////// BoundingSphere += AABBox \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\

        template<class T>
        inline BoundingSphere<T>& operator+=(BoundingSphere<T>& sphere, const AABBox<3, T>& box)
        {
            const auto& mn = box.Min();
            const auto& mx = box.Max();

            const ng::math::Matrix<3, 1, T> corners[8] =
            {
                { mn.x(), mn.y(), mn.z() },
                { mx.x(), mn.y(), mn.z() },
                { mn.x(), mx.y(), mn.z() },
                { mx.x(), mx.y(), mn.z() },
                { mn.x(), mn.y(), mx.z() },
                { mx.x(), mn.y(), mx.z() },
                { mn.x(), mx.y(), mx.z() },
                { mx.x(), mx.y(), mx.z() }
            };

            T maxDistSq = sphere.Radius() * sphere.Radius();
            for (const auto& corner : corners)
            {
                const auto d = corner - sphere.Center();
                const T distSq = d^d;           // ggf. d.LengthSquared(), je nach Matrix-API
                if (distSq > maxDistSq) maxDistSq = distSq;
            }

            sphere.Radius()=std::sqrt(maxDistSq);  // ggf. anderer Setter-Name in eurer BoundingSphere-API
            return sphere;
        }

        template<class T>
        inline BoundingSphere<T> operator+(BoundingSphere<T> sphere, const AABBox<3, T>& box)
        {
            sphere += box;
            return sphere;
        }

        ///////////////////////////////////////////////// BoundingSphere2D += AABBox2D \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\
    

        template<typename T>
        inline bool Intersects(const BoundingCircle2D<T>& circle, const AABBox2D<T>& box)
        {
            const auto& center = circle.Center();

            T x = std::max(
                box.Min().x(),
                std::min(center.x(), box.Max().x())
            );

            T y = std::max(
                box.Min().y(),
                std::min(center.y(), box.Max().y())
            );


            T dx = center.x() - x;
            T dy = center.y() - y;


            T distanceSquared =
                dx * dx +
                dy * dy;


            T radius = circle.Radius();

            return distanceSquared <=
                radius * radius;
        }


        template<typename T>
        inline bool Intersects(const AABBox2D<T>& box, const BoundingCircle2D<T>& circle)
        {
            return Intersects(circle, box);
        }


        template<typename T>
        inline bool Intersects(const BoundingCircle2D<T>& a,const BoundingCircle2D<T>& b)
        {
            return a.Intersects(b);
        }


        // ================================================================
// AABBox2D + BoundingCircle2D
// ================================================================

        template<class T>
        inline AABBox2D<T>& operator+=(
            AABBox2D<T>& box,
            const BoundingCircle2D<T>& circle)
        {
            using Vec2 = ng::math::Matrix<2, 1, T>;

            Vec2 mn =
                circle.Center() -
                Vec2(circle.Radius(), circle.Radius());

            Vec2 mx =
                circle.Center() +
                Vec2(circle.Radius(), circle.Radius());

            box += mn;
            box += mx;

            return box;
        }


        template<class T>
        inline AABBox2D<T> operator+(
            AABBox2D<T> box,
            const BoundingCircle2D<T>& circle)
        {
            box += circle;
            return box;
        }


        // ================================================================
        // BoundingCircle2D + AABBox2D
        // ================================================================

        template<class T>
        inline BoundingCircle2D<T>& operator+=(
            BoundingCircle2D<T>& circle,
            const AABBox2D<T>& box)
        {
            const auto& mn = box.Min();
            const auto& mx = box.Max();

            using Vec2 = ng::math::Matrix<2, 1, T>;

            // Die vier Ecken der AABB
            const Vec2 corners[4] =
            {
                { mn.x(), mn.y() },
                { mx.x(), mn.y() },
                { mn.x(), mx.y() },
                { mx.x(), mx.y() }
            };


            // Bestehenden Radius berücksichtigen
            T maxDistSq =
                circle.Radius() *
                circle.Radius();


            for (const auto& corner : corners)
            {
                const auto d =
                    corner - circle.Center();

                const T distSq =
                    d.x() * d.x() +
                    d.y() * d.y();

                if (distSq > maxDistSq)
                    maxDistSq = distSq;
            }


            circle.Radius() =
                std::sqrt(maxDistSq);

            return circle;
        }


        template<class T>
        inline BoundingCircle2D<T> operator+(
            BoundingCircle2D<T> circle,
            const AABBox2D<T>& box)
        {
            circle += box;
            return circle;
        }


    }

    
}