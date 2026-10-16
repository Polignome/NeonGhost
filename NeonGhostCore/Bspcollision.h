#pragma once

// ----------------------------------------------------------------------
// BSPCollision.h - Kollisionserkennung auf dem BSPTree
//
// 1. MoveEllipsoid: Spieler / Kamera als Ellipsoid (Collide-and-Slide nach
//    K. Fauerby, "Improved Collision detection and Response"). Liefert die neue
//    Position UND den Slide-Vektor (die tatsaechlich ausgefuehrte Verschiebung).
//
// 2. QuerySphere: Objekte als Kugel. Liefert alle Leaves, deren Zelle die Kugel
//    beruehrt, und alle Polygone, die die Kugel wirklich schneiden/beruehren
//    (mit naechstem Punkt und Abstand) - Grundlage fuer genauere Kollisionspunkte.
//
// Kollidiert wird mit den soliden Polygonen der Leaves (Leaf::GetPolygons()),
// also mit der Geometrie, die nach dem Flood Fill uebrig geblieben ist.
// Polygone sind EINSEITIG: nur Flaechen, die der Bewegung entgegen zeigen,
// blockieren. Der Spieler muss im begehbaren Bereich starten.
// ----------------------------------------------------------------------

#include "BSP.h"

#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdint>

namespace ng
{
    namespace bsp
    {
        namespace col_detail
        {
            template <class T>
            inline T Dot(const ng::math::Matrix<3, 1, T>& a, const ng::math::Matrix<3, 1, T>& b)
            {
                return a(0) * b(0) + a(1) * b(1) + a(2) * b(2);
            }

            // komponentenweise Multiplikation / Division
            template <class T>
            inline ng::math::Matrix<3, 1, T> MulC(const ng::math::Matrix<3, 1, T>& a, const ng::math::Matrix<3, 1, T>& b)
            {
                return ng::math::Matrix<3, 1, T>(a(0) * b(0), a(1) * b(1), a(2) * b(2));
            }

            template <class T>
            inline ng::math::Matrix<3, 1, T> DivC(const ng::math::Matrix<3, 1, T>& a, const ng::math::Matrix<3, 1, T>& b)
            {
                return ng::math::Matrix<3, 1, T>(a(0) / b(0), a(1) / b(1), a(2) / b(2));
            }

            // Kleinste positive Loesung von a*t^2 + b*t + c = 0, die kleiner als maxR ist.
            template <class T>
            inline bool LowestRoot(T a, T b, T c, T maxR, T& root)
            {
                if (std::abs(a) < T(1e-12)) return false;

                const T det = b * b - T(4) * a * c;
                if (det < T(0)) return false;

                const T sq = std::sqrt(det);
                T r1 = (-b - sq) / (T(2) * a);
                T r2 = (-b + sq) / (T(2) * a);
                if (r1 > r2) std::swap(r1, r2);

                if (r1 > T(0) && r1 < maxR) { root = r1; return true; }
                if (r2 > T(0) && r2 < maxR) { root = r2; return true; }
                return false;
            }

            // Punkt (bereits in der Ebene des Dreiecks) im Dreieck?
            template <class T>
            inline bool PointInTriangle(const ng::math::Matrix<3, 1, T>& p,
                const ng::math::Matrix<3, 1, T>& a,
                const ng::math::Matrix<3, 1, T>& b,
                const ng::math::Matrix<3, 1, T>& c)
            {
                const auto v0 = c - a;
                const auto v1 = b - a;
                const auto v2 = p - a;

                const T d00 = Dot(v0, v0);
                const T d01 = Dot(v0, v1);
                const T d02 = Dot(v0, v2);
                const T d11 = Dot(v1, v1);
                const T d12 = Dot(v1, v2);

                const T denom = d00 * d11 - d01 * d01;
                if (std::abs(denom) < T(1e-18)) return false;

                const T inv = T(1) / denom;
                const T u = (d11 * d02 - d01 * d12) * inv;
                const T v = (d00 * d12 - d01 * d02) * inv;

                const T tol = T(-1e-6);
                return u >= tol && v >= tol && (u + v) <= T(1) - tol;
            }

