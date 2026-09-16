#include "pch.h"
#include "WaveformObj.h"
//#define TINYOBJLOADER_IMPLEMENTATION
#define TINYOBJLOADER_DISABLE_FAST_FLOAT
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

using namespace ng;
using namespace renderer;
using namespace gfx;

// -----------------------------------------------------------------------
// Interne Hilfsfunktion: wertet einen bereits geparsten tinyobj::ObjReader
// aus und fuellt die flachen Ausgabe-Arrays. Fuer Datei- und Speicher-Pfad
// identisch, deshalb hier zentral einmal implementiert.
//
// WICHTIG (Design-Entscheidung): OBJ erlaubt pro Face-Ecke unterschiedliche
// Indizes fuer Position/Normale/UV. Wir "flachen" das ab - jede Face-Ecke
// wird zu einem eigenen Ausgabe-Vertex (kein Wiederverwenden ueber
// unterschiedliche Normal-/UV-Kombinationen hinweg). Das ist der uebliche
// Ansatz beim OBJ-Import (siehe z.B. Assimp), da OBJs "geteilte" Vertices
// im Sinne eines Render-Vertexbuffers ohnehin selten sauber abbilden.
// -----------------------------------------------------------------------
static bool ImportFromReader(
    const tinyobj::ObjReader& reader,
    std::vector<double>& outPos,
    std::vector<double>& outNorm,
    std::vector<double>& outUv,
    std::vector<uint32_t>& outIdx,
    std::vector<_WaveformObject::SubMeshRange>& outSubMeshes,
    bool& hasNormals,
    bool& hasUVs)
{
    const auto& attrib = reader.GetAttrib();
    const auto& shapes = reader.GetShapes();
    const auto& materials = reader.GetMaterials();

    hasNormals = !attrib.normals.empty();
    hasUVs = !attrib.texcoords.empty();

    for (const auto& shape : shapes)
    {
        size_t indexOffsetInFace = 0;
        int currentMaterial = shape.mesh.material_ids.empty() ? -1 : shape.mesh.material_ids[0];
        uint32_t rangeStart = static_cast<uint32_t>(outIdx.size());

        auto flushRange = [&](int matId)
            {
                if (outIdx.size() > rangeStart)
                {
                    std::string matName = (matId >= 0 && matId < (int)materials.size())
                        ? materials[matId].name : std::string();
                    _WaveformObject::SubMeshRange r;
                    r.indexOffset = rangeStart;
                    r.indexCount = static_cast<uint32_t>(outIdx.size() - rangeStart);
                    r.materialId = matId;
                    r.materialName = matName;
                    outSubMeshes.push_back(r);
                }
            };

        for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); ++f)
        {
            unsigned char fv = shape.mesh.num_face_vertices[f];
            int mat = shape.mesh.material_ids.empty() ? -1 : shape.mesh.material_ids[f];

            // Materialwechsel innerhalb desselben Shapes -> aktuellen
            // Indexbereich abschliessen und neu beginnen.
            if (mat != currentMaterial)
            {
                flushRange(currentMaterial);
                rangeStart = static_cast<uint32_t>(outIdx.size());
                currentMaterial = mat;
            }

            // Fan-Triangulierung fuer n-Ecke (n>3). Bei bereits
            // dreieckigen Faces (fv==3) passiert hier nur eine einzige
            // Iteration mit v==2 -> genau ein Dreieck, keine Aenderung
            // am Verhalten.
            uint32_t firstOutIdx = 0;
            for (unsigned char v = 0; v < fv; ++v)
            {
                const auto& idx = shape.mesh.indices[indexOffsetInFace + v];
                uint32_t outVertex = static_cast<uint32_t>(outPos.size() / 3);

                outPos.push_back(attrib.vertices[3 * idx.vertex_index + 0]);
                outPos.push_back(attrib.vertices[3 * idx.vertex_index + 1]);
                outPos.push_back(attrib.vertices[3 * idx.vertex_index + 2]);

                if (hasNormals)
                {
                    if (idx.normal_index >= 0)
                    {
                        outNorm.push_back(attrib.normals[3 * idx.normal_index + 0]);
                        outNorm.push_back(attrib.normals[3 * idx.normal_index + 1]);
                        outNorm.push_back(attrib.normals[3 * idx.normal_index + 2]);
                    }
                    else
                    {
                        // Manche Faces koennen fehlende Normal-Indizes
                        // haben, auch wenn die .obj GENERELL Normalen
                        // enthaelt - Platzhalter, wird spaeter ueber
                        // ComputeSmoothNormals() sowieso nicht mehr
                        // gebraucht, falls hasNormals insgesamt false waere;
                        // hier bleibt es als (0,0,0) stehen.
                        outNorm.push_back(0.0);
                        outNorm.push_back(0.0);
                        outNorm.push_back(0.0);
                    }
                }

                if (hasUVs)
                {
                    if (idx.texcoord_index >= 0)
                    {
                        outUv.push_back(attrib.texcoords[2 * idx.texcoord_index + 0]);
                        outUv.push_back(attrib.texcoords[2 * idx.texcoord_index + 1]);
                    }
                    else
                    {
                        outUv.push_back(0.0);
                        outUv.push_back(0.0);
                    }
                }

                if (v == 0)
                {
                    firstOutIdx = outVertex;
                }
                else if (v >= 2)
                {
                    // Fan: (erster Vertex des Faces, vorheriger, aktueller)
                    outIdx.push_back(firstOutIdx);
                    outIdx.push_back(outVertex - 1);
                    outIdx.push_back(outVertex);
                }
            }
            indexOffsetInFace += fv;
        }

        flushRange(currentMaterial);
    }

    return true;
}

