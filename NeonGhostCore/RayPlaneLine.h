#pragma once
#include <vector>
#include <algorithm>
#include <cmath>

#include "Global.h"
#include "MathUtils.h"
#include "Matrix.h"

namespace ng {
    namespace math {

        template <class T>
        constexpr T PLANE_EPSILON = (T)1e-4;   // Distanz zur Ebene in Welteinheiten

        template <unsigned int N, class T>
        class RayPlaneLine
        {
        public:
            // ----------------------------------------------------------
            // Konstruktion
            // ----------------------------------------------------------

            RayPlaneLine()
                : _length(0), _D((T)0),
                _udCalculated(false), _nCalculated(false),
                _lCalculated(false), _dCalculated(false) {
            }

            RayPlaneLine(const math::Matrix<N, 1, T>& origin,
                const math::Matrix<N, 1, T>& vector)
                : _origin(origin), _vector(vector),
                _length(0), _D((T)0),
                _udCalculated(false), _nCalculated(false),
                _lCalculated(false), _dCalculated(false) {
            }

            // GEAENDERT: rechtshaendige Normale fuer CCW-Dreiecke.
            // Vorher: ((c - b) % (b - a)) -> zeigte in die Gegenrichtung.
            RayPlaneLine(const math::Matrix<N, 1, T>& a,
                const math::Matrix<N, 1, T>& b,
                const math::Matrix<N, 1, T>& c)
                : _origin(a), _vector((b - a) % (c - a)),
                _length(0), _D((T)0),
                _udCalculated(false), _nCalculated(false),
                _lCalculated(false), _dCalculated(false)
            {
                TemplateAssert(N == 3);
            }

            virtual ~RayPlaneLine() {}

            inline RayPlaneLine& flip()
            {
                _vector *= (T)-1;
                flush();
                _udCalculated = false;
                return *this;
            }

            inline RayPlaneLine& operator=(const RayPlaneLine& r)
            {
                _origin = r._origin;
                _vector = r._vector;
                _normal = r._normal;
                _up = r._up;
                _right = r._right;
                _length = r._length;
                _D = r._D;
                _nCalculated = r._nCalculated;
                _lCalculated = r._lCalculated;
                _dCalculated = r._dCalculated;
                _udCalculated = r._udCalculated;
                return *this;
            }

            enum {
                BACK = 0,
                OUTSIDE = 0,
                INSIDE = 1,
                INTERSECT = 1,
                FRONT = 1,
                PLANAR = 2,
                SPANNING = 3,
                CLIP = 3
            };

            // ----------------------------------------------------------
            // Ebenengleichung
            // ----------------------------------------------------------

            inline const math::Matrix<N, 1, T>& normal() const
            {
                if (!_nCalculated)
                {
                    _normal = _vector;
                    _normal.normalize();
                    _nCalculated = true;
                }
                return _normal;
            }

            inline T D() const
            {
                if (!_dCalculated)
                {
                    _D = -(origin() ^ normal());
                    _dCalculated = true;
                }
                return _D;
            }

            // GEAENDERT: direkte Auswertung statt Hilfsstrahl.
            // Vorzeichen jetzt: positiv = vor der Ebene.
            inline T distance(const math::Matrix<N, 1, T>& point) const
            {
                return (point ^ normal()) + D();
            }

            
            inline int classify(const math::Matrix<3, 1, T>& v) const
            {
                const T d = distance(v);

                if (d > math::PLANE_EPSILON<T>) return FRONT;
                if (d < -math::PLANE_EPSILON<T>) return BACK;
                return PLANAR;
            }

            // GEAENDERT: const ergaenzt.
            inline int classify(const math::Matrix<3, 1, T>& v0,
                const math::Matrix<3, 1, T>& v1) const
            {
                const int a = classify(v0);
                const int b = classify(v1);

                if (a == b) return a;

                if (a == PLANAR && (b == FRONT || b == BACK)) return b;
                if (b == PLANAR && (a == FRONT || a == BACK)) return a;

                return SPANNING;
            }

            // GEAENDERT: const ergaenzt.
            inline int classify(const std::vector<math::Matrix<3, 1, T>>& verts) const
            {
                bool front = false;
                bool back = false;

                for (const auto& v : verts)
                {
                    const int res = classify(v);

                    if (res == FRONT) front = true;
                    if (res == BACK)  back = true;

                    if (front && back) return SPANNING;
                }

                if (front) return FRONT;
                if (back)  return BACK;
                return PLANAR;
            }

