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
#include <functional>
#include <queue>
#include <unordered_map>
#include <unordered_set>

namespace ng
{
    namespace bsp
    {

        template<class T>
        struct BSPNode;

        // Ein CSG-Brush als Menge von Halbraum-Ebenen (Normalen nach aussen).
        template<class T>
        struct BrushHull
        {
            std::vector<ng::math::RayPlaneLine<3, T>> planes;
        };

        template<class T>
        inline bool PointInsideBrush(
            const BrushHull<T>& brush,
            const ng::math::Matrix<3, 1, T>& p,
            T epsilon = static_cast<T>(1e-4))
        {
            for (const auto& plane : brush.planes)
                if (PlaneDistance(plane, p) > epsilon)
                    return false;
            return true;
        }

        template<class T>
        inline bool PointInsideAnyBrush(
            const std::vector<BrushHull<T>>& brushes,
            const ng::math::Matrix<3, 1, T>& p,
            T epsilon = static_cast<T>(1e-4))
        {
            for (const auto& b : brushes)
                if (PointInsideBrush(b, p, epsilon))
                    return true;
            return false;
        }
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
      /*
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
        */
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
                if (node->leafType == BSPNode<T>::LeafType::Unknown) {
                    printf("Make node Solid\n");
                    node->leafType = BSPNode<T>::LeafType::Solid;
                }
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

        // Kernlogik extrahiert, akzeptiert direkt einen Start-Leaf-Pointer.
        template<class T>
        inline LeakInfo<T> FloodFillFromLeaf(
            BSPNode<T>* startLeaf,
            BSPNode<T>* checkLeaf = nullptr)
        {
            LeakInfo<T> leak;

            if (!startLeaf || startLeaf->leafType == BSPNode<T>::LeafType::Solid)
            {
                leak.hasLeak = true;
                return leak;
            }

            struct Parent { BSPNode<T>* from; ng::math::Matrix<3, 1, T> viaPortalCenter; };
            std::unordered_map<BSPNode<T>*, Parent> parent;
            std::unordered_set<BSPNode<T>*> visited;
            std::queue<BSPNode<T>*> q;

            visited.insert(startLeaf);
            startLeaf->leafType = BSPNode<T>::LeafType::Empty;
            q.push(startLeaf);

            BSPNode<T>* leakLeaf = nullptr;

            while (!q.empty())
            {
                BSPNode<T>* leaf = q.front(); q.pop();

                for (Portal<T>* portal : leaf->portals)
                {
                    BSPNode<T>* neighbor = (portal->nodes[0] == leaf) ? portal->nodes[1] : portal->nodes[0];
                    if (!neighbor || visited.count(neighbor)) continue;

                    visited.insert(neighbor);
                    neighbor->leafType = BSPNode<T>::LeafType::Empty;
                    parent[neighbor] = { leaf, PortalCenter(*portal) };
                    q.push(neighbor);

                    if (neighbor == checkLeaf) leakLeaf = neighbor;
                }
            }

            if (checkLeaf && leakLeaf)
            {
                leak.hasLeak = true;
                std::vector<ng::math::Matrix<3, 1, T>> reversed;
                BSPNode<T>* cur = leakLeaf;
                while (parent.count(cur)) { reversed.push_back(parent[cur].viaPortalCenter); cur = parent[cur].from; }
                leak.path.assign(reversed.rbegin(), reversed.rend());
            }

            return leak;
        }


        ///////////////////////////////////

        // Bestehende punktbasierte Variante baut jetzt darauf auf:
        template<class T>
        inline LeakInfo<T> FloodFillOutside(
            BSPNode<T>* root,
            const ng::math::Matrix<3, 1, T>& outsideSeed,
            const ng::math::Matrix<3, 1, T>* insideCheckPoint = nullptr)
        {
            BSPNode<T>* startLeaf = FindLeaf(root, outsideSeed);
            BSPNode<T>* checkLeaf = insideCheckPoint ? FindLeaf(root, *insideCheckPoint) : nullptr;
            return FloodFillFromLeaf(startLeaf, checkLeaf);
        }

