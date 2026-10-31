#pragma once

#include "Camera.h"
#include "BSPCollision.h"

#include <vector>
#include <cmath>
#include <algorithm>

// -------------------------------------------------------------
// Player-Wrapper: haelt Ellipsoid-Position getrennt von der Kamera
// und schiebt jede Bewegung durch den BSP.
//
// Konventionen:
//  - Y ist oben
//  - m_position = Ellipsoid-MITTELPUNKT, Auge = m_position + eyeHeight
//  - camera.forward() ist die RUECKWAERTS-Achse (echter Blick = -forward())
//
// Portal-Durchgang (NEU):
//  Teleportiert wird weiterhin, wenn das AUGE die Portalebene durchquert (so bleibt
//  das Bild ohne Sprung). Das Auge sitzt aber 28 Units ueber dem Mittelpunkt und der
//  Koerper hat Ausdehnung. Bei Boden-/Schraegportalen blieb der Koerper deshalb an der
//  Nische/Rueckwand haengen, bevor das Auge die Ebene erreichte. Darum gibt es jetzt
//  zwei Phasen, in denen die Geometrie HINTER dem Portal ignoriert wird (Ghosting):
//
//   Dive : Der Koerper ueberlappt eine begehbare Portalflaeche und bewegt sich darauf zu.
//          Mindest-Tauchgeschwindigkeit, bis das Auge die Ebene durchquert.
//          Abbruch (mit Heraus-Schieben) bei Timeout oder seitlichem Verlassen der Oeffnung.
//   Exit : Nach dem Teleport sitzt das Auge direkt vor dem Zielportal, der Koerper steckt
//          aber noch dahinter. Er wird kinematisch aus der Oeffnung geschoben (ohne
//          Schwerkraft), und das Zielportal teleportiert in dieser Zeit nicht zurueck.
// -------------------------------------------------------------
namespace ng {
    namespace controller {
        template <class T>
        class PlayerController
        {
        public:
            using Vec3 = ng::math::Matrix<3, 1, T>;

            // Stellschrauben fuer den Portal-Durchgang (Units bzw. Sekunden)
            struct PortalTuning
            {
                T diveSpeed = (T)120;       // Mindestgeschwindigkeit in das Portal hinein
                T exitSpeed = (T)260;       // Schub aus dem Zielportal heraus
                T rimTolerance = (T)8;      // Toleranz am Rand der Oeffnung
                T enterMargin = (T)1;       // Eintritt, wenn Koerper die Ebene (fast) beruehrt
                T approachMin = (T)0.5;     // Mindest-Annaeherungsgeschwindigkeit
                T diveTimeout = (T)1.0;     // danach Abbruch + Heraus-Schieben
                T exitTimeout = (T)1.0;     // Sicherung gegen endloses Schieben
                T cooldown = (T)0.5;        // Sperre fuer erneutes Eintauchen nach Abbruch
                T ghostInflate = (T)80;     // Reichweite des Ghostings um die Oeffnung
                T bobFadeDist = (T)24;      // View-Bob blendet in dieser Naehe zu Portalen aus
            };

            PortalTuning portalTuning;

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
                m_actualVel = Vec3(0, 0, 0);
                m_onGround = false;
                m_initialized = true;
                ResetPortalState();
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
            inline void SetEnabled(bool enabled)
            {
                m_enabled = enabled;
                if (!enabled) ResetPortalState();
            }
            inline bool IsEnabled() const { return m_enabled; }

            // Freier Vektor (x,y,z) in Units/s^2, wirkt nur wenn Kollision aktiv ist.
            inline void SetGravity(const Vec3& g) { m_gravity = g; }
            inline const Vec3& Gravity() const { return m_gravity; }

