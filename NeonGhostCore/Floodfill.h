#pragma once
// =====================================================================
// Floodfill.h
//
// Klassifiziert alle BSP-Blaetter als Empty (begehbar/Luft) oder Solid
// (massiv, nie von einem Beobachter erreichbar) - der letzte Schritt
// vor "unsichtbare Polygone entfernen" und Portal-basiertem Rendering.
//
// ABLAUF:
//   1) FloodFillOutside() flutet per Breitensuche vom Aussenpunkt
//      (z.B. Skybox-Zentrum) ueber alle Portale - jedes erreichte
//      Blatt wird Empty.
//   2) FinalizeLeafTypes() markiert alle NICHT erreichten Blaetter als
//      Solid (massives Gestein/Wandinneres).
//
// LECK-ERKENNUNG (klassische Technik, wie in id Softwares qbsp):
// Der Aufrufer gibt zusaetzlich einen "insideCheckPoint" an (z.B. eine
// Position irgendwo im Schlossinneren, die garantiert NICHT direkt mit
// der Aussenwelt verbunden sein sollte - typischerweise "player start").
// Wird das Blatt dieses Punktes durch die AUSSEN-Flutung erreicht,
// bedeutet das: irgendwo gibt es eine Luecke in der Geometrie, durch
// die Aussenluft ins Innere "durchsickert". LeakInfo::path liefert dann
// die Kette von Portal-Mittelpunkten vom Aussenpunkt bis zum Leck - zum
// Visualisieren/Debuggen der Fundstelle.
// =====================================================================

#include "PortalGen.h"
#include <queue>
#include <unordered_map>
#include <unordered_set>

namespace ng
{
    namespace bsp
    {
        // ------------------------------------------------------------
        // Findet das Blatt, das einen gegebenen Punkt enthaelt - steigt
        // den Baum anhand der Ebenen-Tests ab.
        // ------------------------------------------------------------
        template<class T>
        inline BSPNode<T>* FindLeaf(BSPNode<T>* node, const ng::math::Matrix<3, 1, T>& point)
        {
            if (!node) return nullptr;
            while (!node->isLeaf)
            {

                auto d = node->plane.classify(point);
                node = (d == ng::math::RayPlaneLine<3, T>::FRONT) ? node->front.get() : node->back.get();
                if (!node) return nullptr;
            }
            return node;
        }

        template<class T>
        inline ng::math::Matrix<3, 1, T> PortalCenter(const Portal<T>& portal)
        {
            ng::math::Matrix<3, 1, T> c(0, 0, 0);
            for (const auto& v : portal.winding)
                c = c + v.world();
            if (!portal.winding.empty())
                c = c * (static_cast<T>(1) / static_cast<T>(portal.winding.size()));
            return c;
        }

        // ------------------------------------------------------------
        // Ergebnis eines Floodfill-Durchlaufs.
        // ------------------------------------------------------------
        template<class T>
        struct LeakInfo
        {
            bool hasLeak = false;
            // Kette von Portal-Mittelpunkten vom Aussenpunkt bis zur
            // Leck-Stelle (nur gueltig, wenn hasLeak == true).
            std::vector<ng::math::Matrix<3, 1, T>> path;
        };