            // NEU: Gueltigkeitspruefung fuer entartete Dreiecke.
            inline bool valid() const
            {
                return _vector.length() > math::EPSILON<T>;
            }

            // NEU: Punkt auf der Ebene, ohne Annahme ueber _origin.
            inline math::Matrix<N, 1, T> pointOnPlane() const
            {
                return normal() * (-D());
            }

            // ----------------------------------------------------------
            // Geometrie
            // ----------------------------------------------------------

            inline math::Matrix<N, 1, T> closest(const math::Matrix<N, 1, T>& point) const
            {
                return point - normal() * distance(point);
            }

            // GEAENDERT: invertiert jetzt _vector statt _normal.
            // Vorher wurde _normal gedreht und direkt danach von flush()
            // verworfen - die Inversion war wirkungslos.
            inline void invert()
            {
                _vector *= (T)-1;
                flush();
                _udCalculated = false;
            }

            inline bool intersect(const RayPlaneLine& r, T& time) const
            {
                time = 0;

                const T denom = normal() ^ r.normal();
                if (denom == (T)0) return false;

                const T numer = normal() ^ r.origin();
                time = -((numer + D()) / denom);

                return true;
            }

            inline bool intersection(const RayPlaneLine<3, T>& b,
                const RayPlaneLine<3, T>& c,
                math::Matrix<3, 1, T>& out) const
            {
                const math::Matrix<3, 1, T> c1 = b.normal() % c.normal();
                const math::Matrix<3, 1, T> c2 = c.normal() % normal();
                const math::Matrix<3, 1, T> c3 = normal() % b.normal();

                const T denom = normal() ^ c1;
                if (denom == (T)0) return false;

                out = (-(D() * c1) + -(b.D() * c2) + -(c.D() * c3)) / denom;
                return true;
            }

            // ENTFERNT: splitV() gab eine Referenz auf ein Temporary
            // zurueck. Nur noch die by-value-Variante.
            inline math::Matrix<3, 1, T> split(const math::Matrix<3, 1, T>& a,
                const math::Matrix<3, 1, T>& b) const
            {
                const T aDot = (a ^ normal());
                const T bDot = (b ^ normal());
                const T scale = (-D() - aDot) / (bDot - aDot);

                return a + (scale * (b - a));
            }

            // ----------------------------------------------------------
            // Laenge / Achsen
            // ----------------------------------------------------------

            inline T length() const
            {
                if (!_lCalculated)
                {
                    _length = vector().length();
                    _lCalculated = true;
                }
                return _length;
            }

            inline void setLength(const T len)
            {
                if (_nCalculated) _vector = _normal * len;
                else              _vector.setLength(len);

                _length = len;
                _lCalculated = true;
                _dCalculated = false;
            }

            inline math::Matrix<N, 1, T> end() const { return _origin + _vector; }
            inline math::Matrix<N, 1, T> end(const T len) const { return _origin + normal() * len; }

            // GEAENDERT: const ergaenzt.
            inline T AxisScore() const
            {
                const T ax = std::abs(normal().x());
                const T ay = std::abs(normal().y());
                const T az = std::abs(normal().z());

                return std::max(std::max(ax, ay), az);
            }

            // GEAENDERT: const ergaenzt.
            inline unsigned char GetAxisAlignement() const
            {
                math::Matrix<N, 1, T> a = normal();
                a.abs();

                if (a.x() >= a.y() && a.x() >= a.z()) return 0;
                if (a.y() >= a.x() && a.y() >= a.z()) return 1;
                return 2;
            }

            inline const math::Matrix<N, 1, T>& GetUpVector() const
            {
                CalcUpRightVector();
                return _up;
            }

            inline const math::Matrix<N, 1, T>& GetRightVector() const
            {
                CalcUpRightVector();
                return _right;
            }

            inline void flush() const
            {
                _nCalculated = false;
                _lCalculated = false;
                _dCalculated = false;
            }

            // ----------------------------------------------------------
            // Zugriff
            // ----------------------------------------------------------

            inline const math::Matrix<N, 1, T>& origin() const { return _origin; }
            inline       math::Matrix<N, 1, T>& origin()
            {
                _dCalculated = false; _udCalculated = false; return _origin;
            }

            inline const math::Matrix<N, 1, T>& vector() const { return _vector; }
            inline       math::Matrix<N, 1, T>& vector()
            {
                flush(); _udCalculated = false; return _vector;
            }