        // NEU: Start ueber direkt gefundenes Leaf (z.B. via
        // FindOutsideLeafViaBoxMarker aus BSPInsert.h).
        template<class T>
        inline LeakInfo<T> FloodFillOutsideFromLeaf(
            BSPNode<T>* startLeaf,
            BSPNode<T>* root = nullptr,
            const ng::math::Matrix<3, 1, T>* insideCheckPoint = nullptr)
        {
            BSPNode<T>* checkLeaf = (root && insideCheckPoint) ? FindLeaf(root, *insideCheckPoint) : nullptr;
            return FloodFillFromLeaf(startLeaf, checkLeaf);
        }

        // ------------------------------------------------------------
// Stufe 1: Void-Flood von einem Leaf ausserhalb der Welt.
// (startLeaf typischerweise via FindOutsideLeafViaBoxMarker())
// ------------------------------------------------------------
        template<class T>
        inline void FloodFillVoid(BSPNode<T>* startLeaf)
        {
            if (!startLeaf) return;

            std::unordered_set<BSPNode<T>*> visited;
            std::queue<BSPNode<T>*> q;

            visited.insert(startLeaf);
            startLeaf->leafType = BSPNode<T>::LeafType::Void;
            q.push(startLeaf);

            while (!q.empty())
            {
                BSPNode<T>* leaf = q.front(); q.pop();

                for (Portal<T>* portal : leaf->portals)
                {
                    BSPNode<T>* neighbor =
                        (portal->nodes[0] == leaf) ? portal->nodes[1] : portal->nodes[0];

                    if (!neighbor || visited.count(neighbor))
                        continue;

                    visited.insert(neighbor);
                    neighbor->leafType = BSPNode<T>::LeafType::Void;
                    q.push(neighbor);
                }
            }
        }

        // ------------------------------------------------------------
        // Stufe 2: Playable-Flood von einem oder mehreren bekannten
        // Innenpunkten (Player-Start, Monster-Spawns, ...). Läuft NUR
        // über Leafs, die noch nicht Void sind - kann also nicht mehr
        // über eine Leck-Stelle "entkommen".
        // ------------------------------------------------------------
        template<class T>
        inline void FloodFillPlayable(
            BSPNode<T>* root,
            const std::vector<ng::math::Matrix<3, 1, T>>& insideSeeds)
        {
            std::queue<BSPNode<T>*> q;

            for (const auto& seed : insideSeeds)
            {
                BSPNode<T>* leaf = FindLeaf(root, seed);

                if (leaf && leaf->leafType == BSPNode<T>::LeafType::Unknown)
                {
                    leaf->leafType = BSPNode<T>::LeafType::Empty;
                    q.push(leaf);
                }
            }

            while (!q.empty())
            {
                BSPNode<T>* leaf = q.front(); q.pop();

                for (Portal<T>* portal : leaf->portals)
                {
                    BSPNode<T>* neighbor =
                        (portal->nodes[0] == leaf) ? portal->nodes[1] : portal->nodes[0];

                    if (!neighbor)
                        continue;

                    // Nur ueber noch unklassifizierte Leafs weiterfluten -
                    // Void und bereits als Empty markierte werden uebersprungen.
                    if (neighbor->leafType != BSPNode<T>::LeafType::Unknown)
                        continue;

                    neighbor->leafType = BSPNode<T>::LeafType::Empty;
                    q.push(neighbor);
                }
            }
        }

