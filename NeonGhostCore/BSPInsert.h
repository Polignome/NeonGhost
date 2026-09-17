#pragma once
// =====================================================================
// BSPInsert.h
//
// Nachtraegliches Einfuegen von Geometrie in einen BEREITS fertigen
// BSP-Baum inkl. bereits generierter Portale.
//
// Es werden dabei NIEMALS neue Splits erzeugt -> die Baumstruktur und
// alle bestehenden Portale bleiben unveraendert gueltig.
//
// Zwei Anwendungsfaelle:
//
//   1) InsertPolygon() / InsertPolygons()
//      Ein Polygon wird rekursiv entlang der VORHANDENEN Ebenen
//      geklippt, bis seine Fragmente in den jeweiligen Leafs landen
//      (node->insertedPolygons). Damit fuegt man z.B. die 6 Flaechen
//      der Weltbegrenzungs-Box ein (markiert mit PF_BoundsBox), um
//      danach programmatisch einen garantiert aussenliegenden Leaf
//      fuer den FloodFill-Start zu finden - ohne einen Weltpunkt von
//      Hand raten zu muessen.
//
//   2) InsertVolumeBrush()
//      Ein konvexes Bounding-Volumen (Wasser-/Lava-Brush) wird:
//        a) flaechenweise wie normale Polygone eingefuegt
//           (PF_VolumeFace, fuer Rendering/Debug - z.B. die
//           Wasseroberflaeche dort, wo sie den Level schneidet)
//        b) zusaetzlich wird JEDES Empty-Leaf ueber einen
//           repraesentativen Innenpunkt (Mittelwert seiner Portal-
//           Vertices) gegen die Halbraeume des Brush getestet.
//           Liegt der Punkt innerhalb, gilt das GESAMTE Leaf als
//           "in diesem Volumen" - auch wenn keine einzige Brush-
//           Flaeche das Leaf ueberhaupt schneidet (voll eingetauchte
//           Leafs, z.B. mitten in einem grossen Wasserbecken!).
//
//      Laufzeit-Abfrage dann simpel:
//          BSPNode<T>* leaf = FindLeaf(root, playerPos);
//          bool inWater = leaf && Contains(leaf->volumeIds, WATER_ID);
// =====================================================================

#include "BSPBuilder.h"
#include "PortalGen.h"
#include "Floodfill.h"   // FindLeaf(), Portal, PortalCenter()

#include <vector>
#include <algorithm>

namespace ng
{
    namespace bsp
    {




        // ------------------------------------------------------------
        // Markierungs-Flags.
        // ------------------------------------------------------------
        enum PolygonFlags : uint32_t
        {
            PF_None = 0,
            PF_BoundsBox = 1u << 0, // Seitenflaeche der Weltbegrenzungs-Box
            PF_VolumeFace = 1u << 1, // Seitenflaeche eines Bounding-Volumens
        };


        // ------------------------------------------------------------
        // Rekursives Einfuegen EINES Polygons in den bestehenden Baum.
        // Erzeugt keine neuen Knoten - endet immer in einem Leaf.
        // ------------------------------------------------------------
        template<class T>
        inline void InsertPolygon(
            BSPNode<T>* node,
            BSPPolygon<T> poly,
            T epsilon = static_cast<T>(1e-5))
        {
            if (!node || poly.verts.size() < 3)
                return;

            if (node->isLeaf)
            {
                node->insertedPolygons.push_back(std::move(poly));
                return;
            }

            int s = ClassifyPolygon(node->plane, poly, epsilon);

            switch (s)
            {
            case ng::math::RayPlaneLine<3, T>::FRONT:
            case ng::math::RayPlaneLine<3, T>::PLANAR:
                InsertPolygon(node->front.get(), std::move(poly), epsilon);
                break;

            case ng::math::RayPlaneLine<3, T>::BACK:
                InsertPolygon(node->back.get(), std::move(poly), epsilon);
                break;

            case ng::math::RayPlaneLine<3, T>::SPANNING:
            {
                BSPPolygon<T> frontPoly;
                BSPPolygon<T> backPoly;

                SplitPolygon(node->plane, poly, epsilon, frontPoly, backPoly);

                if (frontPoly.verts.size() >= 3)
                    InsertPolygon(node->front.get(), std::move(frontPoly), epsilon);

                if (backPoly.verts.size() >= 3)
                    InsertPolygon(node->back.get(), std::move(backPoly), epsilon);

                break;
            }
            }
        }

