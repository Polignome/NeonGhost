#pragma once
// =====================================================================
// Octree.h
//
// Statischer Octree fuer raeumliches Culling. Nutzt eure bestehende
// ng::bounding::AABBox<3,T> als Bounding-Volume - keine eigene Box-
// Implementierung noetig.
//
// Kerngedanke (siehe Chatverlauf):
//   - Einmal gebaut (z.B. beim Level-Laden), NICHT pro Frame neu.
//   - TraverseFrontToBack() besucht Knoten/Objekte in Nah-Fern-
//     Reihenfolge relativ zur Kamera - ersetzt das explizite Sortieren
//     einer flachen Objektliste.
//   - Frustum- UND Occlusion-Test passieren auf KNOTEN-Ebene: ein
//     kompletter Teilbaum wird uebersprungen, sobald sein AABB nicht
//     sichtbar ist, ohne dass jedes einzelne Objekt darin einzeln
//     getestet werden muss.
//
// Bewegliche Objekte gehoeren NICHT in diesen Baum (staendiges Neu-
// Einfuegen waere teuer) - die behandelt ihr weiterhin per einfacher
// Liste + Sort (siehe RenderPipeline::SortObjectsByDistanceToCamera).
// Nur die STATISCHEN Objekte einer Szene profitieren hier.
// =====================================================================




//   Init 
//  
//  ng::bounding::AABBox<3, T> worldBounds( /*Level-Grenzen*/ );
//  ng::spatial::Octree<T, objects::Mesh<T>*> staticOctree(worldBounds);
//  
//  for (auto& mesh : staticMeshes)
//  staticOctree.Insert(&mesh, mesh.Bounds());
//  
//  Use:
//  
//  m_render_pipeline_3d.Begin();
//  
//  staticOctree.TraverseFrontToBack(
//      m_camera_pos,
//      /* frustumTest */  [&](const auto& box) { return m_frustum.intersectsAABB(box); },
//      /* occludedTest */ [&](const auto& box) { return !MOCTestAABBoxVisibility(box); },
//      /* visit */        [&](objects::Mesh<T>* mesh, const auto& box) { RenderMesh(*mesh); }
//  );
//  
//  m_render_pipeline_3d.End();
//  





#include "Global.h"
#include "AABBox.h"
#include <array>
#include <memory>
#include <vector>
#include <algorithm>
#include <cstddef>

namespace ng
{
    namespace spatial
    {
        template<class T, class ObjectPtrT>
        class Octree
        {
        public:
            using AABB = ng::bounding::AABBox<3, T>;
            using Vec3 = ng::math::Matrix<3, 1, T>;

            struct Entry
            {
                ObjectPtrT object;
                AABB bounds;
            };

            // ------------------------------------------------------------
            // worldBounds: AABB, die die gesamte Szene (bzw. den Bereich,
            //              den der Baum abdecken soll) umschliesst.
            // maxDepth:    Obergrenze fuer die Rekursionstiefe - verhindert
            //              endlose Unterteilung bei ungluecklich verteilter
            //              Geometrie.
            // maxEntriesPerNode: ab wie vielen Objekten ein Blatt-Knoten
            //              unterteilt wird.
            // ------------------------------------------------------------
            explicit Octree(
                const AABB& worldBounds,
                int maxDepth = 8,
                size_t maxEntriesPerNode = 8)
                : m_maxDepth(maxDepth)
                , m_maxEntriesPerNode(maxEntriesPerNode)
            {
                m_root = std::make_unique<Node>();
                m_root->bounds = worldBounds;
            }

            // Einmal pro statischem Objekt beim Aufbau aufrufen.
            void Insert(ObjectPtrT obj, const AABB& bounds)
            {
                InsertRecursive(m_root.get(), Entry{ obj, bounds }, 0);
            }

            void Clear()
            {
                AABB bounds = m_root->bounds;
                m_root = std::make_unique<Node>();
                m_root->bounds = bounds;
            }

