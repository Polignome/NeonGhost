#pragma once

#include "Object3D.h"
#include "Vertex.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include "Global.h"

namespace ng {
    namespace renderer {
        namespace objects {

            template<typename T>
            class Mesh : public Object3D<T>
            {
                static_assert(std::is_floating_point<T>::value,
                    "Mesh<T> requires a floating point type (float/double).");

            public:

                using Vec3 = typename Object3D<T>::Vec3;
                using Vec2 = ng::math::Matrix<2, 1, T>;
                using Vertex = ng::geometry::Vertex<T>;


                // ============================================================
                // Raycast Ergebnis
                // ============================================================

                struct RayHit
                {
                    bool     hit = false;
                    T        distance = std::numeric_limits<T>::max();
                    Vec3     point{ 0, 0, 0 };
                    Vec3     normal{ 0, 0, 0 };
                    uint32_t triangleIndex = 0;
                    uint32_t i0 = 0, i1 = 0, i2 = 0;
                    Vec2     uv{ 0, 0 };                 // NEU
                    T        baryU = 0, baryV = 0, baryW = 0; // NEU
                };

                //

                enum class UVAxis { X, Y, Z };

                // ============================================================
                // SubMesh
                // ============================================================

                struct SubMesh
                {
                    uint32_t indexOffset = 0;
                    uint32_t indexCount = 0;
                    int      materialId = -1;
                };

                struct LODDescription
                {
                    T distance;
                    T ratio;
                };

                // ============================================================
                // LOD
                //
                // Das Originalmesh ist immer LOD0.
                //
                // Zusätzliche LODs enthalten nur einen alternativen
                // Indexbuffer und optional eigene SubMeshes.
                //
                // Beispiel:
                //
                // LOD0 = Original
                // LOD1 = ab 20m
                // LOD2 = ab 50m
                // LOD3 = ab 100m
                // ============================================================

                struct LODLevel
                {
                    // Ab dieser Entfernung wird dieses LOD verwendet.
                    T distance = static_cast<T>(0);

                    // Optional:
                    // Hysterese verhindert Flackern beim Wechseln
                    // zwischen zwei LOD-Stufen.
                    T hysteresis = static_cast<T>(0);

                    // Alternativer Indexbuffer
                    std::vector<uint32_t> indices;

                    // Optionale Materialaufteilung
                    std::vector<SubMesh> subMeshes;
                };


                // ============================================================
                // Konstruktor / Destruktor
                // ============================================================

                Mesh() = default;
                virtual ~Mesh() = default;


                // ============================================================
                // Ray im LOKALEN Raum des Meshes
                // ============================================================
                RayHit RaycastLocal(
                    const Vec3& origin,
                    const Vec3& direction,
                    bool backfaceCull = true) const
                {
                    RayHit best;

                    T dirLen = Length(direction);

                    if (dirLen < static_cast<T>(1e-8))
                        return best;

                    Vec3 dirNorm =
                        direction * (static_cast<T>(1) / dirLen);

                    ng::math::RayPlaneLine<3, T>
                        ray(origin, dirNorm);


                    for (size_t t = 0; t < TriangleCount(); ++t)
                    {
                        uint32_t i0, i1, i2;

                        if (!GetTriangleIndices(t, i0, i1, i2))
                            continue;

                        const Vec3& p0 = m_vertices[i0].world();
                        const Vec3& p1 = m_vertices[i1].world();
                        const Vec3& p2 = m_vertices[i2].world();


                        if (backfaceCull)
                        {
                            Vec3 n = (p1 - p0) % (p2 - p0);

                            if ((n ^ dirNorm) >= static_cast<T>(0))
                                continue;
                        }


                        double time = 0.0;

                        int result =
                            ray.intersect(
                                p0,
                                p1,
                                p2,
                                time);


                        if (result ==
                            ng::math::RayPlaneLine<3, T>::INSIDE)
                        {
                            T dist = static_cast<T>(time);

                            if (dist >= static_cast<T>(0) &&
                                dist < best.distance)
                            {
                                best.hit = true;
                                best.distance = dist;

                                best.point =
                                    origin + dirNorm * dist;

                                best.triangleIndex =
                                    static_cast<uint32_t>(t);

                                best.i0 = i0;
                                best.i1 = i1;
                                best.i2 = i2;

                                best.normal =
                                    ComputeFaceNormal(t);
                            }
                        }
                    }

                    if (best.hit)
                    {
                        const Vec3& p0 = m_vertices[best.i0].world();
                        const Vec3& p1 = m_vertices[best.i1].world();
                        const Vec3& p2 = m_vertices[best.i2].world();
                        ComputeBarycentric(best.point, p0, p1, p2,
                            best.baryU, best.baryV, best.baryW);
                        best.uv = InterpolateUV(best.triangleIndex,
                            best.baryU, best.baryV, best.baryW);
                    }
                    return best;
                    
                }


                // ============================================================
// TEXTURE MAPPING (UV)
// ============================================================
                uint32_t AddVertex(const Vertex& v, T u, T vcoord)
                {
                    uint32_t idx = AddVertex(v);
                    SetUV(idx, u, vcoord);
                    return idx;
                }

                uint32_t AddVertex(T x, T y, T z, T u, T vcoord)
                {
                    uint32_t idx = AddVertex(x, y, z);
                    SetUV(idx, u, vcoord);
                    return idx;
                }
                
                


                void SetUV(size_t idx, T u, T v)
                {
                    if (m_uvs.size() < m_vertices.size())
                        m_uvs.resize(m_vertices.size(), Vec2(0, 0));
                    if (idx >= m_uvs.size())
                        return;
                    m_uvs[idx] = Vec2(u, v);
                }

                void SetUV(size_t idx, const Vec2& uv)
                {
                    SetUV(idx, uv.x(), uv.y());
                }

                Vec2 GetUV(size_t idx) const
                {
                    if (idx >= m_uvs.size())
                        return Vec2(0, 0);
                    return m_uvs[idx];
                }

                const std::vector<Vec2>& UVs() const { return m_uvs; }

                std::vector<Vec2>& UVsMutable()
                {
                    if (m_uvs.size() < m_vertices.size())
                        m_uvs.resize(m_vertices.size(), Vec2(0, 0));
                    return m_uvs;
                }

                bool HasUVs() const
                {
                    return !m_uvs.empty() && m_uvs.size() >= m_vertices.size();
                }

                bool GetTriangleUV(size_t triIdx, Vec2& uv0, Vec2& uv1, Vec2& uv2) const
                {
                    uint32_t i0, i1, i2;
                    if (!GetTriangleIndices(triIdx, i0, i1, i2))
                        return false;
                    uv0 = GetUV(i0);
                    uv1 = GetUV(i1);
                    uv2 = GetUV(i2);
                    return true;
                }


                Vec2 InterpolateUV(size_t triIdx, T baryU, T baryV, T baryW) const
                {
                    Vec2 uv0, uv1, uv2;
                    if (!GetTriangleUV(triIdx, uv0, uv1, uv2))
                        return Vec2(0, 0);
                    return uv0 * baryU + uv1 * baryV + uv2 * baryW;
                }

