#pragma once
#include "Global.h"
#include "RayPlaneLine.h"
#include "Vertex.h"
#include "Frustum.h"

namespace ng {
	namespace geometry {

		// Welche Hälfte ClipByPlane zurückgeben soll.
		// FRONT = Seite in die plane.normal() zeigt (>= 0), BACK = Rest.
		enum class ClipSide
		{
			FRONT = 0,
			BACK = 1
		};

		// ================================================================
		// SplitByPlane - std::vector<math::Matrix<3,1,T>>
		//
		// Sutherland-Hodgman: zerlegt ein KONVEXES, geschlossenes Polygon
		// (verts[i] -> verts[i+1], letzte Kante schließt zu verts[0])
		// in einen Front- und einen Back-Anteil bzgl. `plane`.
		// Schnittpunkte werden in beide Ausgabelisten eingefügt, damit
		// beide Hälften wieder geschlossene Polygone sind.
		// ================================================================
		template<class T>
		inline void SplitByPlane(
			const std::vector<math::Matrix<3, 1, T>>& verts,
			const math::RayPlaneLine<3, T>& plane,
			std::vector<math::Matrix<3, 1, T>>& outFront,
			std::vector<math::Matrix<3, 1, T>>& outBack)
		{
			outFront.clear();
			outBack.clear();

			const size_t n = verts.size();
			if (n == 0)
				return;
			if (n < 3)
			{
				// Kein Polygon - degenerierte Eingabe unverändert einsortieren
				for (const auto& p : verts)
				{
					T d = plane.normal().x() * p.x() +
						plane.normal().y() * p.y() +
						plane.normal().z() * p.z() +
						plane.D();
					(d >= (T)0 ? outFront : outBack).push_back(p);
				}
				return;
			}

			auto distance = [&](const math::Matrix<3, 1, T>& p) -> T
				{
					return plane.normal().x() * p.x() +
						plane.normal().y() * p.y() +
						plane.normal().z() * p.z() +
						plane.D();
				};

			for (size_t i = 0; i < n; ++i)
			{
				const math::Matrix<3, 1, T>& curr = verts[i];
				const math::Matrix<3, 1, T>& next = verts[(i + 1) % n];

				T dCurr = distance(curr);
				T dNext = distance(next);

				bool currFront = dCurr >= (T)0;
				bool nextFront = dNext >= (T)0;

				if (currFront)
					outFront.push_back(curr);
				else
					outBack.push_back(curr);

				// Kante schneidet die Ebene -> Schnittpunkt in beide Listen
				if (currFront != nextFront)
				{
					T denom = dCurr - dNext;
					T t = (std::abs(denom) > math::EPSILON<T>)
						? (dCurr / denom)
						: (T)0;

					math::Matrix<3, 1, T> inter = curr + (next - curr) * t;

					outFront.push_back(inter);
					outBack.push_back(inter);
				}
			}
		}

		// ================================================================
		// SplitByPlane - std::vector<geometry::Vertex<T>>
		//
		// Wie oben, interpoliert an Schnittpunkten zusätzlich alle
		// Vertex-Attribute (world, texture, fxtexture, normal, color)
		// linear mit demselben t wie world().
		// ================================================================
		template<class T>
		inline void SplitByPlane(
			const std::vector<geometry::Vertex<T>>& verts,
			const math::RayPlaneLine<3, T>& plane,
			std::vector<geometry::Vertex<T>>& outFront,
			std::vector<geometry::Vertex<T>>& outBack)
		{
			outFront.clear();
			outBack.clear();

			const size_t n = verts.size();
			if (n == 0)
				return;

			auto distance = [&](const geometry::Vertex<T>& v) -> T
				{
					const auto& p = v.world();
					return plane.normal().x() * p.x() +
						plane.normal().y() * p.y() +
						plane.normal().z() * p.z() +
						plane.D();
				};

			auto lerpVertex = [&](const geometry::Vertex<T>& a,
				const geometry::Vertex<T>& b,
				T t) -> geometry::Vertex<T>
				{
					geometry::Vertex<T> out;
					out.world() = a.world() + (b.world() - a.world()) * t;
					out.texture() = a.texture() + (b.texture() - a.texture()) * t;
					out.fxtexture() = a.fxtexture() + (b.fxtexture() - a.fxtexture()) * t;
					out.normal() = a.normal() + (b.normal() - a.normal()) * t;
					out.color() = a.color() + (b.color() - a.color()) * t;
					out.selected() = a.selected(); // bool - kein Sinn zu interpolieren
					out.fxtexture()= a.fxtexture() + (b.fxtexture() - a.fxtexture()) * t;
					return out;
				};

			if (n < 3)
			{
				for (const auto& v : verts)
					(distance(v) >= (T)0 ? outFront : outBack).push_back(v);
				return;
			}

			for (size_t i = 0; i < n; ++i)
			{
				const geometry::Vertex<T>& curr = verts[i];
				const geometry::Vertex<T>& next = verts[(i + 1) % n];



				T dCurr = distance(curr);
				T dNext = distance(next);

				bool currFront = dCurr >= (T)0;
				bool nextFront = dNext >= (T)0;

				if (currFront)
					outFront.push_back(curr);
				else
					outBack.push_back(curr);

				if (currFront != nextFront)
				{
					T denom = dCurr - dNext;
					T t = (std::abs(denom) > math::EPSILON<T>)
						? (dCurr / denom)
						: (T)0;

					geometry::Vertex<T> inter = lerpVertex(curr, next, t);

					outFront.push_back(inter);
					outBack.push_back(inter);
				}
			}
		}