            void Update(ng::scene::Camera<T>& camera, T dt, T wishX, T wishZ, bool jump)
            {
                if (!m_initialized) return;

                m_teleported = false;
                m_landImpact = (T)0;

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
                    m_actualVel = Vec3(0, 0, 0);
                    m_onGround = false;
                    m_bobScale = (T)1;

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

                // ---- Portal-Phasen (Dive / Exit): kann Geschwindigkeit aendern,
                //      liefert das zu ignorierende Portal und einen kinematischen Schub.
                const ng::bsp::WarpPortal<T>* ghost = nullptr;
                Vec3 push(0, 0, 0);
                if (m_teleport && m_warps)
                    UpdatePortalPhase(dt, ghost, push);
                else
                    m_phase = Phase::None;

                m_collision.SetGhostPortal(ghost, portalTuning.ghostInflate);

                const Vec3 desired = (m_velocity + push) * dt;
                const Vec3 moveH(desired(0), (T)0, desired(2));
                const Vec3 moveV((T)0, desired(1), (T)0);

                const Vec3 eyeOffset(0, m_eyeHeight, 0);
                const Vec3 eyeOld = m_position + eyeOffset;     // Auge VOR der Bewegung
                const Vec3 posOld = m_position;
                const bool wasOnGround = m_onGround;
                const T vyBefore = m_velocity(1);

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

                // tatsaechliche Bewegung (fuer View-Bob) und Landung
                if (dt > (T)1e-6)
                    m_actualVel = (m_position - posOld) * ((T)1 / dt);

                if (!wasOnGround && m_onGround && vyBefore < 0)
                    m_landImpact = -vyBefore;

                // ---- Portal-Teleport: mit dem AUGE testen, weil die Kamera dort sitzt.
                if (m_teleport && m_warps)
                {
                    const Vec3 eyeNew = m_position + eyeOffset;

                    for (size_t ai = 0; ai < m_warps->size(); ++ai)
                    {
                        const auto& A = (*m_warps)[ai];

                        if (!(A.flags & ng::bsp::WARP_TELEPORT) || A.partner < 0) continue;
                        if ((size_t)A.partner >= m_warps->size()) continue;

                        // Waehrend des Austritts nicht sofort durch das Zielportal zurueck.
                        if (m_phase == Phase::Exit && (int32_t)ai == m_phasePortal) continue;

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

                        // Austritt: Koerper steckt noch hinter B -> kinematisch herausschieben.
                        m_phase = Phase::Exit;
                        m_phasePortal = A.partner;
                        m_phaseTime = (T)0;
                        m_onGround = false;

                        m_teleported = true;
                        break;      // hoechstens ein Portal pro Frame
                    }
                }

                // View-Bob: in der Naehe von Portalen ausblenden, damit die Kamera nicht
                // durch einen Versatz hinter die Portalebene rutscht.
                UpdateBobScale(m_position + eyeOffset);

                Vec3 eye = m_position;
                eye(1) += m_eyeHeight;
                camera.SetPosition(eye);
            }

            bool OnGround() const { return m_onGround; }
            const Vec3& FeetPosition() const { return m_position; }   // (eigentlich Mittelpunkt)

            // ---- NEU: Daten fuer ViewMotion (Kopf-Wippen / Schwingen) ----
            // Tatsaechliche Geschwindigkeit im letzten Update (nach Kollision), Units/s.
            inline const Vec3& ActualVelocity() const { return m_actualVel; }
            // Aufprallgeschwindigkeit, wenn im letzten Update gelandet wurde, sonst 0.
            inline T LandImpact() const { return m_landImpact; }
            // 0..1: Staerke, mit der Kamera-Effekte wirken sollen (0 nahe/in Portalen).
            inline T ViewBobScale() const { return m_bobScale; }
            // Gerade im Portal-Durchgang?
            inline bool InPortalTransit() const { return m_phase != Phase::None; }

            // Portale + Schalter
            void SetWarpPortals(const std::vector<ng::bsp::WarpPortal<T>>* w)
            {
                m_warps = w;
                m_collision.SetWarpPortals(w);
                ResetPortalState();
            }

