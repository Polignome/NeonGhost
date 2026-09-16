#pragma once
#include "Global.h"
#include "Matrix.h"
#include "RayPlaneLine.h"
#include "Vertex.h"


namespace ng {
	namespace geometry {
        template<unsigned int N, class T>
        class Primitive
        {
        public:

            using VertexType = Vertex<T>;
            using VectorType = math::Matrix<3, 1, T>;
            using PlaneType = math::RayPlaneLine<N, T>;

        public:

            Primitive() = default;


            explicit Primitive(const std::vector<math::Matrix<3,1,T>>& verts)
                
            {
                for (auto v : verts) m_vertices.push_back(VertexType(v));
                UpdatePlane();
            }


            explicit Primitive(const std::vector<VertexType>& vertices)
                : m_vertices(vertices)
            {
                UpdatePlane();
            }

            


            // ------------------------------------------------------------
            // Vertices
            // ------------------------------------------------------------

            unsigned int VertexCount() const
            {
                return static_cast<unsigned int>(m_vertices.size());
            }

            const std::vector<VertexType>& Vertices() const
            {
                return m_vertices;
            }

            std::vector<VertexType>& Vertices()
            {
                return m_vertices;
            }

            const VertexType& operator[](size_t i) const
            {
                return m_vertices[i];
            }

            VertexType& operator[](size_t i)
            {
                return m_vertices[i];
            }


            void AddVertex(const VertexType& vertex)
            {
                m_vertices.push_back(vertex);
            }

            void Clear()
            {
                m_vertices.clear();
            }


            // ------------------------------------------------------------
            // Plane
            // ------------------------------------------------------------

            const PlaneType& Plane() const
            {
                return m_plane;
            }

            PlaneType& Plane()
            {
                return m_plane;
            }

            void UpdatePlane(bool counterClockwise = true)
            {
                if (m_vertices.size() < 3)
                    return;

                m_plane.origin() = m_vertices[0].world();

                VectorType e0 =
                    m_vertices[1].world() -
                    m_vertices[0].world();

                VectorType e1 =
                    m_vertices[2].world() -
                    m_vertices[1].world();

                m_plane.vector() = e1 % e0;

                if (!counterClockwise)
                    m_plane.vector() = -m_plane.vector();
            }


            // ------------------------------------------------------------
            // Basic geometry
            // ------------------------------------------------------------

            VectorType Center() const
            {
                VectorType center(0, 0, 0);

                if (m_vertices.empty())
                    return center;

                for (const auto& v : m_vertices)
                    center += v.world();

                return center * ((T)1 / (T)m_vertices.size());
            }


            T Area() const
            {
                if (m_vertices.size() < 3)
                    return (T)0;

                T area = 0;

                const VectorType& p0 =
                    m_vertices[0].world();

                for (size_t i = 1; i + 1 < m_vertices.size(); ++i)
                {
                    VectorType e0 =
                        m_vertices[i].world() - p0;

                    VectorType e1 =
                        m_vertices[i + 1].world() - p0;

                    area += (e0 % e1).length() * (T)0.5;
                }

                return area;
            }


            bool IsValid() const
            {
                return m_vertices.size() >= 3;
            }


            // ------------------------------------------------------------
            // Orientation
            // ------------------------------------------------------------

            void Reverse()
            {
                std::reverse(
                    m_vertices.begin(),
                    m_vertices.end()
                );

                UpdatePlane();
            }


            // ------------------------------------------------------------
            // Operators
            // ------------------------------------------------------------

            Primitive& operator+=(const VertexType& vertex)
            {
                AddVertex(vertex);
                return *this;
            }

        private:

            std::vector<VertexType> m_vertices;
            PlaneType m_plane;
        };

}}