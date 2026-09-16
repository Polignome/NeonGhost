#pragma once
#include "Global.h"
#include "Matrix.h"

namespace ng {
    namespace bounding {


        template<typename T>
        class BoundingSphere
        {
        private:
             using Vec3 = ng::math::Matrix<3, 1, T>;
             Vec3 _center;
             T _radius;
        public:


            inline const Vec3& Center() const { return _center; }
            inline       Vec3& Center()       { return _center; }

            inline const T& Radius() const { return _radius; }
            inline       T& Radius()       { return _radius; }
            // ------------------------------------------------------------
            // Konstruktoren
            // ------------------------------------------------------------

            constexpr BoundingSphere(): _center(0, 0, 0), _radius(0)
            {
            }

            constexpr BoundingSphere(const Vec3& center, T radius) : _center(center), _radius(radius)
            {
            }

            constexpr BoundingSphere(const T& cx, const T& cy, const T& cz, T radius) : _center(Vec3(cx,cy,cz)), _radius(radius)
            {
            }

            constexpr BoundingSphere(const std::vector<Vec3>& vec) : _center(0), _radius(0)
            {
                Expand(vec);
            }

            // ------------------------------------------------------------
            // Reset
            // ------------------------------------------------------------

            void Reset()
            {
                Center() = Vec3(0, 0, 0);
                Radius() = 0;
            }

            // ------------------------------------------------------------
            // Punkt enthalten?
            // ------------------------------------------------------------

            bool Contains(const Vec3& point) const
            {
                Vec3 d = point - Center();
                return d.LengthSquared() <= Radius() * Radius();
            }

            // ------------------------------------------------------------
            // Abstand zum Punkt
            // ------------------------------------------------------------

            T Distance(const Vec3& point) const
            {
                Vec3 d = point - Center();
                return d.Length() - Radius();
            }

            // ------------------------------------------------------------
            // Kugel überschneidet Punkt
            // ------------------------------------------------------------

            bool Intersects(const Vec3& point) const
            {
                return Contains(point);
            }

            // ------------------------------------------------------------
            // Kugel gegen Kugel
            // ------------------------------------------------------------

            bool Intersects(const BoundingSphere& sphere) const
            {
                Vec3 d = sphere.Center() - Center();

                T radiusSum = Radius() + sphere.Radius();

                return d.LengthSquared() <=
                    radiusSum * radiusSum;
            }

            // ------------------------------------------------------------
            // Kugel vollständig enthalten?
            // ------------------------------------------------------------

            bool Contains(const BoundingSphere& sphere) const
            {
                Vec3 d = sphere.Center() - Center();

                T distance = d.Length();

                return distance + sphere.Radius() <= Radius();
            }

            // ------------------------------------------------------------
            // Punkt hinzufügen
            //
            // Erweitert die BoundingSphere so,
            // dass der Punkt enthalten ist.
            // ------------------------------------------------------------

            void Expand(const Vec3& point)
            {
                Vec3 d = point - Center();

                T distanceSquared = d.LengthSquared();

                if (distanceSquared <= Radius() * Radius())
                    return;

                T distance = std::sqrt(distanceSquared);

                if (distance <= static_cast<T>(0))
                    return;

                // Neue Kugel, die alte Kugel und Punkt enthält
                T newRadius =
                    (Radius() + distance) *
                    static_cast<T>(0.5);

                T move =
                    (newRadius - Radius()) / distance;

                Center() += d * move;
                Radius() = newRadius;
            }

            // ------------------------------------------------------------
            // Andere Kugel hinzufügen
            // ------------------------------------------------------------

            void Expand(const BoundingSphere& sphere)
            {
                Vec3 d = sphere.Center() - Center;

                T distance = d.Length();

                // sphere liegt vollständig innerhalb
                if (distance + sphere.Radius() <= Radius())
                    return;

                // Diese Kugel liegt vollständig innerhalb
                if (distance + Radius() <= sphere.Radius())
                {
                    Center() = sphere.Center();
                    Radius() = sphere.Radius();
                    return;
                }

                if (distance <= static_cast<T>(0))
                    return;

                T newRadius =
                    (Radius() + distance + sphere.Radius()) *
                    static_cast<T>(0.5);

                T move =
                    (newRadius - Radius()) / distance;

                Center() += d * move;
                Radius() = newRadius;
            }

            // ------------------------------------------------------------
            // Bounding Sphere aus Punkten
            // ------------------------------------------------------------

            static BoundingSphere FromPoints(
                const std::vector<Vec3>& points)
            {
                BoundingSphere result;

                if (points.empty())
                    return result;

                // --------------------------------------------------------
                // Einfache Variante:
                // AABB bestimmen und daraus Sphere erzeugen.
                // --------------------------------------------------------

                Vec3 min = points[0];
                Vec3 max = points[0];

                for (const Vec3& p : points)
                {
                    if (p.x() < min.x()) min.x() = p.x();
                    if (p.y() < min.y()) min.y() = p.y();
                    if (p.z() < min.z()) min.z() = p.z();

                    if (p.x() > max.x()) max.x() = p.x();
                    if (p.y() > max.y()) max.y() = p.y();
                    if (p.z() > max.z()) max.z() = p.z();
                }

                result.Center() =
                    (min + max) * static_cast<T>(0.5);

                result.Radius() =
                    (max - result.center()).Length();

                return result;
            }

            // ------------------------------------------------------------
            // AABB der Sphere
            // ------------------------------------------------------------

            Vec3 Min() const
            {
                return Center() -
                    Vec3(Radius(), Radius(), Radius());
            }

            Vec3 Max() const
            {
                return Center() +
                    Vec3(Radius(), Radius(), Radius());
            }
        };
    }
}