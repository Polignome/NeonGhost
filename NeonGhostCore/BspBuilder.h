#pragma once
// =====================================================================
// BSPBuilder.h
//
// Polygon-basierter BSP-Baum.
//
// Plane wurde vollständig durch
// ng::math::RayPlaneLine<3,T> ersetzt.
//
// Die Seite, in die die Normalen zeigen, ist FRONT.
// Die Gegenseite ist BACK.
//
// Der BSP-Baum entscheidet NICHT über Solid/Empty.
// Diese Klassifikation erfolgt später über Portal/Floodfill.
// =====================================================================

#include "Matrix.h"
#include "Vertex.h"
#include "RayPlaneLine.h"

#include <vector>
#include <memory>
#include <limits>
#include <cmath>
#include <algorithm>
#include <cstdint>

namespace ng
{
    namespace bsp
    {

        // ------------------------------------------------------------
        // BSPPolygon
        // ------------------------------------------------------------

        template<class T>
        struct BSPPolygon
        {
            using Plane = math::RayPlaneLine<3, T>;
            using Vertex = ng::geometry::Vertex<T>;

            std::vector<Vertex> verts;

            Plane plane;

            int materialId = -1;
            uint32_t objectId = 0;

            static BSPPolygon FromTriangle(
                const Vertex& v0,
                const Vertex& v1,
                const Vertex& v2,
                int materialId = -1,
                uint32_t objectId = 0)
            {
                BSPPolygon poly;

                poly.verts =
                {
                    v0,
                    v1,
                    v2
                };

                poly.plane =
                    Plane(
                        v0.world(),
                        v1.world(),
                        v2.world());

                poly.materialId = materialId;
                poly.objectId = objectId;

                return poly;
            }
        };


       

        // ------------------------------------------------------------
        // Abstand Punkt -> Ebene
        //
        // RayPlaneLine:
        //
        //       N * P + D = 0
        //
        // Deshalb:
        //
        //       distance = N * P + D
        // ------------------------------------------------------------

        template<class T>
        inline T PlaneDistance(
            const math::RayPlaneLine<3, T>& plane,
            const math::Matrix<3, 1, T>& point)
        {
            return (point ^ plane.normal()) + plane.D();
        }


        // ------------------------------------------------------------
        // Klassifiziert ein Polygon gegen eine Ebene.
        //
        // Wichtig:
        // Das epsilon kommt vom BSP-Aufrufer und wird deshalb hier
        // NICHT durch RayPlaneLine::classify() ersetzt.
        // ------------------------------------------------------------

