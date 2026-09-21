#pragma once

#include "Matrix.h"
#include "Vertex.h"
#include "RayPlaneLine.h"
#include "Clipping.h"
#include "AABBox.h"

#include <vector>
#include <memory>
#include <limits>
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <utility>
#include <cstdio>
#include <chrono>
#include <unordered_map>

namespace ng
{
    namespace bsp
    {
        template <class T> class BSPNode;
        template <class T> class BSPTree;
        template <class T> class Portal;

        enum : uint32_t
        {
            UNKNOWN = 1 << 0,
            NODE = 1 << 1,
            LEAF = 1 << 2,
            SOLID = 1 << 3,
            EMPTY = 1 << 4,
            INTOTHEVOID = 1 << 5,
            HAS_ILLEGAL_GEOMETRY = 1 << 6
        };

        enum : uint32_t
        {
            POLY_FLAG_NONE = 0,
            POLY_FLAG_OUT_SIDE = 1 << 1,        // Polygone der Umschliessungsbox (Normale nach innen!)
            POLY_FLAG_SPLITTER_USED = 1 << 2    // intern: Ebene wurde schon als Splitter benutzt
        };

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
            uint32_t flags = 0;
            int volumeId = -1;

            static BSPPolygon FromTriangle(
                const Vertex& v0, const Vertex& v1, const Vertex& v2,
                int materialId = -1, uint32_t objectId = 0)
            {
                BSPPolygon poly;
                poly.verts = { v0, v1, v2 };
                poly.plane = Plane(v0.world(), v1.world(), v2.world());
                poly.materialId = materialId;
                poly.objectId = objectId;
                return poly;
            }
        };

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

            out.world() = a.world() + (b.world() - a.world()) * t;
            out.normal() = a.normal() + (b.normal() - a.normal()) * t;
            out.texture() = a.texture() + (b.texture() - a.texture()) * t;
            out.color() = a.color() + (b.color() - a.color()) * t;
            out.alpha() = a.alpha() + (b.alpha() - a.alpha()) * t;