            inline void SetTeleportEnabled(bool e)
            {
                m_teleport = e;
                m_collision.portalsWalkable = e;
                if (!e) ResetPortalState();
            }
            inline bool TeleportEnabled() const { return m_teleport; }

            // Wurde im letzten Update teleportiert? Dann Blickrichtung uebernehmen.
            inline bool Teleported() const { return m_teleported; }
            inline const Vec3& LastLook() const { return m_lastLook; }   // gewarptes forward()
            inline const Vec3& LastUp() const { return m_lastUp; }

        private:
            enum class Phase { None, Dive, Exit };

            ng::bsp::BSPCollision<T> m_collision;

            Vec3 m_position{ 0, 0, 0 };
            Vec3 m_velocity{ 0, 0, 0 };
            Vec3 m_actualVel{ 0, 0, 0 };
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

            // Portal-Durchgang
            Phase m_phase = Phase::None;
            int32_t m_phasePortal = -1;
            T m_phaseTime = 0;
            int32_t m_cooldownPortal = -1;
            T m_cooldownTime = 0;

            T m_landImpact = 0;
            T m_bobScale = 1;

            static constexpr T kMoveSpeed = (T)250;
            static constexpr T kJumpSpeed = (T)300;
            static constexpr T kMinPortalDist = (T)0.01;
            static constexpr T kClearEps = (T)4;     // Hysterese: "ganz vor dem Portal"

            void ResetPortalState()
            {
                m_phase = Phase::None;
                m_phasePortal = -1;
                m_phaseTime = 0;
                m_cooldownPortal = -1;
                m_cooldownTime = 0;
                m_collision.SetGhostPortal(nullptr);
            }

            // Ausdehnung des Ellipsoids entlang einer Richtung (Support-Funktion)
            inline T SupportAlong(const Vec3& n) const
            {
                const T a = m_radii(0) * n(0);
                const T b = m_radii(1) * n(1);
                const T c = m_radii(2) * n(2);
                return std::sqrt(a * a + b * b + c * c);
            }

            static inline T SignedDist(const ng::bsp::WarpPortal<T>& w, const Vec3& p)
            {
                return (p - w.origin) ^ w.normal;
            }

            // Liegt p, auf die Portalebene projiziert, in der Oeffnung (plus Toleranz)?
            static bool InsideRect(const ng::bsp::WarpPortal<T>& w, const Vec3& p, T tol)
            {
                const size_t n = w.verts.size();
                if (n < 3) return false;

                const Vec3 q = p - w.normal * SignedDist(w, p);

                for (size_t i = 0; i < n; ++i)
                {
                    const Vec3 a = w.verts[i];
                    const Vec3 b = w.verts[(i + 1) % n];
                    const Vec3 e = b - a;
                    const T len = e.length();
                    if (len < (T)1e-9) continue;

                    // >= 0 : innen (gleiche Orientierung wie ng::bsp::CrossedPortal)
                    const T d = (((e % (q - a)) ^ w.normal)) / len;
                    if (d < -tol) return false;
                }
                return true;
            }

            inline bool IsWalkable(const ng::bsp::WarpPortal<T>& w) const
            {
                return (w.flags & ng::bsp::WARP_TELEPORT) && w.partner >= 0 &&
                    (size_t)w.partner < m_warps->size();
            }

            // Koerper vor das Portal zurueckschieben (Abbruch eines Tauchgangs)
            void EjectInFront(const ng::bsp::WarpPortal<T>& W, int32_t index)
            {
                const T dc = SignedDist(W, m_position);
                const T sup = SupportAlong(W.normal);
                if (dc < sup + kClearEps)
                    m_position = m_position + W.normal * (sup + kClearEps - dc);

                m_velocity(1) = 0;
                m_cooldownPortal = index;
                m_cooldownTime = portalTuning.cooldown;
            }

