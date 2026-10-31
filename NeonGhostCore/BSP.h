#pragma once

#include "Matrix.h"
#include "Vertex.h"
#include "RayPlaneLine.h"
#include "Clipping.h"
#include "AABBox.h"
#include "Texture.h"

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
#include <random>
#include "WarpPortal.h"
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
            HAS_ILLEGAL_GEOMETRY = 1 << 6,
            HAS_EXTRA_POLYS = 1 << 7,      // Leaf enthaelt Extra-Polygone (Wasser, Glas, ...)
            HAS_VOLUME = 1 << 8            // Leaf enthaelt (Teile von) Volumen-Brushes
        };

        enum : uint32_t
        {
            POLY_FLAG_NONE = 0,
            POLY_FLAG_OUT_SIDE = 1 << 1,        // Polygone der Umschliessungsbox (Normale nach innen!)
            POLY_FLAG_SPLITTER_USED = 1 << 2,   // intern: Ebene wurde schon als Splitter benutzt
            POLY_FLAG_VOLUME_FACE = 1 << 3,     // Flaeche eines eingefuegten Volumen-Brushes
            POLY_FLAG_VOLUME_CAP = 1 << 4,       // beim Klippen erzeugte Schnittflaeche (Deckel)
            POLY_FLAG_PORTAL = 1 << 5          // Face eines Warp-Portals

        };

        // ------------------------------------------------------------
        // BSPPolygon
        // ------------------------------------------------------------

        template<class T>
        struct BSPPolygon
        {
            using Plane = math::RayPlaneLine<3, T>;
            using Vertex = ng::geometry::Vertex<T>;
            ng::renderer::gfx::Texture texture;
            std::vector<Vertex> verts;
            Plane plane;

            int materialId = -1;
            uint32_t objectId = 0;
            uint32_t flags = 0;
            int volumeId = -1;
            int32_t portalId = -1;

            // ---- Lightmap-Zuordnung, gefuellt von BuildLightmapAtlas() /
            //      BakeExtraPolygonLightmaps() ----
            int32_t lmPosX = -1, lmPosY = -1;
            int32_t lmWidth = 0, lmHeight = 0;
            math::Matrix<3, 1, T> lmOrigin;
            math::Matrix<3, 1, T> lmEdgeU;
            math::Matrix<3, 1, T> lmEdgeV;


            // Konvertiert ein BrushPoly (oder jeden Typ mit verts(), plane(), textureIndex())
            // in ein BSPPolygon. Das Polygon bleibt konvex und wird NICHT trianguliert.
            //
            // WICHTIG: Die UV bleiben in Texel-Einheiten, weil Brush::ApplyUVs() mit
            // texW=texH=1 rechnet. FinalizeTextureUVs() muss danach GENAU EINMAL
            // aufgerufen werden, nachdem "texture" gesetzt ist.
            template <class P>
            static BSPPolygon FromBrushPoly(const P& src, uint32_t objectId = 0,
                const ng::renderer::gfx::Texture& tex = ng::renderer::gfx::Texture())
            {
                BSPPolygon poly;
                poly.verts = src.verts();
                poly.plane = src.plane();
                poly.materialId = src.textureIndex();   // Texturindex, fuer data.m_id_text[...]
                poly.objectId = objectId;
                poly.texture = tex;                     // optional, sonst spaeter setzen
                return poly;
            }

            // Nach dem Erzeugen von polys/extra_polys, bevor der BSP gebaut wird:
            
            void FinalizeTextureUVs( bool flipV=false)
            {
                
                
                if (!texture) return;
                    const T invW = T(1) / T(std::max(1, texture->Width()));
                    const T invH = T(1) / T(std::max(1, texture->Height()));

                    T minU = std::numeric_limits<T>::max();
                    T minV = std::numeric_limits<T>::max();
                    for (auto& v : verts)
                    {
                        T u = v.texture().x() * invW;
                        T t = v.texture().y() * invH;
                        if (flipV) t = T(1) - t;
                        v.texture().x() = u;
                        v.texture().y() = t;
                        minU = std::min(minU, u);
                        minV = std::min(minV, t);
                    }

                    // Ganzzahliger Versatz: Wiederholung bleibt exakt erhalten
                    const T du = std::floor(minU), dv = std::floor(minV);
                    for (auto& v : verts)
                    {
                        v.texture().x() -= du;
                        v.texture().y() -= dv;
                    }
                
            }

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
            out.fxtexture() = a.fxtexture() + (b.fxtexture() - a.fxtexture()) * t; // NEU: Lightmap-UV interpolieren
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
            frontOut.texture = backOut.texture = poly.texture;
            frontOut.portalId = backOut.portalId = poly.portalId;

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
        inline bool SamePlane(
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
        inline bool Coplanar(
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
        inline bool AddUniquePlane(
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
        inline PlaneKey MakePlaneKey(const math::RayPlaneLine<3, T>& plane)
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
        inline bool FindBestSplitter(
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
        inline bool ClipPortalByAABB(Portal<T>& portal, ng::bounding::AABBox<3, T>& bbox)
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
        inline T PortalArea(const std::vector<ng::math::Matrix<3, 1, T>>& v)
        {
            ng::math::Matrix<3, 1, T> sum(0, 0, 0);
            for (size_t i = 1; i + 1 < v.size(); ++i)
                sum = sum + ((v[i] - v[0]) % (v[i + 1] - v[0]));
            return sum.length() * T(0.5);
        }

        template <class T>
        inline T PortalPerimeter(const std::vector<ng::math::Matrix<3, 1, T>>& v)
        {
            T s = T(0);
            for (size_t i = 0; i < v.size(); ++i)
                s += (v[(i + 1) % v.size()] - v[i]).length();
            return s;
        }

        // Grobe "Breite": 2 * Flaeche / Umfang. Duenne Streifen (Nahtreste) haben
        // eine sehr kleine Breite, echte Oeffnungen nicht.
        template <class T>
        inline T PortalMinWidth(const std::vector<ng::math::Matrix<3, 1, T>>& v)
        {
            const T per = PortalPerimeter(v);
            if (per <= T(0)) return T(0);
            return T(2) * PortalArea(v) / per;
        }

        // Zieht ein (konvexes) Wand-Polygon, das in derselben Ebene liegt, von
        // einem konvexen Portal ab. Liefert die konvexen Reststuecke AUSSERHALB
        // des Polygons.
        template <class T>
        inline void SubtractPolygonFromPortal(
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
        // Volumen-Stuecke
        //
        // Ein Volumen (Wasser, Lava, ...) ist ein geschlossener KONVEXER Brush.
        // Beim Einfuegen in den BSP wird er an den Splitter-Ebenen geteilt. Damit
        // jedes Stueck geschlossen bleibt, bekommt es an der Schnittebene einen
        // Deckel (POLY_FLAG_VOLUME_CAP). Normalen zeigen nach INNEN.
        // ------------------------------------------------------------

        template <class T>
        struct BSPVolumePiece
        {
            using Vec3 = ng::math::Matrix<3, 1, T>;

            int volumeId = -1;
            uint32_t medium = 0;                 // frei waehlbare Flags (z.B. Wasser/Lava)
            std::vector<BSPPolygon<T>> faces;    // geschlossen, konvex, Normalen nach innen

            // Liegt der Punkt im Stueck? (Innenseite = Vorderseite jeder Ebene)
            bool Contains(const Vec3& p, T eps) const
            {
                if (faces.empty()) return false;

                for (const auto& f : faces)
                    if (f.plane.distance(p) < -eps)
                        return false;

                return true;
            }
        };

        // Baut die Ebene eines Polygons aus seinen Vertices so, dass ihre Normale
        // zu "desiredNormal" zeigt. Dreht dazu ggf. die Vertex-Reihenfolge um.
        template <class T>
        inline bool BuildOrientedPlane(std::vector<ng::geometry::Vertex<T>>& verts,
            const ng::math::Matrix<3, 1, T>& desiredNormal,
            ng::math::RayPlaneLine<3, T>& out)
        {
            using Vec3 = ng::math::Matrix<3, 1, T>;
            using Plane = ng::math::RayPlaneLine<3, T>;

            const size_t n = verts.size();
            if (n < 3) return false;

            // Dreieck (0, 1, k) mit der groessten Flaeche
            const Vec3 a = verts[0].world();
            const Vec3 b = verts[1].world();

            size_t bestK = 0;
            T bestLen = T(0);

            for (size_t k = 2; k < n; ++k)
            {
                const T len = ((b - a) % (verts[k].world() - a)).length();
                if (len > bestLen) { bestLen = len; bestK = k; }
            }

            if (bestK == 0 || bestLen <= T(1e-12))
                return false;

            const Vec3 c = verts[bestK].world();

            Plane pl(a, b, c);
            const Vec3 nn = pl.normal();
            const T dot = nn(0) * desiredNormal(0) + nn(1) * desiredNormal(1) + nn(2) * desiredNormal(2);

            if (dot < T(0))
            {
                pl = Plane(a, c, b);
                std::reverse(verts.begin(), verts.end());
            }

            out = pl;
            return true;
        }

        // FRONT (auch "ganz auf der Ebene"), BACK oder SPANNING
        template <class T>
        inline int ClassifyVolumePiece(const ng::math::RayPlaneLine<3, T>& plane,
            const BSPVolumePiece<T>& piece, T eps)
        {
            using Plane = ng::math::RayPlaneLine<3, T>;

            bool front = false, back = false;

            for (const auto& f : piece.faces)
            {
                for (const auto& v : f.verts)
                {
                    const T d = plane.distance(v.world());
                    if (d > eps)       front = true;
                    else if (d < -eps) back = true;

                    if (front && back) return Plane::SPANNING;
                }
            }

            return back ? Plane::BACK : Plane::FRONT;
        }

        // Teilt ein konvexes Volumen-Stueck an einer Ebene und schliesst beide
        // Teile mit einem Deckel.
        template <class T>
        inline void SplitVolumePiece(const BSPVolumePiece<T>& in,
            const ng::math::RayPlaneLine<3, T>& plane, T eps,
            BSPVolumePiece<T>& front, BSPVolumePiece<T>& back)
        {
            using Vec3 = ng::math::Matrix<3, 1, T>;
            using Plane = ng::math::RayPlaneLine<3, T>;

            front.volumeId = back.volumeId = in.volumeId;
            front.medium = back.medium = in.medium;
            front.faces.clear();
            back.faces.clear();

            const T minArea = eps * eps;

            for (const auto& f : in.faces)
            {
                switch (ClassifyVerts(plane, f.verts, eps))
                {
                case Plane::FRONT:
                    front.faces.push_back(f);
                    break;

                case Plane::BACK:
                    back.faces.push_back(f);
                    break;

                case Plane::PLANAR:
                {
                    const Vec3 a = plane.normal();
                    const Vec3 c = f.plane.normal();
                    if (a(0) * c(0) + a(1) * c(1) + a(2) * c(2) >= T(0))
                        front.faces.push_back(f);
                    else
                        back.faces.push_back(f);
                    break;
                }

                default: // SPANNING
                {
                    BSPPolygon<T> fp, bp;
                    SplitPolygon(plane, f, eps, fp, bp);

                    if (fp.verts.size() >= 3 && VertsArea(fp.verts) > minArea) front.faces.push_back(std::move(fp));
                    if (bp.verts.size() >= 3 && VertsArea(bp.verts) > minArea) back.faces.push_back(std::move(bp));
                    break;
                }
                }
            }

            // Alle Punkte auf der Trennebene sammeln -> Umriss des Deckels
            std::vector<Vec3> pts;

            auto collect = [&](const std::vector<BSPPolygon<T>>& faces)
                {
                    for (const auto& f : faces)
                    {
                        for (const auto& v : f.verts)
                        {
                            const Vec3 p = v.world();
                            if (std::abs(plane.distance(p)) > eps) continue;

                            bool dup = false;
                            for (const auto& q : pts)
                                if ((q - p).length() <= eps * T(2)) { dup = true; break; }

                            if (!dup) pts.push_back(p);
                        }
                    }
                };

            collect(front.faces);
            collect(back.faces);

            if (pts.size() < 3) return;

            Vec3 c(0, 0, 0);
            for (const auto& p : pts) c = c + p;
            c = c * (T(1) / T(pts.size()));

            const Vec3 n = plane.normal();

            // Basis in der Ebene, Punkte nach Winkel sortieren
            Vec3 axis(0, 0, 0);
            if (std::abs(n(0)) <= std::abs(n(1)) && std::abs(n(0)) <= std::abs(n(2))) axis(0) = T(1);
            else if (std::abs(n(1)) <= std::abs(n(2)))                                axis(1) = T(1);
            else                                                                      axis(2) = T(1);

            Vec3 u = axis % n;
            u.normalize();
            Vec3 w = n % u;
            w.normalize();

            std::vector<std::pair<T, size_t>> ang;
            ang.reserve(pts.size());
            for (size_t i = 0; i < pts.size(); ++i)
            {
                const Vec3 d = pts[i] - c;
                const T du = d(0) * u(0) + d(1) * u(1) + d(2) * u(2);
                const T dw = d(0) * w(0) + d(1) * w(1) + d(2) * w(2);
                ang.emplace_back(std::atan2(dw, du), i);
            }
            std::sort(ang.begin(), ang.end(),
                [](const std::pair<T, size_t>& a, const std::pair<T, size_t>& b) { return a.first < b.first; });

            std::vector<ng::geometry::Vertex<T>> capVerts;
            capVerts.reserve(pts.size());
            for (const auto& a : ang)
            {
                ng::geometry::Vertex<T> vt;
                vt.world() = pts[a.second];
                capVerts.push_back(vt);
            }

            if (VertsArea(capVerts) <= minArea) return;

            // Deckel des Vorderstuecks: Normale +n (Innenraum liegt vorne)
            {
                BSPPolygon<T> cap;
                cap.verts = capVerts;
                cap.volumeId = in.volumeId;
                cap.flags = POLY_FLAG_VOLUME_CAP;
                if (BuildOrientedPlane(cap.verts, n, cap.plane))
                    front.faces.push_back(std::move(cap));
            }

            // Deckel des Hinterstuecks: Normale -n
            {
                BSPPolygon<T> cap;
                cap.verts = capVerts;
                cap.volumeId = in.volumeId;
                cap.flags = POLY_FLAG_VOLUME_CAP;
                if (BuildOrientedPlane(cap.verts, n * T(-1), cap.plane))
                    back.faces.push_back(std::move(cap));
            }
        }

        struct PosKey { int64_t x, y, z; bool operator==(const PosKey& o) const { return x == o.x && y == o.y && z == o.z; } };
        struct PosHash { size_t operator()(const PosKey& k) const { return (size_t)(k.x * 73856093 ^ k.y * 19349663 ^ k.z * 83492791); } };

        template <class T>
        void SmoothNormals(std::vector<ng::bsp::BSPPolygon<T>>& polys, T maxAngleDeg = T(60))
        {
            using Vec3 = ng::math::Matrix<3, 1, T>;
            const T cosLimit = std::cos(maxAngleDeg * T(3.14159265358979) / T(180));
            auto key = [](const Vec3& p) {
                return PosKey{ (int64_t)std::llround((double)p(0) * 100.0),
                               (int64_t)std::llround((double)p(1) * 100.0),
                               (int64_t)std::llround((double)p(2) * 100.0) };
                };

            // Position -> Flaechennormalen (gewichtet mit der Flaeche)
            std::unordered_map<PosKey, std::vector<Vec3>, PosHash> map;
            for (auto& p : polys)
            {
                const Vec3 n = p.plane.normal();
                const Vec3 wn = n * ng::bsp::VertsArea(p.verts);
                for (auto& v : p.verts) map[key(v.world())].push_back(wn);
            }

            for (auto& p : polys)
            {
                const Vec3 fn = p.plane.normal();
                for (auto& v : p.verts)
                {
                    Vec3 sum(0, 0, 0);
                    for (const auto& wn : map[key(v.world())])
                    {
                        const T len = wn.length();
                        if (len < T(1e-12)) continue;
                        if ((wn ^ fn) / len >= cosLimit) sum = sum + wn;   // nur aehnliche Flaechen mitteln
                    }
                    const T l = sum.length();
                    v.normal() = (l > T(1e-12)) ? sum * (T(1) / l) : fn;
                }
            }
        }

        
        template<class T>
        void SmoothNormals(
            const std::vector<ng::bsp::BSPPolygon<T>*>& polys,
            T maxAngleDeg = T(60))
        {
            using Vec3 = ng::math::Matrix<3, 1, T>;

            const T cosLimit =
                std::cos(maxAngleDeg * T(3.14159265358979) / T(180));

            auto key = [](const Vec3& p)
                {
                    return PosKey{
                        (int64_t)std::llround((double)p(0) * 100.0),
                        (int64_t)std::llround((double)p(1) * 100.0),
                        (int64_t)std::llround((double)p(2) * 100.0)
                    };
                };

            // Weltposition -> flächengewichtete Normalen aller angrenzenden Polygone
            std::unordered_map<PosKey, std::vector<Vec3>, PosHash> map;

            for (const auto* poly : polys)
            {
                if (!poly || poly->verts.size() < 3)
                    continue;

                const Vec3 n = poly->plane.normal();
                const Vec3 wn = n * ng::bsp::VertsArea(poly->verts);

                for (const auto& v : poly->verts)
                    map[key(v.world())].push_back(wn);
            }

            // Direkt die Originalvertices im BSP aktualisieren
            for (auto* poly : polys)
            {
                if (!poly || poly->verts.size() < 3)
                    continue;
              
                const Vec3 fn = poly->plane.normal();

                for (auto& v : poly->verts)
                {
                    Vec3 sum(0, 0, 0);

                    const auto it = map.find(key(v.world()));
                    if (it == map.end())
                    {
                        v.normal() = fn;
                        continue;
                    }

                    for (const auto& wn : it->second)
                    {
                        const T len = wn.length();
                        if (len < T(1e-12))
                            continue;

                        // Nur Flaechennormalen innerhalb des Winkels einbeziehen
                        if ((wn ^ fn) / len >= cosLimit)
                            sum = sum + wn;
                    }

                    const T len = sum.length();
                    v.normal() =
                        (len > T(1e-12))
                        ? sum * (T(1) / len)
                        : fn;
                }
            }

          
        }

        template <class T>
        struct BSPOptions
        {
            bool fixTJunctions = true;
            bool rebuild_normals = true;
        };

        
        
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
            inline std::shared_ptr<BSPNode<T>> GetFrontNode() const { return m_front_node; }  // NEU: const
            inline std::shared_ptr<BSPNode<T>> GetBackNode() const { return m_back_node; }    // NEU: const
            inline const BSPNode<T>* FrontPtr() const { return m_front_node.get(); }
            inline const BSPNode<T>* BackPtr()  const { return m_back_node.get(); }
            inline const std::vector<BSPPolygon<T>>& GetPolygons() const { return polys; }

            // Extra-Polygone (Wasser, Glas, ...) und Volumen-Stuecke dieses Leaves.
            inline const std::vector<BSPPolygon<T>>& GetExtraPolygons() const { return extraPolys; }
            inline const std::vector<BSPVolumePiece<T>>& GetVolumes() const { return volumes; }
            inline void DeleteExtra()
            {
                extraPolys.clear();
                volumes.clear();
                m_state &= ~static_cast<uint32_t>(HAS_EXTRA_POLYS | HAS_VOLUME);
            }

            inline Plane GetPlane() const { return m_plane; }
            inline int32_t GetID() const { return m_id; }
            inline uint32_t& State() { return m_state; }
            inline const uint32_t& State() const { return m_state; }

            inline void SetSolid() { m_state |= SOLID; }
            inline void DeletePolys() { polys.clear(); }

            inline const Plane& PlaneRef()      const { return m_plane; }

        private:
            friend class BSPTree<T>;

            struct PortalFrag {
                std::vector<Vec3> verts;
                int32_t leaf;
            };

            std::vector<BSPPolygon<T>> polys;          // nur in Leaves
            std::vector<BSPPolygon<T>> extraPolys;     // nur in Leaves: Wasser, Glas, ...
            std::vector<BSPVolumePiece<T>> volumes;    // nur in Leaves: Stuecke von Volumen-Brushes
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
                    const size_t before = tree->m_portals.size();   // vor dem Aufruf merken
                    n->EmitPortalsForNode(tree, ancestry);

                    const size_t made = tree->m_portals.size() - before;
                    if (made > 10000)
                        printf("[Portale] Knoten tiefe=%zu erzeugt %zu Portale\n", it.depth, made);
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
        inline std::unique_ptr<BSPNode<T>> BuildBsp(const std::vector<BSPPolygon<T>>& polygons)
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

            inline size_t FixTJunctions()
            {
                using Vec3 = ng::math::Matrix<3, 1, T>;

                if (!m_root || m_leaflist.empty())
                    return 0;

                const T eps = m_epsilon;
                const T eps2 = eps * eps;

                // 1. Alle eindeutigen Vertices aus den Leaf-Polygonen sammeln.
                std::vector<Vec3> points;

                auto dist2 = [](const Vec3& a, const Vec3& b) -> T
                    {
                        const Vec3 d = a - b;
                        return d ^ d; // Nur verwenden, falls Matrix * Skalarprodukt liefert.
                    };

                for (const auto& leaf : m_leaflist)
                {
                    if (!leaf) continue;

                    for (const auto& poly : leaf->polys)
                    {
                        for (const auto& vertex : poly.verts)
                        {
                            const Vec3 p = vertex.world();

                            bool found = false;
                            for (const auto& q : points)
                            {
                                if (dist2(p, q) <= eps2)
                                {
                                    found = true;
                                    break;
                                }
                            }

                            if (!found)
                                points.push_back(p);
                        }
                    }
                }

                size_t inserted = 0;

                // 2. Jede Kante um Vertices ergänzen, die auf ihr liegen.
                for (const auto& leaf : m_leaflist)
                {
                    if (!leaf) continue;

                    for (auto& poly : leaf->polys)
                    {
                        if (poly.verts.size() < 3)
                            continue;

                        using Vertex = typename std::decay_t<
                            decltype(poly.verts)>::value_type;

                        std::vector<Vertex> fixed;
                        fixed.reserve(poly.verts.size() + 8);

                        const size_t count = poly.verts.size();

                        for (size_t i = 0; i < count; ++i)
                        {
                            const Vertex& va = poly.verts[i];
                            const Vertex& vb = poly.verts[(i + 1) % count];

                            const Vec3 a = va.world();
                            const Vec3 b = vb.world();
                            const Vec3 ab = b - a;

                            const T len2 = ab ^ ab;

                            fixed.push_back(va);

                            if (len2 <= eps2)
                                continue;

                            struct Candidate
                            {
                                T t;
                                Vec3 p;
                            };

                            std::vector<Candidate> candidates;

                            for (const auto& p : points)
                            {
                                const Vec3 ap = p - a;
                                const T t = (ap ^ ab) / len2;

                                // Endpunkte nicht erneut einfügen.
                                if (t <= eps / std::sqrt(len2) ||
                                    t >= T(1) - eps / std::sqrt(len2))
                                    continue;

                                const Vec3 closest = a + ab * t;
                                const Vec3 delta = p - closest;
                                const T dd = (delta ^ delta);
                                if (dd <= eps2)
                                    candidates.push_back({ t, p });
                            }

                            std::sort(candidates.begin(), candidates.end(),
                                [](const Candidate& lhs, const Candidate& rhs)
                                {
                                    return lhs.t < rhs.t;
                                });
/*
                            for (const auto& c : candidates)
                            {
                                // Wichtig: Das ist zunächst nur eine geometrische
                                // Position. Ein echter Vertex muss seine Attribute
                                // aus der ursprünglichen Kante interpolieren.
                                Vertex v = va;

                                // Diese Zuweisung ggf. an deine Vertex-API anpassen.
                                v.world() = c.p;

                                fixed.push_back(std::move(v));
                                ++inserted;
                            }
                        */
                            for (const auto& c : candidates)
                            {
                                // t ist der Parameter auf der Kante a->b, hier schon berechnet
                                Vertex v = LerpVertex(va, vb, c.t);
                                v.world() = c.p;   // exakte Position des Nachbar-Vertex übernehmen (kein Spalt)
                                fixed.push_back(std::move(v));
                                ++inserted;
                            }
                        }

                        poly.verts = std::move(fixed);
                    }
                }

                return inserted;
            }

       
            void CollectPolygonPointers(
                std::shared_ptr<BSPNode<T>> node,
                std::vector<ng::bsp::BSPPolygon<T>*>& out)
            {
                if (!node)
                    return;

                if (node->m_state == LEAF)
                {
                    for (auto& poly : node->polys)
                        out.push_back(&poly);

                    for (auto& poly : node->extraPolys)
                        out.push_back(&poly);

                    return;
                }

                CollectPolygonPointers(node->m_front_node, out);
                CollectPolygonPointers(node->m_back_node, out);
            }


            inline void Build(const std::vector<BSPPolygon<T>>& polys,const BSPOptions<T>&options= BSPOptions<T>())
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

                

                if (options.fixTJunctions) {
                    printf("[BSP] Fix TJunctions\n");
                    FixTJunctions();
                }

                if (options.rebuild_normals && m_root) {
                    std::vector<ng::bsp::BSPPolygon<T>*> out;
                    

                    CollectPolygonPointers(m_root, out);
                    printf("[BSP] Collect Polygonpointers %llu\n", out.size());
                    printf("[BSP] Smooth normals \n");

                    SmoothNormals(out);
                }

                

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
            inline const std::shared_ptr<BSPNode<T>>& GetRoot() const { return m_root; }
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
                    m_leaflist[i]->DeleteExtra();
                    m_leaflist[i]->State() |= INTOTHEVOID;
                }

                return removed;
            }

            inline std::vector<BSPPolygon<T>> CollectRemainingPolygons(/*bool add_extr_polygons = true*/) const
            {
                std::vector<BSPPolygon<T>> out;

                for (const auto& leaf : m_leaflist) {
                    out.insert(out.end(), leaf->polys.begin(), leaf->polys.end());
                    //  out.insert(out.end(), leaf->extraPolys.begin(), leaf->extraPolys.end());
                }

                return out;
            }
            
            inline std::vector<BSPPolygon<T>> CollectRemainingExtraPolygons(/*bool add_extr_polygons = true*/) const
            {
                std::vector<BSPPolygon<T>> out;

                for (const auto& leaf : m_leaflist) {
                     out.insert(out.end(), leaf->extraPolys.begin(), leaf->extraPolys.end());
                }

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
            // InsertExtraPolygons
            //
            // Fuegt NACH dem Build zusaetzliche Polygone (Wasser, Glas, ...) ein.
            // Sie laufen wie normale Polygone durch den Baum und werden an den
            // Splitter-Ebenen geteilt, landen aber in einer EIGENEN Liste der Leaves
            // (Node::GetExtraPolygons()). Baum, Portale und Flood bleiben unberuehrt.
            // Ein Polygon exakt in einer Splitter-Ebene kommt in beide Seiten
            // (doppelseitig sichtbar).
            //
            // skipVoidLeaves: Leaves, die der Flood als Aussenraum markiert hat
            // (INTOTHEVOID), bekommen nichts.
            // Rueckgabe: Anzahl gespeicherter Fragmente.
            // --------------------------------------------------------
            inline size_t InsertExtraPolygons(const std::vector<BSPPolygon<T>>& polys,
                bool skipVoidLeaves = true)
            {
                using Plane = ng::math::RayPlaneLine<3, T>;

                if (!m_root) return 0;

                const T eps = m_epsilon;
                const T minArea = eps * eps;
                size_t stored = 0;

                struct Item
                {
                    BSPNode<T>* node;
                    BSPPolygon<T> poly;
                };

                std::vector<Item> stack;

                for (const auto& src : polys)
                {
                    if (src.verts.size() < 3) continue;

                    BSPPolygon<T> start = src;
                    start.flags &= ~static_cast<uint32_t>(POLY_FLAG_SPLITTER_USED);
                    stack.push_back(Item{ m_root.get(), std::move(start) });

                    while (!stack.empty())
                    {
                        Item it = std::move(stack.back());
                        stack.pop_back();

                        BSPNode<T>* n = it.node;

                        if (n->isLeaf())
                        {
                            if (skipVoidLeaves && (n->m_state & INTOTHEVOID)) continue;

                            n->extraPolys.push_back(std::move(it.poly));
                            n->m_state |= HAS_EXTRA_POLYS;
                            ++stored;
                            continue;
                        }

                        const Plane plane = n->m_plane;
                        BSPNode<T>* f = n->m_front_node.get();
                        BSPNode<T>* b = n->m_back_node.get();

                        switch (ClassifyVerts(plane, it.poly.verts, eps))
                        {
                        case Plane::FRONT:
                            stack.push_back(Item{ f, std::move(it.poly) });
                            break;

                        case Plane::BACK:
                            stack.push_back(Item{ b, std::move(it.poly) });
                            break;

                        case Plane::PLANAR:
                            stack.push_back(Item{ f, it.poly });
                            stack.push_back(Item{ b, std::move(it.poly) });
                            break;

                        default: // SPANNING
                        {
                            BSPPolygon<T> fp, bp;
                            SplitPolygon(plane, it.poly, eps, fp, bp);

                            if (fp.verts.size() >= 3 && VertsArea(fp.verts) > minArea)
                                stack.push_back(Item{ f, std::move(fp) });
                            if (bp.verts.size() >= 3 && VertsArea(bp.verts) > minArea)
                                stack.push_back(Item{ b, std::move(bp) });
                            break;
                        }
                        }
                    }
                }

                return stored;
            }

            // --------------------------------------------------------
            // InsertVolume
            //
            // Fuegt NACH dem Build einen kompletten, geschlossenen KONVEXEN Brush
            // (Wasser, Lava, ...) ein. Er wird wie Polygone an den Splitter-Ebenen
            // geklippt, bleibt aber in jedem Leaf geschlossen (an den Schnittebenen
            // entstehen Deckel, POLY_FLAG_VOLUME_CAP). Die Leaves bekommen HAS_VOLUME.
            //
            // faces : die Flaechen des Brushes. Die Normalen werden automatisch nach
            //         INNEN ausgerichtet. Nicht-konvexe Brushes vorher in konvexe teilen.
            // medium: frei waehlbare Flags (z.B. WATER | LIQUID), siehe MediumAtPoint().
            //
            // Rueckgabe: Anzahl gespeicherter Stuecke.
            // --------------------------------------------------------
            inline size_t InsertVolume(const std::vector<BSPPolygon<T>>& faces,
                int volumeId, uint32_t medium, bool skipVoidLeaves = true)
            {
                using Plane = ng::math::RayPlaneLine<3, T>;

                if (!m_root || faces.size() < 4) return 0;

                const T eps = m_epsilon;

                // Schwerpunkt = sicher im Inneren eines konvexen Volumens
                Vec3 c(0, 0, 0);
                size_t cnt = 0;
                for (const auto& f : faces)
                    for (const auto& v : f.verts) { c = c + v.world(); ++cnt; }

                if (cnt == 0) return 0;
                c = c * (T(1) / T(cnt));

                BSPVolumePiece<T> piece;
                piece.volumeId = volumeId;
                piece.medium = medium;

                for (const auto& f : faces)
                {
                    if (f.verts.size() < 3) continue;

                    BSPPolygon<T> face = f;
                    face.flags |= POLY_FLAG_VOLUME_FACE;
                    face.volumeId = volumeId;

                    // Zeigt die Normale nach aussen, umdrehen
                    if (face.plane.distance(c) < -eps)
                    {
                        const Vec3 n = face.plane.normal();
                        BuildOrientedPlane(face.verts, n * T(-1), face.plane);
                    }

                    piece.faces.push_back(std::move(face));
                }

                if (piece.faces.size() < 4) return 0;

                struct Item
                {
                    BSPNode<T>* node;
                    BSPVolumePiece<T> piece;
                };

                std::vector<Item> stack;
                stack.push_back(Item{ m_root.get(), std::move(piece) });

                size_t stored = 0;

                while (!stack.empty())
                {
                    Item it = std::move(stack.back());
                    stack.pop_back();

                    BSPNode<T>* n = it.node;

                    if (n->isLeaf())
                    {
                        if (skipVoidLeaves && (n->m_state & INTOTHEVOID)) continue;
                        if (it.piece.faces.size() < 4) continue;

                        n->volumes.push_back(std::move(it.piece));
                        n->m_state |= HAS_VOLUME;
                        ++stored;
                        continue;
                    }

                    const Plane plane = n->m_plane;

                    switch (ClassifyVolumePiece(plane, it.piece, eps))
                    {
                    case Plane::BACK:
                        stack.push_back(Item{ n->m_back_node.get(), std::move(it.piece) });
                        break;

                    case Plane::SPANNING:
                    {
                        BSPVolumePiece<T> fp, bp;
                        SplitVolumePiece(it.piece, plane, eps, fp, bp);

                        if (fp.faces.size() >= 4) stack.push_back(Item{ n->m_front_node.get(), std::move(fp) });
                        if (bp.faces.size() >= 4) stack.push_back(Item{ n->m_back_node.get(),  std::move(bp) });
                        break;
                    }

                    default: // FRONT
                        stack.push_back(Item{ n->m_front_node.get(), std::move(it.piece) });
                        break;
                    }
                }

                return stored;
            }

            // In welchem Medium liegt der Punkt? ODER ueber die "medium"-Flags aller
            // Volumen, die den Punkt enthalten (0 = normale Luft).
            inline uint32_t MediumAtPoint(const Vec3& p) const
            {
                const int32_t id = FindLeafId(p);
                if (id < 0 || (size_t)id >= m_leaflist.size()) return 0;

                uint32_t medium = 0;
                for (const auto& piece : m_leaflist[id]->volumes)
                    if (piece.Contains(p, m_epsilon))
                        medium |= piece.medium;

                return medium;
            }

            // Leaf zu einer ID (nullptr bei ungueltiger ID).
            inline std::shared_ptr<BSPNode<T>> GetLeaf(int32_t id) const
            {
                if (id < 0 || (size_t)id >= m_leaflist.size()) return nullptr;
                return m_leaflist[id];
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


   // ------------------------------------------------------------
   // BSPRayTest / BSPRayOccluded / BSPRayNearestHit
   //
   // Traversiert einen BSPTree entlang eines Strahls und liefert den
   // naechsten Treffer, ohne alle Polygone der Szene testen zu muessen.
   // Macht Lightmap-/Radiosity-Occlusion um Groessenordnungen schneller
   // bei vielen Polygonen.
   //
   // "excludeSource" schliesst ein Polygon per Zeigervergleich aus -
   // nuetzlich, wenn Aufrufer und Baum DIESELBEN Objekte referenzieren.
   // Arbeitet der Baum mit eigenen Kopien (z.B. der BSPTree einer Map),
   // greift dieser Ausschluss nicht - tMin=eps verhindert Selbsttreffer
   // dann stattdessen ueber die Distanz (der Strahl startet exakt auf
   // der eigenen Ebene, t waere dort ~0 und faellt unter tMin).
   // ------------------------------------------------------------
        template<class T>
        struct BSPRayHit
        {
            bool hit = false;
            T t = std::numeric_limits<T>::max();
            const ng::bsp::BSPPolygon<T>* poly = nullptr;
        };

        template<class T>
        inline bool RayIntersectsPolygon(const ng::math::Matrix<3,1,T>& origin, const ng::math::Matrix<3,1,T>& dir,
            const ng::bsp::BSPPolygon<T>& poly, T& outT, T eps)
        {
            const ng::math::Matrix<3,1,T> n = poly.plane.normal();
            const T denom = n(0) * dir(0) + n(1) * dir(1) + n(2) * dir(2);
            if (std::abs(denom) < eps) return false;

            const T denomEps = T(1e-6); // FIX: fester, kleiner Wert fuer den Dot-Product-Test - unabhaengig von posEp
            if (std::abs(denom) < denomEps) return false;   // <- statt posEps

            const T t = -poly.plane.distance(origin) / denom;
            if (t < eps) return false;





            outT = t;
            const ng::math::Matrix<3,1,T> p = origin + dir * t;

            const size_t cnt = poly.verts.size();
            bool sign = false;
            bool first = true;

            for (size_t i = 0; i < cnt; ++i)
            {
                const ng::math::Matrix<3,1,T> a = poly.verts[i].world();
                const ng::math::Matrix<3,1,T> b = poly.verts[(i + 1) % cnt].world();
                const ng::math::Matrix<3,1,T> edge = b - a;
                const ng::math::Matrix<3,1,T> toP = p - a;
                const ng::math::Matrix<3,1,T> c = edge % toP;
                const T d = c(0) * n(0) + c(1) * n(1) + c(2) * n(2);

                const bool s = d >= T(0);
                if (first) { sign = s; first = false; }
                else if (s != sign) return false; // ausserhalb des konvexen Polygons
            }

            return true;
        }




        template<class T>
        inline BSPRayHit<T> BSPRayTest(
            const ng::bsp::BSPNode<T>* node,
            const ng::math::Matrix<3, 1, T>& origin,
            const ng::math::Matrix<3, 1, T>& dir,
            T tMin, T tMax, T eps,
            const ng::bsp::BSPPolygon<T>* excludeSource = nullptr)
        {
            if (!node) return BSPRayHit<T>{};

            if (node->isLeaf())
            {
                BSPRayHit<T> best;
                for (const auto& poly : node->GetPolygons())
                {
                    if (&poly == excludeSource) continue;
                    if (poly.verts.size() < 3) continue;

                    T t;
                    if (RayIntersectsPolygon(origin, dir, poly, t, eps) && t >= tMin && t <= tMax)
                    {
                        if (t < best.t) { best.hit = true; best.t = t; best.poly = &poly; }
                    }
                }
                return best;
            }

            const auto& plane = node->PlaneRef();
            const auto n = plane.normal();
            const T denom = n(0) * dir(0) + n(1) * dir(1) + n(2) * dir(2);
            const T originDist = plane.distance(origin);

            const ng::bsp::BSPNode<T>* front = node->FrontPtr();
            const ng::bsp::BSPNode<T>* back = node->BackPtr();

            if (std::abs(denom) < eps || std::abs(originDist) <= eps)
            {
                BSPRayHit<T> hitA = BSPRayTest(front, origin, dir, tMin, tMax, eps, excludeSource);
                BSPRayHit<T> hitB = BSPRayTest(back, origin, dir, tMin, tMax, eps, excludeSource);
                if (!hitA.hit) return hitB;
                if (!hitB.hit) return hitA;
                return (hitA.t <= hitB.t) ? hitA : hitB;
            }

            const T tPlane = -originDist / denom;
            const ng::bsp::BSPNode<T>* nearNode = (originDist > T(0)) ? front : back;
            const ng::bsp::BSPNode<T>* farNode = (originDist > T(0)) ? back : front;

            if (tPlane < tMin || tPlane > tMax)
                return BSPRayTest(nearNode, origin, dir, tMin, tMax, eps, excludeSource);

            BSPRayHit<T> nearHit = BSPRayTest(nearNode, origin, dir, tMin, tPlane, eps, excludeSource);
            if (nearHit.hit) return nearHit;

            return BSPRayTest(farNode, origin, dir, tPlane, tMax, eps, excludeSource);
        }



        template<class T>
        inline bool BSPRayOccluded(
            const ng::bsp::BSPTree<T>& tree,
            const ng::math::Matrix<3,1,T>& origin,
            const ng::math::Matrix<3,1,T>& dir,
            T maxDist,
            T eps,
            const ng::bsp::BSPPolygon<T>* excludeSource = nullptr)
        {
            auto hit = BSPRayTest(tree.GetRoot().get(), origin, dir, eps, maxDist, eps, excludeSource);
            return hit.hit;
        }

        template<class T>
        inline BSPRayHit<T> BSPRayNearestHit(
            const ng::bsp::BSPTree<T>& tree,
            const ng::math::Matrix<3,1,T>& origin,
            const ng::math::Matrix<3,1,T>& dir,
            T maxDist,
            T eps,
            const ng::bsp::BSPPolygon<T>* excludeSource = nullptr)
        {
            return BSPRayTest(tree.GetRoot().get(), origin, dir, eps, maxDist, eps, excludeSource);
        }


        // TEMP-DEBUG: direkt nach dem Bake-Aufruf, VOR Radiosity, einmalig ausführen
        template<class T>
        inline  void DebugCompareBSPvsBruteforce(
            const ng::bsp::BSPTree<T>& tree,
            const std::vector<ng::bsp::BSPPolygon<T>>& allPolys,
            int numTestRays,
            T eps)
        {
            std::mt19937 rng(42);
            std::uniform_real_distribution<double> dist01(0.0, 1.0);

            int mismatches = 0;

            for (int i = 0; i < numTestRays; ++i)
            {
                // Zufaelliges Polygon, zufaelligen Punkt darauf, zufaellige Richtung
                if (allPolys.empty()) break;
                const auto& poly = allPolys[rng() % allPolys.size()];
                if (poly.verts.size() < 3) continue;

                // Punkt = grober Schwerpunkt des Polygons
                ng::math::Matrix<3,1,T> origin(0, 0, 0);
                for (const auto& v : poly.verts) origin = origin + v.world();
                origin = origin * (T(1) / T(poly.verts.size()));
                origin = origin + poly.plane.normal() * T(0.5); // leicht ueber die Flaeche

                // Zufaellige Richtung ueber die Hemisphaere um die Normale
                const T u1 = (T)dist01(rng), u2 = (T)dist01(rng);
                const T r = std::sqrt(u1);
                const T theta = T(2.0 * 3.14159265358979323846) * u2;
                ng::math::Matrix<3,1,T> n = poly.plane.normal();
                ng::math::Matrix<3,1,T> axis(0, 0, 0);
                if (std::abs(n(0)) <= std::abs(n(1)) && std::abs(n(0)) <= std::abs(n(2))) axis(0) = T(1);
                else if (std::abs(n(1)) <= std::abs(n(2))) axis(1) = T(1); else axis(2) = T(1);
                ng::math::Matrix<3,1,T> tangent = axis % n; tangent.normalize();
                ng::math::Matrix<3,1,T> bitangent = n % tangent; bitangent.normalize();
                ng::math::Matrix<3,1,T> dir = tangent * (r * std::cos(theta)) + bitangent * (r * std::sin(theta)) + n * std::sqrt(std::max(T(0), T(1) - u1));

                const T maxDist = T(2000);

                // BSP-Ergebnis
                bool bspHit = BSPRayOccluded(tree, origin, dir, maxDist, eps, &poly);

                // Brute-Force-Ergebnis
                bool bruteHit = false;
                T bestT = maxDist;
                for (const auto& p2 : allPolys)
                {
                    if (&p2 == &poly) continue;
                    T t;
                    if (ng::bsp::RayIntersectsPolygon(origin, dir, p2, t, eps) && t < bestT)
                    {
                        bruteHit = true;
                        break;
                    }
                }

                if (bspHit != bruteHit)
                {
                    ++mismatches;
                    if (mismatches <= 10)
                        printf("[DEBUG] Mismatch #%d: BSP=%d Brute=%d origin=(%.1f,%.1f,%.1f) dir=(%.2f,%.2f,%.2f)\n",
                            mismatches, bspHit, bruteHit,
                            (double)origin(0), (double)origin(1), (double)origin(2),
                            (double)dir(0), (double)dir(1), (double)dir(2));
                }
            }

            printf("[DEBUG] %d/%d Rays getestet, %d Abweichungen (%.1f%%)\n",
                numTestRays, numTestRays, mismatches, 100.0 * mismatches / std::max(1, numTestRays));
        }






        enum : uint32_t
        {
            WARP_TELEPORT = 1 << 0,   // Spieler/Objekte werden versetzt
            WARP_RENDER = 1 << 1    // Sicht der Gegenseite wird gerendert
        };

        template <class T>
        struct WarpPortal
        {
            using Vec3 = math::Matrix<3, 1, T>;
            using Plane = math::RayPlaneLine<3, T>;
            float seeThrough = (T)1.0;
            float wobble = 0.0f;       // NEU
            int32_t  id = -1;
            int32_t  partner = -1;
            uint32_t flags = WARP_TELEPORT | WARP_RENDER;

            
            


            std::vector<Vec3> verts;      // konvexes Portal-Polygon (Welt)
            Plane plane;                  // Normale zeigt in den begehbaren Raum
            Vec3 origin, right, up, normal;

            std::vector<int32_t> leaves;     // Zellen, die Fragmente dieses Portals enthalten
            std::vector<int32_t> visCells;   // ODER der PVS-Zeilen dieser Zellen (sortiert);
            // = was man DURCH dieses Portal als Ziel sieht

// Normale so ausrichten, dass sie zu "roomPoint" zeigt (z.B. Position des
// Entities, das direkt vor dem Face steht), dann Basis aufbauen.
            void Finalize(const Vec3& roomPoint, const Vec3& worldUp)
            {
                origin = Vec3(0, 0, 0);
                for (const auto& v : verts) origin = origin + v;
                origin = origin * (T(1) / T(verts.size()));

                normal = plane.normal();
                normal.normalize();
                if (((roomPoint - origin) ^ normal) < T(0))
                {
                    normal = normal * T(-1);
                    // Plane mit umgedrehter Orientierung neu aufbauen
                    if (verts.size() >= 3) plane = Plane(verts[0], verts[2], verts[1]);
                }

                Vec3 u = worldUp - normal * (worldUp ^ normal);
                if (u.length() < T(1e-3))                     // Boden/Decke: anderer Hinweis
                {
                    const Vec3 alt(1, 0, 0);
                    u = alt - normal * (alt ^ normal);
                }
                u.normalize();
                up = u;
                right = up % normal;
                right.normalize();
            }
        };

        // Richtung von Portal a nach Portal b (180 Grad um "up": man tritt VOR a ein
        // und kommt VOR b heraus).
        template <class T>
        inline math::Matrix<3, 1, T> WarpDir(const WarpPortal<T>& a, const WarpPortal<T>& b,
            const math::Matrix<3, 1, T>& d)
        {
            const T lx = d ^ a.right, ly = d ^ a.up, lz = d ^ a.normal;
            return b.right * (-lx) + b.up * ly + b.normal * (-lz);
        }

        template <class T>
        inline math::Matrix<3, 1, T> WarpPoint(const WarpPortal<T>& a, const WarpPortal<T>& b,
            const math::Matrix<3, 1, T>& p)
        {
            return b.origin + WarpDir(a, b, p - a.origin);
        }

        // Polygon gegen mehrere Ebenen klippen (behaelt jeweils die FRONT-Seite).
        template <class T>
        std::vector<math::Matrix<3, 1, T>> ClipPolyToPlanes(
            std::vector<math::Matrix<3, 1, T>> verts,
            const std::vector<math::RayPlaneLine<3, T>>& planes)
        {
            for (const auto& pl : planes)
            {
                verts = ng::geometry::ClipByPlane(verts, pl, ng::geometry::ClipSide::FRONT);
                if (verts.size() < 3) return {};
            }
            return verts;
        }

        // Seitenebenen einer Pyramide: Auge -> Kanten des Polygons. Innen = vorne.
        template <class T>
        std::vector<math::RayPlaneLine<3, T>> FrustumFromPolygon(
            const math::Matrix<3, 1, T>& eye,
            const std::vector<math::Matrix<3, 1, T>>& poly)
        {
            using Plane = math::RayPlaneLine<3, T>;
            std::vector<Plane> out;

            math::Matrix<3, 1, T> c(0, 0, 0);
            for (const auto& v : poly) c = c + v;
            c = c * (T(1) / T(poly.size()));

            for (size_t i = 0; i < poly.size(); ++i)
            {
                const auto& a = poly[i];
                const auto& b = poly[(i + 1) % poly.size()];
                if (((a - eye) % (b - eye)).length() < T(1e-9)) continue;   // entartet

                Plane pl(eye, a, b);
                if (pl.distance(c) < T(0)) pl = Plane(eye, b, a);
                out.push_back(pl);
            }
            return out;

        }

        template <class T>
        bool BuildNextFrustum(const WarpPortal<T>& a, const WarpPortal<T>& b,
            const math::Matrix<3, 1, T>& eye,
            const std::vector<math::RayPlaneLine<3, T>>& curFrustum,
            math::Matrix<3, 1, T>& nextEye,
            std::vector<math::RayPlaneLine<3, T>>& nextPlanes,
            T minArea = T(1),
            std::vector<math::Matrix<3, 1, T>>* entryOut = nullptr)
        {
            using Vec3 = math::Matrix<3, 1, T>;

            // 1. Eintritts-Portal gegen aktuellen Frustum
            std::vector<Vec3> p = ClipPolyToPlanes(a.verts, curFrustum);
            if (p.size() < 3 || PortalArea(p) < minArea) return false;
            if (entryOut) *entryOut = p;     // sichtbares Stueck von A (nativer Raum)

            // 2. nach B abbilden
            nextEye = WarpPoint(a, b, eye);
            std::vector<Vec3> q;
            q.reserve(p.size());
            for (const auto& v : p) q.push_back(WarpPoint(a, b, v));

            // 3. gegen die echte Oeffnung von B beschneiden (falls A und B verschieden gross)
            q = ClipPolyToPlanes(q, FrustumFromPolygon(nextEye, b.verts));
            if (q.size() < 3 || PortalArea(q) < minArea) return false;

            // 4. neuer Frustum + Portalebene von B als Near-Plane
            nextPlanes = FrustumFromPolygon(nextEye, q);
            nextPlanes.push_back(b.plane);
            return nextPlanes.size() >= 4;
        }

        // Hat die Bewegung p0 -> p1 das Portal (von vorne nach hinten) durchquert?
        template <class T>
        bool CrossedPortal(const WarpPortal<T>& w,
            const math::Matrix<3, 1, T>& p0, const math::Matrix<3, 1, T>& p1)
        {
            using Vec3 = math::Matrix<3, 1, T>;

            const T d0 = (p0 - w.origin) ^ w.normal;
            const T d1 = (p1 - w.origin) ^ w.normal;
            if (!(d0 > T(0) && d1 <= T(0))) return false;

            const T t = d0 / (d0 - d1);
            const Vec3 hit = p0 + (p1 - p0) * t;

            const size_t n = w.verts.size();
            for (size_t i = 0; i < n; ++i)
            {
                const Vec3 a = w.verts[i], b = w.verts[(i + 1) % n];
                if ((((b - a) % (hit - a)) ^ w.normal) < T(0)) return false;   // ausserhalb
            }
            return true;
        }
        // ------------------------------------------------------------
        // RigidXform: starre Transformation p' = R*p + t (nur Drehung + Verschiebung)
        //
        // FromPortals(B, A) bildet einen Punkt im Raum um B in den Raum um A ab.
        // Verkettung fuer Rekursion: next.toScreen = view.toScreen.After(FromPortals(B, A))
        // ------------------------------------------------------------
        template <class T>
        struct RigidXform
        {
            using Vec3 = math::Matrix<3, 1, T>;

            Vec3 c0, c1, c2;   // Spalten der Rotation
            Vec3 t;

            RigidXform() : c0(1, 0, 0), c1(0, 1, 0), c2(0, 0, 1), t(0, 0, 0) {}

            Vec3 Dir(const Vec3& d) const { return c0 * d(0) + c1 * d(1) + c2 * d(2); }
            Vec3 Point(const Vec3& p) const { return Dir(p) + t; }

            static RigidXform FromPortals(const WarpPortal<T>& src, const WarpPortal<T>& dst)
            {
                RigidXform r;
                r.c0 = WarpDir(src, dst, Vec3(1, 0, 0));
                r.c1 = WarpDir(src, dst, Vec3(0, 1, 0));
                r.c2 = WarpDir(src, dst, Vec3(0, 0, 1));
                r.t = dst.origin - r.Dir(src.origin);
                return r;
            }

            // this(inner(p))
            RigidXform After(const RigidXform& inner) const
            {
                RigidXform r;
                r.c0 = Dir(inner.c0);
                r.c1 = Dir(inner.c1);
                r.c2 = Dir(inner.c2);
                r.t = Point(inner.t);
                return r;
            }
        };

        // Klippt ein Vertex-Polygon (mit Attribut-Interpolation) gegen mehrere Ebenen,
        // behaelt jeweils die FRONT-Seite. false = nichts uebrig.
        template <class T>
        bool ClipVertsToPlanes(const std::vector<ng::geometry::Vertex<T>>& in,
            const std::vector<math::RayPlaneLine<3, T>>& planes, T eps,
            std::vector<ng::geometry::Vertex<T>>& out)
        {
            using Plane = math::RayPlaneLine<3, T>;

            out = in;
            std::vector<ng::geometry::Vertex<T>> tmp;

            for (const auto& pl : planes)
            {
                const int cls = ClassifyVerts(pl, out, eps);
                if (cls == Plane::FRONT || cls == Plane::PLANAR) continue;
                if (cls == Plane::BACK) return false;

                tmp.clear();
                const size_t n = out.size();
                for (size_t i = 0; i < n; ++i)
                {
                    const auto& cur = out[i];
                    const auto& nxt = out[(i + 1) % n];
                    const T dc = pl.distance(cur.world());
                    const T dn = pl.distance(nxt.world());

                    if (dc >= -eps) tmp.push_back(cur);

                    if ((dc > eps && dn < -eps) || (dc < -eps && dn > eps))
                        tmp.push_back(LerpVertex(cur, nxt, dc / (dc - dn)));
                }

                if (tmp.size() < 3) return false;
                out.swap(tmp);
            }
            return true;
        }

        // true, wenn die Box komplett hinter einer der Ebenen liegt (= unsichtbar).
        template <class T>
        bool AABBOutsidePlanes(const ng::bounding::AABBox<3, T>& box,
            const std::vector<math::RayPlaneLine<3, T>>& planes)
        {
            const auto mn = box.Min();
            const auto mx = box.Max();

            for (const auto& pl : planes)
            {
                const auto n = pl.normal();
                const math::Matrix<3, 1, T> p(
                    n(0) >= T(0) ? mx(0) : mn(0),
                    n(1) >= T(0) ? mx(1) : mn(1),
                    n(2) >= T(0) ? mx(2) : mn(2));

                if (pl.distance(p) < T(0)) return true;
            }
            return false;
        }

        template <class T>
        bool BuildNextFrustumFromEntry(const WarpPortal<T>& a, const WarpPortal<T>& b,
            const std::vector<math::Matrix<3, 1, T>>& entry,
            const math::Matrix<3, 1, T>& eye,
            const std::vector<math::RayPlaneLine<3, T>>& curFrustum,
            math::Matrix<3, 1, T>& nextEye,
            std::vector<math::RayPlaneLine<3, T>>& nextPlanes,
            std::vector<math::Matrix<3, 1, T>>& visibleEntry,
            T minArea = T(1))
        {
            using Vec3 = math::Matrix<3, 1, T>;

            visibleEntry = ClipPolyToPlanes(entry, curFrustum);
            if (visibleEntry.size() < 3 || PortalArea(visibleEntry) < minArea) return false;

            nextEye = WarpPoint(a, b, eye);
            std::vector<Vec3> q;
            q.reserve(visibleEntry.size());
            for (const auto& v : visibleEntry) q.push_back(WarpPoint(a, b, v));

            // KEIN Clip gegen b.verts mehr: Form von B ist egal.
            nextPlanes = FrustumFromPolygon(nextEye, q);
            nextPlanes.push_back(b.plane);          // Near-Plane: nur was VOR B liegt
            return nextPlanes.size() >= 4;
        }




    } // namespace bsp
} // namespace ng