		// ================================================================
		// ClipByPlane - liefert nur die gewünschte Hälfte
		// ================================================================
		template<class T>
		inline std::vector<math::Matrix<3, 1, T>> ClipByPlane(
			const std::vector<math::Matrix<3, 1, T>>& verts,
			const math::RayPlaneLine<3, T>& plane,
			ClipSide side = ClipSide::FRONT)
		{
			std::vector<math::Matrix<3, 1, T>> front, back;
			SplitByPlane(verts, plane, front, back);
			return (side == ClipSide::FRONT) ? std::move(front) : std::move(back);
		}

		template<class T>
		inline std::vector<geometry::Vertex<T>> ClipByPlane(
			const std::vector<geometry::Vertex<T>>& verts,
			const math::RayPlaneLine<3, T>& plane,
			ClipSide side = ClipSide::FRONT)
		{
			std::vector<geometry::Vertex<T>> front, back;
			SplitByPlane(verts, plane, front, back);
			return (side == ClipSide::FRONT) ? std::move(front) : std::move(back);
		}

	} // namespace geometry
} // namespace ng





namespace ng {
	namespace geometry {

		// ================================================================
		// ClipByFrustum
		//
		// Clippt ein konvexes, geschlossenes Polygon nacheinander gegen
		// ALLE Ebenen eines Frustum<T>. Nach jedem Schritt bleibt nur die
		// Seite übrig, die innerhalb der jeweiligen Ebene liegt (FRONT,
		// gemäß der Frustum-Konvention normal() zeigt nach INNEN).
		//
		// Bricht früh ab, sobald das Polygon komplett wegclippt wurde
		// (weniger als 3 Punkte übrig -> außerhalb des Frustums).
		// ================================================================
		template<class T>
		inline std::vector<math::Matrix<3, 1, T>> ClipByFrustum(
			const std::vector<math::Matrix<3, 1, T>>& verts,
			const ng::renderer::occlusion::Frustum<T>& frustum)
		{
			std::vector<math::Matrix<3, 1, T>> current = verts;

			for (const auto& plane : frustum.planes())
			{
				if (current.size() < 3)
					break;

				current = ClipByPlane(current, plane, ClipSide::FRONT);
			}

			return current;
		}

		template<class T>
		inline std::vector<geometry::Vertex<T>> ClipByFrustum(
			const std::vector<geometry::Vertex<T>>& verts,
			const ng::renderer::occlusion::Frustum<T>& frustum)
		{
			std::vector<geometry::Vertex<T>> current = verts;

			for (const auto& plane : frustum.planes())
			{
				if (current.size() < 3)
					break;

				current = ClipByPlane(current, plane, ClipSide::FRONT);
			}

			return current;
		}

	} // namespace geometry
} // namespace ng



namespace ng {
	namespace geometry {

