#pragma once

#include "Camera.h"
#include "BSPCollision.h"

#include <vector>

// -------------------------------------------------------------
// Player-Wrapper: haelt Ellipsoid-Position getrennt von der Kamera
// und schiebt jede Bewegung durch den BSP.
//
// Konventionen:
//  - Y ist oben
//  - m_position = Ellipsoid-MITTELPUNKT, Auge = m_position + eyeHeight
//  - camera.forward() ist die RUECKWAERTS-Achse (echter Blick = -forward())
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

            inline Vec3& Radius() {
                return m_radii;

            }

            inline const Vec3& Radius() const {
                return m_radii;

            }



            // startFeetPos = FUSSposition
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

            // NEU: Init ueber die AUGENposition (z.B. m_camera.position()).
            // Auge = Mittelpunkt + eyeHeight, Mittelpunkt = Fuesse + radii.y
            void InitFromEye(const Vec3& eyePos, T eyeHeight,
                const Vec3& radii = Vec3(16, 36, 16))
            {
                Vec3 feet = eyePos;
                feet(1) -= (radii(1) + eyeHeight);
                Init(feet, eyeHeight, radii);
            }

            inline bool IsInitialized() const { return m_initialized; }

            // Kollision an/aus. Aus = reines Fliegen ohne Schwerkraft/Waende (Noclip).
            inline void SetEnabled(bool enabled) { m_enabled = enabled; }
            inline bool IsEnabled() const { return m_enabled; }

            // Freier Vektor (x,y,z) in Units/s^2, wirkt nur wenn Kollision aktiv ist.
            inline void SetGravity(const Vec3& g) { m_gravity = g; }
            inline const Vec3& Gravity() const { return m_gravity; }

            void Update(ng::scene::Camera<T>& camera, T dt, T wishX, T wishZ, bool jump)
            {
                if (!m_initialized) return;

                m_teleported = false;

                Vec3 fwd = camera.forward();
                Vec3 right = camera.right();
                fwd(1) = 0;  right(1) = 0;

                if (fwd.length() > (T)1e-6)   fwd.normalize();
                if (right.length() > (T)1e-6) right.normalize();

                // forward() ist die Rueckwaertsachse: W (wishZ = -1) laeuft nach vorne.
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

                const Vec3 desired = m_velocity * dt;
                const Vec3 moveH(desired(0), (T)0, desired(2));
                const Vec3 moveV((T)0, desired(1), (T)0);

                const Vec3 eyeOffset(0, m_eyeHeight, 0);
                const Vec3 eyeOld = m_position + eyeOffset;     // Auge VOR der Bewegung

                // ---- Bewegung in zwei Schritten (wie im Header von BSPCollision empfohlen):
                //      erst horizontal, dann Schwerkraft. So gehoert hitNormal im zweiten
                //      Schritt eindeutig zum Boden/zur Decke.
                auto rh = m_collision.MoveEllipsoid(m_position, moveH, m_radii);
                m_position = rh.position;

                auto rv = m_collision.MoveEllipsoid(m_position, moveV, m_radii);
                m_position = rv.position;

                m_onGround = false;
                if (rv.collided)
                {
                    if (rv.hitNormal(1) > (T)0.7 && m_velocity(1) <= 0)
                        m_onGround = true;

                    m_velocity(1) = 0;      // Boden oder Decke getroffen
                }

                // ---- Portal-Teleport: mit dem AUGE testen, weil die Kamera dort sitzt.
                if (m_teleport && m_warps)
                {
                    const Vec3 eyeNew = m_position + eyeOffset;

                    for (const auto& A : *m_warps)
                    {
                        if (!(A.flags & ng::bsp::WARP_TELEPORT) || A.partner < 0) continue;
                        if ((size_t)A.partner >= m_warps->size()) continue;
                        if (!ng::bsp::CrossedPortal(A, eyeOld, eyeNew)) continue;

                        const auto& B = (*m_warps)[A.partner];

                        // Auge exakt warpen: kein Versatz, kein Zusatz-Schieben.
                        Vec3 eyeB = ng::bsp::WarpPoint(A, B, eyeNew);

                        // Nur falls das Auge numerisch auf/hinter der Ebene von B landet.
                        const T dd = (eyeB - B.origin) ^ B.normal;
                        if (dd < kMinPortalDist)
                            eyeB = eyeB + B.normal * (kMinPortalDist - dd);

                        m_position = eyeB - eyeOffset;
                        m_velocity = ng::bsp::WarpDir(A, B, m_velocity);

                        // ACHTUNG: LastLook ist das gewarpte forward() = Rueckwaertsachse.
                        // Echter Blick = -LastLook().
                        m_lastLook = ng::bsp::WarpDir(A, B, camera.forward());
                        m_lastUp = ng::bsp::WarpDir(A, B, camera.up());

                        m_teleported = true;
                        break;      // hoechstens ein Portal pro Frame
                    }
                }

                Vec3 eye = m_position;
                eye(1) += m_eyeHeight;
                camera.SetPosition(eye);
            }

            bool OnGround() const { return m_onGround; }
            const Vec3& FeetPosition() const { return m_position; }   // (eigentlich Mittelpunkt)

            // Portale + Schalter
            void SetWarpPortals(const std::vector<ng::bsp::WarpPortal<T>>* w)
            {
                m_warps = w;
                m_collision.SetWarpPortals(w);
            }

            inline void SetTeleportEnabled(bool e) { m_teleport = e; m_collision.portalsWalkable = e; }
            inline bool TeleportEnabled() const { return m_teleport; }

            // Wurde im letzten Update teleportiert? Dann Blickrichtung uebernehmen.
            inline bool Teleported() const { return m_teleported; }
            inline const Vec3& LastLook() const { return m_lastLook; }   // gewarptes forward()
            inline const Vec3& LastUp() const { return m_lastUp; }

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

            const std::vector<ng::bsp::WarpPortal<T>>* m_warps = nullptr;
            bool m_teleport = true;
            bool m_teleported = false;
            Vec3 m_lastLook{ 0, 0, 1 };
            Vec3 m_lastUp{ 0, 1, 0 };

            static constexpr T kMoveSpeed = (T)250;
            static constexpr T kJumpSpeed = (T)300;
            static constexpr T kMinPortalDist = (T)0.01;
        };
    }
}
