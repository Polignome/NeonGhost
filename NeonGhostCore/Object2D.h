#pragma once
#include "Global.h"
#include "Matrix.h"
#include "Pixelbuffer.h"
#include "AABBox2D.h"
#include "BoundingCircle2D.h"

namespace ng {
	namespace renderer {
		
		namespace objects {
			template<typename T>
			class Object2D
			{
			public:
				using Vec2 = ng::math::Matrix<2, 1, T>;

				virtual ~Object2D() = default;

				void SetPosition(const Vec2& p) { m_position = p; m_boundsDirty = true; }
				void SetPosition(T x, T y) { m_position = Vec2(x, y); m_boundsDirty = true; }
				const Vec2& Position() const { return m_position; }

				void SetScale(const Vec2& s) { m_scale = s; m_boundsDirty = true; }
				void SetScale(T x, T y) { m_scale = Vec2(x, y); m_boundsDirty = true; }
				const Vec2& Scale() const { return m_scale; }

				void SetRotation(T r) { m_rotation = r; m_boundsDirty = true; }
				T Rotation() const { return m_rotation; }

				void SetVelocity(const Vec2& v) { m_velocity = v; }
				void SetVelocity(T x, T y) { m_velocity = Vec2(x, y); }

				void SetAngularVelocity(T a) {  m_angularVelocity=a; }
				T AngularVelocity() { return m_angularVelocity; }
				
				const Vec2& Velocity() const { return m_velocity; }
				Vec2& Velocity() { return m_velocity; }

				void Update(T dt) { m_position += m_velocity * dt; m_boundsDirty = true; }

				const bounding::AABBox2D<T>& Bounds() const
				{
					UpdateBoundsIfNeeded();
					return m_bounds;
				}
				const bounding::BoundingCircle2D<T>& Circle() const
				{
					UpdateBoundsIfNeeded();
					return m_circle;
				}

			protected:
				// Nur DAS muss jede Unterklasse liefern: ihre unskalierte halbe Ausdehnung.
				// Sprite: aus Texture().Width()/Height(). Ein zukünftiges "CircleObject2D"
				// könnte hier einfach (radius, radius) zurückgeben.
				virtual Vec2 GetHalfExtents() const = 0;

				void MarkBoundsDirty() { m_boundsDirty = true; }

			private:
				void UpdateBoundsIfNeeded() const
				{
					if (!m_boundsDirty) return;

					Vec2 half = GetHalfExtents();
					Vec2 scaledHalf(half.x() * std::abs(m_scale.x()), half.y() * std::abs(m_scale.y()));

					T c = std::cos(m_rotation);
					T s = std::sin(m_rotation);
					T rotatedHalfX = std::abs(c) * scaledHalf.x() + std::abs(s) * scaledHalf.y();
					T rotatedHalfY = std::abs(s) * scaledHalf.x() + std::abs(c) * scaledHalf.y();

					m_bounds = bounding::AABBox2D<T>(
						m_position.x() - rotatedHalfX, m_position.y() - rotatedHalfY,
						m_position.x() + rotatedHalfX, m_position.y() + rotatedHalfY);

					T radius = std::sqrt(scaledHalf.x() * scaledHalf.x() + scaledHalf.y() * scaledHalf.y());
					m_circle = bounding::BoundingCircle2D<T>(m_position, radius);

					m_boundsDirty = false;
				}

				Vec2 m_position{ 0, 0 };
				Vec2 m_scale{ 1, 1 };
				Vec2 m_velocity{ 0, 0 };
				T m_rotation{ 0 };
				mutable bounding::AABBox2D<T> m_bounds;
				mutable bounding::BoundingCircle2D<T> m_circle;
				mutable bool m_boundsDirty = true;
				T m_angularVelocity =0;
			};
		}
	}
}