        template<class T>
        inline void InsertPolygons(
            BSPNode<T>* root,
            std::vector<BSPPolygon<T>> polys,
            T epsilon = static_cast<T>(1e-5))
        {
            for (auto& p : polys)
                InsertPolygon(root, std::move(p), epsilon);
        }


        // ------------------------------------------------------------
        // 6 Flaechen einer Weltbegrenzungs-Box, Normalen zeigen NACH
        // INNEN (Konvention: FRONT = Spielraum, siehe BSPBuilder.h).
        // worldSize sollte etwas KLEINER sein als das worldSize, mit
        // dem GeneratePortals() die Headnode-Portale erzeugt hat -
        // sonst liegen Box-Flaeche und Headnode-Portal exakt
        // aufeinander, was an der Epsilon-Grenze Aerger machen kann.
        // ------------------------------------------------------------
        template<class T>
        inline std::vector<BSPPolygon<T>> MakeBoundsBoxPolygons(
            T worldSize,
            uint32_t flags = PF_BoundsBox)
        {
            using Vec3 = ng::math::Matrix<3, 1, T>;
            using Vertex = ng::geometry::Vertex<T>;
            using Plane = typename BSPPolygon<T>::Plane;

            std::vector<BSPPolygon<T>> result;
            T h = worldSize * static_cast<T>(0.5);

            struct FaceDef { Vec3 normal; Vec3 center; Vec3 axisU; Vec3 axisV; };

            // normal zeigt jeweils zum Zentrum (nach innen).
            FaceDef faces[6] =
            {
                { Vec3(-1,0,0), Vec3(h,0,0), Vec3(0,0,1), Vec3(0,1,0) },
                { Vec3(1,0,0), Vec3(-h,0,0), Vec3(0,1,0), Vec3(0,0,1) },
                { Vec3(0,-1,0), Vec3(0, h,0), Vec3(1,0,0), Vec3(0,0,1) },
                { Vec3(0, 1,0), Vec3(0,-h,0), Vec3(0,0,1), Vec3(1,0,0) },
                { Vec3(0,0,-1), Vec3(0,0, h), Vec3(0,1,0), Vec3(1,0,0) },
                { Vec3(0,0, 1), Vec3(0,0,-h), Vec3(1,0,0), Vec3(0,1,0) },
            };

            for (auto& f : faces)
            {
                Vec3 p0 = f.center - f.axisU * h - f.axisV * h;
                Vec3 p1 = f.center + f.axisU * h - f.axisV * h;
                Vec3 p2 = f.center + f.axisU * h + f.axisV * h;
                Vec3 p3 = f.center - f.axisU * h + f.axisV * h;

                Vertex v0, v1, v2, v3;
                v0.world() = p0; v1.world() = p1;
                v2.world() = p2; v3.world() = p3;

                BSPPolygon<T> poly;
                poly.verts = { v0, v1, v2, v3 };
                poly.plane = Plane(f.center, f.normal);
                poly.flags = flags;

                result.push_back(std::move(poly));
            }

            return result;
        }


