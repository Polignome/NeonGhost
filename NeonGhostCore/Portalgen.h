#pragma once

// =====================================================================
// PortalGen.h
//
// Portal-Generierung fuer den BSP-Baum.
//
// Plane wurde vollstaendig durch
// ng::math::RayPlaneLine<3,T> ersetzt.
// =====================================================================

#include "BSPBuilder.h"
#include "RayPlaneLine.h"

#include <vector>
#include <memory>
#include <cmath>
#include <algorithm>

namespace ng
{
    namespace bsp
    {
        // ------------------------------------------------------------
        // Portal
        // ------------------------------------------------------------

        template<class T>
        struct Portal
        {
            using Vertex = ng::geometry::Vertex<T>;
            using Plane = ng::math::RayPlaneLine<3, T>;

            std::vector<Vertex> winding;

            Plane plane;

            // nodes[0] = Seite in Richtung plane.normal()
            // nodes[1] = Gegenseite
            //
            // nullptr = Aussenraum
            BSPNode<T>* nodes[2] = { nullptr, nullptr };
        };


        // ------------------------------------------------------------
        // Ebenen-Hilfsfunktionen
        // ------------------------------------------------------------
        /*
        template<class T>
        inline T PlaneDistance(
            const ng::math::RayPlaneLine<3, T>& plane,
            const ng::math::Matrix<3, 1, T>& point)
        {
            return (point ^ plane.normal()) + plane.D();
        }
        */

        template<class T>
        inline ng::math::RayPlaneLine<3, T> MakePlane(
            const ng::math::Matrix<3, 1, T>& origin,
            const ng::math::Matrix<3, 1, T>& normal)
        {
            return ng::math::RayPlaneLine<3, T>(origin, normal);
        }


        // ------------------------------------------------------------
        // Klassifiziert eine rohe Vertex-Liste gegen eine Ebene.
        //
        // Wir benutzen hier bewusst RayPlaneLine::classify().
        // ------------------------------------------------------------

        template<class T>
        inline int ClassifyWinding(
            const ng::math::RayPlaneLine<3, T>& plane,
            const std::vector<ng::geometry::Vertex<T>>& winding,
            T epsilon)
        {
            bool front = false;
            bool back = false;

            for (const auto& v : winding)
            {
                auto const res = plane.classify(v.world());

                if (res == ng::math::RayPlaneLine<3, T>::FRONT) front = true;
                else if (res == ng::math::RayPlaneLine<3, T>::BACK) back = true;

                if (front && back)
                    return ng::math::RayPlaneLine<3, T>::SPANNING;
            }

            if (front)
                return ng::math::RayPlaneLine<3, T>::FRONT;

            if (back)
                return ng::math::RayPlaneLine<3, T>::BACK;

            return ng::math::RayPlaneLine<3, T>::PLANAR;
        }


        // ------------------------------------------------------------
        // Einseitiges Clipping
        // ------------------------------------------------------------

        template<class T>
        inline std::vector<ng::geometry::Vertex<T>> ClipWindingToPlane(
            const std::vector<ng::geometry::Vertex<T>>& winding,
            const ng::math::RayPlaneLine<3, T>& plane,
            bool keepFront,
            T epsilon)
        {
            std::vector<ng::geometry::Vertex<T>> out;

            const size_t n = winding.size();

            if (n < 2)
                return out;

            for (size_t i = 0; i < n; ++i)
            {
                const auto& curr = winding[i];
                const auto& next = winding[(i + 1) % n];

                T dCurr = PlaneDistance(plane, curr.world());
                T dNext = PlaneDistance(plane, next.world());

                T sCurr = keepFront ? dCurr : -dCurr;
                T sNext = keepFront ? dNext : -dNext;

                bool currIn = sCurr >= -epsilon;
                bool nextIn = sNext >= -epsilon;

                if (currIn)
                    out.push_back(curr);

                if (currIn != nextIn)
                {
                    T denom = dCurr - dNext;

                    if (std::abs(denom) > static_cast<T>(1e-12))
                    {
                        T t = dCurr / denom;
                        out.push_back(LerpVertex(curr, next, t));
                    }
                }
            }

            return out;
        }


        // ------------------------------------------------------------
        // Split Winding
        //
        // RayPlaneLine besitzt bereits split(), aber wir brauchen
        // Vertex-Listen und deshalb bleibt der Wrapper hier sinnvoll.
        // ------------------------------------------------------------

