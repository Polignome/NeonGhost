#pragma once

#include "Matrix.h"
#include "Camera.h"
#include "CameraController.h"

#include <cmath>
#include <algorithm>

// -------------------------------------------------------------
// ViewMotion - Kamera-Bewegungsanimation (rein optisch)
//
//   * Auf-/Abwippen und leichtes Pendeln beim Laufen (Head-Bob)
//   * Seitliches Neigen (Roll) bei Seitwaertsbewegung und Drehung
//     -> schwingt mit einer Feder nach, auch bei abrupten Richtungswechseln
//   * Eintauchen der Kamera beim Landen
//
// Die Physik wird NICHT veraendert: PlayerController setzt die Kamera jeden Frame
// neu auf die Augenposition, ViewMotion legt danach nur einen Versatz darauf.
//
// Einbau pro Frame, in dieser Reihenfolge:
//
//     controller.Update(camera, dt, wishX, wishZ, jump);   // setzt Kameraposition
//     viewMotion.Update(dt, controller, camera);           // berechnet Versatz/Roll
//     viewMotion.Apply(camera, cameraController);          // legt ihn auf die Kamera
//
// Optional: bei Mausdrehung viewMotion.AddTurn(yawDeltaGrad) aufrufen (gleicher Wert,
// der an CameraController::Turn als rightAngle geht). Dann neigt sich die Kamera
// auch beim schnellen Drehen leicht.
// -------------------------------------------------------------
namespace ng {
    namespace controller {

        template <class T>
        class ViewMotion
        {
        public:
            using Vec3 = ng::math::Matrix<3, 1, T>;

            struct Config
            {
                T speedRef = (T)250;        // Laufgeschwindigkeit, bei der die Effekte voll wirken
                T stepLength = (T)90;       // Units pro Schritt
                T bobUp = (T)1.4;           // vertikaler Ausschlag (Units)
                T bobSide = (T)0.8;         // seitliches Pendeln (Units)
                T ampResponse = (T)12;      // wie schnell die Wipp-Staerke folgt (1/s)

                T maxRollDeg = (T)1.8;      // Neigung bei voller Seitwaertsgeschwindigkeit
                T turnRollDeg = (T)1.2;     // Neigung bei sehr schnellem Drehen (360 Grad/s)
                T rollOmega = (T)15;        // Federfrequenz des Neigens (rad/s)
                T rollDamping = (T)0.55;    // < 1 = schwingt nach, 1 = kriechend, 0 = endlos

                T landDip = (T)0.18;        // Units/s Anstoss pro Units/s Aufprall
                T landMaxImpact = (T)700;
                T landOmega = (T)14;
                T landDamping = (T)0.5;

                // -1 = Kamera neigt sich IN die Bewegungs-/Drehrichtung (rechts laufen -> nach
                // rechts kippen). Gefaellt dir die Gegenrichtung besser: auf +1 stellen.
                T rollSign = (T)-1;
                T turnRollSign = (T)-1;
            };

            Config cfg;

            // Drehung dieses Frames in Grad (positiv = nach rechts, wie CameraController::Turn)
            inline void AddTurn(T yawDeltaDeg) { m_turnAccum += yawDeltaDeg; }

            // Alles auf Null, z.B. nach Teleport oder Respawn.
            void Reset()
            {
                m_phase = m_amp = (T)0;
                m_roll = m_rollVel = (T)0;
                m_land = m_landVel = (T)0;
                m_turnAccum = (T)0;
                m_offset = Vec3(0, 0, 0);
            }

            // Bequeme Variante: holt alles vom PlayerController.
            template <class P>
            void Update(T dt, const P& player, const ng::scene::Camera<T>& camera)
            {
                Update(dt,
                    player.ActualVelocity(),
                    camera.right(),
                    player.OnGround(),
                    player.LandImpact(),
                    player.ViewBobScale(),
                    player.IsEnabled());
            }

            void Update(T dt, Vec3 velocity, Vec3 right, bool onGround,
                T landImpact, T scale, bool active)
            {
                if (!(dt > (T)0)) return;
                dt = std::min(dt, (T)0.1);       // Framespitzen nicht in die Feder schicken

                if (!active)
                {
                    Reset();
                    return;
                }

                velocity(1) = 0;
                right(1) = 0;
                if (right.length() > (T)1e-6) right.normalize();

                const T speed = velocity.length();
                const T speedN = std::min(speed / cfg.speedRef, (T)1.2);

                // ---------- Wippen ----------
                const T targetAmp = onGround ? speedN * scale : (T)0;
                m_amp += (targetAmp - m_amp) * ((T)1 - std::exp(-cfg.ampResponse * dt));

                if (onGround)
                {
                    m_phase += (speed * dt / cfg.stepLength) * kPi;     // pi pro Schritt
                    if (m_phase > (T)2 * kPi) m_phase -= (T)2 * kPi;
                }

                const T bobY = std::sin((T)2 * m_phase) * cfg.bobUp * m_amp;
                const T bobX = std::sin(m_phase) * cfg.bobSide * m_amp;

                // ---------- Landen ----------
                if (landImpact > (T)0)
                {
                    const T imp = std::min(landImpact, cfg.landMaxImpact);
                    m_landVel -= imp * cfg.landDip * scale;
                }
                {
                    const T w2 = cfg.landOmega * cfg.landOmega;
                    const T acc = -w2 * m_land - (T)2 * cfg.landDamping * cfg.landOmega * m_landVel;
                    m_landVel += acc * dt;
                    m_land += m_landVel * dt;
                }

                // ---------- Neigen (Roll) ----------
                const T vLat = (velocity ^ right) / cfg.speedRef;                 // -1..1 (ca.)
                const T turnRate = m_turnAccum / dt;                              // Grad/s
                m_turnAccum = (T)0;

                const T maxRoll = cfg.maxRollDeg * kDeg2Rad;
                const T turnRoll = cfg.turnRollDeg * kDeg2Rad;

                T target = cfg.rollSign * std::clamp(vLat, (T)-1, (T)1) * maxRoll
                    + cfg.turnRollSign * std::clamp(turnRate / (T)360, (T)-1, (T)1) * turnRoll;
                target *= (onGround ? (T)1 : (T)0.5) * scale;

                {
                    const T w2 = cfg.rollOmega * cfg.rollOmega;
                    const T acc = w2 * (target - m_roll) - (T)2 * cfg.rollDamping * cfg.rollOmega * m_rollVel;
                    m_rollVel += acc * dt;
                    m_roll += m_rollVel * dt;
                }

                m_offset = Vec3(0, 1, 0) * (bobY + m_land) + right * bobX;
            }

            // Versatz auf die Kamera legen. Aufruf direkt NACH PlayerController::Update.
            void Apply(ng::scene::Camera<T>& camera, CameraController<T>& cameraController) const
            {
                camera.SetPosition(camera.position() + m_offset);
                cameraController.SetRoll(camera, m_roll);
            }

            inline const Vec3& Offset() const { return m_offset; }
            inline T Roll() const { return m_roll; }          // Bogenmass

        private:
            static constexpr T kPi = (T)3.14159265358979323846;
            static constexpr T kDeg2Rad = (T)0.017453292519943295;

            T m_phase = 0;
            T m_amp = 0;
            T m_roll = 0, m_rollVel = 0;
            T m_land = 0, m_landVel = 0;
            T m_turnAccum = 0;
            Vec3 m_offset{ 0, 0, 0 };
        };
    }
}