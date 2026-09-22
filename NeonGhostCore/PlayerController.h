#include "Camera.h"
#include "BSPCollision.h"

// -------------------------------------------------------------
// Player-Wrapper: haelt Ellipsoid-Position getrennt von der Kamera
// und schiebt jede Bewegung durch den BSP.
// -------------------------------------------------------------
namespace ng {
    namespace controller {
        template <class T>
        class PlayerController
        {
        public:
            using Vec3 = ng::math::Matrix<3, 1, T>;

            PlayerController(ng::bsp::BSPTree<T>& tree)
                : m_collision(tree)
            {
            }

            void Init(const Vec3& startFeetPos, T eyeHeight,
                const Vec3& radii = Vec3(16, 36, 16))
            {
                m_radii = radii;
                m_eyeHeight = eyeHeight;
                m_position = startFeetPos;
                m_position(1) += radii(1);
                m_velocity = Vec3(0, 0, 0);
                m_onGround = false;
                m_initialized = true;
            }

            inline bool IsInitialized() const { return m_initialized; }

            // Kollision an/aus. Aus = reines Fliegen ohne Schwerkraft/Wände (Noclip).
            inline void SetEnabled(bool enabled) { m_enabled = enabled; }
            inline bool IsEnabled() const { return m_enabled; }

            // Freier Vektor (x,y,z) in Units/s^2, wirkt nur wenn Kollision aktiv ist.
            inline void SetGravity(const Vec3& g) { m_gravity = g; }
            inline const Vec3& Gravity() const { return m_gravity; }

            void Update(ng::scene::Camera<T>& camera, T dt, T wishX, T wishZ, bool jump)
            {
                if (!m_initialized) return;

                Vec3 fwd = camera.forward();
                Vec3 right = camera.right();
                fwd(1) = 0;  right(1) = 0;

                if (fwd.length() > (T)1e-6)   fwd.normalize();
                if (right.length() > (T)1e-6) right.normalize();

                Vec3 wishDir = right * wishX + fwd * wishZ;
                const T wishLen = wishDir.length();
                if (wishLen > (T)1) wishDir = wishDir * ((T)1 / wishLen);

                if (!m_enabled)
                {
                    // Noclip: direkt bewegen, keine Schwerkraft, keine Kollision.
                    m_velocity = Vec3(0, 0, 0);
                    m_onGround = false;

                    Vec3 free = wishDir * kMoveSpeed;
                    m_position = m_position + free * dt;

                    Vec3 eye = m_position;
                    eye(1) += m_eyeHeight;
                    camera.SetPosition(eye);
                    return;
                }

                // Horizontale Wunschgeschwindigkeit ersetzen, vertikale (Schwerkraft)
                // fortlaufend akkumulieren.
                m_velocity(0) = wishDir(0) * kMoveSpeed;
                m_velocity(2) = wishDir(2) * kMoveSpeed;

                m_velocity = m_velocity + m_gravity * dt;

                if (m_onGround && jump)
                    m_velocity(1) = kJumpSpeed;

                const Vec3 desiredMove = m_velocity * dt;
                auto result = m_collision.MoveEllipsoid(m_position, desiredMove, m_radii);
                m_position = result.position;

                m_onGround = false;
                if (result.collided && result.hitNormal(1) > (T)0.7)
                {
                    m_onGround = true;
                    if (m_velocity(1) < 0) m_velocity(1) = 0;
                }

                Vec3 eye = m_position;
                eye(1) += m_eyeHeight;
                camera.SetPosition(eye);
            }

            bool OnGround() const { return m_onGround; }
            const Vec3& FeetPosition() const { return m_position; }

        private:
            ng::bsp::BSPCollision<T> m_collision;

            Vec3 m_position{ 0, 0, 0 };
            Vec3 m_velocity{ 0, 0, 0 };
            Vec3 m_radii{ 16, 36, 16 };
            Vec3 m_gravity{ 0, -800, 0 };   // simulierte Erdanziehung (Units/s^2)
            T m_eyeHeight = 28;
            bool m_onGround = false;
            bool m_enabled = true;
            bool m_initialized = false;

            static constexpr T kMoveSpeed = (T)250;
            static constexpr T kJumpSpeed = (T)300;
        };
    }
}