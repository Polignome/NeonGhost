
#pragma once
#include <cmath>
#include <type_traits>
#include "Matrix.h"
#include "AABBox.h"
#include "BoundingSphere.h"

namespace ng {
	namespace renderer {
		namespace objects {
			template<typename T>
			class Object3D
			{
				static_assert(std::is_floating_point<T>::value,
					"Object3D<T> requires a floating point type (float/double).");
		   
				
			public:
				using Vec3 = ng::math::Matrix<3, 1, T>;

				virtual ~Object3D() = default;

				// ============================================================
				// Position
				// ============================================================
				void SetPosition(const Vec3& p) { 
					m_position = p; 
					m_boundsDirty = true;
					
				}
				void SetPosition(T x, T y, T z) { 
					m_position = Vec3(x, y, z); 
					m_boundsDirty = true; 
				}
				const Vec3& Position() const { return m_position; }

				// ============================================================
				// Scale
				// ============================================================
				void SetScale(const Vec3& s) { 
					m_scale = s; 
				    m_boundsDirty = true; 
				}
				void SetScale(T x, T y, T z) { 
					m_scale = Vec3(x, y, z);
					m_boundsDirty = true; 

				}
				void SetScale(T uniform) { 
					m_scale = Vec3(uniform, uniform, uniform);
					m_boundsDirty = true; 
				}

				const Vec3& Scale() const { return m_scale; }

				// ============================================================
				// Rotation (Euler, Radiant - Erweiterung auf Quaternion später möglich)
				// ============================================================
				void SetRotation(const Vec3& eulerRad) { 
					m_rotation = eulerRad; 
					m_boundsDirty = true; 
				}
				void SetRotation(T pitch, T yaw, T roll) {
					m_rotation = Vec3(pitch, yaw, roll); 
					m_boundsDirty = true; 
				}

				const Vec3& Rotation() const { return m_rotation; }

				// ============================================================
				// Velocity
				// ============================================================
				void SetVelocity(const Vec3& v) { m_velocity = v; }
				void SetVelocity(T x, T y, T z) { m_velocity = Vec3(x, y, z); }
				const Vec3& Velocity() const { return m_velocity; }
				Vec3& Velocity() { return m_velocity; }



				void SetRotationVelocity(const Vec3& v) { m_rotation_velocity = v; }
				void SetRotationVelocity(T x, T y, T z) { m_rotation_velocity = Vec3(x, y, z); }
				const Vec3& RotationVelocity() const { return m_rotation_velocity; }
				Vec3& RotationVelocity() { return m_rotation_velocity; }

				// ============================================================
				// Update
				// ============================================================
				


				void Update(T dt)
				{
					m_position += m_velocity * dt;
					m_rotation += m_rotation_velocity * dt;
					m_update_matrix = true;
					m_boundsDirty = true;
				}

				// ============================================================
				// Bounds
				// ============================================================
				const bounding::AABBox<3,T>& Bounds() const
				{
					UpdateBoundsIfNeeded();
					return m_bounds;
				}
				const bounding::BoundingSphere<T>& Sphere() const
				{
					UpdateBoundsIfNeeded();
					return m_sphere;
				}

			protected:
				// Nur DAS muss jede Unterklasse liefern: unskalierte halbe Ausdehnung
				// entlang jeder lokalen Achse (vor Rotation/Skalierung).
				virtual Vec3 GetHalfExtents() const = 0;

				void MarkBoundsDirty() { m_boundsDirty = true; }
				virtual Vec3 GetLocalCenterOffset() const { return Vec3(0, 0, 0); }

			private:

