#pragma once
#include <algorithm>
#include <limits>
#include <cmath>
#include <memory>
#include <cstdint>
#include "HierarchicalZBuffer.h"
#include "Vertex.h"
namespace ng {
    namespace renderer {
        namespace occlusion
        {

            enum class VisResult
            {
                Visible,
                Occluded,
                OffScreen
            };
            // ================================================================
            // Projektionsergebnis
            // ================================================================
            template <class T>
            struct ProjectedPoint
            {
                T x = 0;
                T y = 0;
                // 1/z
                // größer = näher
                T oneOverZ = 0;
                bool behindCamera = false;
            };
            // ================================================================
            // HZB Dimensionen eines Levels
            // ================================================================
            inline void HZBMipDims(
                int screenWidth,
                int screenHeight,
                int level,
                int& outW,
                int& outH)
            {
                int w = screenWidth;
                int h = screenHeight;
                for (int i = 0; i < level; ++i)
                {
                    w = std::max(1, w >> 1);
                    h = std::max(1, h >> 1);
                }
                outW = w;
                outH = h;
            }
            // ================================================================
            // Test eines einzelnen HZB-Levels
            //
            // WICHTIG:
            //
            // Wir verwenden NICHT die nächste Ecke der Box.
            //
            // Für einen konservativen Occlusion-Test verwenden wir die
            // WEITESTE mögliche Tiefe der Box.
            //
            // Bei 1/z:
            //
            //     größer = näher
            //     kleiner = weiter weg
            //
            // Wenn also sogar die entfernteste Stelle der Box verdeckt ist,
            // kann die Box sicher verworfen werden.
            // ================================================================
            template <class T, class ProjectFn>
            inline bool TestHZBLevel(
                buffers::HierarchicalZBuffer<T>& hzb,
                int level,
                int screenWidth,
                int screenHeight,
                T minX,
                T minY,
                T maxX,
                T maxY,
                double farthestOneOverZ,
                ProjectFn& project)
            {
                int mw = 0;
                int mh = 0;
                HZBMipDims(
                    screenWidth,
                    screenHeight,
                    level,
                    mw,
                    mh);
                if (mw <= 0 || mh <= 0)
                    return false;
                //std::shared_ptr<T> mip =

                T* buf = hzb.GetRawHZBBuffer(level);;
                if (!buf)
                    return false;
                // ------------------------------------------------------------
                // Screen -> HZB
                // ------------------------------------------------------------
                const double sx =
                    static_cast<double>(mw) /
                    static_cast<double>(screenWidth);
                const double sy =
                    static_cast<double>(mh) /
                    static_cast<double>(screenHeight);
                int x0 = static_cast<int>(
                    std::floor(
                        static_cast<double>(minX) * sx));
                int x1 = static_cast<int>(
                    std::ceil(
                        static_cast<double>(maxX) * sx));
                int y0 = static_cast<int>(
                    std::floor(
                        static_cast<double>(minY) * sy));
                int y1 = static_cast<int>(
                    std::ceil(
                        static_cast<double>(maxY) * sy));
                x0 = std::clamp(x0, 0, mw - 1);
                x1 = std::clamp(x1, 0, mw - 1);
                y0 = std::clamp(y0, 0, mh - 1);
                y1 = std::clamp(y1, 0, mh - 1);
                if (x0 > x1 || y0 > y1)
                    return false;
                // ------------------------------------------------------------
                // Epsilon
                //
                // Wir machen den Test absichtlich etwas konservativer.
                //
                // Die Box muss ein kleines Stück HINTER dem HZB liegen,
                // bevor wir sie als verdeckt akzeptieren.
                // ------------------------------------------------------------
                const double epsilon =
                    1e-6 *
                    std::max(
                        1.0,
                        std::abs(farthestOneOverZ));
                // ------------------------------------------------------------
                // ALLE relevanten HZB-Zellen testen
                //
                // Das ist der entscheidende Unterschied zu deinem alten Code.
                //
                // ALT:
                //
                //     max(HZB)
                //
                //     -> ein einziges nahes Pixel konnte die ganze Box
                //        als verdeckt erscheinen lassen.
                //
                // NEU:
                //
                //     Jede HZB-Zelle muss die Box verdecken.
                // ------------------------------------------------------------
                for (int y = y0; y <= y1; ++y)
                {
                    const T* row =
                        buf +
                        static_cast<size_t>(y) *
                        static_cast<size_t>(mw);
                    for (int x = x0; x <= x1; ++x)
                    {
                        const double hzbDepth =
                            static_cast<double>(row[x]);
                        // ----------------------------------------------------
                        // HZB enthält 1/z:
                        //
                        //     größer = näher
                        //
                        // Die Box ist sicher verdeckt, wenn der HZB-Occluder
                        // mindestens so nah ist wie die WEITESTE Stelle
                        // der Box.
                        //
                        // Wenn das nicht der Fall ist, können wir NICHT
                        // beweisen, dass die Box verdeckt ist.
                        // ----------------------------------------------------
                        if (hzbDepth + epsilon <
                            farthestOneOverZ)
                        {
                            return false;
                        }
                    }
                }
                // Jede relevante HZB-Zelle verdeckt selbst die entfernteste
                // Stelle der Box.
                //
                // Damit ist die gesamte Box sicher verdeckt.
                return true;
            }