            // ------------------------------------------------------------
            // TraverseFrontToBack: besucht alle (nicht gecullten) Objekte
            // in ungefaehrer Nah-Fern-Reihenfolge relativ zu cameraPos.
            //
            // frustumTest(bounds)  -> true, wenn die Box (teilweise oder
            //                         ganz) im Frustum liegt/liegen KOENNTE.
            //                         false -> kompletter Teilbaum wird
            //                         uebersprungen.
            // occludedTest(bounds) -> true, wenn die Box bewiesen verdeckt
            //                         ist (z.B. via MOCTestAABBoxVisibility
            //                         negiert). true -> Teilbaum wird
            //                         uebersprungen.
            // visit(object, bounds)-> wird fuer jedes sichtbare Objekt
            //                         aufgerufen, in Nah-Fern-Reihenfolge.
            //
            // WICHTIG: frustumTest/occludedTest werden auch auf
            // Zwischenknoten (nicht nur Blaettern) aufgerufen - das ist
            // der eigentliche Vorteil gegenueber einer flachen Liste.
            // ------------------------------------------------------------
            template<class FrustumTestFn, class OccludedTestFn, class VisitFn>
            void TraverseFrontToBack(
                const Vec3& cameraPos,
                FrustumTestFn frustumTest,
                OccludedTestFn occludedTest,
                VisitFn visit) const
            {
                TraverseNode(m_root.get(), cameraPos, frustumTest, occludedTest, visit);
            }

            // Debug/Statistik
            size_t CountNodes() const
            {
                return CountNodesRecursive(m_root.get());
            }

        private:
            struct Node
            {
                AABB bounds;
                // Objekte, die entweder (a) in diesem Blatt liegen, oder
                // (b) auf Zwischenknoten-Ebene "haengen geblieben" sind,
                // weil sie mehrere Kind-Octanten ueberlappen (siehe
                // GetFittingChild).
                std::vector<Entry> entries;
                std::array<std::unique_ptr<Node>, 8> children;
                bool isLeaf = true;
            };

            void InsertRecursive(Node* node, Entry entry, int depth)
            {
                if (node->isLeaf)
                {
                    node->entries.push_back(std::move(entry));
                    if (node->entries.size() > m_maxEntriesPerNode &&
                        depth < m_maxDepth)
                    {
                        Subdivide(node, depth);
                    }
                    return;
                }
                int childIndex = GetFittingChild(node, entry.bounds);
                if (childIndex >= 0)
                {
                    InsertRecursive(node->children[childIndex].get(), std::move(entry), depth + 1);
                }
                else
                {
                    // Objekt ueberlappt mehrere Kind-Octanten - bleibt auf
                    // dieser Ebene liegen, statt es kuenstlich in einen
                    // Octanten zu zwingen (klassisches "loose octree"-
                    // Verhalten in vereinfachter Form).
                    node->entries.push_back(std::move(entry));
                }
            }

            void Subdivide(Node* node, int depth)
            {
                node->isLeaf = false;
                const Vec3 center = node->bounds.Center();
                const Vec3 mn = node->bounds.Min();
                const Vec3 mx = node->bounds.Max();

                for (int i = 0; i < 8; ++i)
                {
                    Vec3 childMin(
                        (i & 1) ? center.x() : mn.x(),
                        (i & 2) ? center.y() : mn.y(),
                        (i & 4) ? center.z() : mn.z());
                    Vec3 childMax(
                        (i & 1) ? mx.x() : center.x(),
                        (i & 2) ? mx.y() : center.y(),
                        (i & 4) ? mx.z() : center.z());
                    node->children[i] = std::make_unique<Node>();
                    node->children[i]->bounds = AABB(childMin, childMax);
                }

                // Vorhandene Eintraege neu verteilen - jeder landet entweder
                // in genau einem Kind oder bleibt (bei Ueberlappung) hier.
                std::vector<Entry> remaining;
                remaining.reserve(node->entries.size());
                for (auto& e : node->entries)
                {
                    int childIndex = GetFittingChild(node, e.bounds);
                    if (childIndex >= 0)
                        InsertRecursive(node->children[childIndex].get(), std::move(e), depth + 1);
                    else
                        remaining.push_back(std::move(e));
                }
                node->entries = std::move(remaining);
            }