        template<class T>
        inline void SplitWinding(
            const ng::math::RayPlaneLine<3, T>& plane,
            const std::vector<ng::geometry::Vertex<T>>& winding,
            T epsilon,
            std::vector<ng::geometry::Vertex<T>>& frontOut,
            std::vector<ng::geometry::Vertex<T>>& backOut)
        {
            frontOut.clear();
            backOut.clear();

            const size_t n = winding.size();

            if (n < 3)
                return;

            for (size_t i = 0; i < n; ++i)
            {
                const auto& curr = winding[i];
                const auto& next = winding[(i + 1) % n];



                T dCurr = plane.distance(curr.world());
                T dNext = plane.distance(next.world());

                bool currFront = dCurr >= -epsilon;
                bool nextFront = dNext >= -epsilon;

                bool currBack = dCurr <= epsilon;
                bool nextBack = dNext <= epsilon;

                if (currFront)
                    frontOut.push_back(curr);

                if (currBack)
                    backOut.push_back(curr);

                if ((currFront && !nextFront) ||
                    (!currFront && nextFront))
                {
                    T denom = dCurr - dNext;

                    if (std::abs(denom) > static_cast<T>(1e-12))
                    {
                        T t = dCurr / denom;

                        auto intersection =
                            LerpVertex(curr, next, t);

                        frontOut.push_back(intersection);
                        backOut.push_back(intersection);
                    }
                }
            }
        }


        // ------------------------------------------------------------
        // Konvexes Polygon von anderem Polygon abziehen.
        // ------------------------------------------------------------
        /*
        template<class T>
        inline std::vector<std::vector<ng::geometry::Vertex<T>>>
            SubtractConvexPolygon(
                const std::vector<ng::geometry::Vertex<T>>& basePoly,
                const std::vector<ng::geometry::Vertex<T>>& subtractPoly,
                const ng::math::RayPlaneLine<3, T>& facePlane,
                T epsilon)
        {
            using Vec3 = ng::math::Matrix<3, 1, T>;
            using Vertex = ng::geometry::Vertex<T>;
            using Plane = ng::math::RayPlaneLine<3, T>;

            std::vector<std::vector<Vertex>> result;

            if (basePoly.size() < 3 ||
                subtractPoly.size() < 3)
            {
                result.push_back(basePoly);
                return result;
            }

            const size_t k = subtractPoly.size();

            std::vector<Plane> edgePlanes;
            edgePlanes.reserve(k);

            for (size_t i = 0; i < k; ++i)
            {
                const Vec3& a = subtractPoly[i].world();
                const Vec3& b = subtractPoly[(i + 1) % k].world();

                Vec3 edgeDir = b - a;

                //Vec3 n = edgeDir % facePlane.normal();
                Vec3 n = facePlane.normal() % edgeDir;   // N × edgeDir = nach innen
                T len = n.length();

                if (len > static_cast<T>(1e-12))
                    n *= static_cast<T>(1) / len;

                Plane ep(a, n);

                edgePlanes.push_back(ep);
            }

            std::vector<Vertex> remaining = basePoly;

            for (size_t i = 0; i < k; ++i)
            {
                if (remaining.size() < 3)
                    break;

                // Aussenseite der aktuellen Kante.
                auto outside =
                    ClipWindingToPlane(
                        remaining,
                        edgePlanes[i],
                        false,
                        epsilon);

                if (outside.size() >= 3)
                    result.push_back(std::move(outside));

                // Fuer die naechste Kante nur das bisherige Innere.
                remaining =
                    ClipWindingToPlane(
                        remaining,
                        edgePlanes[i],
                        true,
                        epsilon);
            }

            return result;
        }
        */