            template <class T, class AABBoxT, class ProjectFn>
            VisResult TestAABBoxVisibility(
                AABBoxT& box,
                buffers::HierarchicalZBuffer<T>& hzb,
                int screenWidth,
                int screenHeight,
                ProjectFn project)
            {
                auto mn = box.Min();
                auto mx = box.Max();
                const T corners[8][3] =
                {
                    { mn.x(), mn.y(), mn.z() },
                    { mx.x(), mn.y(), mn.z() },
                    { mn.x(), mx.y(), mn.z() },
                    { mx.x(), mx.y(), mn.z() },
                    { mn.x(), mn.y(), mx.z() },
                    { mx.x(), mn.y(), mx.z() },
                    { mn.x(), mx.y(), mx.z() },
                    { mx.x(), mx.y(), mx.z() }
                };
                double minX = std::numeric_limits<double>::max();
                double minY = std::numeric_limits<double>::max();
                double maxX = std::numeric_limits<double>::lowest();
                double maxY = std::numeric_limits<double>::lowest();
                // Bei 1/z: größer = näher
                double nearestOneOverZ = std::numeric_limits<double>::lowest();
                for (int i = 0; i < 8; ++i)
                {
                    math::Matrix<3, 1, T> p3(
                        corners[i][0],
                        corners[i][1],
                        corners[i][2]);
                    ProjectedPoint<T> p = project(p3);
                    // Kamera-Sonderfall: niemals cullen.
                    if (p.behindCamera)
                        return VisResult::Visible;
                    minX = std::min(minX, static_cast<double>(p.x));
                    minY = std::min(minY, static_cast<double>(p.y));
                    maxX = std::max(maxX, static_cast<double>(p.x));
                    maxY = std::max(maxY, static_cast<double>(p.y));
                    // Für den konservativen Test brauchen wir den NÄCHSTEN Punkt
                    // der Box (siehe Kommentar in TestHZBLevel).
                    nearestOneOverZ = std::max(nearestOneOverZ, static_cast<double>(p.oneOverZ));
                }
                // ------------------------------------------------------------
                // Screen clip
                // ------------------------------------------------------------
                minX = std::max(minX, 0.0);
                minY = std::max(minY, 0.0);
                maxX = std::min(maxX, static_cast<double>(screenWidth - 1));
                maxY = std::min(maxY, static_cast<double>(screenHeight - 1));
                if (minX > maxX || minY > maxY)
                    return VisResult::OffScreen;
                // ------------------------------------------------------------
                // Screen-Größe -> HZB Level bestimmen
                //
                // FIX: Wir brauchen ceil(log2(maxSize)), NICHT floor.
                //
                // Ein HZB-Texel auf Level L deckt 2^L Originalpixel ab.
                // Damit EIN Texel die komplette Box-Fläche abdeckt, muss gelten:
                //
                //     2^level >= maxSize
                //     level   >= log2(maxSize)
                //
                // Mit floor (alt: static_cast<int>(log2(maxSize))) wurde fast immer
                // ein zu feiner Level gewählt. Die Box lag dann über MEHRERE
                // HZB-Zellen verteilt. Da BuildMipMaps() pro Zelle das MIN (=
                // schwächster/entferntester Occluder) der 4 Kindpixel bildet,
                // reichte irgendeine einzelne Hintergrund-Lücke innerhalb dieser
                // mehreren Zellen aus, um den Objekt sofort als sichtbar
                // einzustufen (TestHZBLevel bricht bei der ersten nicht-verdeckenden
                // Zelle sofort mit false ab). Bei stark verstreuter Geometrie
                // (Partikel, Sprites mit Lücken dazwischen) ist genau das der
                // Normalfall -> praktisch nichts wurde gecullt.
                // ------------------------------------------------------------
                const double width = maxX - minX;
                const double height = maxY - minY;
                const double maxSize = std::max(width, height);
                int level = 0;
                if (maxSize > 1.0)
                {
                    level = static_cast<int>(std::ceil(std::log2(maxSize)));
                }
                // FIX: clamp-Obergrenze war GetNumMipMapLevels() (= Anzahl der Level,
                // z.B. 8), gültige Indizes sind aber 0..GetNumMipMapLevels()-1 (0..7).
                // Mit level == 8 landete GetRawHZBBuffer() im Fallback-Zweig und gab
                // den VOLLAUFLÖSENDEN Depth-Buffer zurück, während HZBMipDims() für
                // Level 8 weiterhin eine winzige (gemipmapte) Größe berechnet hat.
                // Die Koordinaten-Umrechnung (sx/sy) in TestHZBLevel passte dadurch
                // nicht mehr zum tatsächlichen Speicherlayout -> es wurden praktisch
                // zufällige Speicherbereiche als Tiefenwerte gelesen.
                level = std::clamp(level, 0, hzb.GetNumMipMapLevels() - 1);
                // ------------------------------------------------------------
                // Konservativer Test über die GESAMTE Fläche des Levels
                //
                // Ersetzt die frühere 5-Sample-Heuristik. Jede relevante
                // HZB-Zelle muss die Box verdecken, sonst gilt sie als sichtbar.
                // ------------------------------------------------------------
                bool occluded = TestHZBLevel<T>(
                    hzb,
                    level,
                    screenWidth,
                    screenHeight,
                    static_cast<T>(minX),
                    static_cast<T>(minY),
                    static_cast<T>(maxX),
                    static_cast<T>(maxY),
                    nearestOneOverZ,
                    project);
                return occluded ? VisResult::Occluded : VisResult::Visible;
            }
            template <class T, class AABBoxT, class ProjectFn>
            inline bool IsAABBoxVisible(
                AABBoxT& box,
                buffers::HierarchicalZBuffer<T>& hzb,
                int screenWidth,
                int screenHeight,
                ProjectFn project)

