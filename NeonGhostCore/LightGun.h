#pragma once
#include <vector>
#include <cstddef>
#include "Matrix.h"
#include "BSP.h"

namespace ng::lightning {

    // LightT = dein DynLight (pos, color, intensity, radius, castShadow)

    template <class T, class LightT>
    class LightGun
    {
    public:
        using Vec3 = ng::math::Matrix<3, 1, T>;

        explicit LightGun(size_t ringSize = 16) : m_capacity(ringSize) { m_ring.reserve(ringSize); }

        // Einstellungen
        T speed = T(1200);        // Units/Sekunde
        T maxLife = T(3);         // Sekunden, danach verschwindet das Projektil
        T wallOffset = T(12);     // Abstand des Lichts von der Wand
        T radius = T(300);
        T intensity = T(1.2);
        bool shadows = false;     // bei vielen Lichtern teuer
        T eps = T(0.01);

        // Feuern: Position und Blickrichtung der Kamera
        void Fire(const Vec3& origin, const Vec3& dir, const Vec3& color)
        {
            Projectile p;
            p.pos = origin;
            p.dir = dir * (T(1) / dir.length());
            p.color = color;
            m_flying.push_back(p);
        }

        // Pro Frame aufrufen
        void Update(T dt, const ng::bsp::BSPTree<T>& tree)
        {
            for (size_t i = 0; i < m_flying.size();)
            {
                Projectile& p = m_flying[i];
                const T step = speed * dt;

                const auto hit = ng::bsp::BSPRayNearestHit(tree, p.pos, p.dir, step, eps);
                bool remove = false;

                if (hit.hit && hit.poly)
                {
                    Vec3 n = hit.poly->plane.normal();
                    n = n * (T(1) / n.length());
                    if ((n ^ p.dir) > T(0)) n = n * T(-1);        // Normale zur Kamera drehen

                    const Vec3 hp = p.pos + p.dir * hit.t;
                    Place(hp + n * wallOffset, p.color);
                    remove = true;
                }
                else
                {
                    p.pos = p.pos + p.dir * step;
                    p.age += dt;
                    if (p.age > maxLife) remove = true;
                }

                if (remove) { m_flying[i] = m_flying.back(); m_flying.pop_back(); }
                else ++i;
            }
        }

        // Alle Lichter fuer ApplyDynamicLights: Ring + fliegende Projektile
        void Gather(std::vector<LightT>& out) const
        {
            out.insert(out.end(), m_ring.begin(), m_ring.end());
            for (const auto& p : m_flying)
            {
                LightT l;
                l.pos = p.pos; l.color = p.color;
                l.intensity = intensity; l.radius = radius * T(0.6);
                l.castShadow = false;
                out.push_back(l);
            }
        }

        void Clear() { m_ring.clear(); m_flying.clear(); m_head = 0; }
        size_t Count() const { return m_ring.size(); }

    private:
        struct Projectile { Vec3 pos, dir, color; T age = 0; };

        void Place(const Vec3& pos, const Vec3& color)
        {
            LightT l;
            l.pos = pos; l.color = color;
            l.intensity = intensity; l.radius = radius;
            l.castShadow = shadows;

            if (m_ring.size() < m_capacity) m_ring.push_back(l);
            else { m_ring[m_head] = l; m_head = (m_head + 1) % m_capacity; }   // aeltestes ueberschreiben
        }

        size_t m_capacity;
        size_t m_head = 0;
        std::vector<LightT> m_ring;
        std::vector<Projectile> m_flying;
    };
}