        template<class T>
        inline int ClassifyPolygon(
            const math::RayPlaneLine<3, T>& plane,
            const BSPPolygon<T>& poly,
            T epsilon)
        {
            bool front = false;
            bool back = false;

            for (const auto& v : poly.verts)
            {
                T d = plane.distance(v.world());

                if (d > epsilon)
                    front = true;

                else if (d < -epsilon)
                    back = true;

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
        // Interpoliert alle Vertex-Attribute.
        // ------------------------------------------------------------

        template<class T>
        inline ng::geometry::Vertex<T> LerpVertex(
            const ng::geometry::Vertex<T>& a,
            const ng::geometry::Vertex<T>& b,
            T t)
        {
            ng::geometry::Vertex<T> out = a;

            out.world() =
                a.world() +
                (b.world() - a.world()) * t;

            out.normal() =
                a.normal() +
                (b.normal() - a.normal()) * t;

            out.texture() =
                a.texture() +
                (b.texture() - a.texture()) * t;

            out.color() =
                a.color() +
                (b.color() - a.color()) * t;

            out.alpha() =
                a.alpha() +
                (b.alpha() - a.alpha()) * t;

            return out;
        }


        // ------------------------------------------------------------
        // SplitPolygon
        //
        // Teilt ein Polygon in FRONT und BACK.
        // Koplanare Vertices werden beiden Seiten zugeordnet.
        // ------------------------------------------------------------

        template<class T>
        inline void SplitPolygon(
            const math::RayPlaneLine<3, T>& plane,
            const BSPPolygon<T>& poly,
            T epsilon,
            BSPPolygon<T>& frontOut,
            BSPPolygon<T>& backOut)
        {
            const size_t n = poly.verts.size();

            frontOut.plane = poly.plane;
            backOut.plane = poly.plane;

            frontOut.materialId =
                backOut.materialId =
                poly.materialId;

            frontOut.objectId =
                backOut.objectId =
                poly.objectId;

            for (size_t i = 0; i < n; ++i)
            {
                const auto& curr =
                    poly.verts[i];

                const auto& next =
                    poly.verts[(i + 1) % n];

                T dCurr =
                    PlaneDistance(
                        plane,
                        curr.world());

                T dNext =
                    PlaneDistance(
                        plane,
                        next.world());

                // ----------------------------------------------------
                // Aktuellen Vertex verteilen
                // ----------------------------------------------------

                if (dCurr > epsilon)
                {
                    frontOut.verts.push_back(curr);
                }
                else if (dCurr < -epsilon)
                {
                    backOut.verts.push_back(curr);
                }
                else
                {
                    // Auf der Ebene -> beide Seiten.
                    frontOut.verts.push_back(curr);
                    backOut.verts.push_back(curr);
                }

                // ----------------------------------------------------
                // Kante schneidet die Ebene
                // ----------------------------------------------------

                const bool crossesToBack =
                    (dCurr > epsilon &&
                        dNext < -epsilon);

                const bool crossesToFront =
                    (dCurr < -epsilon &&
                        dNext > epsilon);

                if (crossesToBack || crossesToFront)
                {
                    const T denom =
                        dCurr - dNext;

                    if (std::abs(denom) >
                        static_cast<T>(1e-12))
                    {
                        const T t =
                            dCurr / denom;

                        auto mid =
                            LerpVertex(
                                curr,
                                next,
                                t);

                        frontOut.verts.push_back(mid);
                        backOut.verts.push_back(mid);
                    }
                }
            }
        }


        // ------------------------------------------------------------
        // BSPNode
        // ------------------------------------------------------------

        template<class T>
        struct Portal;

        template<class T>
        struct BSPNode
        {
            using Plane = math::RayPlaneLine<3, T>;

            Plane plane;

            std::vector<BSPPolygon<T>> coplanar;

            // FRONT = Seite, in die plane.normal() zeigt
            std::unique_ptr<BSPNode> front;

            // BACK = Gegenseite
            std::unique_ptr<BSPNode> back;

            bool isLeaf = false;

            // Wird von PortalGen.h gefüllt.
            std::vector<Portal<T>*> portals;

            // Wird später durch Floodfill gesetzt.
            enum class LeafType
            {
                Unknown,
                Empty,
                Solid
            };

            LeafType leafType =
                LeafType::Unknown;
        };


        // ------------------------------------------------------------
        // Wählt den Splitter.
        // ------------------------------------------------------------

        template<class T>
        inline int ChooseSplitter(
            const std::vector<BSPPolygon<T>>& polys,
            T epsilon)
        {
            if (polys.empty())
                return -1;

            const size_t maxCandidates =
                std::min<size_t>(
                    polys.size(),
                    32);

            const size_t step =
                std::max<size_t>(
                    1,
                    polys.size() /
                    maxCandidates);

            int bestIndex = -1;

            T bestScore =
                std::numeric_limits<T>::max();

            const T splitPenalty =
                static_cast<T>(8);

            for (size_t i = 0;
                i < polys.size();
                i += step)
            {
                const auto& candidate =
                    polys[i].plane;

                int numFront = 0;
                int numBack = 0;
                int numSplit = 0;

                for (size_t j = 0;
                    j < polys.size();
                    ++j)
                {
                    if (j == i)
                        continue;

                    int s =
                        ClassifyPolygon(
                            candidate,
                            polys[j],
                            epsilon);

                    switch (s)
                    {
                    case ng::math::RayPlaneLine<3, T>::FRONT:
                        ++numFront;
                        break;

                    case ng::math::RayPlaneLine<3, T>::BACK:
                        ++numBack;
                        break;

                    case ng::math::RayPlaneLine<3, T>::SPANNING:
                        ++numSplit;
                        break;

                    case ng::math::RayPlaneLine<3, T>::PLANAR:
                        break;
                    }
                }

                T score =
                    std::abs(
                        static_cast<T>(numFront) -
                        static_cast<T>(numBack))
                    +
                    splitPenalty *
                    static_cast<T>(numSplit);

                if (score < bestScore)
                {
                    bestScore = score;
                    bestIndex =
                        static_cast<int>(i);
                }
            }

            return bestIndex;
        }


        // ------------------------------------------------------------
        // Baut den BSP-Baum rekursiv.
        // ------------------------------------------------------------

        template<class T>
        inline std::unique_ptr<BSPNode<T>> BuildBSP(
            std::vector<BSPPolygon<T>> polys,
            T epsilon = static_cast<T>(1e-5))
        {
            auto node =
                std::make_unique<BSPNode<T>>();

            // --------------------------------------------------------
            // Leeres Blatt
            // --------------------------------------------------------

            if (polys.empty())
            {
                node->isLeaf = true;
                return node;
            }

            // --------------------------------------------------------
            // Splitter wählen
            // --------------------------------------------------------

            int splitIdx =
                ChooseSplitter(
                    polys,
                    epsilon);

            if (splitIdx < 0)
            {
                node->isLeaf = true;

                node->coplanar =
                    std::move(polys);

                return node;
            }

            // --------------------------------------------------------
            // Split-Ebene übernehmen
            // --------------------------------------------------------

            node->plane =
                polys[
                    static_cast<size_t>(
                        splitIdx)
                ].plane;

            std::vector<BSPPolygon<T>>
                frontList;

            std::vector<BSPPolygon<T>>
                backList;

            frontList.reserve(
                polys.size());

            backList.reserve(
                polys.size());

            // --------------------------------------------------------
            // Polygone verteilen
            // --------------------------------------------------------

            for (size_t i = 0;
                i < polys.size();
                ++i)
            {
                // ----------------------------------------------------
                // Splitter selbst
                // ----------------------------------------------------

                if (static_cast<int>(i) ==
                    splitIdx)
                {
                    node->coplanar.push_back(
                        std::move(polys[i]));

                    continue;
                }

                // ----------------------------------------------------
                // Klassifikation
                // ----------------------------------------------------

                int s =
                    ClassifyPolygon(
                        node->plane,
                        polys[i],
                        epsilon);

                switch (s)
                {
                case ng::math::RayPlaneLine<3, T>::FRONT:
                {
                    frontList.push_back(
                        std::move(polys[i]));

                    break;
                }

                case ng::math::RayPlaneLine<3, T>::BACK:
                {
                    backList.push_back(
                        std::move(polys[i]));

                    break;
                }

                case ng::math::RayPlaneLine<3, T>::PLANAR:
                {
                    node->coplanar.push_back(
                        std::move(polys[i]));

                    break;
                }

                case ng::math::RayPlaneLine<3, T>::SPANNING:
                {
                    BSPPolygon<T> frontPoly;
                    BSPPolygon<T> backPoly;

                    SplitPolygon(
                        node->plane,
                        polys[i],
                        epsilon,
                        frontPoly,
                        backPoly);

                    // Entartete Ergebnisse ignorieren.
                    if (frontPoly.verts.size() >= 3)
                    {
                        frontList.push_back(
                            std::move(frontPoly));
                    }

                    if (backPoly.verts.size() >= 3)
                    {
                        backList.push_back(
                            std::move(backPoly));
                    }

                    break;
                }
                }
            }

            // --------------------------------------------------------
            // IMMER beide Seiten erzeugen.
            //
            // Auch eine leere Seite wird als echtes Leaf erzeugt.
            // --------------------------------------------------------

            node->front =
                BuildBSP(
                    std::move(frontList),
                    epsilon);

            node->back =
                BuildBSP(
                    std::move(backList),
                    epsilon);

            return node;
        }


        // ------------------------------------------------------------
        // Debug / Statistik
        // ------------------------------------------------------------

        template<class T>
        inline void CollectStats(
            const BSPNode<T>* node,
            int depth,
            int& maxDepth,
            int& nodeCount,
            int& leafCount)
        {
            if (!node)
                return;

            ++nodeCount;

            maxDepth =
                std::max(
                    maxDepth,
                    depth);

            if (node->isLeaf)
                ++leafCount;

            CollectStats(
                node->front.get(),
                depth + 1,
                maxDepth,
                nodeCount,
                leafCount);

            CollectStats(
                node->back.get(),
                depth + 1,
                maxDepth,
                nodeCount,
                leafCount);
        }

    } // namespace bsp
} // namespace ng
