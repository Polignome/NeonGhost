#pragma once
#include <vector>
#include "Global.h"
#include "MathUtils.h"
#include "Matrix.h"
#include "RayPlaneLine.h"
#include <array>
namespace ng {
	namespace bounding {
		template <unsigned int N, class T >

		class AABBox
		{
		public:
			AABBox() :_cCalculated(false), _firststart(true) {};

			AABBox(const std::vector<math::Matrix<3,1,T>> vl) :
				_cCalculated(false), _firststart(true)
			{
				for (auto v : vl) (*this) += v;

			};

			AABBox(const AABBox<N, T>& bbox) : _firststart(false)
			{
				this->Min() = bbox.Min();
				this->Max() = bbox.Max();
				this->_center = bbox.Center();
				this->_cCalculated = true;
			}

			AABBox(const math::Matrix<N, 1, T>& min, const math::Matrix<N, 1, T>& max) :_min(min), _max(max), _cCalculated(false), _firststart(false) {}


			inline std::array<ng::math::RayPlaneLine<3, T>, 6> ComputeAABBPlanes()
			{
				using Plane = ng::math::RayPlaneLine<3, T>;
				using Vec3 = ng::math::Matrix<3, 1, T>;

				// RayPlaneLine(origin, vector) -> vector wird beim ersten
				// Zugriff auf normal() normalisiert, origin muss nur
				// irgendein Punkt auf der jeweiligen Ebene sein - dafuer
				// reicht die entsprechende Box-Ecke.

				return
				{
					Plane(Min(), Vec3(T(1),  T(0),  T(0))), // +X-Seite bei x = min, Normale zeigt nach +X (Richtung Boxinneres)
					Plane(Max(), Vec3(T(-1),  T(0),  T(0))), // -X-Seite bei x = max, Normale zeigt nach -X
					Plane(Min(), Vec3(T(0),  T(1),  T(0))), // +Y-Seite bei y = min
					Plane(Max(), Vec3(T(0), T(-1),  T(0))), // -Y-Seite bei y = max
					Plane(Min(), Vec3(T(0),  T(0),  T(1))), // +Z-Seite bei z = min
					Plane(Max(), Vec3(T(0),  T(0), T(-1)))  // -Z-Seite bei z = max
				};
			}

			inline std::vector<std::vector<math::Matrix<3, 1, T>>> Faces() const
			{
				using Vec3 = math::Matrix<3, 1, T>;

				const T xmin = Min().x();
				const T ymin = Min().y();
				const T zmin = Min().z();

				const T xmax = Max().x();
				const T ymax = Max().y();
				const T zmax = Max().z();

				// 8 Eckpunkte der AABB
				Vec3 v000(xmin, ymin, zmin);
				Vec3 v001(xmin, ymin, zmax);
				Vec3 v010(xmin, ymax, zmin);
				Vec3 v011(xmin, ymax, zmax);

				Vec3 v100(xmax, ymin, zmin);
				Vec3 v101(xmax, ymin, zmax);
				Vec3 v110(xmax, ymax, zmin);
				Vec3 v111(xmax, ymax, zmax);

				std::vector<std::vector<Vec3>> faces;
				faces.reserve(6);

				// -X
				faces.push_back({
					v000,
					v001,
					v011,
					v010
					});

				// +X
				faces.push_back({
					v100,
					v110,
					v111,
					v101
					});

				// -Y
				faces.push_back({
					v000,
					v100,
					v101,
					v001
					});

				// +Y
				faces.push_back({
					v010,
					v011,
					v111,
					v110
					});

				// -Z
				faces.push_back({
					v000,
					v010,
					v110,
					v100
					});

				// +Z
				faces.push_back({
					v001,
					v101,
					v111,
					v011
					});


				for (auto f : faces) {
					std::reverse(f.begin(), f.end());
					

				}
				return faces;
			}