            // Liefert den Index (0..7) des Kind-Octanten, in den 'bounds'
            // KOMPLETT hineinpasst, oder -1, wenn die Box mehrere Octanten
            // ueberlappt (dann bleibt sie auf der aktuellen Ebene liegen).
            int GetFittingChild(Node* node, const AABB& bounds) const
            {
                const Vec3 center = node->bounds.Center();
                int index = 0;
                if (bounds.Min().x() >= center.x()) index |= 1;
                else if (bounds.Max().x() > center.x()) return -1;
                if (bounds.Min().y() >= center.y()) index |= 2;
                else if (bounds.Max().y() > center.y()) return -1;
                if (bounds.Min().z() >= center.z()) index |= 4;
                else if (bounds.Max().z() > center.z()) return -1;
                return index;
            }

            template<class FrustumTestFn, class OccludedTestFn, class VisitFn>
            void TraverseNode(
                Node* node,
                const Vec3& cameraPos,
                FrustumTestFn& frustumTest,
                OccludedTestFn& occludedTest,
                VisitFn& visit) const
            {
                if (!node) return;
                if (!frustumTest(node->bounds)) return;
                if (occludedTest(node->bounds)) return;

                // Auf dieser Ebene "haengen gebliebene" Eintraege (Blatt-
                // Inhalt oder ueberlappende Objekte) selbst nach Distanz
                // sortieren, damit auch innerhalb eines Knotens front-to-
                // back gerendert wird.
                if (!node->entries.empty())
                {
                    thread_local std::vector<size_t> order;
                    order.resize(node->entries.size());
                    for (size_t i = 0; i < order.size(); ++i) order[i] = i;

                    std::sort(order.begin(), order.end(),
                        [&](size_t a, size_t b)
                        {
                            Vec3 da = node->entries[a].bounds.Center() - cameraPos;
                            Vec3 db = node->entries[b].bounds.Center() - cameraPos;
                            return (da ^ da) < (db ^ db);
                        });

                    for (size_t idx : order)
                    {
                        const Entry& e = node->entries[idx];
                        if (!frustumTest(e.bounds)) continue;
                        if (occludedTest(e.bounds)) continue;
                        visit(e.object, e.bounds);
                    }
                }

                if (node->isLeaf) return;

                // Kind-Octanten nach Distanz zur Kamera sortieren - liefert
                // die front-to-back Reihenfolge fuer den MOC-Test/Write,
                // ohne dass die Blaetter selbst explizit einsortiert werden
                // muessen.
                std::array<int, 8> childOrder = { 0,1,2,3,4,5,6,7 };
                std::sort(childOrder.begin(), childOrder.end(),
                    [&](int a, int b)
                    {
                        if (!node->children[a]) return false;
                        if (!node->children[b]) return true;
                        Vec3 da = node->children[a]->bounds.Center() - cameraPos;
                        Vec3 db = node->children[b]->bounds.Center() - cameraPos;
                        return (da ^ da) < (db ^ db);
                    });

                for (int ci : childOrder)
                {
                    if (node->children[ci])
                        TraverseNode(node->children[ci].get(), cameraPos, frustumTest, occludedTest, visit);
                }
            }

            size_t CountNodesRecursive(const Node* node) const
            {
                if (!node) return 0;
                size_t count = 1;
                if (!node->isLeaf)
                    for (const auto& c : node->children)
                        count += CountNodesRecursive(c.get());
                return count;
            }

            std::unique_ptr<Node> m_root;
            int m_maxDepth;
            size_t m_maxEntriesPerNode;
        };
    } // namespace spatial
} // namespace ng#pragma once
