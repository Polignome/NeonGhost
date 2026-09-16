#pragma once
#include "Matrix.h"
#include "Camera.h"
#include "Frustum.h"
#include "Primitive.h"

namespace ng::scene {
	enum
	{
		TOP_PLANE = 1 << 1,
		BOTTOM_PLANE = 1 << 2,
		LEFT_PLANE = 1 << 3,
		RIGHT_PLANE = 1 << 4,
		FRONT_PLANE = 1 << 5,
		BACK_PLANE = 1 << 6,

		ALL_PLANES =
		TOP_PLANE |
		BOTTOM_PLANE |
		LEFT_PLANE |
		RIGHT_PLANE |
		FRONT_PLANE |
		BACK_PLANE
	};

	template <class T>
	class CameraFrustum : public ng::renderer::occlusion::Frustum<T> {
	private:
		math::Matrix<3, 1, T> _Cnear;
		math::Matrix<3, 1, T> _Cfar;
		math::Matrix<3, 1, T> _Near_Top_Left;
		math::Matrix<3, 1, T> _Near_Top_Right;
		math::Matrix<3, 1, T> _Near_Bottom_Left;
		math::Matrix<3, 1, T> _Near_Bottom_Right;
		math::Matrix<3, 1, T> _Far_Top_Left;
		math::Matrix<3, 1, T> _Far_Top_Right;
		math::Matrix<3, 1, T> _Far_Bottom_Left;
		math::Matrix<3, 1, T> _Far_Bottom_Right;
	public:
		CameraFrustum() {
		}
		CameraFrustum(const scene::Camera<T>& cam) {
			createByCam(cam);
		}

		// Berechnet die Eckpunkte des durch _frustum definierten konvexen
		// Polyeders als paarweise Schnittpunkte je dreier Ebenen.
		// Konvention: normal() zeigt nach INNEN (d >= 0 => Punkt liegt
		// innerhalb der Halbebene), analog zu posInside()/intersectsAABB().
		inline math::RayPlaneLine<3, T> MakeInwardPlane(
			const math::Matrix<3, 1, T>& p0,
			const math::Matrix<3, 1, T>& p1,
			const math::Matrix<3, 1, T>& p2,
			const math::Matrix<3, 1, T>& interiorPoint)
		{
			math::RayPlaneLine<3, T> plane(p0, p1, p2);

			T d = plane.normal().x() * interiorPoint.x() +
				plane.normal().y() * interiorPoint.y() +
				plane.normal().z() * interiorPoint.z() +
				plane.D();

			if (d < (T)0)   // GEAENDERT: war "d > (T)0"
			{
				// Normale zeigt vom Innenpunkt weg -> Reihenfolge umkehren
				plane = math::RayPlaneLine<3, T>(p0, p2, p1);
			}
			return plane;
		}

		void createByCam(scene::Camera<T> cam)
		{
		
			math::Matrix<3, 1, T> P = cam.position();
			math::Matrix<3, 1, T> v = cam.forward();
			math::Matrix<3, 1, T> up = cam.up();      // echte Kamera-Vektoren, unverändert
			math::Matrix<3, 1, T> w = cam.right();    // echte Kamera-Vektoren, unverändert
			T nDis = cam.nearp();
			T fDis = cam.farp();
			T fov = cam.fov();
			T ar = cam.aspect();
			this->clear();

			T Hnear = (T)2.0 * std::tan(fov / (T)2.0) * nDis;
			T Wnear = Hnear * ar;
			T Hfar = (T)2.0 * std::tan(fov / (T)2.0) * fDis;
			T Wfar = Hfar * ar;

			_Cnear = P + v * nDis;
			_Cfar = P + v * fDis;

			math::Matrix<3, 1, T> wnear = w * (Wnear * (T)0.5);
			math::Matrix<3, 1, T> wfar = w * (Wfar * (T)0.5);
			math::Matrix<3, 1, T> upnear = up * (Hnear * (T)0.5);
			math::Matrix<3, 1, T> ufar = up * (Hfar * (T)0.5);

			_Near_Top_Left = _Cnear + upnear - wnear;
			_Near_Top_Right = _Cnear + upnear + wnear;
			_Near_Bottom_Left = _Cnear - upnear - wnear;
			_Near_Bottom_Right = _Cnear - upnear + wnear;
			_Far_Top_Left = _Cfar + ufar - wfar;
			_Far_Top_Right = _Cfar + ufar + wfar;
			_Far_Bottom_Left = _Cfar - ufar - wfar;
			_Far_Bottom_Right = _Cfar - ufar + wfar;

			this->addVertex(_Near_Top_Left);
			this->addVertex(_Near_Top_Right);
			this->addVertex(_Near_Bottom_Left);
			this->addVertex(_Near_Bottom_Right);
			this->addVertex(_Far_Top_Left);
			this->addVertex(_Far_Top_Right);
			this->addVertex(_Far_Bottom_Right);
			this->addVertex(_Far_Bottom_Left);

			bounding::AABBox<3, T> aabb;
			aabb += this->vertices();
			math::Matrix<3, 1, T> center = aabb.Center();
			
			
			this->planes().push_back(
				MakeInwardPlane(
					_Near_Bottom_Left,
					_Near_Top_Left,
					_Near_Bottom_Right,
					center

				));
			
			this->planes().push_back(
				MakeInwardPlane(
					_Far_Bottom_Right,
					_Far_Bottom_Left,
					_Far_Top_Right,
					center));
			
			// ------------------------------------------------------------
			// Top
			// ------------------------------------------------------------
			this->planes().push_back(
				MakeInwardPlane(
					_Far_Top_Right,
					_Near_Top_Right,
					_Far_Top_Left,
					center

				));

			
			// ------------------------------------------------------------
			// Bottom
			// ------------------------------------------------------------
			this->planes().push_back(
				MakeInwardPlane(
					_Far_Bottom_Right,
					_Far_Bottom_Left,
					_Near_Bottom_Right,
					center

				));
			
			// ------------------------------------------------------------
			// Left
			// ------------------------------------------------------------
			this->planes().push_back(
				MakeInwardPlane(
					_Near_Top_Left,
					_Near_Bottom_Left,
					_Far_Top_Left,
					center

				));

			// ------------------------------------------------------------
			// Right
			// ------------------------------------------------------------
			this->planes().push_back(
				MakeInwardPlane(
					_Near_Bottom_Right,
					_Near_Top_Right,
					_Far_Bottom_Right,
					center
				));


		}
		
	




		const auto& NearTopLeft()     const { return _Near_Top_Left; }
		const auto& NearTopRight()    const { return _Near_Top_Right; }
		const auto& NearBottomLeft()  const { return _Near_Bottom_Left; }
		const auto& NearBottomRight() const { return _Near_Bottom_Right; }
		const auto& FarTopLeft()      const { return _Far_Top_Left; }
		const auto& FarTopRight()     const { return _Far_Top_Right; }
		const auto& FarBottomLeft()   const { return _Far_Bottom_Left; }
		const auto& FarBottomRight()  const { return _Far_Bottom_Right; }
	};
}