			AABBox(const  T& minx, const  T& miny, const  T& minz, const  T& maxx, const  T& maxy, const  T& maxz ) :_min(math::Matrix<N, 1, T>(minx, miny, minz)), _max(math::Matrix<N, 1, T>(maxx, maxy, maxz)), _cCalculated(false), _firststart(false) {}



			inline std::pair< AABBox<N, T>, AABBox<N, T>> SplitAABB(math::RayPlaneLine<N, T> plane) {
				AABBox<N, T> front =  AABBox(*this);
				AABBox<N, T> back = AABBox(*this);

				// dominante Achse bestimmen
				math::Matrix<3,1,T> n = plane.normal();

				if (std::abs(n.x()) > std::abs(n.y()) &&
					std::abs(n.x()) > std::abs(n.z())) {

					front.Min().x() = plane.origin().x();
					back.Max().x() = plane.origin().x();
				}
				else
					if (std::abs(n.y()) > std::abs(n.z())) {
						front.Min().y() = plane.origin().y();
						back.Max().y() = plane.origin().y();
					}
					else {
						front.Min().z() = plane.origin().z();
						back.Max().z() = plane.origin().z();
					}

				return std::pair<AABBox<N, T>, AABBox<N, T>>(front, back);
			}

			inline T SurfaceArea() {
				T dx = Max().x() - Min().x();
				T dy = Max().y() - Min().y();
				T dz = Max().z() - Min().z();

				return (T)2 * (
					dx * dy +
					dx * dz +
					dy * dz
					);
			}

			inline void Reset() {
				_cCalculated = _firststart = true;
				_min = _max = _center = _half = math::Matrix<3, 1, T>(0, 0, 0);

			}

			inline void Add(const math::Matrix<N, 1, T>& p) {
				(*this) += p;
			}

			inline void Add(const std::vector<math::Matrix<N, 1, T>>& pp) {
				for (auto p:pp)
				(*this) += p;
			}

			inline AABBox& operator + (const T& v) {
				AABBox b(*this);
				b.Max() += v;
				b.Min() -= v;
				return b;
			}

			inline AABBox& operator - (const T& v) {
				AABBox b(*this);
				b.Max() -= v;
				b.Min() += v;
				return b;
			}

			inline AABBox& operator -= (const T& v)
			{
				Max() -= v;
				Min() += v;
				return (*this);
			}

			inline AABBox& operator + (const math::Matrix<N, 1, T>& v)
			{
				AABBox b(*this);
				b += v;
				return b;
			}

			inline AABBox& operator + (const std::vector<math::Matrix<N, 1, T>>& vv)
			{
				AABBox b(*this);
				for (auto v:vv)
				b += v;
				return b;
			}



			inline AABBox& operator +=(const math::Matrix<N, 1, T>& v)		// Add a primitive::Vertex<T> to the end
			{
				if (_firststart)
				{
					Min() = Max() = v;
					_firststart = false;
					return *this;
				}

				if (Min().x() > v.x()) Min().x() = v.x();
				if (Min().y() > v.y()) Min().y() = v.y();
				if (Min().z() > v.z()) Min().z() = v.z();

				if (Max().x() < v.x()) Max().x() = v.x();
				if (Max().y() < v.y()) Max().y() = v.y();
				if (Max().z() < v.z()) Max().z() = v.z();
				return *this;
			}

			inline AABBox& operator +=(const std::vector<math::Matrix<N, 1, T>>& vv)		// Add a primitive::Vertex<T> to the end
			{
				for (auto v : vv) {
					if (_firststart)
					{
						Min() = Max() = v;
						_firststart = false;
						return *this;
					}

					if (Min().x() > v.x()) Min().x() = v.x();
					if (Min().y() > v.y()) Min().y() = v.y();
					if (Min().z() > v.z()) Min().z() = v.z();

					if (Max().x() < v.x()) Max().x() = v.x();
					if (Max().y() < v.y()) Max().y() = v.y();
					if (Max().z() < v.z()) Max().z() = v.z();
				}
				return *this;
			}

