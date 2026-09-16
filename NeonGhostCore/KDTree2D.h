#pragma once
#include "Global.h"
#include "Object2d.h"

namespace ng {
	namespace renderer {
		namespace spatial {
			template<typename T>
			class KDTree2D
			{
			public:
				using ObjectPtr = ng::renderer::objects::Object2D<T>*;

				void Build(std::vector<ObjectPtr> objects)
				{
					m_nodes.clear();
					m_nodes.reserve(objects.size());
					m_root = BuildRecursive(objects.begin(), objects.end(), 0);
				}

				// Ruft cb(a, b) für jedes Paar auf, dessen Bounding-Circles sich potenziell überschneiden.
				// Kein "harter" exakter Test hier - das macht der Aufrufer (schneller Broad-Phase-Filter).
				template<typename Callback>
				void ForEachPotentialPair(Callback&& cb) const
				{
					for (auto& node : m_nodes)
					{
						T r = node.obj->Circle().Radius() * T(2); // eigener Radius + Toleranz für Partner
						std::vector<ObjectPtr> nearby;
						RangeQuery(m_root, node.obj->Position(), r, nearby);

						for (auto* other : nearby)
							if (other != node.obj && other > node.obj) // jedes Paar nur einmal
								cb(node.obj, other);
					}
				}
				struct Node { ObjectPtr obj; int left = -1, right = -1; int axis; };
				
				const std::vector<Node>& GetNodes() const
				{
					return m_nodes;
				}
				
				
				int Root() const
				{
					return m_root;
				}
			private:
				
				std::vector<Node> m_nodes;
				int m_root = -1;

				using Iter = std::vector<ObjectPtr>::iterator;

				int BuildRecursive(Iter begin, Iter end, int depth)
				{
					if (begin == end) return -1;
					int axis = depth % 2;
					Iter mid = begin + (end - begin) / 2;

					std::nth_element(begin, mid, end, [axis](ObjectPtr a, ObjectPtr b)
						{
							return axis == 0 ? a->Position().x() < b->Position().x()
								: a->Position().y() < b->Position().y();
						});

					int idx = (int)m_nodes.size();
					m_nodes.push_back({ *mid, -1, -1, axis });
					m_nodes[idx].left = BuildRecursive(begin, mid, depth + 1);
					m_nodes[idx].right = BuildRecursive(mid + 1, end, depth + 1);
					return idx;
				}

				void RangeQuery(int nodeIdx, const typename ng::renderer::objects::Object2D<T>::Vec2& p, T r,
					std::vector<ObjectPtr>& results) const
				{
					if (nodeIdx < 0) return;
					const Node& n = m_nodes[nodeIdx];
					auto op = n.obj->Position();

					T dx = op.x() - p.x(), dy = op.y() - p.y();
					if (dx * dx + dy * dy <= r * r)
						results.push_back(n.obj);

					T diff = (n.axis == 0) ? (p.x() - op.x()) : (p.y() - op.y());
					int nearSide = diff < 0 ? n.left : n.right;
					int farSide = diff < 0 ? n.right : n.left;

					RangeQuery(nearSide, p, r, results);
					if (std::abs(diff) <= r)
						RangeQuery(farSide, p, r, results); // nur absteigen, wenn Splitebene in Reichweite liegt
				}
			};

		}

	}
}