            // Naechster Punkt auf einem Dreieck (Ericson, Real-Time Collision Detection)
            template <class T>
            inline ng::math::Matrix<3, 1, T> ClosestPointOnTriangle(
                const ng::math::Matrix<3, 1, T>& p,
                const ng::math::Matrix<3, 1, T>& a,
                const ng::math::Matrix<3, 1, T>& b,
                const ng::math::Matrix<3, 1, T>& c)
            {
                const auto ab = b - a;
                const auto ac = c - a;
                const auto ap = p - a;

                const T d1 = Dot(ab, ap);
                const T d2 = Dot(ac, ap);
                if (d1 <= T(0) && d2 <= T(0)) return a;

                const auto bp = p - b;
                const T d3 = Dot(ab, bp);
                const T d4 = Dot(ac, bp);
                if (d3 >= T(0) && d4 <= d3) return b;

                const T vc = d1 * d4 - d3 * d2;
                if (vc <= T(0) && d1 >= T(0) && d3 <= T(0))
                {
                    const T v = d1 / (d1 - d3);
                    return a + ab * v;
                }

                const auto cp = p - c;
                const T d5 = Dot(ab, cp);
                const T d6 = Dot(ac, cp);
                if (d6 >= T(0) && d5 <= d6) return c;

                const T vb = d5 * d2 - d1 * d6;
                if (vb <= T(0) && d2 >= T(0) && d6 <= T(0))
                {
                    const T w = d2 / (d2 - d6);
                    return a + ac * w;
                }

                const T va = d3 * d6 - d5 * d4;
                if (va <= T(0) && (d4 - d3) >= T(0) && (d5 - d6) >= T(0))
                {
                    const T w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
                    return b + (c - b) * w;
                }

                const T denom = T(1) / (va + vb + vc);
                const T v = vb * denom;
                const T w = vc * denom;
                return a + ab * v + ac * w;
            }
        }

        // ------------------------------------------------------------
        // Ergebnisse
        // ------------------------------------------------------------

        template <class T>
        struct EllipsoidMoveResult
        {
            ng::math::Matrix<3, 1, T> position;    // neue Position (Mittelpunkt des Ellipsoids)
            ng::math::Matrix<3, 1, T> velocity;    // tatsaechlich ausgefuehrte Verschiebung (Slide-Vektor)
            ng::math::Matrix<3, 1, T> hitNormal;   // Normale der zuletzt getroffenen Flaeche (Welt), 0 ohne Kollision
            ng::math::Matrix<3, 1, T> hitPoint;    // Beruehrpunkt der zuletzt getroffenen Flaeche (Welt)
            bool collided = false;
            int iterations = 0;                    // Zahl der Slide-Schritte
        };

        template <class T>
        struct SphereHit
        {
            int32_t leaf = -1;                            // Leaf, in dem das Polygon liegt
            const BSPPolygon<T>* polygon = nullptr;       // gueltig, solange der Baum nicht veraendert wird
            ng::math::Matrix<3, 1, T> closestPoint;       // naechster Punkt des Polygons zum Kugelmittelpunkt
            T distance = T(0);                            // Abstand Mittelpunkt -> closestPoint
        };

        template <class T>
        struct SphereQueryResult
        {
            std::vector<int32_t> leaves;          // Leaves, deren Zelle die Kugel beruehrt (konservativ)
            std::vector<SphereHit<T>> hits;       // Polygone, die die Kugel wirklich beruehren
        };

        // ------------------------------------------------------------
        // BSPCollision
        // ------------------------------------------------------------

        template <class T>
        class BSPCollision
        {
        public:
            using Vec3 = ng::math::Matrix<3, 1, T>;

            explicit BSPCollision(BSPTree<T>& tree) : m_tree(tree) {}

            bool portalsWalkable = true;

            // Noetig, damit portalId -> WarpPortal aufgeloest werden kann
            void SetWarpPortals(const std::vector<WarpPortal<T>>* w) { m_warps = w; }