            // ----------------------------------------------------------
            // Strahl / Dreieck
            // ----------------------------------------------------------

            inline int intersect(const math::Matrix<3, 1, T>& v0,
                const math::Matrix<3, 1, T>& v1,
                const math::Matrix<3, 1, T>& v2,
                math::Matrix<3, 1, T>& i) const
            {
                T time;
                return intersectTri(v0, v1, v2, i, time);
            }

            inline int intersect(const math::Matrix<3, 1, T>& v0,
                const math::Matrix<3, 1, T>& v1,
                const math::Matrix<3, 1, T>& v2,
                double& time) const
            {
                math::Matrix<3, 1, T> i;
                T t;
                const int res = intersectTri(v0, v1, v2, i, t);
                time = static_cast<double>(t);
                return res;
            }

        private:
            // GEAENDERT: die beiden frueher fast identischen intersect()-
            // Overloads teilen sich jetzt eine Implementierung.
            inline int intersectTri(const math::Matrix<3, 1, T>& v0,
                const math::Matrix<3, 1, T>& v1,
                const math::Matrix<3, 1, T>& v2,
                math::Matrix<3, 1, T>& i,
                T& time) const
            {
                const math::Matrix<3, 1, T> u = v1 - v0;
                const math::Matrix<3, 1, T> v = v2 - v0;
                const math::Matrix<3, 1, T> n = u % v;

                if (n == math::Matrix<3, 1, T>((T)0, (T)0, (T)0)) return OUTSIDE;

                const math::Matrix<3, 1, T> r0 = origin();
                const math::Matrix<3, 1, T> dir = vector();
                const math::Matrix<3, 1, T> w0 = r0 - v0;

                const T a = -(n ^ w0);
                const T b = (n ^ dir);

                if (std::abs(b) < math::EPSILON<T>) return OUTSIDE;

                const T r = a / b;
                if (r < (T)0) return OUTSIDE;

                time = r;
                i = r0 + (dir * r);

                const T uu = (u ^ u);
                const T uv = (u ^ v);
                const T vv = (v ^ v);

                const math::Matrix<3, 1, T> w = (i - v0);

                const T wu = (w ^ u);
                const T wv = (w ^ v);
                const T d = uv * uv - uu * vv;

                if (std::abs(d) < math::EPSILON<T>) return OUTSIDE;

                const T s = (uv * wv - vv * wu) / d;
                if (s < (T)0 || s >(T)1) return OUTSIDE;

                const T t = (uv * wu - uu * wv) / d;
                if (t < (T)0 || (s + t) >(T)1) return OUTSIDE;

                return INSIDE;
            }

            static math::Matrix<3, 1, T> axis[3];

            math::Matrix<N, 1, T> _origin;
            math::Matrix<N, 1, T> _vector;

            mutable math::Matrix<N, 1, T> _up;
            mutable math::Matrix<N, 1, T> _right;
            mutable math::Matrix<N, 1, T> _normal;
            mutable T                     _length;
            mutable T                     _D;

            mutable bool _udCalculated;
            mutable bool _nCalculated;
            mutable bool _lCalculated;
            mutable bool _dCalculated;

            inline void CalcUpRightVector() const
            {
                if (_udCalculated) return;

                const math::Matrix<N, 1, T>& n = normal();

                math::Matrix<N, 1, T> a = n;
                a.abs();

                int major = 2;
                if (a.x() >= a.y() && a.x() >= a.z())      major = 0;
                else if (a.y() >= a.x() && a.y() >= a.z()) major = 1;

                // GEAENDERT: robuste Achsenwahl statt Sonderfaellen mit
                // exakten Gleitkommavergleichen (== 1.0).
                const int pick = (major == 0) ? 1 : 0;

                _right = axis[pick] % n;

                T len = _right.length();
                if (len < math::EPSILON<T>)
                {
                    _right = axis[2] % n;
                    len = _right.length();
                }

                if (len > math::EPSILON<T>)
                    _right *= (T)1 / len;

                _up = n % _right;
                _up.normalize();

                _udCalculated = true;
            }
        };

        template <unsigned int N, class T>
        math::Matrix<3, 1, T> RayPlaneLine<N, T>::axis[3] =
        {
            math::Matrix<3, 1, T>((T)1, (T)0, (T)0),
            math::Matrix<3, 1, T>((T)0, (T)1, (T)0),
            math::Matrix<3, 1, T>((T)0, (T)0, (T)1)
        };

    }
}