                // ------------------------------------------------------------
                // Planare Projektion entlang einer Achse
                // ------------------------------------------------------------
                void GenerateUVsPlanar(
                    UVAxis axis = UVAxis::Y,
                    T tileU = static_cast<T>(1),
                    T tileV = static_cast<T>(1))
                {
                    if (m_vertices.empty())
                        return;
                    RecalculateLocalBoundsIfNeeded();
                    Vec3 size = m_localMax - m_localMin;
                    m_uvs.resize(m_vertices.size());
                    for (size_t i = 0; i < m_vertices.size(); ++i)
                    {
                        const Vec3& p = m_vertices[i].world();
                        T a, b, minA, minB, sizeA, sizeB;
                        switch (axis)
                        {
                        case UVAxis::X:
                            a = p.y(); b = p.z();
                            minA = m_localMin.y(); minB = m_localMin.z();
                            sizeA = std::max(size.y(), static_cast<T>(1e-8));
                            sizeB = std::max(size.z(), static_cast<T>(1e-8));
                            break;
                        case UVAxis::Z:
                            a = p.x(); b = p.y();
                            minA = m_localMin.x(); minB = m_localMin.y();
                            sizeA = std::max(size.x(), static_cast<T>(1e-8));
                            sizeB = std::max(size.y(), static_cast<T>(1e-8));
                            break;
                        case UVAxis::Y:
                        default:
                            a = p.x(); b = p.z();
                            minA = m_localMin.x(); minB = m_localMin.z();
                            sizeA = std::max(size.x(), static_cast<T>(1e-8));
                            sizeB = std::max(size.z(), static_cast<T>(1e-8));
                            break;
                        }
                        m_uvs[i] = Vec2(((a - minA) / sizeA) * tileU,
                            ((b - minB) / sizeB) * tileV);
                    }
                }

                // ------------------------------------------------------------
                // Box-Mapping (pro Dreieck, dominante Normalenachse)
                // Dupliziert Vertices wie ComputeFlatNormals -> keine Naht-
                // Verzerrung, danach ComputeFlatNormals() oder
                // ComputeSmoothNormals() neu aufrufen!
                // ------------------------------------------------------------
                void GenerateUVsBoxMapped(T tileScale = static_cast<T>(1))
                {
                    if (m_vertices.empty() || m_indices.empty())
                        return;
                    RecalculateLocalBoundsIfNeeded();
                    Vec3 size = m_localMax - m_localMin;
                    T sx = std::max(size.x(), static_cast<T>(1e-8));
                    T sy = std::max(size.y(), static_cast<T>(1e-8));
                    T sz = std::max(size.z(), static_cast<T>(1e-8));

                    std::vector<Vertex> newVerts;
                    std::vector<Vec2>   newUVs;
                    std::vector<uint32_t> newIndices;
                    newVerts.reserve(m_indices.size());
                    newUVs.reserve(m_indices.size());
                    newIndices.reserve(m_indices.size());

                    for (size_t t = 0; t < TriangleCount(); ++t)
                    {
                        uint32_t i0, i1, i2;
                        if (!GetTriangleIndices(t, i0, i1, i2))
                            break;
                        Vertex v0 = m_vertices[i0];
                        Vertex v1 = m_vertices[i1];
                        Vertex v2 = m_vertices[i2];
                        Vec3 n = ComputeFaceNormal(t);
                        T ax = std::abs(n.x()), ay = std::abs(n.y()), az = std::abs(n.z());

                        auto project = [&](const Vec3& p) -> Vec2
                            {
                                if (ax >= ay && ax >= az)
                                    return Vec2(((p.z() - m_localMin.z()) / sz) * tileScale,
                                        ((p.y() - m_localMin.y()) / sy) * tileScale);
                                if (ay >= ax && ay >= az)
                                    return Vec2(((p.x() - m_localMin.x()) / sx) * tileScale,
                                        ((p.z() - m_localMin.z()) / sz) * tileScale);
                                return Vec2(((p.x() - m_localMin.x()) / sx) * tileScale,
                                    ((p.y() - m_localMin.y()) / sy) * tileScale);
                            };

                        uint32_t base = static_cast<uint32_t>(newVerts.size());
                        newVerts.push_back(v0); newVerts.push_back(v1); newVerts.push_back(v2);
                        newUVs.push_back(project(v0.world()));
                        newUVs.push_back(project(v1.world()));
                        newUVs.push_back(project(v2.world()));
                        newIndices.push_back(base + 0);
                        newIndices.push_back(base + 1);
                        newIndices.push_back(base + 2);
                    }

                    m_vertices = std::move(newVerts);
                    m_uvs = std::move(newUVs);
                    m_indices = std::move(newIndices);
                    m_normalsDirty = true;
                    MarkGeometryDirty();
                    m_lods.clear();       // Indexbuffer ungültig geworden
                    m_subMeshes.clear();
                }

                // ------------------------------------------------------------
                // Sphärisches Mapping (z.B. für Kugeln/Planeten)
                // ------------------------------------------------------------
                void GenerateUVsSpherical(T tileU = static_cast<T>(1), T tileV = static_cast<T>(1))
                {
                    if (m_vertices.empty())
                        return;
                    const T pi = static_cast<T>(3.14159265358979323846);
                    Vec3 center = GetLocalCenterOffset();
                    m_uvs.resize(m_vertices.size());
                    for (size_t i = 0; i < m_vertices.size(); ++i)
                    {
                        Vec3 p = m_vertices[i].world() - center;
                        T len = Length(p);
                        if (len < static_cast<T>(1e-8)) { m_uvs[i] = Vec2(0, 0); continue; }
                        Vec3 n = p * (static_cast<T>(1) / len);
                        T u = (std::atan2(n.z(), n.x()) / (static_cast<T>(2) * pi) + static_cast<T>(0.5)) * tileU;
                        T v = (std::asin(std::clamp(n.y(), static_cast<T>(-1), static_cast<T>(1))) / pi + static_cast<T>(0.5)) * tileV;
                        m_uvs[i] = Vec2(u, v);
                    }
                }

                // ------------------------------------------------------------
                // Zylindrisches Mapping (z.B. Säulen, Fässer, Bäume)
                // ------------------------------------------------------------
                void GenerateUVsCylindrical(
                    UVAxis axis = UVAxis::Y,
                    T tileU = static_cast<T>(1),
                    T tileV = static_cast<T>(1))
                {
                    if (m_vertices.empty())
                        return;
                    RecalculateLocalBoundsIfNeeded();
                    Vec3 size = m_localMax - m_localMin;
                    Vec3 center = GetLocalCenterOffset();
                    const T pi = static_cast<T>(3.14159265358979323846);
                    m_uvs.resize(m_vertices.size());
                    for (size_t i = 0; i < m_vertices.size(); ++i)
                    {
                        Vec3 p = m_vertices[i].world() - center;
                        T a1, a2, along, axisSize, minAlong;
                        switch (axis)
                        {
                        case UVAxis::X:
                            a1 = p.y(); a2 = p.z(); along = p.x();
                            axisSize = std::max(size.x(), static_cast<T>(1e-8));
                            minAlong = m_localMin.x() - center.x();
                            break;
                        case UVAxis::Z:
                            a1 = p.x(); a2 = p.y(); along = p.z();
                            axisSize = std::max(size.z(), static_cast<T>(1e-8));
                            minAlong = m_localMin.z() - center.z();
                            break;
                        case UVAxis::Y:
                        default:
                            a1 = p.x(); a2 = p.z(); along = p.y();
                            axisSize = std::max(size.y(), static_cast<T>(1e-8));
                            minAlong = m_localMin.y() - center.y();
                            break;
                        }
                        T u = (std::atan2(a2, a1) / (static_cast<T>(2) * pi) + static_cast<T>(0.5)) * tileU;
                        T v = ((along - minAlong) / axisSize) * tileV;
                        m_uvs[i] = Vec2(u, v);
                    }
                }