			inline AABBox& operator + (const AABBox& a)
			{
				 AABBox b(*this);
				b += a.Max();
				b += a.Min();
				return b;
			}


			inline AABBox& operator += (const T& v)
			{
				Max() += v;
				Min() -= v;
				return (*this);
			}
					

			inline AABBox& operator +=(const AABBox& b)		// Add a primitive::Vertex<T> to the end
			{
				*this += b.Min();
				*this += b.Max();
				return *this;
			}

			inline AABBox<N, T>& operator =(const AABBox<N, T>& bbox)
			{
				this->Min() = bbox.Min();
				this->Max() = bbox.Max();
				this->_center = bbox.Center();
				this->_cCalculated = true;
				return *this;
			}

			inline math::Matrix<3, 1, T> GetP(math::Matrix<3, 1, T> normal) {
				math::Matrix<3, 1, T> v = _min;
				if (normal.x() >= (T)0) v.x() = _max.x();
				if (normal.y() >= (T)0) v.y() = _max.y();
				if (normal.z() >= (T)0) v.z() = _max.z();
				return v;
			}

			inline math::Matrix<3, 1, T> GetN(math::Matrix<3, 1, T> normal) {
				math::Matrix<3, 1, T> v = _max;
				if (normal.x() >= (T)0) v.x() = _min.x();
				if (normal.y() >= (T)0) v.y() = _min.y();
				if (normal.z() >= (T)0) v.z() = _min.z();
				return v;
			}

			inline bool IntersectedByBounds(AABBox<3, T> bounds, T  tolerance = (T)0)
			{
				return ((Min().x() - tolerance) <= (bounds.Max().x() + tolerance)) &&
					((Min().y() - tolerance) <= (bounds.Max().y() + tolerance)) &&
					((Min().z() - tolerance) <= (bounds.Max().z() + tolerance)) &&
					((Max().x() + tolerance) >= (bounds.Min().x() - tolerance)) &&
					((Max().y() + tolerance) >= (bounds.Min().y() - tolerance)) &&
					((Max().z() + tolerance) >= (bounds.Min().z() - tolerance));
			}

			inline bool Inside(math::Matrix<3, 1, T> v) {
				if (v.x() >= _min.x() && v.y() >= _min.y() && v.z() >= _min.z() &&
					v.x() <= _max.x() && v.y() <= _max.y() && v.z() <= _max.z()) return true;


				return false;
			}

			inline	const	math::Matrix<N, 1, T>& Min() const { return _min; }

			inline		    math::Matrix<N, 1, T>& Min() { _cCalculated = false; return _min; }

			inline	const	math::Matrix<N, 1, T>& Max() const { return _max; }

			inline		    math::Matrix<N, 1, T>& Max() { _cCalculated = false;  return _max; }

			inline	const	math::Matrix<N, 1, T>& Center() const
			{
				if (!_cCalculated)
				{
					_cCalculated = true;
					_half =_center = (Max() - Min()) / (T)2;
					_center.abs();
					_center += Min();
					
				}
					return _center;

			}

			inline	const	math::Matrix<N, 1, T>& HalfSize() const
			{
				if (!_cCalculated)
				{
					_cCalculated = true;
					_half =_center = (Max() - Min()) / (T)2;
					_center.abs();
					_center += Min();
					
				}
					return _half;

			}

			inline  const T Length() const
			{
				math::Matrix<N, 1, T> d;
				d = (Max() - Min());

				return d.length();
			}

