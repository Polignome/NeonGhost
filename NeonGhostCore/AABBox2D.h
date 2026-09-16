#pragma once

#include <algorithm>
#include <cmath>
#include <vector>
#include <utility>
#include "Global.h"
#include "Matrix.h"

namespace ng
{
    namespace bounding {
        template<typename T>
        class AABBox2D
        {
        public:

            using Vec2 = ng::math::Matrix<2, 1, T>;

            enum
            {
                OUTSIDE = 0,
                INSIDE = 1,
                CLIP = 2
            };

        private:

            Vec2 _min;
            Vec2 _max;

            mutable Vec2 _center;
            mutable Vec2 _half;
            mutable Vec2 _size;

            mutable bool _cCalculated = false;
            mutable bool _firststart = true;


            void Recalc() const
            {
                if (!_cCalculated)
                {
                    _half = (_max - _min) / static_cast<T>(2);
                    _center = _min + _half;
                    _size = _max - _min;
                    _cCalculated = true;
                }
            }

            inline void SetCenter(const Vec2& newCenter)
            {
                Recalc();

                _min = newCenter - _half;
                _max = newCenter + _half;

                _center = newCenter;
                _size = _max - _min;
                _cCalculated = true;
                _firststart = false;
            }

            inline void Rotate(T angle)
            {
                Recalc();

                const T c = std::cos(angle);
                const T s = std::sin(angle);

                const Vec2 center = _center;

                // Halbe Ausdehnung der ursprünglichen Box
                const T hx = _half.x();
                const T hy = _half.y();

                // Neue AABB-Halbausdehnung nach der Rotation
                const T newHx =
                    std::abs(c) * hx +
                    std::abs(s) * hy;

                const T newHy =
                    std::abs(s) * hx +
                    std::abs(c) * hy;

                _half = Vec2(newHx, newHy);

                _min = center - _half;
                _max = center + _half;

                _center = center;
                _size = _max - _min;
                _cCalculated = true;
                _firststart = false;
            }
        public:

            // ============================================================
            // Konstruktoren
            // ============================================================

            AABBox2D()
                : _min(0, 0),
                _max(0, 0),
                _center(0, 0),
                _half(0, 0),
                _size(0, 0),
                _cCalculated(false),
                _firststart(true)
            {
            }


            AABBox2D(const Vec2& min, const Vec2& max)
                : _min(min),
                _max(max),
                _center(0, 0),
                _half(0, 0),
                _size(0, 0),
                _cCalculated(false),
                _firststart(false)
            {
            }


            AABBox2D(
                const T& minx,
                const T& miny,
                const T& maxx,
                const T& maxy)
                : _min(minx, miny),
                _max(maxx, maxy),
                _center(0, 0),
                _half(0, 0),
                _size(0, 0),
                _cCalculated(false),
                _firststart(false)
            {
            }


            AABBox2D(const std::vector<Vec2>& vertices)
                : _min(0, 0),
                _max(0, 0),
                _center(0, 0),
                _half(0, 0),
                _size(0, 0),
                _cCalculated(false),
                _firststart(true)
            {
                for (const auto& v : vertices)
                    (*this) += v;
            }


            // ============================================================
            // Reset
            // ============================================================

            void Reset()
            {
                _min = Vec2(0, 0);
                _max = Vec2(0, 0);
                _center = Vec2(0, 0);
                _half = Vec2(0, 0);
                _size = Vec2(0, 0);
                _cCalculated = false;
                _firststart = true;
            }


            // ============================================================
            // Min / Max
            // ============================================================

            inline const Vec2& Size() const
            {
                return _size;
            }


            inline const Vec2& Min() const
            {
                return _min;
            }


            inline Vec2& Min()
            {
                _cCalculated = false;
                return _min;
            }


            inline const Vec2& Max() const
            {
                return _max;
            }


            inline Vec2& Max()
            {
                _cCalculated = false;
                return _max;
            }


            // ============================================================
            // Center
            // ============================================================

            inline const Vec2& Center() const
            {
                Recalc();
                return _center;
            }