            {
                return TestAABBoxVisibility<T>(
                    box,
                    hzb,
                    screenWidth,
                    screenHeight,
                    project)

                    != VisResult::Occluded;
            }

            // ================================================================
            // Dreieck gegen HZB testen
            //
            // Gleiches Prinzip wie bei der AABB:
            //
            //   1. Alle 3 Punkte projizieren.
            //   2. Screen-Space Bounding-Box der Dreiecksfläche bilden.
            //   3. Den NÄCHSTEN Punkt des Dreiecks (max 1/z) bestimmen -
            //      das ist der "härteste" Fall: der Occluder muss näher
            //      sein als dieser Punkt, damit das GESAMTE Dreieck
            //      sicher verdeckt ist.
            //   4. Passenden HZB-Level wählen (ceil(log2(size))) und
            //      TestHZBLevel wie beim AABB-Test aufrufen.
            //
            // WICHTIG / Einschränkung:
            //
            //   Wie beim AABB-Test wird über die REKTECKIGE Bounding-Box
            //   der projizierten Punkte getestet, nicht über die exakte
            //   Dreiecksfläche. Bei dünnen/diagonalen Dreiecken ist die
            //   Bounding-Box deutlich größer als das Dreieck selbst ->
            //   der Test wird dadurch konservativer (es wird seltener
            //   gecullt) als eine exakte Scanline-Abdeckung. Für die
            //   meisten Zwecke (Grob-Culling vor dem Rasterizer) reicht
            //   das aber locker aus.
            // ================================================================
            template <class T, class ProjectFn>
            inline VisResult TestTriangleVisibility(
                const math::Matrix<3, 1, T>& p0,
                const math::Matrix<3, 1, T>& p1,
                const math::Matrix<3, 1, T>& p2,
                buffers::HierarchicalZBuffer<T>& hzb,
                int screenWidth,
                int screenHeight,
                ProjectFn project)
            {
                ProjectedPoint<T> proj[3];
                proj[0] = project(p0);
                proj[1] = project(p1);
                proj[2] = project(p2);
                // Kamera-Sonderfall: niemals cullen.
                for (int i = 0; i < 3; ++i)
                {
                    if (proj[i].behindCamera)
                        return VisResult::Visible;
                }
                double minX = std::min(
                    { static_cast<double>(proj[0].x),
                      static_cast<double>(proj[1].x),
                      static_cast<double>(proj[2].x) });
                double maxX = std::max(
                    { static_cast<double>(proj[0].x),
                      static_cast<double>(proj[1].x),
                      static_cast<double>(proj[2].x) });
                double minY = std::min(
                    { static_cast<double>(proj[0].y),
                      static_cast<double>(proj[1].y),
                      static_cast<double>(proj[2].y) });
                double maxY = std::max(
                    { static_cast<double>(proj[0].y),
                      static_cast<double>(proj[1].y),
                      static_cast<double>(proj[2].y) });
                // Nächster Punkt des Dreiecks (größtes 1/z).
                double nearestOneOverZ = std::max(
                    { static_cast<double>(proj[0].oneOverZ),
                      static_cast<double>(proj[1].oneOverZ),
                      static_cast<double>(proj[2].oneOverZ) });
                // ------------------------------------------------------------
                // Screen clip
                // ------------------------------------------------------------
                minX = std::max(minX, 0.0);
                minY = std::max(minY, 0.0);
                maxX = std::min(maxX, static_cast<double>(screenWidth - 1));
                maxY = std::min(maxY, static_cast<double>(screenHeight - 1));
                if (minX > maxX || minY > maxY)
                    return VisResult::OffScreen;
                // ------------------------------------------------------------
                // Screen-Größe -> HZB Level bestimmen
                //
                // ceil(log2(size)), siehe Kommentar in
                // TestAABBoxVisibility für die Begründung.
                // ------------------------------------------------------------
                const double width = maxX - minX;
                const double height = maxY - minY;
                const double maxSize = std::max(width, height);
                int level = 0;
                if (maxSize > 1.0)
                {
                    level = static_cast<int>(std::ceil(std::log2(maxSize)));
                }
                level = std::clamp(level, 0, hzb.GetNumMipMapLevels() - 1);
                bool occluded = TestHZBLevel<T>(
                    hzb,
                    level,
                    screenWidth,
                    screenHeight,
                    static_cast<T>(minX),
                    static_cast<T>(minY),
                    static_cast<T>(maxX),
                    static_cast<T>(maxY),
                    nearestOneOverZ,
                    project);
                return occluded ? VisResult::Occluded : VisResult::Visible;
            }