            // --------------------------------------------------------
            // MoveEllipsoid
            //
            //  position : Mittelpunkt des Ellipsoids (nicht die Augenhoehe!)
            //  velocity : gewuenschte Verschiebung in diesem Schritt (Geschwindigkeit * dt)
            //  radii    : Halbachsen in Weltunits, z.B. (16, 16, 36)
            //
            // Gleitet an Waenden entlang. Fuer Schwerkraft rufst du die Funktion ein
            // zweites Mal mit dem Schwerkraft-Vektor auf (mit der Position aus dem
            // ersten Aufruf); hitNormal zeigt dann, ob der Spieler auf dem Boden steht.
            //
            // maxIterations      : Zahl der Slide-Schritte (5 reicht meist)
            // veryCloseDistance  : Abstand zur Flaeche in Ellipsoid-Einheiten
            // --------------------------------------------------------
            EllipsoidMoveResult<T> MoveEllipsoid(const Vec3& position, const Vec3& velocity,
                const Vec3& radii, int maxIterations = 5, T veryCloseDistance = T(0.005))
            {
                using namespace col_detail;

                EllipsoidMoveResult<T> res;
                res.position = position;
                res.velocity = Vec3(0, 0, 0);
                res.hitNormal = Vec3(0, 0, 0);
                res.hitPoint = position;

                if (radii(0) <= T(0) || radii(1) <= T(0) || radii(2) <= T(0))
                    return res;

                const T velLenWorld = velocity.length();
                if (velLenWorld < T(1e-9))
                    return res;

                // 1. Kandidaten-Polygone: alles im Umkreis der Bewegung. Auch die
                //    spaeteren Slide-Schritte bleiben in dieser Kugel (plus Reserve).
                const T maxR = std::max(radii(0), std::max(radii(1), radii(2)));
                const T R = velLenWorld + T(2) * maxR;

                std::vector<const BSPPolygon<T>*> polys;
                CollectPolygonsInBox(position - Vec3(R, R, R), position + Vec3(R, R, R), polys);

                // 2. Dreiecke in den Ellipsoid-Raum (Ellipsoid = Einheitskugel)
                std::vector<Tri> tris;
                BuildTris(polys, radii, tris);

                // 3. Collide-and-Slide
                Vec3 curPos = DivC(position, radii);
                Vec3 curVel = DivC(velocity, radii);

                for (int iter = 0; iter < maxIterations; ++iter)
                {
                    const T velLen = curVel.length();
                    if (velLen < T(1e-9)) break;

                    Packet pk;
                    pk.base = curPos;
                    pk.vel = curVel;
                    pk.velLen = velLen;
                    pk.nvel = curVel * (T(1) / velLen);
                    pk.found = false;
                    pk.nearest = T(0);
                    pk.ip = Vec3(0, 0, 0);

                    for (const auto& tri : tris)
                        CheckTriangle(pk, tri);

                    if (!pk.found)
                    {
                        curPos = curPos + curVel;
                        break;
                    }

                    ++res.iterations;

                    const Vec3 dest = curPos + curVel;
                    Vec3 newBase = curPos;
                    Vec3 ip = pk.ip;

                    if (pk.nearest >= veryCloseDistance)
                    {
                        newBase = curPos + pk.nvel * (pk.nearest - veryCloseDistance);
                        ip = ip - pk.nvel * veryCloseDistance;
                    }

                    // Gleitebene: durch den Beruehrpunkt, Normale vom Beruehrpunkt zum Zentrum
                    Vec3 slideN = newBase - ip;
                    const T slideLen = slideN.length();

                    res.collided = true;

                    if (slideLen < T(1e-9))
                    {
                        curPos = newBase;
                        break;
                    }

                    slideN = slideN * (T(1) / slideLen);

                    // Normale und Beruehrpunkt zurueck in Weltkoordinaten
                    Vec3 wn = DivC(slideN, radii);
                    const T wl = wn.length();
                    if (wl > T(1e-12)) wn = wn * (T(1) / wl);
                    res.hitNormal = wn;
                    res.hitPoint = MulC(ip, radii);

                    const T distToPlane = Dot(dest - ip, slideN);
                    const Vec3 newDest = dest - slideN * distToPlane;
                    const Vec3 newVel = newDest - ip;

                    curPos = newBase;

                    if (newVel.length() < veryCloseDistance)
                        break;

                    curVel = newVel;
                }

                res.position = MulC(curPos, radii);
                res.velocity = res.position - position;
                return res;
            }