        template<class T>
        inline std::vector<std::vector<ng::geometry::Vertex<T>>>
            SubtractConvexPolygon(
                const std::vector<ng::geometry::Vertex<T>>& basePoly,
                const std::vector<ng::geometry::Vertex<T>>& subtractPoly,
                const ng::math::RayPlaneLine<3, T>& facePlane,
                T epsilon)
        {
            using Vec3 = ng::math::Matrix<3, 1, T>;
            using Vertex = ng::geometry::Vertex<T>;
            using Plane = ng::math::RayPlaneLine<3, T>;

            std::vector<std::vector<Vertex>> result;

            if (basePoly.size() < 3 ||
                subtractPoly.size() < 3)
            {
                result.push_back(basePoly);
                return result;
            }

            // ----------------------------------------------------------
            // NEU: Windung von subtractPoly relativ zu facePlane
            // normalisieren. Wir berechnen die tatsaechliche Normale des
            // subtractPoly ueber Newell's method und vergleichen sie mit
            // facePlane.normal(). Zeigen sie NICHT in dieselbe Richtung,
            // drehen wir die Vertex-Reihenfolge um.
            // ----------------------------------------------------------

            auto computeNewellNormal =
                [](const std::vector<Vertex>& poly) -> Vec3
                {
                    Vec3 n(0, 0, 0);
                    const size_t cnt = poly.size();

                    for (size_t i = 0; i < cnt; ++i)
                    {
                        const Vec3& a = poly[i].world();
                        const Vec3& b = poly[(i + 1) % cnt].world();

                        n.x() += (a.y() - b.y()) * (a.z() + b.z());
                        n.y() += (a.z() - b.z()) * (a.x() + b.x());
                        n.z() += (a.x() - b.x()) * (a.y() + b.y());
                    }

                    return n;
                };

            std::vector<Vertex> normalizedSubtract = subtractPoly;

            Vec3 subNormal = computeNewellNormal(subtractPoly);

            if ((subNormal ^ facePlane.normal()) < static_cast<T>(0))
            {
                std::reverse(
                    normalizedSubtract.begin(),
                    normalizedSubtract.end());
            }

            // ----------------------------------------------------------
            // Ab hier wie bisher, aber mit normalizedSubtract statt
            // subtractPoly.
            // ----------------------------------------------------------

            const size_t k = normalizedSubtract.size();

            std::vector<Plane> edgePlanes;
            edgePlanes.reserve(k);

            for (size_t i = 0; i < k; ++i)
            {
                const Vec3& a = normalizedSubtract[i].world();
                const Vec3& b = normalizedSubtract[(i + 1) % k].world();

                Vec3 edgeDir = b - a;

                Vec3 n = facePlane.normal() % edgeDir;
                T len = n.length();

                if (len > static_cast<T>(1e-12))
                    n *= static_cast<T>(1) / len;

                Plane ep(a, n);

                edgePlanes.push_back(ep);
            }

            std::vector<Vertex> remaining = basePoly;

            for (size_t i = 0; i < k; ++i)
            {
                if (remaining.size() < 3)
                    break;

                auto outside =
                    ClipWindingToPlane(
                        remaining,
                        edgePlanes[i],
                        false,
                        epsilon);

                if (outside.size() >= 3)
                    result.push_back(std::move(outside));

                remaining =
                    ClipWindingToPlane(
                        remaining,
                        edgePlanes[i],
                        true,
                        epsilon);
            }

            return result;
        }



        // ------------------------------------------------------------
        // 6 grosse Aussenportale
        // ------------------------------------------------------------

        template<class T>
        inline std::vector<Portal<T>*> MakeHeadnodePortals(
            BSPNode<T>* root,
            T worldSize,
            std::vector<std::unique_ptr<Portal<T>>>& allPortals)
        {
            using Vec3 = ng::math::Matrix<3, 1, T>;
            using Vertex = typename Portal<T>::Vertex;
            using Plane = typename Portal<T>::Plane;

            std::vector<Portal<T>*> result;

            T h = worldSize * static_cast<T>(0.5);

            struct FaceDef
            {
                Vec3 normal;
                Vec3 center;
                Vec3 axisU;
                Vec3 axisV;
            };

            FaceDef faces[6] =
            {
                { Vec3(1,0,0), Vec3(h,0,0), Vec3(0,1,0), Vec3(0,0,1) },
                { Vec3(-1,0,0), Vec3(-h,0,0), Vec3(0,0,1), Vec3(0,1,0) },

                { Vec3(0, 1,0), Vec3(0, h,0), Vec3(0,0,1), Vec3(1,0,0) },
                { Vec3(0,-1,0), Vec3(0,-h,0), Vec3(1,0,0), Vec3(0,0,1) },

                { Vec3(0,0, 1), Vec3(0,0, h), Vec3(1,0,0), Vec3(0,1,0) },
                { Vec3(0,0,-1), Vec3(0,0,-h), Vec3(0,1,0), Vec3(1,0,0) },
            };

            for (auto& f : faces)
            {
                auto portal = std::make_unique<Portal<T>>();

                // Plane direkt aus Punkt + Normalenvektor.
                portal->plane = Plane(f.center, f.normal);

                Vec3 p0 =
                    f.center -
                    f.axisU * h -
                    f.axisV * h;

                Vec3 p1 =
                    f.center +
                    f.axisU * h -
                    f.axisV * h;

                Vec3 p2 =
                    f.center +
                    f.axisU * h +
                    f.axisV * h;

                Vec3 p3 =
                    f.center -
                    f.axisU * h +
                    f.axisV * h;

                Vertex v0, v1, v2, v3;

                v0.world() = p0;
                v1.world() = p1;
                v2.world() = p2;
                v3.world() = p3;

                portal->winding =
                {
                    v0, v1, v2, v3
                };

                portal->nodes[0] = nullptr;
                portal->nodes[1] = root;

                result.push_back(portal.get());

                allPortals.push_back(std::move(portal));
            }

            return result;
        }