            template <class T, class ProjectFn>
            inline bool IsTriangleVisible(
                const math::Matrix<3, 1, T>& p0,
                const math::Matrix<3, 1, T>& p1,
                const math::Matrix<3, 1, T>& p2,
                buffers::HierarchicalZBuffer<T>& hzb,
                int screenWidth,
                int screenHeight,
                ProjectFn project)
            {
                return TestTriangleVisibility<T>(
                    p0, p1, p2,
                    hzb,
                    screenWidth,
                    screenHeight,
                    project)
                    != VisResult::Occluded;
            }

            // ------------------------------------------------------------
            // Bequeme Overloads für geometry::Vertex<T>
            //
            // Nutzen einfach die Weltposition (.world()) der Vertices und
            // leiten an die Punkt-Version oben weiter.
            // ------------------------------------------------------------
            template <class T, class ProjectFn>
            inline VisResult TestTriangleVisibility(
                const ng::geometry::Vertex<T>& v0,
                const ng::geometry::Vertex<T>& v1,
                const ng::geometry::Vertex<T>& v2,
                buffers::HierarchicalZBuffer<T>& hzb,
                int screenWidth,
                int screenHeight,
                ProjectFn project)
            {
                return TestTriangleVisibility<T>(
                    v0.world(), v1.world(), v2.world(),
                    hzb,
                    screenWidth,
                    screenHeight,
                    project);
            }

            template <class T, class ProjectFn>
            inline bool IsTriangleVisible(
                const ng::geometry::Vertex<T>& v0,
                const ng::geometry::Vertex<T>& v1,
                const ng::geometry::Vertex<T>& v2,
                buffers::HierarchicalZBuffer<T>& hzb,
                int screenWidth,
                int screenHeight,
                ProjectFn project)
            {
                return TestTriangleVisibility<T>(
                    v0, v1, v2,
                    hzb,
                    screenWidth,
                    screenHeight,
                    project)
                    != VisResult::Occluded;
            }
        }
    }
}