_WaveformObject::_WaveformObject() {}

_WaveformObject::_WaveformObject(const std::string& filename)
{
    Load(filename);
}

_WaveformObject::_WaveformObject(size_t size, void* buffer)
{
    Load(size, buffer);
}

_WaveformObject::_WaveformObject(const std::vector<uint8_t>& buffer)
{
    Load(buffer);
}

bool _WaveformObject::Load(const std::vector<uint8_t>& buffer)
{
    return Load(buffer.size(), (void*)buffer.data());
}

void _WaveformObject::Reset()
{
    m_positions.clear();
    m_normals.clear();
    m_uvs.clear();
    m_indices.clear();
    m_subMeshes.clear();
    m_hasNormals = false;
    m_hasUVs = false;
}

bool _WaveformObject::Load(const std::string& filename)
{
    Reset();

    tinyobj::ObjReaderConfig config;
    config.triangulate = false; // wir triangulieren selbst (Fan), siehe oben

    tinyobj::ObjReader reader;
    if (!reader.ParseFromFile(filename, config))
    {
        if (!reader.Error().empty())
            printf("tinyobjloader Fehler (%s): %s\n", filename.c_str(), reader.Error().c_str());
        return false;
    }
    if (!reader.Warning().empty())
        printf("tinyobjloader Warnung (%s): %s\n", filename.c_str(), reader.Warning().c_str());

    return ImportFromReader(reader, m_positions, m_normals, m_uvs, m_indices, m_subMeshes, m_hasNormals, m_hasUVs);
}

bool _WaveformObject::Load(size_t size, void* buffer)
{
    Reset();

    std::string objText(reinterpret_cast<const char*>(buffer), size);

    tinyobj::ObjReaderConfig config;
    config.triangulate = false;

    tinyobj::ObjReader reader;
    // Aus dem Speicher geladen: keine begleitende .mtl-Datei im
    // Dateisystem auffindbar - ohne Materialtext parsen. Geometrie ist
    // trotzdem vollstaendig, Materialnamen bleiben leer/-1.
    if (!reader.ParseFromString(objText, "", config))
    {
        if (!reader.Error().empty())
            printf("tinyobjloader Fehler (memory): %s\n", reader.Error().c_str());
        return false;
    }
    if (!reader.Warning().empty())
        printf("tinyobjloader Warnung (memory): %s\n", reader.Warning().c_str());

    return ImportFromReader(reader, m_positions, m_normals, m_uvs, m_indices, m_subMeshes, m_hasNormals, m_hasUVs);
}