                // ============================================================
                // Ray im WELTRAUM
                // ============================================================

                RayHit RaycastWorld(
                    const Vec3& worldOrigin,
                    const Vec3& worldDirection,
                    bool backfaceCull = true) const
                {
                    Vec3 localOrigin =
                        InverseTransformPoint(worldOrigin);

                    Vec3 localDirection =
                        InverseTransformDirection(worldDirection);


                    RayHit hit =
                        RaycastLocal(
                            localOrigin,
                            localDirection,
                            backfaceCull);


                    if (hit.hit)
                    {
                        hit.point =
                            TransformPoint(hit.point);

                        hit.normal =
                            TransformNormal(hit.normal);

                        hit.distance =
                            Length(hit.point - worldOrigin);
                    }


                    return hit;
                }


                // ============================================================
                // Ray gegen Bounds
                // ============================================================

                bool RayIntersectsBounds(
                    const Vec3& worldOrigin,
                    const Vec3& worldDirection,
                    T* outTMin = nullptr,
                    T* outTMax = nullptr) const
                {
                    const math::Matrix<3, 1, T> o(
                        worldOrigin.x(),
                        worldOrigin.y(),
                        worldOrigin.z());

                    const math::Matrix<3, 1, T> w(
                        worldDirection.x(),
                        worldDirection.y(),
                        worldDirection.z());

                    return this->Bounds().RaySlabTest(
                        o,
                        w,
                        outTMin,
                        outTMax);
                }


                // ============================================================
                // Normalfarben
                // ============================================================

                inline ng::math::Matrix<4, 1, T>
                    SetNormalColor(
                        ng::math::Matrix<3, 1, T> normal,
                        T red = 1,
                        T green = 1,
                        T blue = 1,
                        T alpha = 1)
                {
                    ng::math::Matrix<3, 1, T>
                        color3 =
                        (T)0.5 + normal * (T)0.5;

                    ng::math::Matrix<4, 1, T>
                        color4;


                    T f =
                        (T)(
                            (T)0.299 * color3.x() +
                            (T)0.587 * color3.y() +
                            (T)0.114 * color3.z()
                            );


                    color4.x() = f * (T)red;
                    color4.y() = f * (T)green;
                    color4.z() = f * (T)blue;
                    color4.w() = (T)alpha;

                    return color4;
                }


                // ============================================================
                // Geometrie aufbauen
                // ============================================================
                void Clear()
                {
                    m_vertices.clear();
                    m_indices.clear();
                    m_subMeshes.clear();
                    m_uvs.clear();          // NEU
                    m_lods.clear();
                    MarkGeometryDirty();
                }


                void ReserveVertices(size_t n)
                {
                    m_vertices.reserve(n);
                }


                void ReserveIndices(size_t n)
                {
                    m_indices.reserve(n);
                }

                uint32_t AddVertex(const Vertex& v)
                {
                    m_vertices.push_back(v);
                    m_uvs.push_back(Vec2(0, 0));   // NEU
                    MarkGeometryDirty();
                    return static_cast<uint32_t>(m_vertices.size() - 1);

                }

                uint32_t AddVertex(T x, T y, T z)
                {
                    return AddVertex(Vertex(x, y, z));
                }


                void AddTriangle(
                    uint32_t i0,
                    uint32_t i1,
                    uint32_t i2)
                {
                    m_indices.push_back(i0);
                    m_indices.push_back(i1);
                    m_indices.push_back(i2);

                    m_normalsDirty = true;
                }


                void SetIndices(
                    std::vector<uint32_t> indices)
                {
                    m_indices = std::move(indices);

                    m_normalsDirty = true;
                }


                // ============================================================
                // Direkter Vertexzugriff
                // ============================================================

                std::vector<Vertex>& VerticesMutable()
                {
                    return m_vertices;
                }


                const std::vector<Vertex>& Vertices() const
                {
                    return m_vertices;
                }


                const std::vector<uint32_t>& Indices() const
                {
                    return m_indices;
                }


                size_t VertexCount() const
                {
                    return m_vertices.size();
                }


                size_t IndexCount() const
                {
                    return m_indices.size();
                }


                size_t TriangleCount() const
                {
                    return m_indices.size() / 3;
                }


                const Vertex& GetVertex(size_t idx) const
                {
                    return m_vertices[idx];
                }


                Vertex& GetVertex(size_t idx)
                {
                    return m_vertices[idx];
                }


                bool GetTriangleIndices(
                    size_t triIdx,
                    uint32_t& i0,
                    uint32_t& i1,
                    uint32_t& i2) const
                {
                    size_t base = triIdx * 3;

                    if (base + 2 >= m_indices.size())
                        return false;

                    i0 = m_indices[base + 0];
                    i1 = m_indices[base + 1];
                    i2 = m_indices[base + 2];

                    return true;
                }


                // ============================================================
                // SubMeshes
                // ============================================================

                void AddSubMesh(
                    uint32_t indexOffset,
                    uint32_t indexCount,
                    int materialId)
                {
                    m_subMeshes.push_back(
                        SubMesh{
                            indexOffset,
                            indexCount,
                            materialId
                        });
                }


                const std::vector<SubMesh>&
                    SubMeshes() const
                {
                    return m_subMeshes;
                }


                std::vector<SubMesh>
                    EffectiveSubMeshes() const
                {
                    if (!m_subMeshes.empty())
                        return m_subMeshes;


                    return
                    {
                        SubMesh
                        {
                            0,
                            static_cast<uint32_t>(
                                m_indices.size()),
                            -1
                        }
                    };
                }


                // ============================================================
                // ============================================================
                // LOD SYSTEM
                // ============================================================
                // ============================================================


                // ------------------------------------------------------------
                // LOD aktivieren / deaktivieren
                //
                // Standardmäßig:
                // m_lodEnabled = false
                //
                // Das bedeutet:
                // Das Mesh verhält sich exakt wie vorher.
                // ------------------------------------------------------------

                void EnableLOD(bool enable = true)
                {
                    m_lodEnabled = enable;
                }


                bool LODEnabled() const
                {
                    return m_lodEnabled;
                }


                // ------------------------------------------------------------
                // Alle LODs entfernen
                // ------------------------------------------------------------

                void ClearLODs()
                {
                    m_lods.clear();
                }