            // --------------------------------------------------------
            // QuerySphere
            //
            // Findet alles, was eine Kugel beruehrt:
            //  - out.leaves : Leaves, deren Zelle die Kugel beruehrt (konservativ)
            //  - out.hits   : Polygone (mit naechstem Punkt und Abstand), die die Kugel
            //                 wirklich beruehren, damit du danach genauere
            //                 Kollisionspunkte berechnen kannst.
            //
            // collectPolygons = false liefert nur die Leaves.
            // frontFacingOnly = true ignoriert Polygone, deren Rueckseite zur Kugel zeigt.
            // --------------------------------------------------------
            void QuerySphere(const Vec3& center, T radius, SphereQueryResult<T>& out,
                bool collectPolygons = true, bool frontFacingOnly = false)
            {
                using namespace col_detail;

                out.leaves.clear();
                out.hits.clear();

                Traverse(center,
                    [&](const Vec3& n) { return radius * n.length(); },
                    [&](BSPNode<T>* leaf)
                    {
                        out.leaves.push_back(leaf->GetID());

                        if (!collectPolygons) return;

                        for (const auto& poly : leaf->GetPolygons())
                        {
                            if (poly.verts.size() < 3) continue;
                            if (IsWalkablePortal(poly)) continue;

                            if (frontFacingOnly && poly.plane.distance(center) < T(0))
                                continue;

                            Vec3 cp;
                            T dist;
                            if (!ClosestPointOnPolygon(center, poly, cp, dist)) continue;
                            if (dist > radius) continue;

                            SphereHit<T> hit;
                            hit.leaf = leaf->GetID();
                            hit.polygon = &poly;
                            hit.closestPoint = cp;
                            hit.distance = dist;
                            out.hits.push_back(hit);
                        }
                    });
            }

        private:
            BSPTree<T>& m_tree;



            struct Tri
            {
                Vec3 p[3];      // Eckpunkte im Ellipsoid-Raum
                Vec3 n;         // Normale (zeigt zur Vorderseite)
                T d = T(0);     // Ebene: dot(n, x) = d
            };

            struct Packet
            {
                Vec3 base;      // Position (Ellipsoid-Raum)
                Vec3 vel;       // Geschwindigkeit (Ellipsoid-Raum)
                Vec3 nvel;      // normalisiert
                T velLen = T(0);
                bool found = false;
                T nearest = T(0);
                Vec3 ip;        // Beruehrpunkt
            };

            const std::vector<WarpPortal<T>>* m_warps = nullptr;

            // true, wenn das Polygon eine begehbare Portalflaeche ist
            inline bool IsWalkablePortal(const BSPPolygon<T>& poly) const
            {
                if (!portalsWalkable || !m_warps) return false;
                if (!(poly.flags & POLY_FLAG_PORTAL)) return false;
                if (poly.portalId < 0 || (size_t)poly.portalId >= m_warps->size()) return false;

                const auto& w = (*m_warps)[poly.portalId];
                return (w.flags & WARP_TELEPORT) && w.partner >= 0;
            }

            // Baum durchlaufen: Zellen, die von der Kugel/Box um "center" beruehrt werden.
            // extentFn(planeNormal) liefert die Ausdehnung des Koerpers entlang der Normale.
            template <class ExtentFn, class LeafFn>
            void Traverse(const Vec3& center, ExtentFn&& extentFn, LeafFn&& leafFn)
            {
                BSPNode<T>* root = m_tree.GetRoot().get();
                if (!root) return;

                std::vector<BSPNode<T>*> stack;
                stack.push_back(root);

                while (!stack.empty())
                {
                    BSPNode<T>* n = stack.back();
                    stack.pop_back();

                    if (!n) continue;

                    if (n->isLeaf())
                    {
                        leafFn(n);
                        continue;
                    }

                    const auto plane = n->GetPlane();
                    const Vec3 nn = plane.normal();
                    const T d = plane.distance(center);
                    const T r = extentFn(nn);

                    BSPNode<T>* f = n->GetFrontNode().get();
                    BSPNode<T>* b = n->GetBackNode().get();

                    if (d > r)        stack.push_back(f);
                    else if (d < -r)  stack.push_back(b);
                    else
                    {
                        stack.push_back(f);
                        stack.push_back(b);
                    }
                }
            }