        // ------------------------------------------------------------
        // Einstieg: Void -> Playable -> Rest = Solid
        // ------------------------------------------------------------
        template<class T>
        inline LeakInfo<T> ClassifyLevel(
            BSPNode<T>* root,
            BSPNode<T>* voidStartLeaf,
            const std::vector<ng::math::Matrix<3, 1, T>>& insideSeeds)
        {
            FloodFillVoid(voidStartLeaf);
            FloodFillPlayable(root, insideSeeds);

            LeakInfo<T> leak;

            // Leck-Check: liegt einer der Innen-Seeds in einem Void-Leaf?
            for (const auto& seed : insideSeeds)
            {
                BSPNode<T>* leaf = FindLeaf(root, seed);

                if (leaf && leaf->leafType == BSPNode<T>::LeafType::Void)
                {
                    leak.hasLeak = true;
                    break;
                }
            }

            // Alles, was weder Void noch Empty erreicht hat, ist Solid.
            std::function<void(BSPNode<T>*)> finalize = [&](BSPNode<T>* node)
                {
                    if (!node) return;

                    if (node->isLeaf)
                    {
                        if (node->leafType == BSPNode<T>::LeafType::Unknown)
                            node->leafType = BSPNode<T>::LeafType::Solid;
                        return;
                    }

                    finalize(node->front.get());
                    finalize(node->back.get());
                };

            finalize(root);

            return leak;
        }

        // Enthaelt der Teilbaum irgendwo ein begehbares Leaf?
        template<class T>
        inline bool ContainsPlayableLeaf(const BSPNode<T>* node)
        {
            if (!node) return false;

            if (node->isLeaf)
                return node->leafType == BSPNode<T>::LeafType::Empty;

            return ContainsPlayableLeaf(node->front.get()) ||
                ContainsPlayableLeaf(node->back.get());
        }

        // Loescht alle Splitter-Polygone (node->coplanar), die auf KEINER
        // der beiden Seiten an begehbaren Raum grenzen - egal ob die andere
        // Seite Solid, Void oder gemischt ist.
        template<class T>
        inline void RemoveInvisiblePolygons(BSPNode<T>* node)
        {
            if (!node || node->isLeaf)
                return;

            bool frontPlayable = ContainsPlayableLeaf(node->front.get());
            bool backPlayable = ContainsPlayableLeaf(node->back.get());

            if (!frontPlayable && !backPlayable)
                node->coplanar.clear();

            RemoveInvisiblePolygons(node->front.get());
            RemoveInvisiblePolygons(node->back.get());
        }


        template<class T>
        inline void ClassifyLeafsFinal(
            BSPNode<T>* root,
            const std::vector<BrushHull<T>>& originalBrushes,
            T epsilon = static_cast<T>(1e-4))
        {
            // 1) Void: Multi-Seed-Flood von allen box-markierten Leafs.
            std::vector<BSPNode<T>*> boxLeafs;
            CollectBoxLeafs(root, boxLeafs);

            std::unordered_set<BSPNode<T>*> visited;
            std::queue<BSPNode<T>*> q;

            for (auto* leaf : boxLeafs)
            {
                if (visited.insert(leaf).second)
                {
                    leaf->leafType = BSPNode<T>::LeafType::Void;
                    q.push(leaf);
                }
            }

            while (!q.empty())
            {
                BSPNode<T>* leaf = q.front(); q.pop();

                for (Portal<T>* portal : leaf->portals)
                {
                    BSPNode<T>* neighbor =
                        (portal->nodes[0] == leaf) ? portal->nodes[1] : portal->nodes[0];

                    if (!neighbor || visited.count(neighbor))
                        continue;

                    visited.insert(neighbor);
                    neighbor->leafType = BSPNode<T>::LeafType::Void;
                    q.push(neighbor);
                }
            }

            // 2) Solid vs. Empty (Playable): rein geometrisch gegen die
            //    Original-Brushes, unabhaengig vom Flood.
            std::function<void(BSPNode<T>*)> classifyRest = [&](BSPNode<T>* node)
                {
                    if (!node) return;

                    if (node->isLeaf)
                    {
                        if (node->leafType == BSPNode<T>::LeafType::Void)
                            return; // schon klassifiziert

                        ng::math::Matrix<3, 1, T> p;

                        if (LeafRepresentativePoint(node, p) &&
                            PointInsideAnyBrush(originalBrushes, p, epsilon))
                        {
                            node->leafType = BSPNode<T>::LeafType::Solid;
                        }
                        else
                        {
                            node->leafType = BSPNode<T>::LeafType::Empty; // Playable
                        }
                        return;
                    }

                    classifyRest(node->front.get());
                    classifyRest(node->back.get());
                };

            classifyRest(root);
        }

    } // namespace bsp
} // namespace ng