                // ------------------------------------------------------------
                // Anzahl der LOD-Stufen
                //
                // LOD0 wird nicht mitgezählt.
                // ------------------------------------------------------------

                size_t LODCount() const
                {
                    return m_lods.size();
                }


                // ------------------------------------------------------------
                // LOD hinzufügen
                //
                // distance:
                // Ab dieser Entfernung wird das LOD aktiv.
                //
                // Beispiel:
                //
                // AddLOD(20, lod1);
                // AddLOD(50, lod2);
                // AddLOD(100, lod3);
                // ------------------------------------------------------------

                void AddLOD(
                    T distance,
                    const std::vector<uint32_t>& indices)
                {
                    LODLevel lod;

                    lod.distance = distance;
                    lod.indices = indices;

                    m_lods.push_back(
                        std::move(lod));

                    SortLODs();
                }


                // ------------------------------------------------------------
                // LOD hinzufügen + SubMeshes
                // ------------------------------------------------------------

                void AddLOD(
                    T distance,
                    const std::vector<uint32_t>& indices,
                    const std::vector<SubMesh>& subMeshes)
                {
                    LODLevel lod;

                    lod.distance = distance;
                    lod.indices = indices;
                    lod.subMeshes = subMeshes;

                    m_lods.push_back(
                        std::move(lod));

                    SortLODs();
                }


                // ------------------------------------------------------------
                // LOD mit Hysterese hinzufügen
                //
                // hysteresis verhindert LOD-Flackern.
                //
                // Beispiel:
                //
                // distance = 50
                // hysteresis = 5
                //
                // Beim Entfernen:
                // LOD2 ab 50m
                //
                // Beim Zurückkommen:
                // erst unter 45m wieder zurück
                //
                // Hinweis:
                // Die eigentliche Hysterese wird erst dann vollständig
                // wirksam, wenn du den zuletzt verwendeten LOD-Zustand
                // speicherst. Die einfache Distanzselektion unten bleibt
                // deshalb deterministisch.
                // ------------------------------------------------------------

                void SetLODHysteresis(
                    size_t lodIndex,
                    T hysteresis)
                {
                    if (lodIndex >= m_lods.size())
                        return;

                    m_lods[lodIndex].hysteresis =
                        std::max(
                            static_cast<T>(0),
                            hysteresis);
                }


                // ------------------------------------------------------------
                // LOD Entfernung ändern
                //
                // lodIndex:
                // 0 = erstes zusätzliches LOD
                // 1 = zweites zusätzliches LOD
                // usw.
                // ------------------------------------------------------------

                bool SetLODDistance(
                    size_t lodIndex,
                    T distance)
                {
                    if (lodIndex >= m_lods.size())
                        return false;

                    m_lods[lodIndex].distance =
                        distance;

                    SortLODs();

                    return true;
                }


                // ------------------------------------------------------------
                // LOD-Information
                // ------------------------------------------------------------

                const LODLevel*
                    GetLOD(size_t lodIndex) const
                {
                    if (lodIndex >= m_lods.size())
                        return nullptr;

                    return &m_lods[lodIndex];
                }


                // ------------------------------------------------------------
                // Aktuelles LOD bestimmen
                //
                // Rückgabe:
                //
                // 0 = Originalmesh
                // 1 = m_lods[0]
                // 2 = m_lods[1]
                // 3 = m_lods[2]
                // ...
                // ------------------------------------------------------------

                size_t SelectLOD(
                    const Vec3& cameraPosition) const
                {
                    // LOD abgeschaltet
                    if (!m_lodEnabled)
                        return 0;


                    // Keine LODs vorhanden
                    if (m_lods.empty())
                        return 0;


                    // Weltposition des Mesh-Zentrums
                    Vec3 center =
                        TransformPoint(
                            GetLocalCenterOffset());


                    T distance =
                        Length(
                            center -
                            cameraPosition);


                    size_t selectedLOD = 0;


                    for (size_t i = 0;
                        i < m_lods.size();
                        ++i)
                    {
                        if (distance >=
                            m_lods[i].distance)
                        {
                            selectedLOD = i + 1;
                        }
                        else
                        {
                            break;
                        }
                    }


                    return selectedLOD;
                }

                void BuildLODs(
                    const std::vector<LODDescription>& descriptions)
                {
                    m_lods.clear();

                    if (m_indices.empty() || m_vertices.empty())
                        return;

                    for (const auto& desc : descriptions)
                    {
                        if (desc.ratio >= static_cast<T>(1))
                            continue;

                        if (desc.ratio <= static_cast<T>(0))
                            continue;

                        std::vector<uint32_t> lodIndices =
                            BuildLODIndices(desc.ratio);

                        if (lodIndices.empty())
                            continue;

                        AddLOD(desc.distance, lodIndices);
                    }
                }

