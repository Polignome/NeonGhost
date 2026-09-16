#pragma once
#include "Global.h"
#include "Mesh.h"
#include <string>
#include <vector>
#include <cstdint>
#include "ResourceManager.h"
#include "BspBuilder.h"

// Vorwaertsdeklarationen fuer tinyxml2 (nur Zeiger im Interface,
// tinyxml2.h muss hier nicht included werden).
namespace tinyxml2 { class XMLElement; }

namespace ng {
    namespace renderer {
        namespace gfx {
            // ============================================================
            // _WaveformObject
            //
            // Reine Geometrie-Resource - haelt die aus einer .obj-Datei
            // geparsten Rohdaten (Positionen/Normalen/UVs/Indices/Sub-
            // Mesh-Bereiche pro Material). KEINE tinyobjloader-
            // Abhaengigkeit in diesem Header - die steckt komplett in
            // der .cpp. Immer in double gespeichert, ImportInto<T>()
            // castet passend nach float/double beim Uebertragen in
            // Mesh<T>.
            // ============================================================
            class _WaveformObject
            {
            public:
                struct SubMeshRange
                {
                    uint32_t indexOffset = 0;
                    uint32_t indexCount = 0;
                    int materialId = -1;          // Index in tinyobj::materials, -1 = kein Material
                    std::string materialName;      // nur informativ/Debug
                };

                _WaveformObject();
                _WaveformObject(const std::string& filename);
                _WaveformObject(size_t size, void* buffer);
                _WaveformObject(const std::vector<uint8_t>& buffer);

                bool Load(const std::string& filename);
                bool Load(size_t size, void* buffer);
                bool Load(const std::vector<uint8_t>& buffer);

                size_t VertexCount() const { return m_positions.size() / 3; }
                size_t IndexCount() const { return m_indices.size(); }
                bool HasNormals() const { return m_hasNormals; }
                bool HasUVs() const { return m_hasUVs; }
                const std::vector<SubMeshRange>& SubMeshes() const { return m_subMeshes; }

                // ------------------------------------------------------
                // Import in eine bestehende Mesh<T>-Instanz. Templated
                // (float/double), deshalb komplett inline hier im Header -
                // arbeitet ausschliesslich auf den bereits geparsten,
                // simplen double-Arrays, keine tinyobjloader-Abhaengigkeit
                // an dieser Stelle noetig.
                //
                // computeNormalsIfMissing: falls die .obj keine Normalen
                // enthielt, automatisch ComputeSmoothNormals() aufrufen.
                // ------------------------------------------------------
                template<class T>
                void ImportInto(ng::renderer::objects::Mesh<T>& mesh, bool computeNormalsIfMissing = true) const
                {
                    mesh.Clear();
                    mesh.ReserveVertices(VertexCount());
                    mesh.ReserveIndices(IndexCount());

                    for (size_t i = 0; i < VertexCount(); ++i)
                    {
                        T x = static_cast<T>(m_positions[i * 3 + 0]);
                        T y = static_cast<T>(m_positions[i * 3 + 1]);
                        T z = static_cast<T>(m_positions[i * 3 + 2]);
                        uint32_t idx = mesh.AddVertex(x, y, z);

                        if (m_hasNormals)
                        {
                            ng::math::Matrix<3, 1, T> n(
                                static_cast<T>(m_normals[i * 3 + 0]),
                                static_cast<T>(m_normals[i * 3 + 1]),
                                static_cast<T>(m_normals[i * 3 + 2]));
                            mesh.GetVertex(idx).normal() = n;
                        }
                        if (m_hasUVs)
                        {
                            mesh.SetUV(idx,
                                static_cast<T>(m_uvs[i * 2 + 0]),
                                static_cast<T>(m_uvs[i * 2 + 1]));
                        }
                    }

                    for (size_t i = 0; i + 2 < m_indices.size(); i += 3)
                                                mesh.AddTriangle(m_indices[i + 0], m_indices[i + 1], m_indices[i + 2]);
                    



                    for (const auto& sm : m_subMeshes)
                        mesh.AddSubMesh(sm.indexOffset, sm.indexCount, sm.materialId);

                    if (!m_hasNormals && computeNormalsIfMissing)
                        mesh.ComputeSmoothNormals();
                }


                template<class T>
                void ImportInto(std::vector < ng::bsp::BSPPolygon<T>>& polys) const {
                    
                    std::vector<ng::geometry::Vertex<T>> vertex_list;
                    for (size_t i = 0; i < VertexCount(); ++i)
                    {
                        ng::geometry::Vertex<T> vertex;
                        
                        vertex.world().x() = static_cast<T>(m_positions[i * 3 + 0]);
                        vertex.world().y() = static_cast<T>(m_positions[i * 3 + 1]);
                        vertex.world().z() = static_cast<T>(m_positions[i * 3 + 2]);
                        size_t idx = vertex_list.size();
                        vertex_list.push_back(vertex);

                        if (m_hasNormals)
                        {
                            ng::math::Matrix<3, 1, T> n(
                                static_cast<T>(m_normals[i * 3 + 0]),
                                static_cast<T>(m_normals[i * 3 + 1]),
                                static_cast<T>(m_normals[i * 3 + 2]));
                                
                            
                            vertex_list[idx].normal() = n;
                            if (!m_hasColor) vertex_list[idx].color() = math::Matrix<4, 1, T>::NormalToColor(n);

                        }
                        if (m_hasUVs)
                        {
                            vertex_list[idx].texture().x() = m_uvs[i * 2 + 0];
                            vertex_list[idx].texture().y() = m_uvs[i * 2 + 1];
                        }
                    }

                    for (size_t i = 0; i + 2 < m_indices.size(); i += 3) {
                        
                        ng::bsp::BSPPolygon<T> poly;
                        
                        poly = ng::bsp::BSPPolygon<T>::FromTriangle(vertex_list[m_indices[i + 0]], vertex_list[m_indices[i + 1]], vertex_list[m_indices[i + 2]]);
                        
                        polys.push_back(poly);
                    }
                }

            protected:
            private:
                void Reset();

                std::vector<double> m_positions; // xyz je (Face-)Vertex
                std::vector<double> m_normals;   // xyz je Vertex (nur wenn m_hasNormals)
                std::vector<double> m_uvs;       // uv je Vertex (nur wenn m_hasUVs)
                std::vector<uint32_t> m_indices; // bereits trianguliert
                std::vector<SubMeshRange> m_subMeshes;
                bool m_hasNormals = false;
                bool m_hasUVs = false;
                bool m_hasColor = false;         // noch nicht implementert macht vertexnormal zur color
            };

            typedef ng::resource::Resource<_WaveformObject> WaveformObj;
        }
    }

    namespace resource {
        class WaveformObjResourceManager : public TemplateResourceManager<renderer::gfx::_WaveformObject>
        {
        public:
            WaveformObjResourceManager();
            ~WaveformObjResourceManager();

            static uint64_t LoadObjFromFile(std::string resname, std::string filename, bool keep_source = false, unsigned long attrib = DELETE_WHEN_UNUSED);
            static uint64_t LoadObjFromMemory(std::string resname, unsigned char* buffer, unsigned long buffer_size, bool keep_source = false, unsigned long attrib = DELETE_WHEN_UNUSED);
        protected:
            static uint64_t Add(std::string resname, renderer::gfx::_WaveformObject* ptr, unsigned long attrib = DELETE_WHEN_UNUSED);
            virtual ng::renderer::gfx::_WaveformObject* CreateDummyResourceEvent();
            virtual unsigned long ParseXMLElement(tinyxml2::XMLElement* element);
        };
    }
}
