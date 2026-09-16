#pragma once
#include "Global.h"
#include "KDTree2D.h"
#include "PixelBuffer.h"
#include "Canvas2D.h"
namespace ng {
	namespace renderer {
		namespace gfx2d {



            template<typename T>
			void DrawTreeNode(const ng::renderer::spatial::KDTree2D<T>& tree, int nodeIndex, ng::renderer::gfx2d::Canvas2D& canvas)
			{
				if (nodeIndex < 0)
					return;

				const auto& nodes = tree.GetNodes();
				const auto& node = nodes[nodeIndex];

				auto p = node.obj->Position();

				if (node.left >= 0)
				{
					auto p2 =
						nodes[node.left].obj->Position();

					canvas.DrawLine((int)p.x(), (int)p.y(), (int)p2.x(), (int)p2.y(), 0xffffffff);

					DrawTreeNode(tree, node.left, canvas);
				}

				if (node.right >= 0)
				{
					auto p2 =
						nodes[node.right].obj->Position();

					canvas.DrawLine((int)p.x(), (int)p.y(), (int)p2.x(), (int)p2.y(), 0xffffffff);

					DrawTreeNode(tree, node.right, canvas);
				}
			}

			template<typename T>
			void DrawTree(const ng::renderer::spatial::KDTree2D<T>& tree, ng::renderer::gfx2d::Canvas2D& canvas)
			{
				const auto& nodes = tree.GetNodes();

				if (tree.Root() < 0)
					return;

				DrawTreeNode(
					tree,
					tree.Root(),
					canvas);
			}


            template<typename T>
            void DrawKDNode(
                const ng::renderer::spatial::KDTree2D<T>& tree,
                int nodeIndex,
                ng::renderer::gfx2d::Canvas2D& canvas,
                T minX,
                T minY,
                T maxX,
                T maxY)
            {
                if (nodeIndex < 0)
                    return;

                const auto& nodes = tree.GetNodes();
                const auto& node = nodes[nodeIndex];

                auto p = node.obj->Position();

                if (node.axis == 0)
                {
                    // --------------------------------------------------------
                    // X-Split
                    // --------------------------------------------------------

                    canvas.DrawLine(
                        (int)p.x(),
                        (int)minY,
                        (int)p.x(),
                        (int)maxY,
                        0xffff0000);


                    // LEFT
                    DrawKDNode(
                        tree,
                        node.left,
                        canvas,
                        minX,
                        minY,
                        p.x(),
                        maxY);


                    // RIGHT
                    DrawKDNode(
                        tree,
                        node.right,
                        canvas,
                        p.x(),
                        minY,
                        maxX,
                        maxY);
                }
                else
                {
                    // --------------------------------------------------------
                    // Y-Split
                    // --------------------------------------------------------

                    canvas.DrawLine(
                        
                        (int)minX,
                        (int)p.y(),
                        (int)maxX,
                        (int)p.y(),
                        0xff00ff00);


                    // LEFT / BELOW
                    DrawKDNode(
                        tree,
                        node.left,
                        canvas,
                        minX,
                        minY,
                        maxX,
                        p.y());


                    // RIGHT / ABOVE
                    DrawKDNode(
                        tree,
                        node.right,
                        canvas,
                        minX,
                        p.y(),
                        maxX,
                        maxY);
                }
            }


			template<typename T>
			void DrawKDTree(const ng::renderer::spatial::KDTree2D<T>& tree, ng::renderer::gfx2d::Canvas2D& canvas,T minX,T minY,T maxX,T maxY)
			{
				if (tree.Root() < 0)
					return;

				DrawKDNode(
					tree,
					tree.Root(),
					canvas,
					minX,
					minY,
					maxX,
					maxY);
			}

            enum {
                VISUALIZE_KDTREE2D_CONECTIONS = 1,
                VISUALIZE_KDTREE2D_SPLITS = 2
            };

			template <class T>
			void Visualize(ng::renderer::gfx2d::Canvas2D& canvas, ng::renderer::spatial::KDTree2D<T> tree, uint32_t mode = VISUALIZE_KDTREE2D_SPLITS) {
                if (mode & VISUALIZE_KDTREE2D_CONECTIONS) DrawTree(tree, canvas);
                if (mode & VISUALIZE_KDTREE2D_SPLITS) DrawKDTree(tree, canvas, (T)0, (T)0, (T)canvas.Width(), (T)canvas.Height());
            }



			template <class T>
			void Visualize(ng::renderer::buffers::PixelBufferRGBA& buffer, ng::renderer::spatial::KDTree2D<T> tree, uint32_t mode = VISUALIZE_KDTREE2D_SPLITS) {
				ng::renderer::gfx2d::Canvas2D canvas(buffer);
				Visualize(canvas, tree,mode);
			}
		}
	}
}