            // Alle Polygone in Leaves, die die Box [mn, mx] beruehren (und deren eigene
            // Box die Box beruehrt).
            void CollectPolygonsInBox(const Vec3& mn, const Vec3& mx,
                std::vector<const BSPPolygon<T>*>& out)
            {
                const Vec3 center = (mn + mx) * T(0.5);
                const Vec3 half = (mx - mn) * T(0.5);

                Traverse(center,
                    [&](const Vec3& n)
                    {
                        return std::abs(n(0)) * half(0) + std::abs(n(1)) * half(1) + std::abs(n(2)) * half(2);
                    },
                    [&](BSPNode<T>* leaf)
                    {
                        for (const auto& poly : leaf->GetPolygons())
                        {
                            if (poly.verts.size() < 3) continue;
                            if (IsWalkablePortal(poly)) continue;

                            Vec3 pmn = poly.verts[0].world();
                            Vec3 pmx = pmn;
                            for (size_t i = 1; i < poly.verts.size(); ++i)
                            {
                                const Vec3 p = poly.verts[i].world();
                                for (int k = 0; k < 3; ++k)
                                {
                                    if (p(k) < pmn(k)) pmn(k) = p(k);
                                    if (p(k) > pmx(k)) pmx(k) = p(k);
                                }
                            }

                            if (pmx(0) < mn(0) || pmn(0) > mx(0) ||
                                pmx(1) < mn(1) || pmn(1) > mx(1) ||
                                pmx(2) < mn(2) || pmn(2) > mx(2))
                                continue;

                            out.push_back(&poly);
                        }
                    });
            }

            // Polygone (konvex) als Dreiecksfaecher in den Ellipsoid-Raum bringen.
            static void BuildTris(const std::vector<const BSPPolygon<T>*>& polys,
                const Vec3& radii, std::vector<Tri>& out)
            {
                using namespace col_detail;

                for (const BSPPolygon<T>* pp : polys)
                {
                    const auto& v = pp->verts;
                    if (v.size() < 3) continue;

                    // Normale einer Ebene transformiert sich mit "mal Radius"
                    Vec3 en = MulC(pp->plane.normal(), radii);
                    const T enl = en.length();
                    if (enl > T(1e-12)) en = en * (T(1) / enl);

                    const Vec3 a = DivC(v[0].world(), radii);

                    for (size_t i = 1; i + 1 < v.size(); ++i)
                    {
                        const Vec3 b = DivC(v[i].world(), radii);
                        const Vec3 c = DivC(v[i + 1].world(), radii);

                        Vec3 cn = (b - a) % (c - a);
                        const T len = cn.length();
                        if (len < T(1e-12)) continue;

                        cn = cn * (T(1) / len);

                        // Orientierung an der gespeicherten Polygon-Normale ausrichten
                        if (Dot(cn, en) < T(0)) cn = cn * T(-1);

                        Tri t;
                        t.p[0] = a;
                        t.p[1] = b;
                        t.p[2] = c;
                        t.n = cn;
                        t.d = Dot(cn, a);
                        out.push_back(t);
                    }
                }
            }