		// ================================================================
		// ClipLineByFrustum - math::Matrix<3,1,T>
		//
		// Cyrus-Beck-Clipping: Parametrisiert die Linie als
		//     P(t) = p0 + t * (p1 - p0),   t in [0,1]
		// und schneidet das Intervall [tMin,tMax] an jeder Frustum-Ebene
		// weiter ein. Konvention wie überall: normal() zeigt nach INNEN,
		// d.h. d(P) = normal()·P + D >= 0 ist innerhalb.
		//
		// Rückgabe: true, wenn ein sichtbares Teilstück übrig bleibt.
		// outP0/outP1 enthalten dann die geclippten Endpunkte.
		// ================================================================
		template<class T>
		inline bool ClipLineByFrustum(
			const math::Matrix<3, 1, T>& p0,
			const math::Matrix<3, 1, T>& p1,
			const ng::renderer::occlusion::Frustum<T>& frustum,
			math::Matrix<3, 1, T>& outP0,
			math::Matrix<3, 1, T>& outP1)
		{
			T tMin = (T)0;
			T tMax = (T)1;

			math::Matrix<3, 1, T> dir = p1 - p0;

			auto distance = [](const math::RayPlaneLine<3, T>& plane,
				const math::Matrix<3, 1, T>& p) -> T
				{
					return plane.normal().x() * p.x() +
						plane.normal().y() * p.y() +
						plane.normal().z() * p.z() +
						plane.D();
				};

			for (const auto& plane : frustum.planes())
			{
				T d0 = distance(plane, p0);

				// Ableitung von d entlang der Linie:
				// d(t) = d0 + t * (normal() . dir)
				T denom = plane.normal().x() * dir.x() +
					plane.normal().y() * dir.y() +
					plane.normal().z() * dir.z();

				if (std::abs(denom) <= math::EPSILON<T>)
				{
					// Linie liegt (nahezu) parallel zur Ebene.
					// Wenn Startpunkt außerhalb -> komplett verworfen.
					if (d0 < (T)0)
						return false;
					// sonst: Ebene beeinflusst dieses Segment nicht
					continue;
				}

				T t = -d0 / denom;

				if (denom > (T)0)
				{
					// Linie tritt an t in den gültigen Halbraum ein
					// (d wächst mit t) -> t ist eine untere Schranke
					if (t > tMin)
						tMin = t;
				}
				else
				{
					// Linie verlässt den gültigen Halbraum bei t
					// -> t ist eine obere Schranke
					if (t < tMax)
						tMax = t;
				}

				if (tMin > tMax)
					return false; // Segment komplett weggeclippt
			}

			outP0 = p0 + dir * tMin;
			outP1 = p0 + dir * tMax;
			return true;
		}

		// ================================================================
		// ClipLineByFrustum - geometry::Vertex<T>
		//
		// Wie oben, interpoliert an den geclippten Enden zusätzlich alle
		// Vertex-Attribute (world, texture, fxtexture, normal, color)
		// mit denselben tMin/tMax wie world().
		// ================================================================
		template<class T>
		inline bool ClipLineByFrustum(
			const geometry::Vertex<T>& v0,
			const geometry::Vertex<T>& v1,
			const ng::renderer::occlusion::Frustum<T>& frustum,
			geometry::Vertex<T>& outV0,
			geometry::Vertex<T>& outV1)
		{
			T tMin = (T)0;
			T tMax = (T)1;

			math::Matrix<3, 1, T> p0 = v0.world();
			math::Matrix<3, 1, T> dir = v1.world() - p0;

			auto distance = [](const math::RayPlaneLine<3, T>& plane,
				const math::Matrix<3, 1, T>& p) -> T
				{
					return plane.normal().x() * p.x() +
						plane.normal().y() * p.y() +
						plane.normal().z() * p.z() +
						plane.D();
				};

			for (const auto& plane : frustum.planes())
			{
				T d0 = distance(plane, p0);

				T denom = plane.normal().x() * dir.x() +
					plane.normal().y() * dir.y() +
					plane.normal().z() * dir.z();

				if (std::abs(denom) <= math::EPSILON<T>)
				{
					if (d0 < (T)0)
						return false;
					continue;
				}

				T t = -d0 / denom;

				if (denom > (T)0)
				{
					if (t > tMin) tMin = t;
				}
				else
				{
					if (t < tMax) tMax = t;
				}

				if (tMin > tMax)
					return false;
			}

			auto lerpVertex = [&](T t) -> geometry::Vertex<T>
				{
					geometry::Vertex<T> out;
					out.world() = v0.world() + (v1.world() - v0.world()) * t;
					out.texture() = v0.texture() + (v1.texture() - v0.texture()) * t;
					out.fxtexture() = v0.fxtexture() + (v1.fxtexture() - v0.fxtexture()) * t;
					out.normal() = v0.normal() + (v1.normal() - v0.normal()) * t;
					out.color() = v0.color() + (v1.color() - v0.color()) * t;
					out.selected() = v0.selected();
					return out;
				};

			outV0 = lerpVertex(tMin);
			outV1 = lerpVertex(tMax);
			return true;
		}

	} // namespace geometry
} // namespace ng