        // ------------------------------------------------------------
        // Flutet vom Aussenpunkt aus per Breitensuche ueber alle
        // Portale. Markiert jedes erreichte Blatt als Empty.
        //
        // insideCheckPoint (optional): falls angegeben und dessen Blatt
        // von der Aussen-Flutung erreicht wird, gilt das als Leck (siehe
        // Kopfkommentar) - der Rueckgabewert enthaelt dann den Pfad
        // dorthin.
        // ------------------------------------------------------------
        template<class T>
        inline LeakInfo<T> FloodFillOutside(
            BSPNode<T>* root,
            const ng::math::Matrix<3, 1, T>& outsideSeed,
            const ng::math::Matrix<3, 1, T>* insideCheckPoint = nullptr)
        {
            LeakInfo<T> leak;

            BSPNode<T>* startLeaf = FindLeaf(root, outsideSeed);
            if (!startLeaf)
            {
                leak.hasLeak = true; // Seed-Punkt liegt ausserhalb des Baums
                return leak;
            }
            if (startLeaf->leafType == BSPNode<T>::LeafType::Solid)
            {
                // outsideSeed liegt in massivem Gestein - z.B. Skybox zu
                // klein/knapp gewaehlt, oder Seed-Punkt falsch platziert.
                leak.hasLeak = true;
                return leak;
            }

            BSPNode<T>* checkLeaf = insideCheckPoint ? FindLeaf(root, *insideCheckPoint) : nullptr;

            struct Parent
            {
                BSPNode<T>* from;
                ng::math::Matrix<3, 1, T> viaPortalCenter;
            };
            std::unordered_map<BSPNode<T>*, Parent> parent;
            std::unordered_set<BSPNode<T>*> visited;
            std::queue<BSPNode<T>*> q;

            visited.insert(startLeaf);
            startLeaf->leafType = BSPNode<T>::LeafType::Empty;
            q.push(startLeaf);

            BSPNode<T>* leakLeaf = nullptr;

            while (!q.empty())
            {
                BSPNode<T>* leaf = q.front();
                q.pop();

                for (Portal<T>* portal : leaf->portals)
                {
                    BSPNode<T>* neighbor = (portal->nodes[0] == leaf) ? portal->nodes[1] : portal->nodes[0];
                    if (!neighbor) continue; // Aussenraum-Sentinel - kein echtes Blatt
                    if (visited.count(neighbor)) continue;

                    visited.insert(neighbor);
                    neighbor->leafType = BSPNode<T>::LeafType::Empty;
                    parent[neighbor] = { leaf, PortalCenter(*portal) };
                    q.push(neighbor);

                    if (neighbor == checkLeaf)
                        leakLeaf = neighbor;
                }
            }

            if (checkLeaf && leakLeaf)
            {
                leak.hasLeak = true;
                // Pfad rueckwaerts vom Leck bis zum Aussenpunkt
                // rekonstruieren, dann umdrehen fuer "Seed -> Leck".
                std::vector<ng::math::Matrix<3, 1, T>> reversed;
                BSPNode<T>* cur = leakLeaf;
                while (parent.count(cur))
                {
                    reversed.push_back(parent[cur].viaPortalCenter);
                    cur = parent[cur].from;
                }
                leak.path.assign(reversed.rbegin(), reversed.rend());
            }

            return leak;
        }

        // ------------------------------------------------------------
        // Nach FloodFillOutside(): alle NICHT erreichten Blaetter
        // (leafType noch Unknown) sind Solid - massives Gestein/
        // Wandinneres, das kein Beobachter jemals erreichen kann.
        // ------------------------------------------------------------
        template<class T>
        inline void FinalizeLeafTypes(BSPNode<T>* node)
        {
            if (!node) return;
            if (node->isLeaf)
            {
                if (node->leafType == BSPNode<T>::LeafType::Unknown)
                    node->leafType = BSPNode<T>::LeafType::Solid;
                return;
            }
            FinalizeLeafTypes(node->front.get());
            FinalizeLeafTypes(node->back.get());
        }

        // ------------------------------------------------------------
        // Komfort-Einstieg: fuehrt Floodfill + Finalisierung in einem
        // Aufruf durch. Gibt LeakInfo zurueck - PRUEFT hasLeak, bevor
        // ihr dem Ergebnis vertraut!
        // ------------------------------------------------------------
        template<class T>
        inline LeakInfo<T> ClassifyLeaves(
            BSPNode<T>* root,
            const ng::math::Matrix<3, 1, T>& outsideSeed,
            const ng::math::Matrix<3, 1, T>* insideCheckPoint = nullptr)
        {
            LeakInfo<T> leak = FloodFillOutside(root, outsideSeed, insideCheckPoint);
            FinalizeLeafTypes(root);
            return leak;
        }

        // ------------------------------------------------------------
        // BONUS: entfernt dauerhaft alle Polygone, deren BEIDE
        // angrenzenden Regionen komplett Solid sind (z.B. Innenflaechen
        // doppelt dicker Waende, die kein Beobachter je erreichen kann -
        // siehe urspruengliche Diskussion im Chat). Ein Polygon an einem
        // Knoten trennt node->front und node->back - beide Teilbaeume
        // muessen VOLLSTAENDIG (alle Blaetter) Solid sein, damit das
        // Polygon sicher geloescht werden darf.
        // ------------------------------------------------------------
        template<class T>
        inline bool IsEntirelySolid(const BSPNode<T>* node)
        {
            if (!node) return true; // sollte nach BuildBSP nicht vorkommen, sicherheitshalber
            if (node->isLeaf)
                return node->leafType == BSPNode<T>::LeafType::Solid;
            return IsEntirelySolid(node->front.get()) && IsEntirelySolid(node->back.get());
        }

        template<class T>
        inline void RemoveDoubleSolidPolygons(BSPNode<T>* node)
        {
            if (!node || node->isLeaf) return;

            bool frontSolid = IsEntirelySolid(node->front.get());
            bool backSolid = IsEntirelySolid(node->back.get());
            if (frontSolid && backSolid)
                node->coplanar.clear();

            RemoveDoubleSolidPolygons(node->front.get());
            RemoveDoubleSolidPolygons(node->back.get());
        }

    } // namespace bsp
} // namespace ng