                std::vector<uint32_t> BuildLODIndices(T ratio) const
                {
                    if (m_indices.empty() || m_vertices.empty())
                        return {};

                    if (ratio >= static_cast<T>(1))
                        return m_indices;

                    size_t triangleCount = TriangleCount();

                    size_t targetTriangles =
                        static_cast<size_t>(
                            static_cast<T>(triangleCount) * ratio);

                    if (targetTriangles < 1)
                        targetTriangles = 1;

                    // ------------------------------------------------------------
                    // Bounding Box bestimmen
                    // ------------------------------------------------------------

                    Vec3 mn = m_vertices[0].world();
                    Vec3 mx = m_vertices[0].world();

                    for (const auto& v : m_vertices)
                    {
                        const Vec3& p = v.world();

                        mn.x() = std::min(mn.x(), p.x());
                        mn.y() = std::min(mn.y(), p.y());
                        mn.z() = std::min(mn.z(), p.z());

                        mx.x() = std::max(mx.x(), p.x());
                        mx.y() = std::max(mx.y(), p.y());
                        mx.z() = std::max(mx.z(), p.z());
                    }

                    Vec3 size = mx - mn;

                    // ------------------------------------------------------------
                    // Zellgröße anhand des gewünschten Reduktionsfaktors
                    // ------------------------------------------------------------

                    T volume =
                        std::max(
                            size.x() * size.y() * size.z(),
                            static_cast<T>(1e-8));

                    T wantedCells =
                        static_cast<T>(
                            std::max(
                                static_cast<size_t>(1),
                                static_cast<size_t>(m_vertices.size() * ratio)));

                    T cellSize =
                        std::cbrt(volume / wantedCells);

                    if (cellSize < static_cast<T>(1e-6))
                        cellSize = static_cast<T>(1e-6);

                    // ------------------------------------------------------------
                    // Vertex -> Cluster
                    // ------------------------------------------------------------

                    struct Cell
                    {
                        int x;
                        int y;
                        int z;

                        bool operator==(const Cell& o) const
                        {
                            return
                                x == o.x &&
                                y == o.y &&
                                z == o.z;
                        }
                    };

                    struct CellHash
                    {
                        size_t operator()(const Cell& c) const
                        {
                            size_t h1 =
                                std::hash<int>{}(c.x);

                            size_t h2 =
                                std::hash<int>{}(c.y);

                            size_t h3 =
                                std::hash<int>{}(c.z);

                            return
                                h1 ^
                                (h2 << 1) ^
                                (h3 << 2);
                        }
                    };

                    struct Cluster
                    {
                        Vec3 position{ 0, 0, 0 };
                        size_t count = 0;
                    };

                    std::unordered_map<Cell, uint32_t, CellHash>
                        cellMap;

                    std::vector<Cluster>
                        clusters;

                    std::vector<uint32_t>
                        vertexCluster;

                    vertexCluster.resize(
                        m_vertices.size());

                    // ------------------------------------------------------------
                    // Vertices clustern
                    // ------------------------------------------------------------

                    for (size_t i = 0;
                        i < m_vertices.size();
                        ++i)
                    {
                        const Vec3& p =
                            m_vertices[i].world();

                        Cell cell;

                        cell.x =
                            static_cast<int>(
                                std::floor(
                                    (p.x() - mn.x()) /
                                    cellSize));

                        cell.y =
                            static_cast<int>(
                                std::floor(
                                    (p.y() - mn.y()) /
                                    cellSize));

                        cell.z =
                            static_cast<int>(
                                std::floor(
                                    (p.z() - mn.z()) /
                                    cellSize));

                        auto it =
                            cellMap.find(cell);

                        uint32_t clusterIndex;

                        if (it == cellMap.end())
                        {
                            clusterIndex =
                                static_cast<uint32_t>(
                                    clusters.size());

                            cellMap.emplace(
                                cell,
                                clusterIndex);

                            clusters.push_back(
                                Cluster{});
                        }
                        else
                        {
                            clusterIndex =
                                it->second;
                        }

                        Cluster& cluster =
                            clusters[clusterIndex];

                        cluster.position += p;
                        cluster.count++;

                        vertexCluster[i] =
                            clusterIndex;
                    }

                    // ------------------------------------------------------------
                    // Clusterpositionen mitteln
                    // ------------------------------------------------------------

                    for (auto& cluster : clusters)
                    {
                        if (cluster.count > 0)
                        {
                            cluster.position =
                                cluster.position *
                                (static_cast<T>(1) /
                                    static_cast<T>(cluster.count));
                        }
                    }

                    // ------------------------------------------------------------
                    // Neue Indices erzeugen
                    // ------------------------------------------------------------

                    std::vector<uint32_t>
                        result;

                    result.reserve(
                        targetTriangles * 3);

                    std::unordered_set<uint64_t>
                        usedTriangles;

                    for (size_t t = 0;
                        t < triangleCount;
                        ++t)
                    {
                        uint32_t i0, i1, i2;

                        if (!GetTriangleIndices(
                            t,
                            i0,
                            i1,
                            i2))
                        {
                            continue;
                        }

                        uint32_t a =
                            vertexCluster[i0];

                        uint32_t b =
                            vertexCluster[i1];

                        uint32_t c =
                            vertexCluster[i2];

                        // Degeneriertes Dreieck
                        if (a == b ||
                            b == c ||
                            a == c)
                        {
                            continue;
                        }

                        // sortierte ID nur zum Duplikat-Test
                        uint32_t s0 = a;
                        uint32_t s1 = b;
                        uint32_t s2 = c;

                        if (s0 > s1)
                            std::swap(s0, s1);

                        if (s1 > s2)
                            std::swap(s1, s2);

                        if (s0 > s1)
                            std::swap(s0, s1);

                        uint64_t key =
                            (static_cast<uint64_t>(s0) << 42) |
                            (static_cast<uint64_t>(s1) << 21) |
                            static_cast<uint64_t>(s2);

                        if (!usedTriangles.insert(key).second)
                            continue;

                        result.push_back(a);
                        result.push_back(b);
                        result.push_back(c);

                        if (result.size() / 3 >= targetTriangles)
                            break;
                    }

                    return result;
                }

                // ------------------------------------------------------------
                // Distanz zur Kamera
                // ------------------------------------------------------------

                T DistanceToCamera(
                    const Vec3& cameraPosition) const
                {
                    Vec3 center =
                        TransformPoint(
                            GetLocalCenterOffset());


                    return Length(
                        center -
                        cameraPosition);
                }


                // ------------------------------------------------------------
                // Aktiver Indexbuffer
                //
                // LOD deaktiviert:
                //     m_indices
                //
                // LOD0:
                //     m_indices
                //
                // LOD1:
                //     m_lods[0].indices
                //
                // usw.
                // ------------------------------------------------------------

                const std::vector<uint32_t>&
                    GetRenderIndices(
                        const Vec3& cameraPosition) const
                {
                    size_t lod =
                        SelectLOD(cameraPosition);


                    if (lod == 0)
                        return m_indices;


                    return m_lods[lod - 1].indices;
                }


                // Alias für Renderer
                const std::vector<uint32_t>&
                    ActiveIndices(
                        const Vec3& cameraPosition) const
                {
                    return GetRenderIndices(
                        cameraPosition);
                }


                // ------------------------------------------------------------
                // Anzahl der Dreiecke des aktiven LOD
                // ------------------------------------------------------------

                size_t RenderTriangleCount(
                    const Vec3& cameraPosition) const
                {
                    return
                        GetRenderIndices(
                            cameraPosition).size() / 3;
                }


                // ------------------------------------------------------------
                // Aktive SubMeshes
                // ------------------------------------------------------------

                std::vector<SubMesh>
                    GetRenderSubMeshes(
                        const Vec3& cameraPosition) const
                {
                    size_t lod =
                        SelectLOD(cameraPosition);


                    // Originalmesh
                    if (lod == 0)
                        return EffectiveSubMeshes();


                    const LODLevel& level =
                        m_lods[lod - 1];


                    // LOD besitzt eigene SubMeshes
                    if (!level.subMeshes.empty())
                        return level.subMeshes;


                    // Kein SubMesh:
                    // kompletten LOD-Indexbuffer als ein SubMesh
                    // behandeln.
                    return
                    {
                        SubMesh
                        {
                            0,
                            static_cast<uint32_t>(
                                level.indices.size()),
                            -1
                        }
                    };
                }


                // ------------------------------------------------------------
                // Triangle des aktiven LOD abfragen
                // ------------------------------------------------------------

                bool GetRenderTriangleIndices(
                    const Vec3& cameraPosition,
                    size_t triIdx,
                    uint32_t& i0,
                    uint32_t& i1,
                    uint32_t& i2) const
                {
                    const auto& indices =
                        GetRenderIndices(
                            cameraPosition);


                    size_t base =
                        triIdx * 3;


                    if (base + 2 >=
                        indices.size())
                    {
                        return false;
                    }


                    i0 = indices[base + 0];
                    i1 = indices[base + 1];
                    i2 = indices[base + 2];


                    return true;
                }


                // ------------------------------------------------------------
                // Welches LOD wird gerade benutzt?
                //
                // Praktisch für Debugging / ImGui.
                // ------------------------------------------------------------

                size_t GetActiveLOD(
                    const Vec3& cameraPosition) const
                {
                    return SelectLOD(
                        cameraPosition);
                }


