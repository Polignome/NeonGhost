#pragma once
#include "Global.h"
#include "Matrix.h"

namespace ng
{
    namespace bounding {
      
        template<typename T>
        class BoundingCircle2D
        {
        public:

            using Vec2 = ng::math::Matrix<2, 1, T>;

        private:

            Vec2 _center;
            T _radius;


        public:

            // ============================================================
            // Konstruktoren
            // ============================================================
            ~BoundingCircle2D()
            {
            }

            BoundingCircle2D() : _center(0, 0),  _radius(0)
            {
            }


             BoundingCircle2D(const Vec2& center, T radius)
                 : _center(center),
                 _radius(radius)
             {
             }


             BoundingCircle2D(const T& cx, const T& cy, T radius)
                 : _center(cx, cy),
                 _radius(radius)
             {
             }


            // ============================================================
            // Zugriff
            // ============================================================

            inline const Vec2& Center() const
            {
                return _center;
            }


            inline Vec2& Center()
            {
                return _center;
            }


            inline const T& Radius() const
            {
                return _radius;
            }


            inline T& Radius()
            {
                return _radius;
            }


            // ============================================================
            // Reset
            // ============================================================

            void Reset()
            {
                _center = Vec2(0, 0);
                _radius = 0;
            }


            // ============================================================
            // Contains Punkt
            // ============================================================

            bool Contains(const Vec2& point) const
            {
                Vec2 d = point - _center;

                return d.length() <= _radius;
            }


            // ============================================================
            // Abstand Punkt -> Kreis
            // ============================================================

            T Distance(const Vec2& point) const
            {
                Vec2 d = point - _center;

                return d.length() - _radius;
            }


            // ============================================================
            // Kreis gegen Punkt
            // ============================================================

            bool Intersects(const Vec2& point) const
            {
                return Contains(point);
            }


            // ============================================================
            // Kreis gegen Kreis
            // ============================================================

            bool Intersects(const BoundingCircle2D<T>& circle) const
            {
                Vec2 d = circle.Center() - Center();

                T radiusSum =
                    Radius() + circle.Radius();

                return
                    d.length() * d.length() <=
                    radiusSum * radiusSum;
            }



            // ============================================================
            // Kreis vollständig in Kreis?
            // ============================================================

            bool Contains(
                const BoundingCircle2D<T>& circle) const
            {
                Vec2 d = circle.Center() - Center();

                T distance = d.length();

                return
                    distance + circle.Radius() <=
                    Radius();
            }


            // ============================================================
            // Punkt hinzufügen
            // ============================================================

            void Expand(const Vec2& point)
            {
                Vec2 d = point - Center();

                T distanceSquared =
                    d.x() * d.x() +
                    d.y() * d.y();

                if (distanceSquared <= Radius() * Radius())
                    return;


                T distance =
                    std::sqrt(distanceSquared);

                if (distance <= static_cast<T>(0))
                    return;


                T newRadius =
                    (Radius() + distance) *
                    static_cast<T>(0.5);


                T move =
                    (newRadius - Radius()) /
                    distance;


                Center() += d * move;

                Radius() = newRadius;
            }


            // ============================================================
            // Kreis hinzufügen
            // ============================================================

            void Expand(const BoundingCircle2D<T>& circle)
            {
                Vec2 d =
                    circle.Center() - Center();

                T distance = d.length();


                // Kreis liegt vollständig innerhalb
                if (distance + circle.Radius() <= Radius())
                    return;


                // Dieser Kreis liegt vollständig innerhalb
                if (distance + Radius() <= circle.Radius())
                {
                    Center() = circle.Center();
                    Radius() = circle.Radius();

                    return;
                }


                if (distance <= static_cast<T>(0))
                    return;


                T newRadius =
                    (Radius() +
                        distance +
                        circle.Radius()) *
                    static_cast<T>(0.5);


                T move =
                    (newRadius - Radius()) /
                    distance;


                Center() += d * move;

                Radius() = newRadius;
            }


            // ============================================================
            // AABB des Kreises
            // ============================================================

            Vec2 Min() const
            {
                return Center() -
                    Vec2(Radius(), Radius());
            }


            Vec2 Max() const
            {
                return Center() +
                    Vec2(Radius(), Radius());
            }
        };

    }
}