// ===========================================================================
// Resource-Manager
// ===========================================================================
namespace ng {
    namespace resource {

        WaveformObjResourceManager::WaveformObjResourceManager()
            : TemplateResourceManager<renderer::gfx::_WaveformObject>("WAVEFORMOBJ_RESOURCE") {
        }
        WaveformObjResourceManager::~WaveformObjResourceManager() {}

        uint64_t WaveformObjResourceManager::LoadObjFromFile(std::string resname, std::string filename, bool keep_source, unsigned long attrib)
        {
            renderer::gfx::_WaveformObject* obj = new renderer::gfx::_WaveformObject;
            bool ok = obj->Load(filename);
            if (!ok) { delete obj; obj = nullptr; }
            return Add(resname, obj, attrib);
        }

        uint64_t WaveformObjResourceManager::LoadObjFromMemory(std::string resname, unsigned char* buffer, unsigned long buffer_size, bool keep_source, unsigned long attrib)
        {
            renderer::gfx::_WaveformObject* obj = new renderer::gfx::_WaveformObject;
            bool ok = obj->Load((size_t)buffer_size, (void*)buffer);
            if (!ok) { delete obj; obj = nullptr; }
            return Add(resname, obj, attrib);
        }

        uint64_t WaveformObjResourceManager::Add(std::string resname, renderer::gfx::_WaveformObject* ptr, unsigned long attrib)
        {
            return AddResource(resname, ptr, attrib);
        }

        // Dummy-Ressource fuer fehlende/kaputte OBJ-Dateien: ein simples
        // Dreieck, damit ein fehlerhafter Verweis wenigstens sichtbar
        // (statt komplett leer/crashend) bleibt - analog zum Schachbrett-
        // Dummy bei ImageResourceManager.
        ng::renderer::gfx::_WaveformObject* WaveformObjResourceManager::CreateDummyResourceEvent()
        {
            static const char* dummyObj =
                "v 0 0 0\n"
                "v 1 0 0\n"
                "v 0 1 0\n"
                "vt 0 0\n"
                "vt 1 0\n"
                "vt 0 1\n"
                "vn 0 0 1\n"
                "f 1/1/1 2/2/1 3/3/1\n";

            ng::renderer::gfx::_WaveformObject* obj = new ng::renderer::gfx::_WaveformObject;
            obj->Load(strlen(dummyObj), (void*)dummyObj);
            return obj;
        }

        unsigned long WaveformObjResourceManager::ParseXMLElement(tinyxml2::XMLElement* element)
        {
            if (element == nullptr) return 1;
            if (std::string("OBJ") != std::string(element->Name())) {
                return 1;
            }
            if (element->FindAttribute("name") == nullptr) {
                return 1;
            }
            std::string name = element->Attribute("name");
            bool keep_source = false;
            if (element->FindAttribute("keep_source") != nullptr) {
                keep_source = element->BoolAttribute("keep_source");
            }
            unsigned long attrib = ng::resource::DELETE_WHEN_UNUSED;
            if (element->FindAttribute("attrib") != nullptr) {
                attrib = (unsigned long)element->UnsignedAttribute("attrib");
            }
            if (element->FindAttribute("filename") != nullptr) {
                std::string filename = ng::resource::Manager::GetResourcePath() + element->Attribute("filename");
                LoadObjFromFile(name, filename, keep_source, attrib);
                return 0;
            }
            return 1;
        }

    }
}