            return out;
        }

        // ------------------------------------------------------------
        // SplitPolygon
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

            frontOut.materialId = backOut.materialId = poly.materialId;
            frontOut.objectId = backOut.objectId = poly.objectId;
            frontOut.flags = backOut.flags = poly.flags;
            frontOut.volumeId = backOut.volumeId = poly.volumeId;

            for (size_t i = 0; i < n; ++i)
            {
                const auto& curr = poly.verts[i];
                const auto& next = poly.verts[(i + 1) % n];

                const T dCurr = plane.distance(curr.world());
                const T dNext = plane.distance(next.world());

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
                    frontOut.verts.push_back(curr);
                    backOut.verts.push_back(curr);
                }

                const bool crossesToBack = (dCurr > epsilon && dNext < -epsilon);
                const bool crossesToFront = (dCurr < -epsilon && dNext > epsilon);

                if (crossesToBack || crossesToFront)
                {
                    const T denom = dCurr - dNext;

                    if (std::abs(denom) > static_cast<T>(1e-12))
                    {
                        const T t = dCurr / denom;
                        auto mid = LerpVertex(curr, next, t);
                        frontOut.verts.push_back(mid);
                        backOut.verts.push_back(mid);
                    }
                }
            }
        }

        // ------------------------------------------------------------
        // Ebenen-Hilfsfunktionen
        // ------------------------------------------------------------

        template<class T>
        bool SamePlane(
            const ng::math::RayPlaneLine<3, T>& a,
            const ng::math::RayPlaneLine<3, T>& b,
            T epsilon = ng::math::EPSILON<T>)
        {
            const auto& na = a.normal();
            const auto& nb = b.normal();

            if (std::abs(na.x() - nb.x()) > epsilon ||
                std::abs(na.y() - nb.y()) > epsilon ||
                std::abs(na.z() - nb.z()) > epsilon)
                return false;

            if (std::abs(a.D() - b.D()) > epsilon)
                return false;

            return true;
        }

        // Gleiche Ebene, Orientierung egal.
        template<class T>
        bool Coplanar(
            const ng::math::RayPlaneLine<3, T>& a,
            const ng::math::RayPlaneLine<3, T>& b,
            T epsilon)
        {
            const auto na = a.normal();
            const auto nb = b.normal();

            const auto cross = na % nb;
            if (cross.length() > epsilon)
                return false;

            const T len2 = na ^ na;
            if (len2 <= epsilon * epsilon)
                return false;

            const auto p = na * (-a.D() / len2);
            return std::abs(b.distance(p)) <= epsilon;
        }

        template<class T>
        bool AddUniquePlane(
            std::vector<ng::math::RayPlaneLine<3, T>>& planes,
            const ng::math::RayPlaneLine<3, T>& plane,
            T epsilon = ng::math::EPSILON<T>)
        {
            for (const auto& existing : planes)
            {
                if (Coplanar(existing, plane, epsilon))
                    return false;
            }

            planes.push_back(plane);
            return true;
        }

        template <class T>
        inline int classify(const ng::math::RayPlaneLine<3, T>& plane,
            const std::vector<ng::geometry::Vertex<T>>& verts)
        {
            using Plane = ng::math::RayPlaneLine<3, T>;
            bool front = false;
            bool back = false;

            for (const auto& v : verts)
            {
                const int res = plane.classify(v.world());

                if (res == Plane::FRONT) front = true;
                if (res == Plane::BACK)  back = true;

                if (front && back) return Plane::SPANNING;
            }

            if (front) return Plane::FRONT;
            if (back)  return Plane::BACK;
            return Plane::PLANAR;
        }

        // ------------------------------------------------------------
        // Epsilon-konsistente Klassifikation und Teilung
        //
        // Klassifikation UND Teilung benutzen dasselbe Epsilon (Abstand in
        // Weltunits). Nur so ist sichergestellt, dass ein als SPANNING
        // klassifiziertes Polygon auch wirklich geteilt wird und ein
        // PLANAR klassifiziertes nie zerschnitten wird.
        // ------------------------------------------------------------

        template <class T>
        inline int ClassifyVerts(const math::RayPlaneLine<3, T>& plane,
            const std::vector<ng::geometry::Vertex<T>>& verts, T eps)
        {
            using Plane = math::RayPlaneLine<3, T>;
            bool front = false, back = false;

            for (const auto& v : verts)
            {
                const T d = plane.distance(v.world());
                if (d > eps)       front = true;
                else if (d < -eps) back = true;

                if (front && back) return Plane::SPANNING;
            }

            if (front) return Plane::FRONT;
            if (back)  return Plane::BACK;
            return Plane::PLANAR;
        }

        template <class T>
        inline int ClassifyPoints(const math::RayPlaneLine<3, T>& plane,
            const std::vector<math::Matrix<3, 1, T>>& pts, T eps)
        {
            using Plane = math::RayPlaneLine<3, T>;
            bool front = false, back = false;

            for (const auto& v : pts)
            {
                const T d = plane.distance(v);
                if (d > eps)       front = true;
                else if (d < -eps) back = true;

                if (front && back) return Plane::SPANNING;
            }

            if (front) return Plane::FRONT;
            if (back)  return Plane::BACK;
            return Plane::PLANAR;
        }

        // Teilt ein konvexes Punktpolygon. Punkte auf der Ebene kommen in beide Teile.
        template <class T>
        inline void SplitPoints(const std::vector<math::Matrix<3, 1, T>>& in,
            const math::RayPlaneLine<3, T>& plane, T eps,
            std::vector<math::Matrix<3, 1, T>>& front,
            std::vector<math::Matrix<3, 1, T>>& back)
        {
            front.clear();
            back.clear();

            const size_t n = in.size();
            for (size_t i = 0; i < n; ++i)
            {
                const auto& curr = in[i];
                const auto& next = in[(i + 1) % n];

                const T dCurr = plane.distance(curr);
                const T dNext = plane.distance(next);

                if (dCurr > eps)       front.push_back(curr);
                else if (dCurr < -eps) back.push_back(curr);
                else { front.push_back(curr); back.push_back(curr); }

                if ((dCurr > eps && dNext < -eps) || (dCurr < -eps && dNext > eps))
                {
                    const T denom = dCurr - dNext;
                    if (std::abs(denom) > static_cast<T>(1e-12))
                    {
                        const T t = dCurr / denom;
                        const auto mid = curr + (next - curr) * t;
                        front.push_back(mid);
                        back.push_back(mid);
                    }
                }
            }
        }

        template <class T>
        inline T VertsArea(const std::vector<ng::geometry::Vertex<T>>& v)
        {
            if (v.size() < 3) return T(0);

            math::Matrix<3, 1, T> sum(0, 0, 0);
            const auto v0 = v[0].world();

            for (size_t i = 1; i + 1 < v.size(); ++i)
                sum = sum + ((v[i].world() - v0) % (v[i + 1].world() - v0));

            return sum.length() * T(0.5);
        }

        // ------------------------------------------------------------
        // FindBestSplitter
        //
        // Holt sich die Kandidaten selbst aus den Polygonen der Zelle:
        // jede noch nicht benutzte Polygon-Ebene (ohne Duplikate) ist
        // ein Kandidat. Da jeder Kandidat mindestens ein Polygon der
        // Zelle enthaelt, wird jede Wand irgendwann Zellgrenze.
        //
        // maxCandidates > 0 begrenzt die Zahl der bewerteten Kandidaten
        // (gleichmaessig verteilt) - spart Zeit bei grossen Karten.
        // ------------------------------------------------------------

        // Kanonischer Schluessel einer Ebene (Orientierung egal) fuer das Dedupe
        // per Hash. Ersetzt den O(n^2)-Vergleich mit AddUniquePlane.
        struct PlaneKey
        {
            int64_t a = 0, b = 0, c = 0, d = 0;
            bool operator==(const PlaneKey& o) const
            {
                return a == o.a && b == o.b && c == o.c && d == o.d;
            }
        };

        struct PlaneKeyHash
        {
            size_t operator()(const PlaneKey& k) const
            {
                uint64_t h = 1469598103934665603ull;
                const int64_t v[4] = { k.a, k.b, k.c, k.d };
                for (int i = 0; i < 4; ++i)
                {
                    h ^= static_cast<uint64_t>(v[i]);
                    h *= 1099511628211ull;
                }
                return static_cast<size_t>(h);
            }
        };

        template <class T>
        PlaneKey MakePlaneKey(const math::RayPlaneLine<3, T>& plane)
        {
            const auto n = plane.normal();

            int64_t a = static_cast<int64_t>(std::llround(double(n(0)) * 1e4));
            int64_t b = static_cast<int64_t>(std::llround(double(n(1)) * 1e4));
            int64_t c = static_cast<int64_t>(std::llround(double(n(2)) * 1e4));
            double  d = double(plane.D());

            bool flip;
            if (a != 0)      flip = a < 0;
            else if (b != 0) flip = b < 0;
            else             flip = c < 0;

            if (flip) { a = -a; b = -b; c = -c; d = -d; }

            PlaneKey k;
            k.a = a; k.b = b; k.c = c;
            k.d = static_cast<int64_t>(std::llround(d * 100.0));
            return k;
        }

        template <class T>
        bool FindBestSplitter(
            const std::vector<BSPPolygon<T>>& polygons,
            math::RayPlaneLine<3, T>& bestPlane,
            size_t& bestSource,               // Index des Polygons, aus dem der Splitter stammt
            T eps,
            long splitHeuristic = 8,
            size_t maxCandidates = 64)
        {
            using Plane = math::RayPlaneLine<3, T>;

            struct Candidate
            {
                const Plane* plane;
                size_t source;
            };

            std::vector<Candidate> candidates;
            std::unordered_map<PlaneKey, char, PlaneKeyHash> seen;
            seen.reserve(polygons.size());

            for (size_t i = 0; i < polygons.size(); ++i)
            {
                const auto& p = polygons[i];
                if (p.verts.size() < 3) continue;
                if (p.flags & POLY_FLAG_SPLITTER_USED) continue;

                if (seen.emplace(MakePlaneKey(p.plane), char(1)).second)
                    candidates.push_back({ &p.plane, i });
            }

            if (candidates.empty())
                return false;

            const size_t stride =
                (maxCandidates > 0 && candidates.size() > maxCandidates)
                ? candidates.size() / maxCandidates : 1;

            T bestScore = std::numeric_limits<T>::max();
            size_t bestIndex = 0;
            bool found = false;

            for (size_t i = 0; i < candidates.size(); i += stride)
            {
                const Plane& splitter = *candidates[i].plane;

                const T axisPenalty = (T(1) - splitter.AxisScore()) * T(50);

                int lFront = 0, lBack = 0, lSplits = 0;
                bool aborted = false;

                for (const auto& poly : polygons)
                {
                    if (poly.verts.size() < 3)
                        continue;

                    switch (ClassifyVerts(splitter, poly.verts, eps))
                    {
                    case Plane::FRONT:  ++lFront;  break;
                    case Plane::BACK:   ++lBack;   break;
                    case Plane::PLANAR:            break;
                    default:            ++lSplits; break;
                    }

                    // Early-Out nur ueber Groessen, die nur wachsen koennen.
                    if (found &&
                        T(lSplits) * T(splitHeuristic) + axisPenalty >= bestScore)
                    {
                        aborted = true;
                        break;
                    }
                }

                if (aborted)
                    continue;

                const T score =
                    T(std::abs(lFront - lBack)) +
                    T(lSplits) * T(splitHeuristic) +
                    axisPenalty;

                if (!found || score < bestScore)
                {
                    bestScore = score;
                    bestIndex = i;
                    found = true;
                }
            }

            if (!found)
                return false;

            bestPlane = *candidates[bestIndex].plane;
            bestSource = candidates[bestIndex].source;
            return true;
        }

        // ------------------------------------------------------------
        // Portal
        //
        // m_leafa = Leaf auf der FRONT-Seite der Splitter-Ebene.
        // m_leafb = Leaf auf der BACK-Seite.
        // ------------------------------------------------------------

        template <class T>
        class Portal {
        public:
            Portal(const ng::math::RayPlaneLine<3, T>& plane, const T& scale = T(1000000)) {
                CreateByPlane(plane, scale);
            }

            Portal(const ng::math::RayPlaneLine<3, T>& plane,
                const std::vector<ng::math::Matrix<3, 1, T>>& verts)
                : m_verts(verts), m_plane(plane) {
            }

            inline std::vector<ng::math::Matrix<3, 1, T>>& verts() { return m_verts; }
            inline const std::vector<ng::math::Matrix<3, 1, T>>& verts() const { return m_verts; }

            inline const ng::math::RayPlaneLine<3, T>& plane() const { return m_plane; }

            inline int32_t leafFront() const { return m_leafa; }
            inline int32_t leafBack() const { return m_leafb; }

        private:
            friend class BSPNode<T>;
            friend class BSPTree<T>;

            std::vector<ng::math::Matrix<3, 1, T>> m_verts;
            math::RayPlaneLine<3, T> m_plane;
            int32_t m_leafa = -1;
            int32_t m_leafb = -1;

            void CreateByPlane(const math::RayPlaneLine<3, T>& plane, const T& scale)
            {
                m_verts.clear();
                m_plane = plane;

                const math::Matrix<3, 1, T> n = plane.normal();

                math::Matrix<3, 1, T> axis(0, 0, 0);

                if (std::abs(n(0)) <= std::abs(n(1)) &&
                    std::abs(n(0)) <= std::abs(n(2)))
                {
                    axis(0) = T(1);
                }
                else if (std::abs(n(1)) <= std::abs(n(2)))
                {
                    axis(1) = T(1);
                }
                else
                {
                    axis(2) = T(1);
                }

                math::Matrix<3, 1, T> u = axis % n;
                u.normalize();

                math::Matrix<3, 1, T> v = u % n;
                v.normalize();

                u *= scale;
                v *= scale;

                const math::Matrix<3, 1, T> o = plane.origin();

                m_verts.push_back(o + u - v);
                m_verts.push_back(o - u - v);
                m_verts.push_back(o - u + v);
                m_verts.push_back(o + u + v);
            }
        };

        template <class T>
        bool ClipPortalByAABB(Portal<T>& portal, ng::bounding::AABBox<3, T>& bbox)
        {
            std::vector<ng::math::Matrix<3, 1, T>> verts = portal.verts();

            auto planes = bbox.ComputeAABBPlanes();

            for (const auto& plane : planes)
            {
                verts = ng::geometry::ClipByPlane(verts, plane, ng::geometry::ClipSide::FRONT);

                if (verts.size() < 3)
                {
                    portal.verts() = verts;
                    return false;
                }
            }

            portal.verts() = verts;
            return true;
        }

        // ------------------------------------------------------------
        // Portal-Geometrie-Hilfsfunktionen
        // ------------------------------------------------------------

        template <class T>
        T PortalArea(const std::vector<ng::math::Matrix<3, 1, T>>& v)
        {
            ng::math::Matrix<3, 1, T> sum(0, 0, 0);
            for (size_t i = 1; i + 1 < v.size(); ++i)
                sum = sum + ((v[i] - v[0]) % (v[i + 1] - v[0]));
            return sum.length() * T(0.5);
        }

        template <class T>
        T PortalPerimeter(const std::vector<ng::math::Matrix<3, 1, T>>& v)
        {
            T s = T(0);
            for (size_t i = 0; i < v.size(); ++i)
                s += (v[(i + 1) % v.size()] - v[i]).length();
            return s;
        }

        // Grobe "Breite": 2 * Flaeche / Umfang. Duenne Streifen (Nahtreste) haben
        // eine sehr kleine Breite, echte Oeffnungen nicht.
        template <class T>
        T PortalMinWidth(const std::vector<ng::math::Matrix<3, 1, T>>& v)
        {
            const T per = PortalPerimeter(v);
            if (per <= T(0)) return T(0);
            return T(2) * PortalArea(v) / per;
        }

        // Zieht ein (konvexes) Wand-Polygon, das in derselben Ebene liegt, von
        // einem konvexen Portal ab. Liefert die konvexen Reststuecke AUSSERHALB
        // des Polygons.
        template <class T>
        void SubtractPolygonFromPortal(
            const std::vector<ng::math::Matrix<3, 1, T>>& portalVerts,
            const BSPPolygon<T>& poly,
            std::vector<std::vector<ng::math::Matrix<3, 1, T>>>& outside,
            T eps)
        {
            using Vec3 = ng::math::Matrix<3, 1, T>;

            const size_t cnt = poly.verts.size();
            if (cnt < 3) { outside.push_back(portalVerts); return; }

            const Vec3 n = poly.plane.normal();

            Vec3 c(0, 0, 0);
            for (const auto& v : poly.verts) c = c + v.world();
            c = c * (T(1) / T(cnt));

            std::vector<Vec3> rest = portalVerts;

            for (size_t i = 0; i < cnt && rest.size() >= 3; ++i)
            {
                const Vec3 a = poly.verts[i].world();
                const Vec3 b = poly.verts[(i + 1) % cnt].world();

                if ((b - a).length() < ng::math::EPSILON<T>)
                    continue;

                // Ebene senkrecht zum Polygon durch die Kante
                ng::math::RayPlaneLine<3, T> edge(a, b, a + n);

                // "innen" ist die Seite, auf der der Schwerpunkt liegt
                const bool insideIsFront = edge.distance(c) > T(0);

                std::vector<Vec3> f, bk;
                SplitPoints(rest, edge, eps, f, bk);

                auto& inner = insideIsFront ? f : bk;
                auto& outer = insideIsFront ? bk : f;

                if (outer.size() >= 3)
                    outside.push_back(outer);

                rest = inner;
            }
            // "rest" liegt komplett im Polygon -> verworfen
        }

        // ------------------------------------------------------------
        // BSPNode
        // ------------------------------------------------------------

        template <class T>
        class BSPNode {
        public:
            using Vec3 = ng::math::Matrix<3, 1, T>;
            using Plane = ng::math::RayPlaneLine<3, T>;

            BSPNode() {}

            // Baut den (Teil-)Baum ab diesem Knoten. ITERATIV (expliziter Stack),
            // damit tiefe Baeume den Call-Stack nicht sprengen.
            BSPNode(BSPTree<T>* tree, std::vector<BSPPolygon<T>> polygons)
            {
                Build(tree, std::move(polygons));
            }

            // Iterativer Abbau: eine tiefe shared_ptr-Kette wuerde beim Zerstoeren
            // sonst rekursiv den Stack sprengen.
            ~BSPNode()
            {
                std::vector<std::shared_ptr<BSPNode<T>>> stack;
                if (m_front_node) stack.push_back(std::move(m_front_node));
                if (m_back_node)  stack.push_back(std::move(m_back_node));

                while (!stack.empty())
                {
                    std::shared_ptr<BSPNode<T>> n = std::move(stack.back());
                    stack.pop_back();

                    if (n.use_count() == 1)
                    {
                        if (n->m_front_node) stack.push_back(std::move(n->m_front_node));
                        if (n->m_back_node)  stack.push_back(std::move(n->m_back_node));
                    }
                }
            }

            inline bool isLeaf() const { return (m_state & LEAF) != 0; }
            inline std::shared_ptr<BSPNode<T>> GetFrontNode() { return m_front_node; }
            inline std::shared_ptr<BSPNode<T>> GetBackNode() { return m_back_node; }
            inline const std::vector<BSPPolygon<T>>& GetPolygons() const { return polys; }

            inline Plane GetPlane() const { return m_plane; }
            inline int32_t GetID() const { return m_id; }
            inline uint32_t& State() { return m_state; }
            inline const uint32_t& State() const { return m_state; }

            inline void SetSolid() { m_state |= SOLID; }
            inline void DeletePolys() { polys.clear(); }

        private:
            friend class BSPTree<T>;

            struct PortalFrag {
                std::vector<Vec3> verts;
                int32_t leaf;
            };

            std::vector<BSPPolygon<T>> polys;          // nur in Leaves
            std::vector<BSPPolygon<T>> m_coplanar;     // nur in Nodes, nur fuer Portal-Bau
            uint32_t m_state = NODE;
            std::shared_ptr<BSPNode<T>> m_front_node;
            std::shared_ptr<BSPNode<T>> m_back_node;
            Plane m_plane;
            int32_t m_id = -1;

            inline void SetId(int32_t id) { m_id = id; }

            // ----------------------------------------------------------
            // Build (iterativ)
            // ----------------------------------------------------------
            void Build(BSPTree<T>* tree, std::vector<BSPPolygon<T>> rootPolygons)
            {
                struct Item
                {
                    std::shared_ptr<BSPNode<T>> owner;   // null beim Root
                    BSPNode<T>* node;
                    std::vector<BSPPolygon<T>> polygons;
                    int depth;
                };

                std::vector<Item> stack;
                stack.push_back(Item{ nullptr, this, std::move(rootPolygons), 0 });

                size_t processed = 0;
                const auto t0 = std::chrono::steady_clock::now();

                if (tree && tree->m_verbose)
                    printf("[BSP] Build: %zu Polygone\n", stack.back().polygons.size());

                while (!stack.empty())
                {
                    Item item = std::move(stack.back());
                    stack.pop_back();

                    BSPNode<T>* node = item.node;
                    std::vector<BSPPolygon<T>>& polygons = item.polygons;

                    if (tree && item.depth > tree->m_maxDepth)
                        tree->m_maxDepth = item.depth;

                    if (tree && tree->m_verbose && (++processed % 2000) == 0)
                    {
                        const double sec = std::chrono::duration<double>(
                            std::chrono::steady_clock::now() - t0).count();
                        printf("[BSP] nodes=%zu leaves=%zu pending=%zu polys(aktuell)=%zu tiefe=%d  %.1fs\n",
                            processed, tree->m_leaflist.size(), stack.size(),
                            polygons.size(), item.depth, sec);
                    }

                    Plane best;
                    size_t bestSource = 0;
                    const T eps = tree ? tree->m_epsilon : T(0.01);
                    const bool makeLeaf = !FindBestSplitter(polygons, best, bestSource, eps);

                    std::vector<int> cls;
                    if (!makeLeaf)
                    {
                        cls.resize(polygons.size());

                        for (size_t i = 0; i < polygons.size(); ++i)
                            cls[i] = ClassifyVerts(best, polygons[i].verts, eps);

                        // Das Polygon, aus dem der Splitter stammt, liegt per Definition
                        // in der Ebene - auch wenn die Numerik etwas anderes behauptet.
                        // Das garantiert in JEDEM Knoten Fortschritt (keine Endlosschleife).
                        cls[bestSource] = Plane::PLANAR;
                    }

                    if (makeLeaf)
                    {
                        node->MakeLeaf(std::move(polygons));
                        if (item.owner && tree) tree->AddLeaf(item.owner);
                        continue;
                    }

                    node->m_plane = best;

                    std::vector<BSPPolygon<T>> front;
                    std::vector<BSPPolygon<T>> back;

                    for (size_t i = 0; i < polygons.size(); ++i)
                    {
                        BSPPolygon<T>& p = polygons[i];

                        switch (cls[i])
                        {
                        case Plane::FRONT:
                            front.push_back(std::move(p));
                            break;

                        case Plane::BACK:
                            back.push_back(std::move(p));
                            break;

                        case Plane::PLANAR:
                            p.flags |= POLY_FLAG_SPLITTER_USED;
                            node->m_coplanar.push_back(p);   // nur fuer den Portal-Bau
                            if ((best.normal() ^ p.plane.normal()) >= (T)0)
                                front.push_back(std::move(p));
                            else
                                back.push_back(std::move(p));
                            break;

                        default: // SPANNING
                        {
                            BSPPolygon<T> fp, bp;
                            SplitPolygon(best, p, eps, fp, bp);

                            // Sliver (Flaeche im Bereich von eps^2) verwerfen
                            const T minArea = eps * eps;
                            if (fp.verts.size() >= 3 && VertsArea(fp.verts) > minArea) front.push_back(std::move(fp));
                            if (bp.verts.size() >= 3 && VertsArea(bp.verts) > minArea) back.push_back(std::move(bp));
                        }
                        break;
                        }
                    }

                    // Beide Kinder immer anlegen - leere Zellen sind begehbare Leaves.
                    node->m_front_node = std::make_shared<BSPNode<T>>();
                    node->m_back_node = std::make_shared<BSPNode<T>>();

                    // Back zuerst legen, damit Front zuerst abgearbeitet wird.
                    stack.push_back(Item{ node->m_back_node,  node->m_back_node.get(),  std::move(back),  item.depth + 1 });
                    stack.push_back(Item{ node->m_front_node, node->m_front_node.get(), std::move(front), item.depth + 1 });
                }
            }

            void MakeLeaf(std::vector<BSPPolygon<T>>&& polygons)
            {
                m_state = LEAF;
                polys = std::move(polygons);

                for (auto& p : polys)
                {
                    p.flags &= ~static_cast<uint32_t>(POLY_FLAG_SPLITTER_USED);
                    if (p.flags & POLY_FLAG_OUT_SIDE)
                        m_state |= HAS_ILLEGAL_GEOMETRY;
                }
            }

            // ----------------------------------------------------------
            // PushPortal
            //
            // Schickt eine KOPIE der Portal-Vertices durch den Teilbaum und
            // liefert pro erreichtem Leaf ein Fragment.
            // ----------------------------------------------------------

            void PushPortal(const std::vector<Vec3>& startVerts, std::vector<PortalFrag>& out, T eps)
            {
                struct Item
                {
                    BSPNode<T>* node;
                    std::vector<Vec3> verts;
                };

                std::vector<Item> stack;
                stack.push_back(Item{ this, startVerts });

                while (!stack.empty())
                {
                    Item item = std::move(stack.back());
                    stack.pop_back();

                    if (item.verts.size() < 3)
                        continue;

                    BSPNode<T>* n = item.node;

                    if (n->isLeaf())
                    {
                        out.push_back({ std::move(item.verts), n->m_id });
                        continue;
                    }

                    switch (ClassifyPoints(n->m_plane, item.verts, eps))
                    {
                    case Plane::FRONT:
                        if (n->m_front_node)
                            stack.push_back(Item{ n->m_front_node.get(), std::move(item.verts) });
                        break;

                    case Plane::BACK:
                        if (n->m_back_node)
                            stack.push_back(Item{ n->m_back_node.get(), std::move(item.verts) });
                        break;

                    case Plane::PLANAR:
                        // Nur numerisch moeglich. Nur EINE Seite, sonst Doppelportale.
                        if (n->m_front_node)
                            stack.push_back(Item{ n->m_front_node.get(), std::move(item.verts) });
                        break;

                    default: // SPANNING
                    {
                        std::vector<Vec3> f, b;
                        SplitPoints(item.verts, n->m_plane, eps, f, b);
                        if (n->m_front_node) stack.push_back(Item{ n->m_front_node.get(), std::move(f) });
                        if (n->m_back_node)  stack.push_back(Item{ n->m_back_node.get(),  std::move(b) });
                        break;
                    }
                    }
                }
            }

            // ----------------------------------------------------------
            // EmitPortalsForNode / BuildPortals (iterativ)
            //
            // "ancestry" enthaelt fuer jeden Vorfahren dessen Splitter-Ebene
            // und ob wir im Front- oder Back-Teilbaum liegen.
            // ----------------------------------------------------------

            // Erzeugt die Portale, die an der Splitter-Ebene DIESES Knotens entstehen.
            void EmitPortalsForNode(BSPTree<T>* tree,
                const std::vector<std::pair<Plane, bool>>& ancestry)
            {
                // 1. Quad in der Ebene, an die Weltbox geclippt
                Portal<T> portal(m_plane, tree->m_worldScale);
                auto box = tree->m_worldBox;
                ClipPortalByAABB(portal, box);

                // 2. Auf die Zelle dieses Knotens beschneiden
                std::vector<Vec3> verts = portal.verts();
                for (const auto& anc : ancestry)
                {
                    if (verts.size() < 3) break;

                    std::vector<Vec3> f, b;
                    SplitPoints(verts, anc.first, tree->m_epsilon, f, b);
                    verts = anc.second ? f : b;
                }

                if (verts.size() < 3)
                    return;

                // 3. Wand-Polygone in dieser Ebene sind keine Oeffnung
                std::vector<std::vector<Vec3>> pieces;
                pieces.push_back(verts);

                for (const auto& wall : m_coplanar)
                {
                    std::vector<std::vector<Vec3>> next;
                    for (const auto& piece : pieces)
                        SubtractPolygonFromPortal(piece, wall, next, tree->m_epsilon);
                    pieces.swap(next);
                }

                // 4. Front-Teilbaum, danach jedes Fragment durch den Back-Teilbaum
                const T minArea = ng::math::EPSILON<T>;
                const T minWidth = tree->m_minPortalWidth;

                for (const auto& piece : pieces)
                {
                    std::vector<PortalFrag> frontFrags;
                    m_front_node->PushPortal(piece, frontFrags, tree->m_epsilon);

                    for (const auto& ff : frontFrags)
                    {
                        std::vector<PortalFrag> backFrags;
                        m_back_node->PushPortal(ff.verts, backFrags, tree->m_epsilon);

                        for (const auto& bf : backFrags)
                        {
                            if (bf.verts.size() < 3) continue;
                            if (PortalArea(bf.verts) < minArea) continue;
                            if (PortalMinWidth(bf.verts) < minWidth) continue;

                            Portal<T> out(m_plane, bf.verts);
                            out.m_leafa = ff.leaf;   // FRONT-Seite
                            out.m_leafb = bf.leaf;   // BACK-Seite
                            tree->AddPortal(out);
                        }
                    }
                }
            }

            // Laeuft den Baum iterativ ab. "ancestry" haelt fuer den aktuellen Knoten
            // die Splitter-Ebenen aller Vorfahren (plus Front/Back-Seite).
            void BuildPortals(BSPTree<T>* tree)
            {
                struct Item
                {
                    BSPNode<T>* node;
                    size_t depth;      // Anzahl Vorfahren dieses Knotens
                    Plane parentPlane; // Eintrag, der an ancestry[depth-1] gehoert
                    bool isFront;
                };

                std::vector<std::pair<Plane, bool>> ancestry;
                std::vector<Item> stack;
                stack.push_back(Item{ this, 0, Plane(), true });

                size_t processed = 0;
                const auto t0 = std::chrono::steady_clock::now();

                while (!stack.empty())
                {
                    Item it = std::move(stack.back());
                    stack.pop_back();

                    if (it.depth > 0)
                    {
                        if (ancestry.size() > it.depth - 1)
                            ancestry.erase(ancestry.begin() + (it.depth - 1), ancestry.end());
                        ancestry.push_back({ it.parentPlane, it.isFront });
                    }

                    BSPNode<T>* n = it.node;

                    if (n->isLeaf() || !n->m_front_node || !n->m_back_node)
                        continue;

                    n->EmitPortalsForNode(tree, ancestry);

                    if (tree->m_verbose && (++processed % 1000) == 0)
                    {
                        const double sec = std::chrono::duration<double>(
                            std::chrono::steady_clock::now() - t0).count();
                        printf("[Portale] knoten=%zu portale=%zu tiefe=%zu  %.1fs\n",
                            processed, tree->m_portals.size(), it.depth, sec);
                    }

                    stack.push_back(Item{ n->m_back_node.get(),  it.depth + 1, n->m_plane, false });
                    stack.push_back(Item{ n->m_front_node.get(), it.depth + 1, n->m_plane, true });
                }
            }
        };

        // ------------------------------------------------------------
        // Freie Hilfsfunktion: BSP ohne Tree-Objekt (keine Leaf-IDs,
        // keine Portale). Fuer den normalen Workflow BSPTree::Build().
        // ------------------------------------------------------------

        template <class T>
        std::unique_ptr<BSPNode<T>> BuildBsp(const std::vector<BSPPolygon<T>>& polygons)
        {
            std::vector<BSPPolygon<T>> work;
            for (const auto& p : polygons)
            {
                if (p.verts.size() < 3) continue;
                work.push_back(p);
                work.back().flags &= ~static_cast<uint32_t>(POLY_FLAG_SPLITTER_USED);
            }
            return std::make_unique<BSPNode<T>>(nullptr, std::move(work));
        }

        // ------------------------------------------------------------
        // BSPTree
        // ------------------------------------------------------------

        template <class T>
        class BSPTree {
        public:
            using Vec3 = ng::math::Matrix<3, 1, T>;

            BSPTree() {}

            inline void Build(const std::vector<BSPPolygon<T>>& polys)
            {
                m_leaflist.clear();
                m_portals.clear();
                m_maxDepth = 0;

                std::vector<BSPPolygon<T>> work;
                work.reserve(polys.size());

                m_worldBox.Reset();
                T maxAbs = T(0);

                for (const auto& p : polys)
                {
                    if (p.verts.size() < 3) continue;

                    work.push_back(p);
                    work.back().flags &= ~static_cast<uint32_t>(POLY_FLAG_SPLITTER_USED);

                    m_worldBox += p.verts;

                    for (const auto& v : p.verts)
                    {
                        const auto w = v.world();
                        maxAbs = std::max({ maxAbs, std::abs(w(0)), std::abs(w(1)), std::abs(w(2)) });
                    }
                }

                m_worldScale = maxAbs * T(4) + T(1);

                m_root = std::make_shared<BSPNode<T>>(this, std::move(work));

                // Nur ein einziges Leaf: der Ctor registriert es nicht selbst.
                if (m_root && m_root->isLeaf() && m_leaflist.empty())
                    AddLeaf(m_root);

                if (m_verbose)
                    printf("[BSP] fertig: leaves=%zu tiefe=%d\n", m_leaflist.size(), m_maxDepth);
            }

            inline void BuildPortals()
            {
                if (!m_root) return;
                m_portals.clear();
                m_root->BuildPortals(this);

                if (m_verbose)
                    printf("[Portale] fertig: %zu Portale\n", m_portals.size());
            }

            // Portale schmaler als dieser Wert werden verworfen (Nahtreste).
            inline void SetMinPortalWidth(T w) { m_minPortalWidth = w; }

            // Tiefe des zuletzt gebauten Baums (zur Diagnose).
            inline int GetMaxDepth() const { return m_maxDepth; }

            // Fortschrittsausgaben (printf) an/aus.
            inline void SetVerbose(bool v) { m_verbose = v; }

            // Abstandstoleranz fuer Klassifikation/Teilung (Weltunits).
            // Vor Build() setzen. Zu klein: Streifen/Doppelpolygone bei schraegen
            // Flaechen. Zu gross: Details werden verschluckt.
            inline void SetEpsilon(T e) { m_epsilon = e; }

            inline std::shared_ptr<BSPNode<T>>& GetRoot() { return m_root; }
            inline const std::vector<std::shared_ptr<BSPNode<T>>>& GetLeaves() const { return m_leaflist; }
            inline const std::vector<Portal<T>>& GetPortals() const { return m_portals; }

            // --------------------------------------------------------
            // Punkt -> Leaf-ID (-1 wenn kein Baum)
            // --------------------------------------------------------
            inline int32_t FindLeafId(const Vec3& p) const
            {
                const BSPNode<T>* n = m_root.get();

                while (n && !(n->m_state & LEAF))
                {
                    const bool back =
                        n->m_plane.classify(p) == ng::math::RayPlaneLine<3, T>::BACK;
                    n = back ? n->m_back_node.get() : n->m_front_node.get();
                }

                return n ? n->m_id : -1;
            }

            // --------------------------------------------------------
            // Flood Fill ueber die Portale, transitiv.
            // Rueckgabe: pro Leaf 1 = erreicht.
            // --------------------------------------------------------
            inline std::vector<uint8_t> Flood(const std::vector<int32_t>& seeds) const
            {
                const int32_t n = (int32_t)m_leaflist.size();

                std::vector<std::vector<int32_t>> adj(n);
                for (const auto& p : m_portals)
                {
                    const int32_t a = p.leafFront();
                    const int32_t b = p.leafBack();
                    if (a < 0 || b < 0 || a >= n || b >= n) continue;
                    adj[a].push_back(b);
                    adj[b].push_back(a);
                }

                std::vector<uint8_t> reached(n, 0);
                std::vector<int32_t> stack;

                for (int32_t s : seeds)
                {
                    if (s < 0 || s >= n || reached[s]) continue;
                    reached[s] = 1;
                    stack.push_back(s);
                }

                while (!stack.empty())
                {
                    const int32_t c = stack.back();
                    stack.pop_back();

                    for (int32_t nb : adj[c])
                    {
                        if (!reached[nb])
                        {
                            reached[nb] = 1;
                            stack.push_back(nb);
                        }
                    }
                }

                return reached;
            }

            // Seeds = alle Leaves, die das uebergebene Flag tragen.
            // Standard: HAS_ILLEGAL_GEOMETRY = Leaves mit Box-Polygonen.
            inline std::vector<uint8_t> FloodFromFlag(uint32_t flag = HAS_ILLEGAL_GEOMETRY) const
            {
                std::vector<int32_t> seeds;
                for (size_t i = 0; i < m_leaflist.size(); ++i)
                    if (m_leaflist[i]->State() & flag)
                        seeds.push_back((int32_t)i);

                return Flood(seeds);
            }

            // --------------------------------------------------------
            // Polygone in erreichten Leaves loeschen. Rueckgabe: Anzahl.
            // --------------------------------------------------------
            inline size_t RemoveReachedPolygons(const std::vector<uint8_t>& reached)
            {
                size_t removed = 0;

                for (size_t i = 0; i < m_leaflist.size() && i < reached.size(); ++i)
                {
                    if (!reached[i]) continue;

                    removed += m_leaflist[i]->polys.size();
                    m_leaflist[i]->DeletePolys();
                    m_leaflist[i]->State() |= INTOTHEVOID;
                }

                return removed;
            }

            inline std::vector<BSPPolygon<T>> CollectRemainingPolygons() const
            {
                std::vector<BSPPolygon<T>> out;

                for (const auto& leaf : m_leaflist)
                    out.insert(out.end(), leaf->polys.begin(), leaf->polys.end());

                return out;
            }

            // --------------------------------------------------------
            // CollapseSmallCells
            //
            // Fasst Teilbaeume zu EINEM Leaf zusammen, wenn darin nur wenig
            // "Innenwand" steckt: Summe der Flaeche der Polygone, die INNERHALB des
            // Teilbaums als Splitter verbraucht wurden (Waende auf dem Rand der Zelle
            // zaehlen nicht). Ein Teilbaum ist eine konvexe Zelle - das Ergebnis
            // bleibt also ein gueltiger BSP mit konvexen Leaves.
            //
            // Konservativ fuer die Sichtbarkeit: Waende im Inneren werden ignoriert
            // (mehr sichtbar, nie weniger).
            //
            // Danach BuildPortals() NEU ausfuehren: Portale zwischen den
            // zusammengefassten Leaves verschwinden, in Stuecke zerlegte Portale
            // nach aussen werden wieder ein Portal.
            //
            // NUR fuer Vis-Baeume! Nicht fuer den Baum des Flood Fill benutzen
            // (der braucht exakte Zellgrenzen).
            //
            // Rueckgabe: Anzahl zusammengefasster Teilbaeume.
            // --------------------------------------------------------
            inline size_t CollapseSmallCells(T maxOccluderArea)
            {
                if (!m_root || m_root->isLeaf()) return 0;

                // 1. Pre-Order-Liste aller Knoten mit Elternindex
                //    (Eltern haben immer einen kleineren Index als ihre Kinder).
                std::vector<BSPNode<T>*> nodes;
                std::vector<int32_t> parent;
                {
                    struct Item { BSPNode<T>* node; int32_t parent; };
                    std::vector<Item> stack;
                    stack.push_back(Item{ m_root.get(), -1 });

                    while (!stack.empty())
                    {
                        const Item it = stack.back();
                        stack.pop_back();

                        const int32_t idx = (int32_t)nodes.size();
                        nodes.push_back(it.node);
                        parent.push_back(it.parent);

                        if (!it.node->isLeaf())
                        {
                            if (it.node->m_back_node)  stack.push_back(Item{ it.node->m_back_node.get(),  idx });
                            if (it.node->m_front_node) stack.push_back(Item{ it.node->m_front_node.get(), idx });
                        }
                    }
                }

                // 2. Innenwand-Flaeche pro Teilbaum (Kinder vor Eltern aufsummieren)
                std::vector<T> occ(nodes.size(), T(0));

                for (size_t i = 0; i < nodes.size(); ++i)
                {
                    if (nodes[i]->isLeaf()) continue;
                    for (const auto& p : nodes[i]->m_coplanar)
                        occ[i] += VertsArea(p.verts);
                }

                for (size_t i = nodes.size(); i-- > 1;)
                    occ[parent[i]] += occ[i];

                // 3. Groebste Aufteilung: 0 = behalten, 1 = Wurzel eines zusammengefassten
                //    Teilbaums, 2 = liegt innerhalb eines solchen.
                std::vector<uint8_t> state(nodes.size(), 0);

                for (size_t i = 0; i < nodes.size(); ++i)
                {
                    if (parent[i] >= 0 && state[parent[i]] != 0) { state[i] = 2; continue; }
                    if (!nodes[i]->isLeaf() && occ[i] <= maxOccluderArea) state[i] = 1;
                }

                // 4. Zusammenfassen
                size_t collapsed = 0;

                for (size_t i = 0; i < nodes.size(); ++i)
                {
                    if (state[i] != 1) continue;

                    BSPNode<T>* n = nodes[i];

                    std::vector<BSPPolygon<T>> gathered;
                    uint32_t mergedState = 0;

                    std::vector<BSPNode<T>*> stack;
                    stack.push_back(n->m_front_node.get());
                    stack.push_back(n->m_back_node.get());

                    while (!stack.empty())
                    {
                        BSPNode<T>* c = stack.back();
                        stack.pop_back();
                        if (!c) continue;

                        if (c->isLeaf())
                        {
                            for (auto& poly : c->polys) gathered.push_back(std::move(poly));
                            c->polys.clear();
                            mergedState |= (c->m_state & ~static_cast<uint32_t>(NODE | LEAF));
                            continue;
                        }

                        stack.push_back(c->m_front_node.get());
                        stack.push_back(c->m_back_node.get());
                    }

                    n->m_front_node.reset();
                    n->m_back_node.reset();
                    n->m_coplanar.clear();
                    n->m_coplanar.shrink_to_fit();
                    n->polys = std::move(gathered);
                    n->m_state = static_cast<uint32_t>(LEAF) | mergedState;

                    ++collapsed;
                }

                // 5. Leaf-Liste mit neuen IDs aufbauen, alte Portale sind ungueltig
                m_leaflist.clear();
                m_portals.clear();

                std::vector<std::shared_ptr<BSPNode<T>>> st;
                st.push_back(m_root);

                while (!st.empty())
                {
                    std::shared_ptr<BSPNode<T>> c = std::move(st.back());
                    st.pop_back();

                    if (c->isLeaf()) { AddLeaf(c); continue; }

                    if (c->m_back_node)  st.push_back(c->m_back_node);
                    if (c->m_front_node) st.push_back(c->m_front_node);
                }

                return collapsed;
            }

            // --------------------------------------------------------
            // Leaves mit einem State-Flag sammeln
            // --------------------------------------------------------
            inline std::vector<std::shared_ptr<BSPNode<T>>>
                CollectLeavesWithFlag(uint32_t flag) const
            {
                std::vector<std::shared_ptr<BSPNode<T>>> result;

                for (const auto& leaf : m_leaflist)
                    if (leaf && (leaf->State() & flag) != 0)
                        result.push_back(leaf);

                return result;
            }

            // Direkte Nachbar-Leaves ueber Portale (eine Stufe).
            inline std::vector<std::shared_ptr<BSPNode<T>>>
                FindNeighborLeaves(const std::vector<std::shared_ptr<BSPNode<T>>>& sources) const
            {
                const int32_t leafCount = (int32_t)m_leaflist.size();

                std::vector<uint8_t> isSource(leafCount, 0);
                std::vector<uint8_t> added(leafCount, 0);

                for (const auto& s : sources)
                {
                    if (!s) continue;
                    const int32_t id = s->GetID();
                    if (id >= 0 && id < leafCount) isSource[id] = 1;
                }

                std::vector<std::shared_ptr<BSPNode<T>>> result;

                for (const auto& portal : m_portals)
                {
                    const int32_t a = portal.leafFront();
                    const int32_t b = portal.leafBack();

                    if (a < 0 || b < 0 || a >= leafCount || b >= leafCount)
                        continue;

                    int32_t neighbor = -1;

                    if (isSource[a] && !isSource[b])      neighbor = b;
                    else if (isSource[b] && !isSource[a]) neighbor = a;

                    if (neighbor < 0 || added[neighbor])
                        continue;

                    added[neighbor] = 1;
                    result.push_back(m_leaflist[neighbor]);
                }

                return result;
            }

        private:
            friend class BSPNode<T>;

            std::shared_ptr<BSPNode<T>> m_root;
            std::vector<std::shared_ptr<BSPNode<T>>> m_leaflist;
            std::vector<Portal<T>> m_portals;

            ng::bounding::AABBox<3, T> m_worldBox;
            T m_worldScale = T(10000);
            T m_minPortalWidth = T(0.05);
            int m_maxDepth = 0;
            bool m_verbose = true;
            T m_epsilon = T(0.01);   // Abstandstoleranz in Weltunits

            inline void AddLeaf(std::shared_ptr<BSPNode<T>> leaf)
            {
                leaf->SetId((int32_t)m_leaflist.size());
                m_leaflist.push_back(leaf);
            }

            inline void AddPortal(const Portal<T>& portal)
            {
                m_portals.push_back(portal);
            }
        };

    } // namespace bsp
} // namespace ng