            // ============================================================
            // HalfSize
            // ============================================================

            inline const Vec2& HalfSize() const
            {
                Recalc();
                return _half;
            }


            // ============================================================
            // Width / Height
            // ============================================================

            inline T Width() const
            {
                return _size.x();
            }


            inline T Height() const
            {
                return _size.y();
            }


            // ============================================================
            // Add Point
            // ============================================================

            inline void Add(const Vec2& p)
            {
                (*this) += p;
            }


            inline AABBox2D& operator+=(const Vec2& p)
            {
                if (_firststart)
                {
                    _min = p;
                    _max = p;

                    _firststart = false;
                    _cCalculated = false;

                    return *this;
                }


                if (p.x() < _min.x())
                    _min.x() = p.x();

                if (p.y() < _min.y())
                    _min.y() = p.y();

                if (p.x() > _max.x())
                    _max.x() = p.x();

                if (p.y() > _max.y())
                    _max.y() = p.y();

                _cCalculated = false;

                return *this;
            }


            // ============================================================
            // Add Box
            // ============================================================

            inline AABBox2D& operator+=(const AABBox2D& box)
            {
                (*this) += box.Min();
                (*this) += box.Max();

                return *this;
            }


            // ============================================================
            // Expand
            // ============================================================

            inline AABBox2D& operator+=(const T& value)
            {
                _min.x() -= value;
                _min.y() -= value;

                _max.x() += value;
                _max.y() += value;

                _cCalculated = false;

                return *this;
            }


            inline AABBox2D operator+(const T& value) const
            {
                AABBox2D result(*this);
                result += value;

                return result;
            }


            // ============================================================
            // Inside
            // ============================================================

            bool Inside(const Vec2& p) const
            {
                return
                    p.x() >= _min.x() &&
                    p.y() >= _min.y() &&
                    p.x() <= _max.x() &&
                    p.y() <= _max.y();
            }

            bool Inside(const T& x, const T& y) const
            {
                return
                    x >= _min.x() &&
                    y >= _min.y() &&
                    x <= _max.x() &&
                    y <= _max.y();
            }


            // ============================================================
            // AABB gegen AABB
            // ============================================================

            bool IntersectedByBounds(
                const AABBox2D& box,
                T tolerance = static_cast<T>(0)) const
            {
                return
                    (_min.x() - tolerance <= box._max.x() + tolerance) &&
                    (_max.x() + tolerance >= box._min.x() - tolerance) &&
                    (_min.y() - tolerance <= box._max.y() + tolerance) &&
                    (_max.y() + tolerance >= box._min.y() - tolerance);
            }


            // ============================================================
            // Kollisionsnormale AABB -> AABB
            //
            // Liefert die Richtung, in die der erste Körper zurückgedrückt
            // werden müsste.
            //
            // Beispiel:
            // (-1,0) = Kollision von rechts
            // ( 1,0) = Kollision von links
            // (0,-1) = Kollision von unten
            // (0, 1) = Kollision von oben
            // ============================================================

            Vec2 CollisionNormal(const AABBox2D& box) const
            {
                T left = box.Max().x() - Min().x();
                T right = Max().x() - box.Min().x();

                T top = box.Max().y() - Min().y();
                T bottom = Max().y() - box.Min().y();


                T overlapX = std::min(left, right);
                T overlapY = std::min(top, bottom);


                if (overlapX < overlapY)
                {
                    if (Center().x() < box.Center().x())
                        return Vec2(-1, 0);
                    else
                        return Vec2(1, 0);
                }
                else
                {
                    if (Center().y() < box.Center().y())
                        return Vec2(0, -1);
                    else
                        return Vec2(0, 1);
                }
            }


            // ============================================================
            // Länge der Box-Diagonale
            // ============================================================

            T Length() const
            {
                Vec2 d = _max - _min;

                return d.length();
            }
        };


        // ================================================================
        // Freie Operatoren
        // ================================================================

        template<typename T>
        inline bool Intersects(
            const AABBox2D<T>& a,
            const AABBox2D<T>& b)
        {
            return a.IntersectedByBounds(b);
        }

    }
}