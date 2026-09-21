#pragma once
#include "Global.h"
#include "Matrix.h"
#include "RayPlaneLine.h"
#include "Brush.h"
#include "Clipping.h"
#include <memory>

namespace ng {
	namespace map {

		template <class T >
		class BrushPoly;

		namespace csg {
			template <class T>
			class TreeNode
			{
			public:


				TreeNode() :m_front(nullptr), m_back(nullptr), m_leave_type(EDG_NO_LEAV)
				{
				}

				~TreeNode()
				{
					m_front = nullptr;
					m_back = nullptr;
				}

				enum {
					EDG_NONE_SOLID_SPACE_LEAVE = -1,
					EDG_NO_LEAV = 0,
					EDG_SOLID_SPACE_LEAVE = 1
				};

				int m_leave_type;
				math::RayPlaneLine<3, T> m_plane;
				std::shared_ptr<TreeNode<T>> m_front;
				std::shared_ptr<TreeNode<T>> m_back;





				inline int GetDeepestNode(int i = 1)
				{
					int o = i;
					if (m_front == nullptr && m_back == nullptr) return o;
					if (m_front != nullptr) o = max(o, m_front->GetDeepestNode(i + 1));
					if (m_back != nullptr) o = max(o, m_back->GetDeepestNode(i + 1));

					return o;
				}

				inline void  BuildUnion(std::vector<math::RayPlaneLine<3, T>> ray_list)
				{
					assert(ray_list.size());
					m_plane = ray_list[ray_list.size() - 1];

					m_front = std::shared_ptr<TreeNode<T>>(new TreeNode);
					m_front->m_leave_type = EDG_SOLID_SPACE_LEAVE;

					m_back = std::shared_ptr<TreeNode<T>>(new TreeNode);

					if (ray_list.size() == 1)
					{
						m_back->m_leave_type = EDG_NONE_SOLID_SPACE_LEAVE;

						return;
					}
					ray_list.pop_back();
					m_back->BuildUnion(ray_list);
				}

				inline std::vector<ng::map::BrushPoly<T> > ClipUnion(std::vector<ng::map::BrushPoly<T>> pl, bool strong)
				{
					std::vector<ng::map::BrushPoly<T>> fl;
					std::vector<ng::map::BrushPoly<T>> bl;
					std::vector<ng::map::BrushPoly<T>> outl;


					if (m_leave_type == EDG_NONE_SOLID_SPACE_LEAVE) return fl;
					if (m_leave_type == EDG_SOLID_SPACE_LEAVE) return pl;


					for (unsigned int i = 0; i < pl.size(); i++)
					{
						ng::map::BrushPoly<T> p = pl[i];
						int result = p.classify(m_plane);

						if (result == math::RayPlaneLine<3, T>::FRONT) fl.push_back(p);
						if (result == math::RayPlaneLine<3, T>::BACK) bl.push_back(p);
						if (result == math::RayPlaneLine<3, T>::SPANNING)
						{
							ng::map::BrushPoly<T> f, b;
							ng::geometry::SplitByPlane(p.verts(), m_plane, f.verts(), b.verts());

							f.aggregate() = p.aggregate();
							b.aggregate() = p.aggregate();

							f.textureIndex() = p.textureIndex();
							b.textureIndex() = p.textureIndex();

							f.plane() = p.plane();
							b.plane() = p.plane();
							
							if (f.verts().size() > 2 && f.area() > math::EPSILON<T>) fl.push_back(f);
							if (b.verts().size() > 2 && b.area() > math::EPSILON<T>) bl.push_back(b);
						}
						if (result == math::RayPlaneLine<3, T>::PLANAR)
						{
							if (((m_plane.normal() ^ p.plane().normal()) >= (T)0))
							{

								if (strong == true)
								{

									fl.push_back(p);
								}
								else
								{

									bl.push_back(p);
								}
							}
							else  bl.push_back(p);

						}
					}

					if (fl.size() > 0) outl = m_front->ClipUnion(fl, true); // TRUE!!!
					if (bl.size() > 0)
					{

						bl = m_back->ClipUnion(bl, strong);
						for (unsigned int i = 0; i < bl.size(); i++) outl.push_back(bl[i]);
					}

					return outl;
				}

			};



			template <class T>
			class Tree
			{
			public:

				Tree() :m_root(nullptr), m_mode(0)
				{
				}

				~Tree()
				{
					m_root = nullptr;
				}

				inline void BuildUnion(std::vector<math::RayPlaneLine<3, T>> ray_list)
				{
					m_root = std::shared_ptr<TreeNode<T>>(new TreeNode<T>);
					m_root->BuildUnion(ray_list);
				}

				inline std::vector < ng::map::BrushPoly<T>> ClipUnion(std::vector < ng::map::BrushPoly<T>> vl, bool strong) {
					if (m_root == nullptr) return vl;
					std::vector < ng::map::BrushPoly<T>> l = m_root->ClipUnion(vl, strong);

					return l;
				}



				inline void reset()
				{
					m_root = nullptr;
				}

				inline int GetDeepestNodeCount()
				{
					if (m_root == nullptr) return 0;
					return m_root->GetDeepestNode(1);
				}

			protected:
			private:
				std::shared_ptr<TreeNode<T>> m_root;


				int m_mode;

			};
		}
	}
}