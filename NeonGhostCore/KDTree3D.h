#pragma once
#include <vector>
#include <algorithm>
#include <cmath>
#include "Object3D.h"

namespace ng {
	namespace renderer {
		namespace spatial {

			template<typename T>
			class KDTree3D
			{
			public:
				using ObjectPtr = ng::renderer::objects::Object3D<T>*;

				// Achtung: 'objects' wird intern per nth_element umsortiert
				// (Pointer-Reihenfolge ändert sich, Pointer selbst bleiben gültig).
				void Build(std::vector<ObjectPtr>& objects)
				{
					m_nodes.clear();
					m_nodes.reserve(objects.size());
					m_root = BuildRecursive(objects.begin(), objects.end(), 0);
				}

				template<typename Callback>
				void ForEachPotentialPair(Callback&& cb) const
				{
					for (auto& node : m_nodes)
					{
						T r = node.obj->Sphere().Radius() * T(2);
						std::vector<ObjectPtr> nearby;
						RangeQuery(m_root, node.obj->Position(), r, nearby);

						for (auto* other : nearby)
							if (other != node.obj && other > node.obj)
								cb(node.obj, other);
					}
				}

			private:
				struct Node { ObjectPtr obj; int left = -1, right = -1; int axis; };
				std::vector<Node> m_nodes;
				int m_root = -1;

				using Iter = typename std::vector<ObjectPtr>::iterator;

				int BuildRecursive(Iter begin, Iter end, int depth)
				{
					if (begin == end) return -1;
					int axis = depth % 3; // x, y, z rotierend

					Iter mid = begin + (end - begin) / 2;
					std::nth_element(begin, mid, end, [axis](ObjectPtr a, ObjectPtr b)
						{
							switch (axis)
							{
							case 0:  return a->Position().x() < b->Position().x();
							case 1:  return a->Position().y() < b->Position().y();
							default: return a->Position().z() < b->Position().z();
							}
						});

					int idx = (int)m_nodes.size();
					m_nodes.push_back({ *mid, -1, -1, axis });
					m_nodes[idx].left = BuildRecursive(begin, mid, depth + 1);
					m_nodes[idx].right = BuildRecursive(mid + 1, end, depth + 1);
					return idx;
				}

				void RangeQuery(int nodeIdx, const typename ng::renderer::objects::Object3D<T>::Vec3& p, T r,
					std::vector<ObjectPtr>& results) const
				{
					if (nodeIdx < 0) return;
					const Node& n = m_nodes[nodeIdx];
					auto op = n.obj->Position();

					T dx = op.x() - p.x(), dy = op.y() - p.y(), dz = op.z() - p.z();
					if (dx * dx + dy * dy + dz * dz <= r * r)
						results.push_back(n.obj);

					T diff = (n.axis == 0) ? (p.x() - op.x())
						: (n.axis == 1) ? (p.y() - op.y())
						: (p.z() - op.z());

					int nearSide = diff < 0 ? n.left : n.right;
					int farSide = diff < 0 ? n.right : n.left;

					RangeQuery(nearSide, p, r, results);
					if (std::abs(diff) <= r)
						RangeQuery(farSide, p, r, results);
				}
			};

		}
	}
}