				bool m_update_matrix = true;
				
				
				void UpdateBoundsIfNeeded() const
				{
					if (!m_boundsDirty)
						return;

					// ------------------------------------------------------------
					// Lokale Bounding-Box
					// ------------------------------------------------------------

					Vec3 localHalf = GetHalfExtents();
					Vec3 localCenter = GetLocalCenterOffset();

					// ------------------------------------------------------------
					// Scale
					// ------------------------------------------------------------

					Vec3 scaledHalf(
						std::abs(localHalf.x() * m_scale.x()),
						std::abs(localHalf.y() * m_scale.y()),
						std::abs(localHalf.z() * m_scale.z())
					);

					Vec3 scaledCenter(
						localCenter.x() * m_scale.x(),
						localCenter.y() * m_scale.y(),
						localCenter.z() * m_scale.z()
					);

					// ------------------------------------------------------------
					// Rotation
					// ------------------------------------------------------------

					Vec3 rotatedHalf =
						ComputeRotatedHalfExtents(
							scaledHalf,
							m_rotation
						);

					Vec3 rotatedCenter =
						RotateEulerXYZ(
							scaledCenter,
							m_rotation
						);

					// ------------------------------------------------------------
					// Translation
					// ------------------------------------------------------------

					Vec3 center =
						m_position +
						rotatedCenter;

					// ------------------------------------------------------------
					// AABB
					// ------------------------------------------------------------

					m_bounds =
						bounding::AABBox<3, T>(
							center.x() - rotatedHalf.x(),
							center.y() - rotatedHalf.y(),
							center.z() - rotatedHalf.z(),

							center.x() + rotatedHalf.x(),
							center.y() + rotatedHalf.y(),
							center.z() + rotatedHalf.z()
						);

					// ------------------------------------------------------------
					// Bounding Sphere
					// ------------------------------------------------------------

					T radius = std::sqrt(
						scaledHalf.x() * scaledHalf.x() +
						scaledHalf.y() * scaledHalf.y() +
						scaledHalf.z() * scaledHalf.z()
					);

					m_sphere =
						bounding::BoundingSphere<T>(
							center,
							radius
						);

					m_boundsDirty = false;
				}
				
			

private:
	// ============================================================
	// Euler-Rotation (Reihenfolge: erst X, dann Y, dann Z ->
	// resultierende Matrix entspricht Rz * Ry * Rx)
	//
	// Konvention: SetRotation(pitch, yaw, roll) aus Object3D<T>
	//   pitch = Rotation um X-Achse
	//   yaw   = Rotation um Y-Achse
	//   roll  = Rotation um Z-Achse
	// ============================================================
	static Vec3 RotateEulerXYZ(const Vec3& p, const Vec3& euler)
	{
		T cx = std::cos(euler.x()), sx = std::sin(euler.x());
		T cy = std::cos(euler.y()), sy = std::sin(euler.y());
		T cz = std::cos(euler.z()), sz = std::sin(euler.z());

		// Rotation um X (pitch)
		T x1 = p.x();
		T y1 = p.y() * cx - p.z() * sx;
		T z1 = p.y() * sx + p.z() * cx;

		// Rotation um Y (yaw)
		T x2 = x1 * cy + z1 * sy;
		T y2 = y1;
		T z2 = -x1 * sy + z1 * cy;

		// Rotation um Z (roll)
		T x3 = x2 * cz - y2 * sz;
		T y3 = x2 * sz + y2 * cz;
		T z3 = z2;

		return Vec3(x3, y3, z3);
	}

	// ============================================================
	// Inverse von RotateEulerXYZ.
	//
	// WICHTIG: Inverse von (Rz * Ry * Rx) ist (Rx(-x) * Ry(-y) * Rz(-z)),
	// d.h. Reihenfolge UND Vorzeichen umkehren - nicht nur die Winkel
	// negieren und dieselbe Reihenfolge anwenden (das wäre falsch,
	// außer bei reiner Ein-Achsen-Rotation).
	// ============================================================
	static Vec3 InverseRotateEulerXYZ(const Vec3& p, const Vec3& euler)
	{
		T cx = std::cos(-euler.x()), sx = std::sin(-euler.x());
		T cy = std::cos(-euler.y()), sy = std::sin(-euler.y());
		T cz = std::cos(-euler.z()), sz = std::sin(-euler.z());

		// zuerst inverse Z (roll)
		T x1 = p.x() * cz - p.y() * sz;
		T y1 = p.x() * sz + p.y() * cz;
		T z1 = p.z();

		// dann inverse Y (yaw)
		T x2 = x1 * cy + z1 * sy;
		T y2 = y1;
		T z2 = -x1 * sy + z1 * cy;

		// dann inverse X (pitch)
		T x3 = x2;
		T y3 = y2 * cx - z2 * sx;
		T z3 = y2 * sx + z2 * cx;

		return Vec3(x3, y3, z3);
	}
			

static Vec3 ComputeRotatedHalfExtents(
	const Vec3& half,
	const Vec3& euler)
{
	const T cx = std::cos(euler.x());
	const T sx = std::sin(euler.x());

	const T cy = std::cos(euler.y());
	const T sy = std::sin(euler.y());

	const T cz = std::cos(euler.z());
	const T sz = std::sin(euler.z());

	// R = Rz * Ry * Rx
	//
	//        | r00 r01 r02 |
	// R =    | r10 r11 r12 |
	//        | r20 r21 r22 |

	const T r00 = cz * cy;
	const T r01 = cz * sy * sx - sz * cx;
	const T r02 = cz * sy * cx + sz * sx;

	const T r10 = sz * cy;
	const T r11 = sz * sy * sx + cz * cx;
	const T r12 = sz * sy * cx - cz * sx;

	const T r20 = -sy;
	const T r21 = cy * sx;
	const T r22 = cy * cx;

	return Vec3(
		std::abs(r00) * half.x() +
		std::abs(r01) * half.y() +
		std::abs(r02) * half.z(),

		std::abs(r10) * half.x() +
		std::abs(r11) * half.y() +
		std::abs(r12) * half.z(),

		std::abs(r20) * half.x() +
		std::abs(r21) * half.y() +
		std::abs(r22) * half.z()
	);
}
				Vec3 m_position{ 0, 0, 0 };
				Vec3 m_scale{ 1, 1, 1 };
				Vec3 m_rotation{ 0, 0, 0 };
				Vec3 m_velocity{ 0, 0, 0 };
				Vec3 m_rotation_velocity{ 0, 0, 0 };
				mutable bounding::AABBox<3,T> m_bounds;
				mutable bounding::BoundingSphere<T> m_sphere;
				mutable bool m_boundsDirty = true;
			};
		}
	}
}