            // Sweep der Einheitskugel gegen ein Dreieck. Merkt sich die naechste Kollision.
            static void CheckTriangle(Packet& pk, const Tri& tri)
            {
                using namespace col_detail;

                // Nur Flaechen, die der Bewegung entgegen zeigen (einseitig)
                if (Dot(tri.n, pk.nvel) > T(0)) return;

                T t0 = T(0);
                T t1 = T(1);
                bool embedded = false;

                const T sd = Dot(tri.n, pk.base) - tri.d;
                const T ndv = Dot(tri.n, pk.vel);

                if (std::abs(ndv) < T(1e-12))
                {
                    // Bewegung parallel zur Ebene
                    if (std::abs(sd) >= T(1)) return;
                    embedded = true;
                }
                else
                {
                    t0 = (T(-1) - sd) / ndv;
                    t1 = (T(1) - sd) / ndv;
                    if (t0 > t1) std::swap(t0, t1);

                    if (t0 > T(1) || t1 < T(0)) return;

                    if (t0 < T(0)) t0 = T(0);
                    if (t1 > T(1)) t1 = T(1);
                }

                Vec3 cp(0, 0, 0);
                bool foundC = false;
                T t = T(1);

                // Treffer im Inneren des Dreiecks
                if (!embedded)
                {
                    const Vec3 pip = (pk.base - tri.n) + pk.vel * t0;
                    if (PointInTriangle(pip, tri.p[0], tri.p[1], tri.p[2]))
                    {
                        foundC = true;
                        t = t0;
                        cp = pip;
                    }
                }

                // Sonst Ecken und Kanten pruefen
                if (!foundC)
                {
                    const T vsq = Dot(pk.vel, pk.vel);
                    T newT = T(0);

                    for (int k = 0; k < 3; ++k)
                    {
                        const Vec3& P = tri.p[k];

                        const T a = vsq;
                        const T b = T(2) * Dot(pk.vel, pk.base - P);
                        const T c = Dot(P - pk.base, P - pk.base) - T(1);

                        if (LowestRoot(a, b, c, t, newT))
                        {
                            t = newT;
                            foundC = true;
                            cp = P;
                        }
                    }

                    for (int k = 0; k < 3; ++k)
                    {
                        const Vec3& e0 = tri.p[k];
                        const Vec3& e1 = tri.p[(k + 1) % 3];

                        const Vec3 edge = e1 - e0;
                        const Vec3 btv = e0 - pk.base;

                        const T esq = Dot(edge, edge);
                        if (esq < T(1e-12)) continue;

                        const T edv = Dot(edge, pk.vel);
                        const T edb = Dot(edge, btv);

                        const T a = esq * (-vsq) + edv * edv;
                        const T b = esq * (T(2) * Dot(pk.vel, btv)) - T(2) * edv * edb;
                        const T c = esq * (T(1) - Dot(btv, btv)) + edb * edb;

                        if (LowestRoot(a, b, c, t, newT))
                        {
                            const T f = (edv * newT - edb) / esq;
                            if (f >= T(0) && f <= T(1))
                            {
                                t = newT;
                                foundC = true;
                                cp = e0 + edge * f;
                            }
                        }
                    }
                }

                if (foundC)
                {
                    const T dist = t * pk.velLen;
                    if (!pk.found || dist < pk.nearest)
                    {
                        pk.found = true;
                        pk.nearest = dist;
                        pk.ip = cp;
                    }
                }
            }

            // Naechster Punkt eines (konvexen) Polygons zu einem Punkt.
            static bool ClosestPointOnPolygon(const Vec3& p, const BSPPolygon<T>& poly,
                Vec3& closest, T& dist)
            {
                using namespace col_detail;

                const auto& v = poly.verts;
                if (v.size() < 3) return false;

                const Vec3 a = v[0].world();
                T bestSq = std::numeric_limits<T>::max();
                bool any = false;

                for (size_t i = 1; i + 1 < v.size(); ++i)
                {
                    const Vec3 cp = ClosestPointOnTriangle(p, a, v[i].world(), v[i + 1].world());
                    const Vec3 d = cp - p;
                    const T sq = Dot(d, d);

                    if (sq < bestSq)
                    {
                        bestSq = sq;
                        closest = cp;
                        any = true;
                    }
                }

                if (!any) return false;

                dist = std::sqrt(bestSq);
                return true;
            }
        };

    } // namespace bsp
} // namespace ng