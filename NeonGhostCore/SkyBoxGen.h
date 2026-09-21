#pragma once
// =====================================================================
// SkyboxGen.h
//
// Erzeugt eine Box-Skybox aus einer AABB (oder direkt aus einer
// bestehenden Polygon-Liste berechnet) und fuegt sie VOR dem BSP-Build
// in den Polygon-Pool ein. Dadurch werden die Skybox-Flaechen zu
// echten node->coplanar-Polygonen im Baum - der Floodfill-Startpunkt
// (naechster Schritt) ist dann einfach irgendein Punkt INNERHALB der
// Skybox (offene Luft ueber/um das Schloss).
//
// WICHTIG: Normalenkonvention ist hier UMGEKEHRT zu einer normalen
// Wand. "Leer/begehbar" (Himmel, Luft) liegt INNERHALB der Box, alles
// jenseits davon existiert fuer die Engine nicht. Die Normalen der
// Skybox-Flaechen zeigen deshalb NACH INNEN - genau das, was
// AABBox<3,T>::Faces() bereits liefert (das dortige abschliessende
// std::reverse() dreht die Wicklung genau dafuer um).
// =====================================================================

#include "BSP.h"
#include "AABBox.h"
#include <vector>

namespace ng
{
    namespace bsp
    {
        // ------------------------------------------------------------
        // Berechnet die AABB ueber alle Vertices einer Polygon-Liste -
        // z.B. eurer importierten Schlossgeometrie, BEVOR die Skybox
        // hinzugefuegt wird.
        // ------------------------------------------------------------
        template<class T>
        inline ng::bounding::AABBox<3, T> ComputeSceneBounds(const std::vector<BSPPolygon<T>>& polys)
        {
            ng::bounding::AABBox<3, T> box;
            for (const auto& poly : polys)
                for (const auto& v : poly.verts)
                    box += v.world();
            return box;
        }

        // ------------------------------------------------------------
        // Erzeugt die 6 Skybox-Quads (als je 2 Dreiecke) um eine AABB,
        // mit optionalem Sicherheitsabstand (margin) nach aussen.
        // Normalen zeigen NACH INNEN (siehe Kopfkommentar).
        // ------------------------------------------------------------
        template<class T>
        inline std::vector<BSPPolygon<T>> GenerateSkyboxPolygons(
            ng::bounding::AABBox<3, T> bounds,
            T margin = static_cast<T>(0),
            int materialId = -1)
        {
            using Vertex = ng::geometry::Vertex<T>;

            if (margin > static_cast<T>(0))
                bounds += margin; // AABBox::operator+= VERGROESSERT die Box

            std::vector<BSPPolygon<T>> result;
            auto faces = bounds.Faces(); // 6 Quads, bereits nach innen gewickelt
       
            const ng::math::Matrix<2, 1, T> uv[4] = {{0,0},{1,0},{1,1},{0,1}};
            

            for (auto& face : faces)
            {
                // Fan-Triangulierung (face[0], face[i], face[i+1]) -
                // aendert die Wicklungsrichtung/Normale NICHT, da alle
                // Dreiecke denselben Start-Vertex teilen.
                for (size_t i = 1; i + 1 < face.size(); ++i)
                {
                    Vertex v0, v1, v2;
                    v0.world() = face[0];
                    v0.texture() = uv[0];

                    v1.world() = face[i];
                    v1.texture() = uv[i];

                    v2.world() = face[i + 1];
                    v2.texture() = uv[i + 1];
                    auto tri = BSPPolygon<T>::FromTriangle(v2, v1, v0, materialId);
                    tri.flags = POLY_FLAG_OUT_SIDE;
                    result.push_back(tri);
                }
            }
            return result;
        }

        // ------------------------------------------------------------
        // Komfort-Funktion: berechnet die Szene-AABB direkt aus polys,
        // erzeugt die Skybox mit Sicherheitsabstand margin und haengt
        // sie an polys an. AUFRUFEN, BEVOR BuildBSP() laeuft.
        //
        // Gibt die verwendete (bereits um margin vergroesserte) AABB
        // zurueck - die wird spaeter fuer den Floodfill-Seed-Punkt und
        // fuer die Portal-Kopfbox (GeneratePortalsForScene) gebraucht.
        // ------------------------------------------------------------
        template<class T>
        inline ng::bounding::AABBox<3, T> AddSkyboxToScene(
            std::vector<BSPPolygon<T>>& polys,
            T margin = static_cast<T>(50),
            int materialId = -1)
        {
            auto bounds = ComputeSceneBounds(polys);
            auto skyboxBounds = bounds;
            if (margin > static_cast<T>(0))
                skyboxBounds += margin;

            auto skyboxPolys = GenerateSkyboxPolygons(bounds, margin, materialId);
            polys.insert(polys.end(),
                std::make_move_iterator(skyboxPolys.begin()),
                std::make_move_iterator(skyboxPolys.end()));

            return skyboxBounds;
        }

    } // namespace bsp
} // namespace ng