        // ------------------------------------------------------------
        // Kernrekursion
        // ------------------------------------------------------------

        template<class T>
        inline void MakeNodePortals(
            BSPNode<T>* node,
            std::vector<Portal<T>*> incoming,
            std::vector<std::unique_ptr<Portal<T>>>& allPortals,
            T epsilon = static_cast<T>(1e-5))
        {
            if (!node)
                return;

            if (node->isLeaf)
            {
                node->portals = std::move(incoming);
                return;
            }

            using Vec3 = ng::math::Matrix<3, 1, T>;
            using Vertex = typename Portal<T>::Vertex;
            using Plane = typename Portal<T>::Plane;

            // --------------------------------------------------------
            // 1. Grosses Polygon auf der Split-Ebene
            // --------------------------------------------------------

            const Vec3 n = node->plane.normal();

            Vec3 up =
                (std::abs(n.y()) < static_cast<T>(0.9))
                ? Vec3(0, 1, 0)
                : Vec3(1, 0, 0);

            Vec3 axisU = up % n;

            T axisULen = axisU.length();

            if (axisULen > static_cast<T>(1e-8))
                axisU *= static_cast<T>(1) / axisULen;

            Vec3 axisV = n % axisU;

            const T big = static_cast<T>(1e6);

            // Punkt auf der Ebene:
            //
            // n*x + D = 0
            //
            // x = -D*n
            Vec3 pointOnPlane =
                n * (-node->plane.D());

            std::vector<Vertex> candidate;

            {
                Vertex v0, v1, v2, v3;

                v0.world() =
                    pointOnPlane -
                    axisU * big -
                    axisV * big;

                v1.world() =
                    pointOnPlane +
                    axisU * big -
                    axisV * big;

                v2.world() =
                    pointOnPlane +
                    axisU * big +
                    axisV * big;

                v3.world() =
                    pointOnPlane -
                    axisU * big +
                    axisV * big;

                candidate =
                {
                    v0, v1, v2, v3
                };
            }

            // --------------------------------------------------------
            // Kandidaten mit eingehenden Portalen beschneiden
            // --------------------------------------------------------

            for (Portal<T>* p : incoming)
            {
                bool nodeIsFront =
                    (p->nodes[0] == node);

                candidate =
                    ClipWindingToPlane(
                        candidate,
                        p->plane,
                        nodeIsFront,
                        epsilon);

                if (candidate.size() < 3)
                    break;
            }

            // --------------------------------------------------------
            // 2. Wandflaechen dieses Knotens subtrahieren
            // --------------------------------------------------------

            std::vector<std::vector<Vertex>> fragments;

            if (candidate.size() >= 3)
                fragments.push_back(std::move(candidate));

            for (const auto& wallPoly : node->coplanar)
            {
                std::vector<std::vector<Vertex>> next;

                for (auto& frag : fragments)
                {
                    auto pieces =
                        SubtractConvexPolygon(
                            frag,
                            wallPoly.verts,
                            node->plane,
                            epsilon);

                    for (auto& piece : pieces)
                        next.push_back(std::move(piece));
                }

                fragments = std::move(next);
            }

            // --------------------------------------------------------
            // 3. Neue Portale auf Split-Ebene erzeugen
            // --------------------------------------------------------

            std::vector<Portal<T>*> newPortals;

            for (auto& frag : fragments)
            {
                auto np = std::make_unique<Portal<T>>();

                np->plane = node->plane;
                np->winding = std::move(frag);

                np->nodes[0] = node->front.get();
                np->nodes[1] = node->back.get();

                newPortals.push_back(np.get());

                allPortals.push_back(std::move(np));
            }

            // --------------------------------------------------------
            // 4. Eingehende Portale verteilen
            // --------------------------------------------------------

            std::vector<Portal<T>*> frontList;
            std::vector<Portal<T>*> backList;

            for (Portal<T>* p : incoming)
            {
                int side =
                    (p->nodes[0] == node)
                    ? 0
                    : 1;

                int cls =
                    ClassifyWinding(
                        node->plane,
                        p->winding,
                        epsilon);

                if (cls == ng::math::RayPlaneLine<3, T>::FRONT ||
                    cls == ng::math::RayPlaneLine<3, T>::PLANAR)
                {
                    p->nodes[side] =
                        node->front.get();

                    frontList.push_back(p);
                }
                else if (cls == ng::math::RayPlaneLine<3, T>::BACK)
                {
                    p->nodes[side] =
                        node->back.get();

                    backList.push_back(p);
                }
                else
                {
                    std::vector<Vertex> fw;
                    std::vector<Vertex> bw;

                    SplitWinding(
                        node->plane,
                        p->winding,
                        epsilon,
                        fw,
                        bw);

                    if (fw.size() >= 3)
                    {
                        auto pf =
                            std::make_unique<Portal<T>>();

                        pf->plane = p->plane;
                        pf->winding = std::move(fw);

                        pf->nodes[0] = p->nodes[0];
                        pf->nodes[1] = p->nodes[1];

                        pf->nodes[side] =
                            node->front.get();

                        frontList.push_back(pf.get());

                        allPortals.push_back(std::move(pf));
                    }

                    if (bw.size() >= 3)
                    {
                        auto pb =
                            std::make_unique<Portal<T>>();

                        pb->plane = p->plane;
                        pb->winding = std::move(bw);

                        pb->nodes[0] = p->nodes[0];
                        pb->nodes[1] = p->nodes[1];

                        pb->nodes[side] =
                            node->back.get();

                        backList.push_back(pb.get());

                        allPortals.push_back(std::move(pb));
                    }
                }
            }

            // --------------------------------------------------------
            // Neue Portale verbinden beide Seiten
            // --------------------------------------------------------

            for (Portal<T>* np : newPortals)
            {
                frontList.push_back(np);
                backList.push_back(np);
            }

            // --------------------------------------------------------
            // Rekursion
            // --------------------------------------------------------

            MakeNodePortals(
                node->front.get(),
                std::move(frontList),
                allPortals,
                epsilon);

            MakeNodePortals(
                node->back.get(),
                std::move(backList),
                allPortals,
                epsilon);
        }