                // ------------------------------------------------------------
                // Aktuelle LOD-Distanz
                // ------------------------------------------------------------

                T GetActiveLODDistance(
                    const Vec3& cameraPosition) const
                {
                    size_t lod =
                        SelectLOD(
                            cameraPosition);


                    if (lod == 0)
                        return static_cast<T>(0);


                    return
                        m_lods[lod - 1].distance;
                }


                // ============================================================
                // Normalen
                // ============================================================

                void ComputeSmoothNormals()
                {
                    if (m_vertices.empty() ||
                        m_indices.empty())
                        return;


                    for (auto& v : m_vertices)
                    {
                        v.normal() =
                            math::Matrix<3, 1, T>(
                                0, 0, 0);
                    }


                    for (size_t t = 0;
                        t < TriangleCount();
                        ++t)
                    {
                        uint32_t i0, i1, i2;

                        if (!GetTriangleIndices(
                            t,
                            i0,
                            i1,
                            i2))
                        {
                            break;
                        }


                        const Vec3& p0 =
                            m_vertices[i0].world();

                        const Vec3& p1 =
                            m_vertices[i1].world();

                        const Vec3& p2 =
                            m_vertices[i2].world();


                        Vec3 e1 = p1 - p0;
                        Vec3 e2 = p2 - p0;

                        Vec3 n =
                            Cross(e1, e2);


                        m_vertices[i0].normal() += n;
                        m_vertices[i1].normal() += n;
                        m_vertices[i2].normal() += n;
                    }


                    for (auto& v : m_vertices)
                    {
                        T len =
                            Length(v.normal());


                        if (len >
                            static_cast<T>(1e-8))
                        {
                            v.normal() =
                                v.normal() *
                                (static_cast<T>(1) / len);
                        }
                    }


                    m_normalsDirty = false;
                }


                // ============================================================
                // Flat Normals
                // ============================================================
                
                void ComputeFlatNormals()
                {
                    if (m_vertices.empty() ||
                        m_indices.empty())
                        return;

                    std::vector<Vertex> newVerts;
                    std::vector<Vec2>   newUVs;
                    std::vector<uint32_t> newIndices;

                    newVerts.reserve(m_indices.size());
                    newUVs.reserve(m_indices.size());
                    newIndices.reserve(m_indices.size());

                    for (size_t t = 0;
                        t < TriangleCount();
                        ++t)
                    {
                        uint32_t i0, i1, i2;

                        if (!GetTriangleIndices(t, i0, i1, i2))
                            break;

                        Vertex v0 = m_vertices[i0];
                        Vertex v1 = m_vertices[i1];
                        Vertex v2 = m_vertices[i2];

                        Vec3 e1 =
                            v1.world() - v0.world();

                        Vec3 e2 =
                            v2.world() - v0.world();

                        Vec3 n =
                            Cross(e1, e2);

                        T len = Length(n);

                        if (len > static_cast<T>(1e-8))
                        {
                            n =
                                n *
                                (static_cast<T>(1) / len);
                        }

                        v0.normal() = n;
                        v1.normal() = n;
                        v2.normal() = n;

                        uint32_t base =
                            static_cast<uint32_t>(newVerts.size());

                        newVerts.push_back(v0);
                        newVerts.push_back(v1);
                        newVerts.push_back(v2);

                        // ------------------------------------------------------------
                        // UVs des ursprünglichen Dreiecks übernehmen
                        // ------------------------------------------------------------

                        newUVs.push_back(GetUV(i0));
                        newUVs.push_back(GetUV(i1));
                        newUVs.push_back(GetUV(i2));

                        newIndices.push_back(base + 0);
                        newIndices.push_back(base + 1);
                        newIndices.push_back(base + 2);
                    }

                    m_vertices = std::move(newVerts);
                    m_uvs = std::move(newUVs);
                    m_indices = std::move(newIndices);

                    m_normalsDirty = false;

                    // LODs sind wegen der neuen Vertexindizes ungültig
                    m_lods.clear();

                    MarkGeometryDirty();
                }
                
                /*
                void ComputeFlatNormals()
                {
                    if (m_vertices.empty() ||
                        m_indices.empty())
                        return;


                    std::vector<Vertex> newVerts;
                    std::vector<uint32_t> newIndices;


                    newVerts.reserve(
                        m_indices.size());

                    newIndices.reserve(
                        m_indices.size());


                    for (size_t t = 0;
                        t < TriangleCount();
                        ++t)
                    {
                        uint32_t i0, i1, i2;


                        if (!GetTriangleIndices(
                            t,
                            i0,
                            i1,
                            i2))
                        {
                            break;
                        }


                        Vertex v0 =
                            m_vertices[i0];

                        Vertex v1 =
                            m_vertices[i1];

                        Vertex v2 =
                            m_vertices[i2];


                        Vec3 e1 =
                            v1.world() -
                            v0.world();

                        Vec3 e2 =
                            v2.world() -
                            v0.world();


                        Vec3 n =
                            Cross(e1, e2);


                        T len =
                            Length(n);


                        if (len >
                            static_cast<T>(1e-8))
                        {
                            n =
                                n *
                                (static_cast<T>(1) / len);
                        }


                        v0.normal() = n;
                        v1.normal() = n;
                        v2.normal() = n;


                        uint32_t base =
                            static_cast<uint32_t>(
                                newVerts.size());


                        newVerts.push_back(v0);
                        newVerts.push_back(v1);
                        newVerts.push_back(v2);


                        newIndices.push_back(
                            base + 0);

                        newIndices.push_back(
                            base + 1);

                        newIndices.push_back(
                            base + 2);
                    }


                    m_vertices =
                        std::move(newVerts);

                    m_indices =
                        std::move(newIndices);


                    m_normalsDirty = false;


                    // Da sich der Vertexbuffer geändert hat,
                    // sind bestehende LOD-Indexbuffer eventuell
                    // nicht mehr gültig.
                    //
                    // Deshalb sicherheitshalber löschen.
                    m_lods.clear();
                }
                */

                bool NormalsDirty() const
                {
                    return m_normalsDirty;
                }


                // ============================================================
                // Transformation
                // ============================================================

                Vec3 TransformPoint(
                    const Vec3& localPoint) const
                {
                    Vec3 scaled(
                        localPoint.x() *
                        this->Scale().x(),

                        localPoint.y() *
                        this->Scale().y(),

                        localPoint.z() *
                        this->Scale().z());


                    Vec3 rotated =
                        RotateEulerXYZ(
                            scaled,
                            this->Rotation());


                    return rotated +
                        this->Position();
                }


                Vec3 TransformNormal(
                    const Vec3& localNormal) const
                {
                    Vec3 s =
                        this->Scale();


                    Vec3 scaled(
                        s.x() != 0
                        ? localNormal.x() / s.x()
                        : localNormal.x(),

                        s.y() != 0
                        ? localNormal.y() / s.y()
                        : localNormal.y(),

                        s.z() != 0
                        ? localNormal.z() / s.z()
                        : localNormal.z());


                    Vec3 rotated =
                        RotateEulerXYZ(
                            scaled,
                            this->Rotation());


                    T len =
                        Length(rotated);


                    return
                        len >
                        static_cast<T>(1e-8)
                        ?
                        rotated *
                        (static_cast<T>(1) / len)
                        :
                        rotated;
                }