            void UpdatePortalPhase(T dt, const ng::bsp::WarpPortal<T>*& ghost, Vec3& push)
            {
                const auto& warps = *m_warps;
                const PortalTuning& tn = portalTuning;

                if (m_cooldownTime > 0) m_cooldownTime -= dt;

                // -------- laufende Phase --------
                if (m_phase != Phase::None)
                {
                    if (m_phasePortal < 0 || (size_t)m_phasePortal >= warps.size())
                    {
                        m_phase = Phase::None;
                        return;
                    }

                    const auto& W = warps[m_phasePortal];
                    m_phaseTime += dt;

                    const T dc = SignedDist(W, m_position);
                    const T sup = SupportAlong(W.normal);

                    if (m_phase == Phase::Exit)
                    {
                        if (dc >= sup + kClearEps || m_phaseTime > tn.exitTimeout)
                        {
                            // frei: Schwung der Schubrichtung mitnehmen, dann normale Physik
                            m_velocity(1) = W.normal(1) * tn.exitSpeed;
                            m_phase = Phase::None;
                            return;
                        }

                        ghost = &W;
                        push = W.normal * tn.exitSpeed;
                        m_velocity(1) = 0;          // Schwerkraft dieses Frames zuruecknehmen
                        return;
                    }

                    // Phase::Dive
                    if (dc >= sup + kClearEps)
                    {
                        m_phase = Phase::None;      // wieder ganz davor (z.B. rausgesprungen)
                        return;
                    }

                    if (!InsideRect(W, m_position, tn.rimTolerance) || m_phaseTime > tn.diveTimeout)
                    {
                        EjectInFront(W, m_phasePortal);
                        m_phase = Phase::None;
                        return;
                    }

                    ghost = &W;
                    ApplyDiveAssist(W);
                    return;
                }

                // -------- neue Phase? --------
                for (size_t i = 0; i < warps.size(); ++i)
                {
                    const auto& W = warps[i];
                    if (!IsWalkable(W)) continue;
                    if ((int32_t)i == m_cooldownPortal && m_cooldownTime > 0) continue;

                    const T vn = m_velocity ^ W.normal;
                    if (vn > -tn.approachMin) continue;             // nicht auf das Portal zu

                    const T dc = SignedDist(W, m_position);
                    const T sup = SupportAlong(W.normal);

                    // Koerper beruehrt die Ebene (oder erreicht sie in diesem Frame)
                    if (dc > sup + tn.enterMargin + (-vn) * dt) continue;
                    if (dc < -sup) continue;                        // schon ganz dahinter

                    if (!InsideRect(W, m_position, tn.rimTolerance)) continue;

                    m_phase = Phase::Dive;
                    m_phasePortal = (int32_t)i;
                    m_phaseTime = (T)0;
                    ghost = &W;
                    ApplyDiveAssist(W);
                    return;
                }
            }

            // Mindestgeschwindigkeit in das Portal hinein, damit das Auge die Ebene sicher erreicht.
            void ApplyDiveAssist(const ng::bsp::WarpPortal<T>& W)
            {
                const T vn = m_velocity ^ W.normal;
                if (vn > -portalTuning.diveSpeed)
                    m_velocity = m_velocity - W.normal * (vn + portalTuning.diveSpeed);
            }

            void UpdateBobScale(const Vec3& eye)
            {
                m_bobScale = (T)1;

                if (m_phase != Phase::None) { m_bobScale = (T)0; return; }
                if (!m_teleport || !m_warps) return;

                const T fade = portalTuning.bobFadeDist;
                for (const auto& W : *m_warps)
                {
                    if (!IsWalkable(W)) continue;

                    const T d = std::abs(SignedDist(W, eye));
                    if (d >= fade) continue;
                    if (!InsideRect(W, eye, fade)) continue;

                    m_bobScale = std::min(m_bobScale, d / fade);
                }
            }
        };
    }
}