			AABBox Transform(const  math::Matrix<4,4,T> &M)
			{
				//!!!! BITTE nochmal Test/////////////////////////////////////////////////
				//math::Matrix<3, 1, T> newCenter =
					//M * math::Matrix<4, 1, T>(box.center().x(), box.center().y(), box.center().z(), 1);
				math::Matrix<4, 1, T> v(Center().x(), Center().y(), Center().z(), 1);
				math::Matrix<4, 1, T> newCenter = M>>v; ;

				math::Matrix<3, 1, T> newHalfSize;

				newHalfSize.x() =
					std::abs(M(0, 0)) *HalfSize().x() +
					std::abs(M(0, 1)) *HalfSize().y() +
					std::abs(M(0, 2)) *HalfSize().z();

				newHalfSize.y() =
					std::abs(M(1, 0)) *HalfSize().x() +
					std::abs(M(1, 1)) *HalfSize().y() +
					std::abs(M(1, 2)) *HalfSize().z();

				newHalfSize.z() =
					std::abs(M(2, 0)) *HalfSize().x() +
					std::abs(M(2, 1)) *HalfSize().y() +
					std::abs(M(2, 2)) *HalfSize().z();

				return AABBox<3,T>(newCenter,newHalfSize
				);
			}

  		    bool RaySlabTest(const math::Matrix<3, 1, T>& origin, const math::Matrix<3, 1, T>& direction,
				T* outTMin = nullptr, T* outTMax = nullptr)
			{
				auto mn = Min();
				auto mx = Max();

				T tmin = std::numeric_limits<T>::lowest();
				T tmax = std::numeric_limits<T>::max();

				// X-Achse
				if (std::abs(direction.x()) < static_cast<T>(1e-12))
				{
					// Ray parallel zur X-Achse: außerhalb der Slab-Grenzen -> kein Treffer
					if (origin.x() < mn.x() || origin.x() > mx.x())
						return false;
				}
				else
				{
					T invD = static_cast<T>(1) / direction.x();
					T t1 = (mn.x() - origin.x()) * invD;
					T t2 = (mx.x() - origin.x()) * invD;
					if (t1 > t2) std::swap(t1, t2);
					tmin = std::max(tmin, t1);
					tmax = std::min(tmax, t2);
					if (tmin > tmax) return false;
				}

				// Y-Achse
				if (std::abs(direction.y()) < static_cast<T>(1e-12))
				{
					if (origin.y() < mn.y() || origin.y() > mx.y())
						return false;
				}
				else
				{
					T invD = static_cast<T>(1) / direction.y();
					T t1 = (mn.y() - origin.y()) * invD;
					T t2 = (mx.y() - origin.y()) * invD;
					if (t1 > t2) std::swap(t1, t2);
					tmin = std::max(tmin, t1);
					tmax = std::min(tmax, t2);
					if (tmin > tmax) return false;
				}

				// Z-Achse
				if (std::abs(direction.z()) < static_cast<T>(1e-12))
				{
					if (origin.z() < mn.z() || origin.z() > mx.z())
						return false;
				}
				else
				{
					T invD = static_cast<T>(1) / direction.z();
					T t1 = (mn.z() - origin.z()) * invD;
					T t2 = (mx.z() - origin.z()) * invD;
					if (t1 > t2) std::swap(t1, t2);
					tmin = std::max(tmin, t1);
					tmax = std::min(tmax, t2);
					if (tmin > tmax) return false;
				}

				// Box liegt komplett hinter dem Ray-Ursprung
				if (tmax < static_cast<T>(0))
					return false;

				if (outTMin) *outTMin = tmin;
				if (outTMax) *outTMax = tmax;
				return true;
			}
			
			
			
			
			
			
			
			virtual ~AABBox() {}

			enum {
				INSIDE = 1,
				OUTSIDE = 0,
				CLIP = 2
			};

		private:
			math::Matrix<N, 1, T> _min;
			math::Matrix<N, 1, T> _max;
			mutable math::Matrix<N, 1, T> _center;
			mutable math::Matrix<N, 1, T> _half;

			mutable bool _cCalculated=false;
			mutable bool _firststart=true;

			inline void Recalc() {
				if (!_cCalculated)
				{
					_cCalculated = true;
					_center = (Max() - Min()) / (T)2;
					_center.abs();
					_center += Min();
					_half = (Max() - Min()) / (T)2;
				}

			}

		};
       

	}
}