                Vertex GetWorldVertex(
                    size_t idx) const
                {
                    Vertex out =
                        m_vertices[idx];


                    out.world() =
                        TransformPoint(
                            m_vertices[idx].world());


                    out.normal() =
                        TransformNormal(
                            m_vertices[idx].normal());


                    return out;
                }


                // ============================================================
                // Normalen Zugriff
                // ============================================================

                const Vec3& GetNormal(
                    size_t idx) const
                {
                    return m_vertices[idx].normal();
                }


                Vec3& GetNormal(size_t idx)
                {
                    return m_vertices[idx].normal();
                }


                Vec3 GetWorldNormal(
                    size_t idx) const
                {
                    return TransformNormal(
                        m_vertices[idx].normal());
                }


                std::vector<Vec3>
                    ExtractNormals() const
                {
                    std::vector<Vec3> out;

                    out.reserve(
                        m_vertices.size());


                    for (const auto& v :
                        m_vertices)
                    {
                        out.push_back(
                            v.normal());
                    }


                    return out;
                }


                std::vector<Vec3>
                    ExtractWorldNormals() const
                {
                    std::vector<Vec3> out;

                    out.reserve(
                        m_vertices.size());


                    for (const auto& v :
                        m_vertices)
                    {
                        out.push_back(
                            TransformNormal(
                                v.normal()));
                    }


                    return out;
                }


                // ============================================================
                // Triangle Normale
                // ============================================================

                Vec3 GetTriangleNormal(
                    size_t triIdx) const
                {
                    uint32_t i0, i1, i2;


                    if (!GetTriangleIndices(
                        triIdx,
                        i0,
                        i1,
                        i2))
                    {
                        return Vec3(0, 0, 0);
                    }


                    Vec3 n =
                        m_vertices[i0].normal() +
                        m_vertices[i1].normal() +
                        m_vertices[i2].normal();


                    T len =
                        Length(n);


                    return
                        len >
                        static_cast<T>(1e-8)
                        ?
                        n *
                        (static_cast<T>(1) / len)
                        :
                        n;
                }


                // ============================================================
                // Geometrische Face-Normale
                // ============================================================

                Vec3 ComputeFaceNormal(
                    size_t triIdx) const
                {
                    uint32_t i0, i1, i2;


                    if (!GetTriangleIndices(
                        triIdx,
                        i0,
                        i1,
                        i2))
                    {
                        return Vec3(0, 0, 0);
                    }


                    const Vec3& p0 =
                        m_vertices[i0].world();

                    const Vec3& p1 =
                        m_vertices[i1].world();

                    const Vec3& p2 =
                        m_vertices[i2].world();


                    Vec3 n =
                        (p1 - p0) %
                        (p2 - p0);


                    T len =
                        n.length();


                    return
                        len >
                        static_cast<T>(1e-8)
                        ?
                        n *
                        (static_cast<T>(1) / len)
                        :
                        n;
                }


                // ============================================================
                // RayPlaneLine
                // ============================================================

                ng::math::RayPlaneLine<3, T>
                    GetTrianglePlane(
                        size_t triIdx) const
                {
                    uint32_t i0, i1, i2;


                    if (!GetTriangleIndices(
                        triIdx,
                        i0,
                        i1,
                        i2))
                    {
                        return
                            ng::math::RayPlaneLine<3, T>();
                    }


                    const Vec3& p0 =
                        m_vertices[i0].world();

                    const Vec3& p1 =
                        m_vertices[i1].world();

                    const Vec3& p2 =
                        m_vertices[i2].world();


                    ng::math::RayPlaneLine<3, T>
                        plane(p0, p1, p2);


                    Vec3 expectedNormal =
                        ComputeFaceNormal(
                            triIdx);


                    if ((plane.normal() ^
                        expectedNormal) <
                        static_cast<T>(0))
                    {
                        plane.invert();
                    }


                    return plane;
                }


                // ============================================================
                // Vertex Normal Ray
                // ============================================================

                ng::math::RayPlaneLine<3, T>
                    GetVertexNormalRay(
                        size_t idx,
                        T length = static_cast<T>(1)) const
                {
                    const Vec3& pos =
                        m_vertices[idx].world();


                    Vec3 dir =
                        m_vertices[idx].normal() *
                        length;


                    return
                        ng::math::RayPlaneLine<3, T>(
                            pos,
                            dir);
                }


                // ============================================================
                // Face Normal Ray
                // ============================================================

                ng::math::RayPlaneLine<3, T>
                    GetFaceNormalRay(
                        size_t triIdx,
                        T length = static_cast<T>(1)) const
                {
                    uint32_t i0, i1, i2;


                    if (!GetTriangleIndices(
                        triIdx,
                        i0,
                        i1,
                        i2))
                    {
                        return
                            ng::math::RayPlaneLine<3, T>();
                    }


                    const Vec3& p0 =
                        m_vertices[i0].world();

                    const Vec3& p1 =
                        m_vertices[i1].world();

                    const Vec3& p2 =
                        m_vertices[i2].world();


                    Vec3 centroid =
                        (p0 + p1 + p2) *
                        (static_cast<T>(1) /
                            static_cast<T>(3));


                    Vec3 dir =
                        ComputeFaceNormal(
                            triIdx) *
                        length;


                    return
                        ng::math::RayPlaneLine<3, T>(
                            centroid,
                            dir);
                }


                // ============================================================
                // Bulk Triangle Planes
                // ============================================================

                std::vector<
                    ng::math::RayPlaneLine<3, T>>
                    ExtractTrianglePlanes() const
                {
                    std::vector<
                        ng::math::RayPlaneLine<3, T>>
                        planes;


                    planes.reserve(
                        TriangleCount());


                    for (size_t t = 0;
                        t < TriangleCount();
                        ++t)
                    {
                        planes.push_back(
                            GetTrianglePlane(t));
                    }


                    return planes;
                }


            protected:

                // ============================================================
                // Object3D Contract
                // ============================================================

                Vec3 GetHalfExtents() const override
                {
                    RecalculateLocalBoundsIfNeeded();

                    return
                        (m_localMax -
                            m_localMin) *
                        static_cast<T>(0.5);
                }


                Vec3 GetLocalCenterOffset() const override
                {
                    RecalculateLocalBoundsIfNeeded();

                    return
                        (m_localMax +
                            m_localMin) *
                        static_cast<T>(0.5);
                }


            private:

                // ============================================================
                // LOD intern sortieren
                // ============================================================

                void SortLODs()
                {
                    std::sort(
                        m_lods.begin(),
                        m_lods.end(),

                        [](const LODLevel& a,
                            const LODLevel& b)
                        {
                            return
                                a.distance <
                                b.distance;
                        });
                }


                // ============================================================
                // Inverse Transformation
                // ============================================================

