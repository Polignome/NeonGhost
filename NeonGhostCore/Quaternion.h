#pragma once
#pragma once

#include <cmath>
#include <type_traits>
#include "Matrix.h"

namespace ng
{
    namespace math {
        template<typename T>
        class Quaternion
        {
        public:


            // Identity Quaternion
            constexpr Quaternion()
                : _x((T)0), _y((T)0), _z((T)0), _w((T)1)
            {
            }

            constexpr Quaternion(T x, T y, T z, T w)
                : _x(x), _y(y), _z(z), _w(w)
            {
            }

            constexpr Quaternion(const Matrix<4,1,T> &v)
                : _x(v.x()), _y(v.y()), _z(v.z()), _w(v.w())
            {
            }

            inline Matrix<3, 1, T> Rotate(
                const Matrix<3, 1, T>& v) const
            {
                Matrix<3, 1, T> qv(x(), y(), z());

                Matrix<3, 1, T> t =
                    (qv % v) * (T)2;

                return v
                    + t * w()
                    + (qv %t);
            }


            // ------------------------------------------------------------
            // Identity
            // ------------------------------------------------------------

            static constexpr Quaternion Identity()
            {
                return Quaternion(0, 0, 0, 1);
            }

            // ------------------------------------------------------------
            // Axis / Angle
            // angle in radians
            // ------------------------------------------------------------

            static Quaternion FromAxisAngle(
                const Matrix<3, 1, T>& axis,
                T angle)
            {
                T halfAngle = angle * static_cast<T>(0.5);
                T s = std::sin(halfAngle);

                return Quaternion(
                    axis.x() * s,
                    axis.y() * s,
                    axis.z() * s,
                    (T)std::cos(halfAngle)
                );
            }

            // ------------------------------------------------------------
            // Length
            // ------------------------------------------------------------

            T LengthSquared() const
            {
                return x() * x() +
                       y() * y() +
                       z() * z() +
                       w() * w();
            }

            T Length() const
            {
                return std::sqrt(LengthSquared());
            }

            // ------------------------------------------------------------
            // Normalize
            // ------------------------------------------------------------

            void Normalize()
            {
                T length = Length();

                if (length > static_cast<T>(0))
                {
                    T invLength = static_cast<T>(1) / length;

                    x() *= invLength;
                    y() *= invLength;
                    z() *= invLength;
                    w() *= invLength;
                }
            }

            Quaternion Normalized() const
            {
                Quaternion result(*this);
                result.Normalize();
                return result;
            }

            // ------------------------------------------------------------
            // Conjugate
            // ------------------------------------------------------------

            Quaternion Conjugate() const
            {
                return Quaternion(
                    -x(),
                    -y(),
                    -z(),
                     w()
                );
            }

            // ------------------------------------------------------------
            // Inverse
            // ------------------------------------------------------------

            Quaternion Inverse() const
            {
                T lengthSquared = LengthSquared();

                if (lengthSquared <= static_cast<T>(0))
                    return Identity();

                Quaternion result = Conjugate();

                T inv = static_cast<T>(1) / lengthSquared;

                result.x() *= inv;
                result.y() *= inv;
                result.z() *= inv;
                result.w() *= inv;

                return result;
            }

            // ------------------------------------------------------------
            // Quaternion multiplication
            // ------------------------------------------------------------

            Quaternion operator*(const Quaternion& q) const
            {
                return Quaternion(

                    w() * q.x() +
                    x() * q.w() +
                    y() * q.z() -
                    z() * q.y(),

                    w() * q.y() -
                    x() * q.z() +
                    y() * q.w() +
                    z() * q.x(),

                    w() * q.z() +
                    x() * q.y() -
                    y() * q.x() +
                    z() * q.w(),

                    w() * q.w() -
                    x() * q.x() -
                    y() * q.y() -
                    z() * q.z()
                );
            }

            Quaternion& operator*=(const Quaternion& q)
            {
                *this = *this * q;
                return *this;
            }

            Matrix<4, 4, T> ToMatrix() const
            {
                Quaternion q = Normalized();

                const T xx = q.x() * q.x();
                const T yy = q.y() * q.y();
                const T zz = q.z() * q.z();

                const T xy = q.x() * q.y();
                const T xz = q.x() * q.z();
                const T yz = q.y() * q.z();

                const T wx = q.w() * q.x();
                const T wy = q.w() * q.y();
                const T wz = q.w() * q.z();

                Matrix<4, 4, T> m;

                m(0, 0) = (T)1 - (T)2 * (yy + zz);
                m(0, 1) = (T)2 * (xy + wz);
                m(0, 2) = (T)2 * (xz - wy);
                m(0, 3) = (T)0;

                m(1, 0) = (T)2 * (xy - wz);
                m(1, 1) = (T)1 - (T)2 * (xx + zz);
                m(1, 2) = (T)2 * (yz + wx);
                m(1, 3) = (T)0;

                m(2, 0) = (T)2 * (xz + wy);
                m(2, 1) = (T)2 * (yz - wx);
                m(2, 2) = (T)1 - (T)2 * (xx + yy);
                m(2, 3) = (T)0;

                // Homogene Teil explizit festlegen
                m(3, 0) = (T)0;
                m(3, 1) = (T)0;
                m(3, 2) = (T)0;
                m(3, 3) = (T)1;

                return m;
            }
           
            inline	const	T& x() const { return _x; }
            inline		    T& x()       { return _x; }
            inline	const	T& y() const { return _y; }
            inline		    T& y()       { return _y; }
            inline	const	T& z() const { return _z; }
            inline		    T& z()       { return _z; }
            inline	const	T& w() const { return _w; }
            inline		    T& w()       { return _w; }

            inline	operator Matrix<4, 1, T>() { return math::Matrix<4, 1, T>(_x, _y, _z, _w); }

            inline		Quaternion& operator =(const Matrix<4,1,T>& m)
            {
                x() = m.x();
                y() = m.y();
                z() = m.z();
                w() = m.w();
                return *this;
            }

          
             private:
                 T _x = (T)0;
                 T _y = (T)0;
                 T _z = (T)0;
                 T _w = (T)1;


        };
    }
}