        // ------------------------------------------------------------
        // Findet ein Leaf, das ein PF_BoundsBox-markiertes Fragment
        // enthaelt. Da nachtraeglich eingefuegte Polygone NIE Splitter
        // werden, liegt so ein Fragment automatisch nur in echten
        // Aussen-/Void-Leafs - direkt verwendbar als FloodFill-Start.
        // ------------------------------------------------------------
        template<class T>
        inline BSPNode<T>* FindOutsideLeafViaBoxMarker(BSPNode<T>* node)
        {
            if (!node)
                return nullptr;

            if (node->isLeaf)
            {
                for (const auto& p : node->insertedPolygons)
                {
                    if (p.flags & PF_BoundsBox)
                        return node;
                }
                return nullptr;
            }

            if (auto* found = FindOutsideLeafViaBoxMarker(node->front.get()))
                return found;

            return FindOutsideLeafViaBoxMarker(node->back.get());
        }


        // ------------------------------------------------------------
        // Bounding-Volumen (Wasser/Lava/...) als Halbraum-Menge.
        // Normalen zeigen NACH AUSSEN (Standard-Brush-Konvention,
        // Punkt liegt innen, wenn er auf jeder Flaeche <= 0 liegt).
        // ------------------------------------------------------------
        template<class T>
        struct VolumeBrush
        {
            std::vector<BSPPolygon<T>> faces;
            int volumeId = -1;
            int contents = 0; // z.B. CONTENTS_WATER, CONTENTS_LAVA, ...
        };

        template<class T>
        inline bool PointInBrush(
            const VolumeBrush<T>& brush,
            const ng::math::Matrix<3, 1, T>& point,
            T epsilon = static_cast<T>(1e-4))
        {
            for (const auto& f : brush.faces)
            {
                if (PlaneDistance(f.plane, point) > epsilon)
                    return false;
            }
            return true;
        }

        // Mittelwert aller Vertices aller anliegenden Portale eines
        // Leafs - fuer ein konvexes Leaf ein garantiert innenliegender
        // (oder randliegender) Punkt.
        template<class T>
        inline bool LeafRepresentativePoint(
            const BSPNode<T>* leaf,
            ng::math::Matrix<3, 1, T>& outPoint)
        {
            ng::math::Matrix<3, 1, T> sum(0, 0, 0);
            size_t count = 0;

            for (const Portal<T>* portal : leaf->portals)
            {
                for (const auto& v : portal->winding)
                {
                    sum = sum + v.world();
                    ++count;
                }
            }

            if (count == 0)
                return false;

            outPoint = sum * (static_cast<T>(1) / static_cast<T>(count));
            return true;
        }

        template<class T>
        inline void TagVolumeLeafs(
            BSPNode<T>* node,
            const VolumeBrush<T>& brush,
            T epsilon)
        {
            if (!node)
                return;

            if (node->isLeaf)
            {
                if (node->leafType != BSPNode<T>::LeafType::Empty)
                    return; // Solid/Unknown: nie begehbar -> irrelevant.

                ng::math::Matrix<3, 1, T> p;

                if (!LeafRepresentativePoint(node, p))
                    return;

                if (PointInBrush(brush, p, epsilon))
                    node->volumeIds.push_back(brush.volumeId);

                return;
            }

            TagVolumeLeafs(node->front.get(), brush, epsilon);
            TagVolumeLeafs(node->back.get(), brush, epsilon);
        }

        // ------------------------------------------------------------
        // Fuegt ein Bounding-Volumen nachtraeglich ein.
        //
        // Voraussetzung: ClassifyLeaves() wurde bereits aufgerufen,
        // damit leafType (Empty/Solid) gesetzt ist.
        // ------------------------------------------------------------
        template<class T>
        inline void InsertVolumeBrush(
            BSPNode<T>* root,
            const VolumeBrush<T>& brush,
            T epsilon = static_cast<T>(1e-5))
        {
            for (auto face : brush.faces)
            {
                face.flags |= PF_VolumeFace;
                face.volumeId = brush.volumeId;
                InsertPolygon(root, std::move(face), epsilon);
            }

            TagVolumeLeafs(root, brush, epsilon);
        }

    } // namespace bsp
} // namespace ng