                Vec3 InverseTransformPoint(
                    const Vec3& worldPoint) const
                {
                    Vec3 local =
                        worldPoint -
                        this->Position();


                    local =
                        InverseRotateEulerXYZ(
                            local,
                            this->Rotation());


                    Vec3 s =
                        this->Scale();


                    return Vec3(
                        s.x() != 0
                        ? local.x() / s.x()
                        : local.x(),

                        s.y() != 0
                        ? local.y() / s.y()
                        : local.y(),

                        s.z() != 0
                        ? local.z() / s.z()
                        : local.z());
                }


                Vec3 InverseTransformDirection(
                    const Vec3& worldDir) const
                {
                    Vec3 local =
                        InverseRotateEulerXYZ(
                            worldDir,
                            this->Rotation());


                    Vec3 s =
                        this->Scale();


                    return Vec3(
                        s.x() != 0
                        ? local.x() / s.x()
                        : local.x(),

                        s.y() != 0
                        ? local.y() / s.y()
                        : local.y(),

                        s.z() != 0
                        ? local.z() / s.z()
                        : local.z());
                }


                static Vec3
                    InverseRotateEulerXYZ(
                        const Vec3& p,
                        const Vec3& euler)
                {
                    T cx =
                        std::cos(-euler.x());

                    T sx =
                        std::sin(-euler.x());

                    T cy =
                        std::cos(-euler.y());

                    T sy =
                        std::sin(-euler.y());

                    T cz =
                        std::cos(-euler.z());

                    T sz =
                        std::sin(-euler.z());


                    T x1 =
                        p.x() * cz -
                        p.y() * sz;

                    T y1 =
                        p.x() * sz +
                        p.y() * cz;

                    T z1 =
                        p.z();


                    T x2 =
                        x1 * cy +
                        z1 * sy;

                    T y2 =
                        y1;

                    T z2 =
                        -x1 * sy +
                        z1 * cy;


                    T x3 =
                        x2;

                    T y3 =
                        y2 * cx -
                        z2 * sx;

                    T z3 =
                        y2 * sx +
                        z2 * cx;


                    return Vec3(
                        x3,
                        y3,
                        z3);
                }


                // ============================================================
                // Geometry Dirty
                // ============================================================

                void MarkGeometryDirty()
                {
                    m_localBoundsDirty = true;
                    m_normalsDirty = true;

                    this->MarkBoundsDirty();
                }


                // ============================================================
                // Bounds
                // ============================================================

                void RecalculateLocalBoundsIfNeeded() const
                {
                    if (!m_localBoundsDirty)
                        return;


                    if (m_vertices.empty())
                    {
                        m_localMin =
                            Vec3(0, 0, 0);

                        m_localMax =
                            Vec3(0, 0, 0);

                        m_localBoundsDirty =
                            false;

                        return;
                    }


                    Vec3 mn =
                        m_vertices[0].world();

                    Vec3 mx =
                        m_vertices[0].world();


                    for (const auto& v :
                        m_vertices)
                    {
                        const Vec3& p =
                            v.world();


                        mn =
                            Vec3(
                                std::min(
                                    mn.x(),
                                    p.x()),

                                std::min(
                                    mn.y(),
                                    p.y()),

                                std::min(
                                    mn.z(),
                                    p.z()));


                        mx =
                            Vec3(
                                std::max(
                                    mx.x(),
                                    p.x()),

                                std::max(
                                    mx.y(),
                                    p.y()),

                                std::max(
                                    mx.z(),
                                    p.z()));
                    }


                    m_localMin = mn;
                    m_localMax = mx;

                    m_localBoundsDirty =
                        false;
                }


                // ============================================================
                // Euler Rotation
                // Rz * Ry * Rx
                // ============================================================

                static Vec3
                    RotateEulerXYZ(
                        const Vec3& p,
                        const Vec3& euler)
                {
                    T cx =
                        std::cos(euler.x());

                    T sx =
                        std::sin(euler.x());

                    T cy =
                        std::cos(euler.y());

                    T sy =
                        std::sin(euler.y());

                    T cz =
                        std::cos(euler.z());

                    T sz =
                        std::sin(euler.z());


                    // Rotation X
                    T x1 =
                        p.x();

                    T y1 =
                        p.y() * cx -
                        p.z() * sx;

                    T z1 =
                        p.y() * sx +
                        p.z() * cx;


                    // Rotation Y
                    T x2 =
                        x1 * cy +
                        z1 * sy;

                    T y2 =
                        y1;

                    T z2 =
                        -x1 * sy +
                        z1 * cy;


                    // Rotation Z
                    T x3 =
                        x2 * cz -
                        y2 * sz;

                    T y3 =
                        x2 * sz +
                        y2 * cz;

                    T z3 =
                        z2;


                    return Vec3(
                        x3,
                        y3,
                        z3);
                }


                // ============================================================
                // Cross
                // ============================================================

                static Vec3 Cross(
                    const Vec3& a,
                    const Vec3& b)
                {
                    return Vec3(
                        a.y() * b.z() -
                        a.z() * b.y(),

                        a.z() * b.x() -
                        a.x() * b.z(),

                        a.x() * b.y() -
                        a.y() * b.x());
                }


                // ============================================================
                // Length
                // ============================================================

                static T Length(
                    const Vec3& v)
                {
                    return std::sqrt(
                        v.x() * v.x() +
                        v.y() * v.y() +
                        v.z() * v.z());
                }


            private:

                // ============================================================
                // Original Geometrie = LOD0
                // ============================================================
                std::vector<Vec2> m_uvs;
                    
                std::vector<Vertex>
                    m_vertices;

                std::vector<uint32_t>
                    m_indices;

                std::vector<SubMesh>
                    m_subMeshes;


                // ============================================================
                // Zusätzliche LOD-Stufen
                // ============================================================

                std::vector<LODLevel>
                    m_lods;


                // LOD standardmäßig AUS
                bool m_lodEnabled = false;


                // ============================================================
                // Bounds
                // ============================================================

                mutable Vec3
                    m_localMin{ 0, 0, 0 };

                mutable Vec3
                    m_localMax{ 0, 0, 0 };

                mutable bool
                    m_localBoundsDirty = true;


                // ============================================================
                // Normalen
                // ============================================================

                bool
                    m_normalsDirty = true;

                static void ComputeBarycentric(
                    const Vec3& p, const Vec3& a, const Vec3& b, const Vec3& c,
                    T& outU, T& outV, T& outW)
                {
                    Vec3 v0 = b - a, v1 = c - a, v2 = p - a;
                    T d00 = v0 ^ v0, d01 = v0 ^ v1, d11 = v1 ^ v1;
                    T d20 = v2 ^ v0, d21 = v2 ^ v1;
                    T denom = d00 * d11 - d01 * d01;
                    if (std::abs(denom) < static_cast<T>(1e-12))
                    {
                        outU = static_cast<T>(1); outV = 0; outW = 0;
                        return;
                    }
                    T v = (d11 * d20 - d01 * d21) / denom;
                    T w = (d00 * d21 - d01 * d20) / denom;
                    outU = static_cast<T>(1) - v - w;
                    outV = v;
                    outW = w;
                }
            };
        }
    }
}