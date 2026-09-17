#pragma once
// =====================================================================
// BSPRenderer.h
//
// Rendert einen ng::bsp::BSPNode<T>-Baum (siehe BSPBuilder.h) direkt,
// OHNE Portale/PVS - klassisches BSP-Traversal, wie es urspruenglich
// (vor Z-Buffern) fuer korrekte Verdeckung ohne Tiefenpuffer erfunden
// wurde. Zwei Reihenfolgen:
//
//   RenderFrontToBack: naeher am Betrachter zuerst. Sinnvoll fuer
//   OPAQUE Geometrie mit aktivem Z-Test (wie euer Rasterizer es
//   sowieso macht) - nahe Flaechen zuerst fuellt den Tiefenpuffer
//   frueh, sodass weiter entfernte, verdeckte Flaechen den Tiefentest
//   schneller verwerfen (Early-Z-Vorteil, kein Overdraw-Vorteil ohne
//   MOC/HZB davor, aber schadet nicht).
//
//   RenderBackToFront: entfernteste zuerst. Klassisches Painter's-
//   Algorithm-Verhalten - fuer TRANSPARENTE Flaechen (Wasser, Glas)
//   ohne Tiefentest/-schreiben brauchbar, weil dann jede weiter vorne
//   liegende Flaeche die entferntere korrekt ueberschreibt/blendet.
//
// WICHTIGER ZWISCHENSCHRITT (vor Portalen/PVS):
// Dieser Renderer durchlaeuft IMMER den kompletten Baum - keine
// Verdeckungs-/Sichtbarkeitsoptimierung. Fuer eine erste visuelle
// Kontrolle ("wurde mein Schloss korrekt gesplittet?") reicht das.
// Die Leaf-Render-Funktion hier (RenderLeafPolygons) wird SPAETER
// direkt vom Portal-Traversal wiederverwendet - nur die Auswahl,
// WELCHE Blaetter besucht werden, aendert sich dann.
// =====================================================================

#include "BSPBuilder.h"
#include <vector>

namespace ng
{
    namespace bsp
    {
        // ------------------------------------------------------------
        // Rendert alle Polygone eines einzelnen Blattes (bzw. die
        // koplanaren Polygone eines inneren Knotens - jeder Knoten
        // kann eigene koplanare Polygone tragen, siehe BSPBuilder.h).
        // PipelineT muss eine RenderPolygon(const std::vector<Vertex<T>>&)
        // Methode anbieten - siehe Ergaenzung in RenderPipeline.h.
        // ------------------------------------------------------------
        template<class T, class PipelineT>
        inline void RenderNodePolygons(const BSPNode<T>* node, PipelineT& pipeline)
        {
            if (!node) return;
            for (const auto& poly : node->coplanar)
                pipeline.RenderPolygon(poly.verts);
        }

        // ------------------------------------------------------------
        // Front-to-back: fuer opaque Geometrie mit Z-Test (Normalfall).
        // ------------------------------------------------------------
        template<class T, class PipelineT>
        inline void RenderFrontToBack(
            const BSPNode<T>* node,
            const ng::math::Matrix<3, 1, T>& viewerPos,
            PipelineT& pipeline)
        {
            if (!node) return;
            if (node->leafType == BSPNode<T>::LeafType::Solid) return;
            if (node->isLeaf)
            {
                RenderNodePolygons(node, pipeline);
                return;
            }

            // Auf welcher Seite der Ebene steht der Betrachter? Diese
            // Seite ist "naeher" (kann den Betrachter nicht verdecken,
            // die Gegenseite koennte durch diesen Knoten selbst verdeckt
            // sein) - Standard-BSP-Reihenfolge.
            
            using Plane = ng::math::RayPlaneLine<3, T>;
            const auto res = node->plane.classify(viewerPos);
            const bool viewerInFront = (res == Plane::FRONT);
            const BSPNode<T>* nearSide = viewerInFront ? node->front.get() : node->back.get();
            const BSPNode<T>* farSide = viewerInFront ? node->back.get() : node->front.get();
            /*
            T dist = node->plane.distance(viewerPos);
            const BSPNode<T>* nearSide = (dist >= (T)0) ? node->front.get() : node->back.get();
            const BSPNode<T>* farSide = (dist >= (T)0) ? node->back.get() : node->front.get();
            */

            RenderFrontToBack(nearSide, viewerPos, pipeline);
            RenderNodePolygons(node, pipeline); // Polygone auf DIESER Ebene selbst
            RenderFrontToBack(farSide, viewerPos, pipeline);
        }

        // ------------------------------------------------------------
        // Back-to-front: klassisches Painter's-Algorithm-Verhalten,
        // fuer Transparenz-Passes ohne verlaesslichen Z-Test.
        // ------------------------------------------------------------
        template<class T, class PipelineT>
        inline void RenderBackToFront(
            const BSPNode<T>* node,
            const ng::math::Matrix<3, 1, T>& viewerPos,
            PipelineT& pipeline)
        {
            if (!node) return;
            if (node->leafType == BSPNode<T>::LeafType::Solid) return;

            if (node->isLeaf)
            {
                RenderNodePolygons(node, pipeline);
                return;
            }

            using Plane = ng::math::RayPlaneLine<3, T>;
            const auto res = node->plane.classify(viewerPos);
            const bool viewerInFront = (res == Plane::FRONT);
            const BSPNode<T>* nearSide = viewerInFront ? node->front.get() : node->back.get();
            const BSPNode<T>* farSide = viewerInFront ? node->back.get() : node->front.get();

            
//            T dist = node->plane.Distance(viewerPos);
//            const BSPNode<T>* nearSide = (dist >= (T)0) ? node->front.get() : node->back.get();
//            const BSPNode<T>* farSide = (dist >= (T)0) ? node->back.get() : node->front.get();

            RenderBackToFront(farSide, viewerPos, pipeline);
            RenderNodePolygons(node, pipeline);
            RenderBackToFront(nearSide, viewerPos, pipeline);
        }

    } // namespace bsp
} // namespace ng