        // ------------------------------------------------------------
        // Einstiegspunkt
        // ------------------------------------------------------------

        template<class T>
        inline std::vector<std::unique_ptr<Portal<T>>>
            GeneratePortals(
                BSPNode<T>* root,
                T worldSize = static_cast<T>(1e5),
                T epsilon = static_cast<T>(1e-5))
        {
            std::vector<std::unique_ptr<Portal<T>>> allPortals;

            if (!root)
                return allPortals;

            std::vector<Portal<T>*> head =
                MakeHeadnodePortals(
                    root,
                    worldSize,
                    allPortals);

            MakeNodePortals(
                root,
                std::move(head),
                allPortals,
                epsilon);

            return allPortals;
        }

        template<class T>
        inline T ComputeWorldSize(
            const std::vector<BSPPolygon<T>>& levelPolys,
            T marginFactor = static_cast<T>(1.5))
        {
            using Vec3 = ng::math::Matrix<3, 1, T>;

            Vec3 mn(
                std::numeric_limits<T>::max(),
                std::numeric_limits<T>::max(),
                std::numeric_limits<T>::max());

            Vec3 mx(
                -std::numeric_limits<T>::max(),
                -std::numeric_limits<T>::max(),
                -std::numeric_limits<T>::max());

            for (const auto& poly : levelPolys)
            {
                for (const auto& v : poly.verts)
                {
                    const Vec3& p = v.world();

                    mn.x() = std::min(mn.x(), p.x());
                    mn.y() = std::min(mn.y(), p.y());
                    mn.z() = std::min(mn.z(), p.z());

                    mx.x() = std::max(mx.x(), p.x());
                    mx.y() = std::max(mx.y(), p.y());
                    mx.z() = std::max(mx.z(), p.z());
                }
            }

            Vec3 extent = mx - mn;

            T maxExtent = std::max({ extent.x(), extent.y(), extent.z() });

            // Sicherheitsmarge: Box muss die Geometrie komplett umschliessen,
            // inkl. Rand-Fällen (achsen-nahe Brushes, Rundungsfehler beim
            // Splitten, etc.)
            return maxExtent * marginFactor;
        }
        // ------------------------------------------------------------
        // Debug / Statistik
        // ------------------------------------------------------------

        template<class T>
        inline void CollectLeafPortalStats(
            const BSPNode<T>* node,
            int& leafCount,
            int& totalPortalRefs)
        {
            if (!node)
                return;

            if (node->isLeaf)
            {
                ++leafCount;

                totalPortalRefs +=
                    static_cast<int>(
                        node->portals.size());

                return;
            }

            CollectLeafPortalStats(
                node->front.get(),
                leafCount,
                totalPortalRefs);

            CollectLeafPortalStats(
                node->back.get(),
                leafCount,
                totalPortalRefs);
        }

    } // namespace bsp
} // namespace ng