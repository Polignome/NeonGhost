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
#include <type_traits>
#include "Global.h"
namespace ng
{
    namespace renderer
    {
        namespace objects
        {
            template<typename T>
            class Mesh : public Object3D<T>
            {
                static_assert(
                    std::is_floating_point<T>::value,
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
                    bool hit = false;
                    T distance =
                        std::numeric_limits<T>::max();
                    Vec3 point{ 0, 0, 0 };
                    Vec3 normal{ 0, 0, 0 };
                    uint32_t triangleIndex = 0;
                    uint32_t i0 = 0;
                    uint32_t i1 = 0;
                    uint32_t i2 = 0;
                    Vec2 uv{ 0, 0 };
                    T baryU = 0;
                    T baryV = 0;
                    T baryW = 0;
                };
                // ============================================================
                // UV Mapping
                // ============================================================
                enum class UVAxis
                {
                    X,
                    Y,
                    Z
                };
                // ============================================================
                // SubMesh
                // ============================================================
                struct SubMesh
                {
                    uint32_t indexOffset = 0;
                    uint32_t indexCount = 0;
                    int materialId = -1;
                };
                // ============================================================
                // LOD
                // ============================================================
                struct LODDescription
                {
                    T distance;
                    T ratio;
                };
                struct LODLevel
                {
                    T distance =
                        static_cast<T>(0);
                    T hysteresis =
                        static_cast<T>(0);
                    std::vector<uint32_t> indices;
                    std::vector<SubMesh> subMeshes;
                };
                // ============================================================
                // Konstruktor / Destruktor
                // ============================================================
                Mesh() = default;
                virtual ~Mesh() = default;
                // ============================================================
                // Ray im lokalen Raum
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
                        direction *
                        (static_cast<T>(1) / dirLen);
                    ng::math::RayPlaneLine<3, T>
                        ray(origin, dirNorm);
                    for (size_t t = 0;
                        t < TriangleCount();
                        ++t)
                    {
                        uint32_t i0;
                        uint32_t i1;
                        uint32_t i2;
                        if (!GetTriangleIndices(
                            t,
                            i0,
                            i1,
                            i2))
                        {
                            continue;
                        }
                        if (i0 >= m_vertices.size() ||
                            i1 >= m_vertices.size() ||
                            i2 >= m_vertices.size())
                        {
                            continue;
                        }
                        const Vec3& p0 =
                            m_vertices[i0].world();
                        const Vec3& p1 =
                            m_vertices[i1].world();
                        const Vec3& p2 =
                            m_vertices[i2].world();
                        // ----------------------------------------------------
                        // Backface Culling
                        // ----------------------------------------------------
                        if (backfaceCull)
                        {
                            Vec3 n =
                                Cross(
                                    p1 - p0,
                                    p2 - p0);
                            if ((n ^ dirNorm) >=
                                static_cast<T>(0))
                            {
                                continue;
                            }
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
                            T dist =
                                static_cast<T>(time);
                            if (dist >= static_cast<T>(0) &&
                                dist < best.distance)
                            {
                                best.hit = true;
                                best.distance = dist;
                                best.point =
                                    origin +
                                    dirNorm * dist;
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
                    // --------------------------------------------------------
                    // UV nach dem Treffer interpolieren
                    // --------------------------------------------------------
                    if (best.hit)
                    {
                        const Vec3& p0 =
                            m_vertices[best.i0].world();
                        const Vec3& p1 =
                            m_vertices[best.i1].world();
                        const Vec3& p2 =
                            m_vertices[best.i2].world();
                        ComputeBarycentric(
                            best.point,
                            p0,
                            p1,
                            p2,
                            best.baryU,
                            best.baryV,
                            best.baryW);
                        best.uv =
                            InterpolateUV(
                                best.triangleIndex,
                                best.baryU,
                                best.baryV,
                                best.baryW);
                    }
                    return best;
                }
                // ============================================================
                // UV
                // ============================================================
                uint32_t AddVertex(
                    const Vertex& vertex,
                    T u,
                    T vcoord)
                {
                    uint32_t idx =
                        AddVertex(vertex);
                    SetUV(
                        idx,
                        u,
                        vcoord);
                    return idx;
                }
                uint32_t AddVertex(
                    T x,
                    T y,
                    T z,
                    T u,
                    T vcoord)
                {
                    uint32_t idx =
                        AddVertex(
                            x,
                            y,
                            z);
                    SetUV(
                        idx,
                        u,
                        vcoord);
                    return idx;
                }
                void SetUV(
                    size_t idx,
                    T u,
                    T v)
                {
                    EnsureUVs();
                    if (idx >= m_uvs.size())
                        return;
                    m_uvs[idx] =
                        Vec2(u, v);
                }
                void SetUV(
                    size_t idx,
                    const Vec2& uv)
                {
                    SetUV(
                        idx,
                        uv.x(),
                        uv.y());
                }
                Vec2 GetUV(
                    size_t idx) const
                {
                    if (idx >= m_uvs.size())
                        return Vec2(0, 0);
                    return m_uvs[idx];
                }
                const std::vector<Vec2>& UVs() const
                {
                    return m_uvs;
                }
                std::vector<Vec2>& UVsMutable()
                {
                    EnsureUVs();
                    return m_uvs;
                }
                size_t UVCount() const
                {
                    return m_uvs.size();
                }
                bool UVsValid() const
                {
                    return
                        m_uvs.size() ==
                        m_vertices.size();
                }
                bool HasUVs() const
                {
                    return
                        !m_uvs.empty() &&
                        m_uvs.size() ==
                        m_vertices.size();
                }
                void EnsureUVs()
                {
                    if (m_uvs.size() <
                        m_vertices.size())
                    {
                        m_uvs.resize(
                            m_vertices.size(),
                            Vec2(0, 0));
                    }
                    else if (m_uvs.size() >
                        m_vertices.size())
                    {
                        m_uvs.resize(
                            m_vertices.size());
                    }
                }
                bool GetTriangleUV(
                    size_t triIdx,
                    Vec2& uv0,
                    Vec2& uv1,
                    Vec2& uv2) const
                {
                    uint32_t i0;
                    uint32_t i1;
                    uint32_t i2;
                    if (!GetTriangleIndices(
                        triIdx,
                        i0,
                        i1,
                        i2))
                    {
                        return false;
                    }
                    if (i0 >= m_uvs.size() ||
                        i1 >= m_uvs.size() ||
                        i2 >= m_uvs.size())
                    {
                        return false;
                    }
                    uv0 = m_uvs[i0];
                    uv1 = m_uvs[i1];
                    uv2 = m_uvs[i2];
                    return true;
                }
                Vec2 InterpolateUV(
                    size_t triIdx,
                    T baryU,
                    T baryV,
                    T baryW) const
                {
                    Vec2 uv0;
                    Vec2 uv1;
                    Vec2 uv2;
                    if (!GetTriangleUV(
                        triIdx,
                        uv0,
                        uv1,
                        uv2))
                    {
                        return Vec2(0, 0);
                    }
                    return
                        uv0 * baryU +
                        uv1 * baryV +
                        uv2 * baryW;
                }
                // ============================================================
                // Planar Mapping
                // ============================================================
                void GenerateUVsPlanar(
                    UVAxis axis = UVAxis::Y,
                    T tileU = static_cast<T>(1),
                    T tileV = static_cast<T>(1))
                {
                    if (m_vertices.empty())
                        return;
                    RecalculateLocalBoundsIfNeeded();
                    Vec3 size =
                        m_localMax -
                        m_localMin;
                    m_uvs.resize(
                        m_vertices.size());
                    for (size_t i = 0;
                        i < m_vertices.size();
                        ++i)
                    {
                        const Vec3& p =
                            m_vertices[i].world();
                        T a;
                        T b;
                        T minA;
                        T minB;
                        T sizeA;
                        T sizeB;
                        switch (axis)
                        {
                        case UVAxis::X:
                            a = p.y();
                            b = p.z();
                            minA =
                                m_localMin.y();
                            minB =
                                m_localMin.z();
                            sizeA =
                                std::max(
                                    size.y(),
                                    static_cast<T>(1e-8));
                            sizeB =
                                std::max(
                                    size.z(),
                                    static_cast<T>(1e-8));
                            break;
                        case UVAxis::Z:
                            a = p.x();
                            b = p.y();
                            minA =
                                m_localMin.x();
                            minB =
                                m_localMin.y();
                            sizeA =
                                std::max(
                                    size.x(),
                                    static_cast<T>(1e-8));
                            sizeB =
                                std::max(
                                    size.y(),
                                    static_cast<T>(1e-8));
                            break;
                        case UVAxis::Y:
                        default:
                            a = p.x();
                            b = p.z();
                            minA =
                                m_localMin.x();
                            minB =
                                m_localMin.z();
                            sizeA =
                                std::max(
                                    size.x(),
                                    static_cast<T>(1e-8));
                            sizeB =
                                std::max(
                                    size.z(),
                                    static_cast<T>(1e-8));
                            break;
                        }
                        m_uvs[i] =
                            Vec2(
                                ((a - minA) / sizeA) *
                                tileU,
                                ((b - minB) / sizeB) *
                                tileV);
                    }
                }
                // ============================================================
                // Box Mapping
                //
                // Wichtig:
                // Vertices werden pro Dreieck dupliziert.
                // Dadurch kann jede Fläche eigene UVs besitzen.
                // ============================================================
                void GenerateUVsBoxMapped(
                    T tileScale = static_cast<T>(1))
                {
                    if (m_vertices.empty() ||
                        m_indices.empty())
                    {
                        return;
                    }
                    RecalculateLocalBoundsIfNeeded();
                    Vec3 size =
                        m_localMax -
                        m_localMin;
                    T sx =
                        std::max(
                            size.x(),
                            static_cast<T>(1e-8));
                    T sy =
                        std::max(
                            size.y(),
                            static_cast<T>(1e-8));
                    T sz =
                        std::max(
                            size.z(),
                            static_cast<T>(1e-8));
                    std::vector<Vertex>
                        newVerts;
                    std::vector<Vec2>
                        newUVs;
                    std::vector<uint32_t>
                        newIndices;
                    newVerts.reserve(
                        m_indices.size());
                    newUVs.reserve(
                        m_indices.size());
                    newIndices.reserve(
                        m_indices.size());
                    for (size_t t = 0;
                        t < TriangleCount();
                        ++t)
                    {
                        uint32_t i0;
                        uint32_t i1;
                        uint32_t i2;
                        if (!GetTriangleIndices(
                            t,
                            i0,
                            i1,
                            i2))
                        {
                            break;
                        }
                        if (i0 >= m_vertices.size() ||
                            i1 >= m_vertices.size() ||
                            i2 >= m_vertices.size())
                        {
                            continue;
                        }
                        Vertex v0 =
                            m_vertices[i0];
                        Vertex v1 =
                            m_vertices[i1];
                        Vertex v2 =
                            m_vertices[i2];
                        Vec3 n =
                            ComputeFaceNormal(t);
                        T ax =
                            std::abs(n.x());
                        T ay =
                            std::abs(n.y());
                        T az =
                            std::abs(n.z());
                        auto project =
                            [&](const Vec3& p) -> Vec2
                            {
                                if (ax >= ay &&
                                    ax >= az)
                                {
                                    return Vec2(
                                        ((p.z() -
                                            m_localMin.z()) / sz)
                                        * tileScale,
                                        ((p.y() -
                                            m_localMin.y()) / sy)
                                        * tileScale);
                                }
                                if (ay >= ax &&
                                    ay >= az)
                                {
                                    return Vec2(
                                        ((p.x() -
                                            m_localMin.x()) / sx)
                                        * tileScale,
                                        ((p.z() -
                                            m_localMin.z()) / sz)
                                        * tileScale);
                                }
                                return Vec2(
                                    ((p.x() -
                                        m_localMin.x()) / sx)
                                    * tileScale,
                                    ((p.y() -
                                        m_localMin.y()) / sy)
                                    * tileScale);
                            };
                        uint32_t base =
                            static_cast<uint32_t>(
                                newVerts.size());
                        newVerts.push_back(v0);
                        newVerts.push_back(v1);
                        newVerts.push_back(v2);
                        newUVs.push_back(
                            project(v0.world()));
                        newUVs.push_back(
                            project(v1.world()));
                        newUVs.push_back(
                            project(v2.world()));
                        newIndices.push_back(
                            base + 0);
                        newIndices.push_back(
                            base + 1);
                        newIndices.push_back(
                            base + 2);
                    }
                    m_vertices =
                        std::move(newVerts);
                    m_uvs =
                        std::move(newUVs);
                    m_indices =
                        std::move(newIndices);
                    EnsureUVs();
                    m_normalsDirty = true;
                    MarkGeometryDirty();
                    m_lods.clear();
                    m_subMeshes.clear();
                }
                // ============================================================
                // Spherical Mapping
                // ============================================================
                void GenerateUVsSpherical(
                    T tileU = static_cast<T>(1),
                    T tileV = static_cast<T>(1))
                {
                    if (m_vertices.empty())
                        return;
                    const T pi =
                        static_cast<T>(
                            3.14159265358979323846);
                    Vec3 center =
                        GetLocalCenterOffset();
                    m_uvs.resize(
                        m_vertices.size());
                    for (size_t i = 0;
                        i < m_vertices.size();
                        ++i)
                    {
                        Vec3 p =
                            m_vertices[i].world() -
                            center;
                        T len =
                            Length(p);
                        if (len <
                            static_cast<T>(1e-8))
                        {
                            m_uvs[i] =
                                Vec2(0, 0);
                            continue;
                        }
                        Vec3 n =
                            p *
                            (static_cast<T>(1) / len);
                        T u =
                            (std::atan2(
                                n.z(),
                                n.x()) /
                                (static_cast<T>(2) * pi)
                                +
                                static_cast<T>(0.5))
                            * tileU;
                        T v =
                            (std::asin(
                                std::clamp(
                                    n.y(),
                                    static_cast<T>(-1),
                                    static_cast<T>(1))) /
                                pi
                                +
                                static_cast<T>(0.5))
                            * tileV;
                        m_uvs[i] =
                            Vec2(u, v);
                    }
                }
                // ============================================================
                // Cylindrical Mapping
                // ============================================================
                void GenerateUVsCylindrical(
                    UVAxis axis = UVAxis::Y,
                    T tileU = static_cast<T>(1),
                    T tileV = static_cast<T>(1))
                {
                    if (m_vertices.empty())
                        return;
                    RecalculateLocalBoundsIfNeeded();
                    Vec3 size =
                        m_localMax -
                        m_localMin;
                    Vec3 center =
                        GetLocalCenterOffset();
                    const T pi =
                        static_cast<T>(
                            3.14159265358979323846);
                    m_uvs.resize(
                        m_vertices.size());
                    for (size_t i = 0;
                        i < m_vertices.size();
                        ++i)
                    {
                        Vec3 p =
                            m_vertices[i].world() -
                            center;
                        T a1;
                        T a2;
                        T along;
                        T axisSize;
                        T minAlong;
                        switch (axis)
                        {
                        case UVAxis::X:
                            a1 = p.y();
                            a2 = p.z();
                            along = p.x();
                            axisSize =
                                std::max(
                                    size.x(),
                                    static_cast<T>(1e-8));
                            minAlong =
                                m_localMin.x() -
                                center.x();
                            break;
                        case UVAxis::Z:
                            a1 = p.x();
                            a2 = p.y();
                            along = p.z();
                            axisSize =
                                std::max(
                                    size.z(),
                                    static_cast<T>(1e-8));
                            minAlong =
                                m_localMin.z() -
                                center.z();
                            break;
                        case UVAxis::Y:
                        default:
                            a1 = p.x();
                            a2 = p.z();
                            along = p.y();
                            axisSize =
                                std::max(
                                    size.y(),
                                    static_cast<T>(1e-8));
                            minAlong =
                                m_localMin.y() -
                                center.y();
                            break;
                        }
                        T u =
                            (std::atan2(
                                a2,
                                a1) /
                                (static_cast<T>(2) * pi)
                                +
                                static_cast<T>(0.5))
                            * tileU;
                        T v =
                            ((along - minAlong) /
                                axisSize)
                            * tileV;
                        m_uvs[i] =
                            Vec2(u, v);
                    }
                }
                // ============================================================
                // Ray im Weltraum
                // ============================================================
                RayHit RaycastWorld(
                    const Vec3& worldOrigin,
                    const Vec3& worldDirection,
                    bool backfaceCull = true) const
                {
                    Vec3 localOrigin =
                        InverseTransformPoint(
                            worldOrigin);
                    Vec3 localDirection =
                        InverseTransformDirection(
                            worldDirection);
                    RayHit hit =
                        RaycastLocal(
                            localOrigin,
                            localDirection,
                            backfaceCull);
                    if (hit.hit)
                    {
                        hit.point =
                            TransformPoint(
                                hit.point);
                        hit.normal =
                            TransformNormal(
                                hit.normal);
                        hit.distance =
                            Length(
                                hit.point -
                                worldOrigin);
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
                // Normalfarbe
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
                        static_cast<T>(0.5) +
                        normal *
                        static_cast<T>(0.5);
                    ng::math::Matrix<4, 1, T>
                        color4;
                    T f =
                        static_cast<T>(0.299) *
                        color3.x() +
                        static_cast<T>(0.587) *
                        color3.y() +
                        static_cast<T>(0.114) *
                        color3.z();
                    color4.x() =
                        f * red;
                    color4.y() =
                        f * green;
                    color4.z() =
                        f * blue;
                    color4.w() =
                        alpha;
                    return color4;
                }
                // ============================================================
                // Geometrie
                // ============================================================
                void Clear()
                {
                    m_vertices.clear();
                    m_indices.clear();
                    m_subMeshes.clear();
                    m_uvs.clear();
                    m_lods.clear();
                    MarkGeometryDirty();
                }
                void ReserveVertices(
                    size_t n)
                {
                    m_vertices.reserve(n);
                    m_uvs.reserve(n);
                }
                void ReserveIndices(
                    size_t n)
                {
                    m_indices.reserve(n);
                }
                uint32_t AddVertex(
                    const Vertex& v)
                {
                    m_vertices.push_back(v);
                    // BUGFIX: die urspruengliche Position "Vec2(0, 0)" hat
                    // v.texture() komplett ignoriert - jede UV, die ein
                    // Aufrufer (z.B. MeshGenerator::CreateSphere/-Plane/
                    // -Cone) direkt am Vertex gesetzt hatte, ging dadurch
                    // sofort verloren, sobald RenderMesh() ueber
                    // object.GetUV(idx) den (immer (0,0) gebliebenen)
                    // m_uvs-Eintrag zurueckschrieb. m_uvs ist der einzige
                    // UV-Speicher, den Rendering/GenerateUVsXxx/SetUV/GetUV
                    // tatsaechlich verwenden - v.texture() dient nur noch
                    // als Ausgangswert dafuer.
                    m_uvs.push_back(
                        Vec2(v.texture().x(), v.texture().y()));
                    MarkGeometryDirty();
                    return static_cast<uint32_t>(
                        m_vertices.size() - 1);
                }
                uint32_t AddVertex(
                    T x,
                    T y,
                    T z)
                {
                    return AddVertex(
                        Vertex(x, y, z));
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
                    m_indices =
                        std::move(indices);
                    m_normalsDirty = true;
                }
                // ============================================================
                // Vertexzugriff
                // ============================================================
                std::vector<Vertex>&
                    VerticesMutable()
                {
                    EnsureUVs();
                    return m_vertices;
                }
                const std::vector<Vertex>&
                    Vertices() const
                {
                    return m_vertices;
                }
                const std::vector<uint32_t>&
                    Indices() const
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
                    return
                        m_indices.size() / 3;
                }
                const Vertex&
                    GetVertex(size_t idx) const
                {
                    return m_vertices[idx];
                }
                Vertex&
                    GetVertex(size_t idx)
                {
                    return m_vertices[idx];
                }
                bool GetTriangleIndices(
                    size_t triIdx,
                    uint32_t& i0,
                    uint32_t& i1,
                    uint32_t& i2) const
                {
                    size_t base =
                        triIdx * 3;
                    if (base + 2 >=
                        m_indices.size())
                    {
                        return false;
                    }
                    i0 =
                        m_indices[base + 0];
                    i1 =
                        m_indices[base + 1];
                    i2 =
                        m_indices[base + 2];
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
                        SubMesh{
                            0,
                            static_cast<uint32_t>(
                                m_indices.size()),
                            -1
                        }
                    };
                }
                // ============================================================
                // LOD
                // ============================================================
                void EnableLOD(
                    bool enable = true)
                {
                    m_lodEnabled = enable;
                }
                bool LODEnabled() const
                {
                    return m_lodEnabled;
                }
                void ClearLODs()
                {
                    m_lods.clear();
                    m_currentLOD = 0;
                }
                size_t LODCount() const
                {
                    return m_lods.size();
                }
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
                void SetLODHysteresis(
                    size_t lodIndex,
                    T hysteresis)
                {
                    if (lodIndex >=
                        m_lods.size())
                    {
                        return;
                    }
                    m_lods[lodIndex].hysteresis =
                        std::max(
                            static_cast<T>(0),
                            hysteresis);
                }
                bool SetLODDistance(
                    size_t lodIndex,
                    T distance)
                {
                    if (lodIndex >=
                        m_lods.size())
                    {
                        return false;
                    }
                    m_lods[lodIndex].distance =
                        distance;
                    SortLODs();
                    return true;
                }
                const LODLevel*
                    GetLOD(size_t lodIndex) const
                {
                    if (lodIndex >=
                        m_lods.size())
                    {
                        return nullptr;
                    }
                    return &m_lods[lodIndex];
                }
                // ------------------------------------------------------------
                // BUGFIX/NEU: Hysterese-Unterstuetzung.
                //
                // Ohne Hysterese wuerde ein Objekt an der LOD-Grenze
                // "flackern" (staendiger Wechsel zwischen zwei Detailstufen),
                // sobald die Kameradistanz um die Schwelle herum schwankt.
                //
                // m_currentLOD merkt sich das zuletzt gewaehlte LOD (mutable,
                // da SelectLOD() const ist). Steht das Objekt bereits auf
                // einem HOEHEREN LOD (groessere Distanz-Stufe) als Level i,
                // muss die Distanz die Schwelle von Level i um "hysteresis"
                // UNTERSCHREITEN, um wieder zurueckzuschalten - das erzeugt
                // eine kleine "tote Zone" um jede LOD-Grenze.
                // ------------------------------------------------------------
                size_t SelectLOD(
                    const Vec3& cameraPosition) const
                {
                    if (!m_lodEnabled || m_lods.empty())
                    {
                        m_currentLOD = 0;
                        return 0;
                    }
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
                        T threshold =
                            m_lods[i].distance;
                        if (m_currentLOD > i)
                        {
                            threshold -=
                                m_lods[i].hysteresis;
                        }
                        if (distance >= threshold)
                        {
                            selectedLOD =
                                i + 1;
                        }
                        else
                        {
                            break;
                        }
                    }
                    m_currentLOD = selectedLOD;
                    return selectedLOD;
                }
                void BuildLODs(
                    const std::vector<LODDescription>& descriptions)
                {
                    m_lods.clear();
                    m_currentLOD = 0;
                    if (m_indices.empty() ||
                        m_vertices.empty())
                    {
                        return;
                    }
                    for (const auto& desc :
                        descriptions)
                    {
                        if (desc.ratio >=
                            static_cast<T>(1))
                        {
                            continue;
                        }
                        if (desc.ratio <=
                            static_cast<T>(0))
                        {
                            continue;
                        }
                        std::vector<uint32_t>
                            lodIndices =
                            BuildLODIndices(
                                desc.ratio);
                        if (lodIndices.empty())
                            continue;
                        AddLOD(
                            desc.distance,
                            lodIndices);
                    }
                }
                // ============================================================
                // LOD Indexbuffer erzeugen - Edge-Collapse mit Quadric Error
                // Metric (Garland/Heckbert), statt raeumlichem Grid-
                // Clustering.
                //
                // WARUM DER WECHSEL: reines Grid-Clustering kennt nur
                // Vertex-POSITIONEN, keine Mesh-Topologie. Bei niedrigem
                // ratio konnte eine Zelle groesser werden als eine ganze
                // Wuerfelseite -> alle 4 Eckvertices dieser Seite fielen in
                // denselben Cluster -> das Dreieck wurde komplett entartet
                // und verworfen -> die ganze Flaeche verschwand statt sich
                // graduell zu vereinfachen. Bei Kugeln (ungleichmaessige
                // Vertexdichte, an den Polen viel dichter) fuehrte eine
                // EINHEITLICHE Zellgroesse dazu, dass die dichten Pol-
                // Bereiche extrem ueberkomprimiert wurden - sichtbar als
                // "in sich zusammenfallen".
                //
                // Edge-Collapse arbeitet dagegen NUR entlang echter
                // Mesh-Kanten und bewertet jede Kante ueber eine Fehler-
                // Quadrik (Summe der Ebenengleichungen aller angrenzenden
                // Dreiecke). Es wird immer die guenstigste Kante zuerst
                // kollabiert - scharfe Kanten/Ecken haben hohe Fehlerwerte
                // und bleiben dadurch automatisch bis zuletzt erhalten.
                //
                // Vereinfachung ggue. vollem QEM: der Kollaps-Zielpunkt ist
                // IMMER einer der beiden bestehenden Endpunkte (nie ein neu
                // berechneter Zwischenpunkt) - dadurch bleibt der
                // gemeinsame Vertex-/UV-Puffer ueber alle LOD-Stufen hinweg
                // gueltig, ohne dass neue Vertices eingefuegt werden
                // muessten (LOD-Level speichern nur Indizes, keinen
                // eigenen Vertexbuffer - siehe Klassenkommentar an m_uvs).
                // ============================================================
                std::vector<uint32_t>
                    BuildLODIndices(T ratio) const
                {
                    if (m_indices.empty() ||
                        m_vertices.empty())
                    {
                        return {};
                    }
                    if (ratio >=
                        static_cast<T>(1))
                    {
                        return m_indices;
                    }
                    const size_t vertexCount =
                        m_vertices.size();
                    const size_t triangleCount =
                        TriangleCount();
                    size_t targetTriangles =
                        static_cast<size_t>(
                            static_cast<T>(
                                triangleCount) *
                            ratio);
                    if (targetTriangles < 1)
                        targetTriangles = 1;

                    // --------------------------------------------------------
                    // 1) Pro-Vertex Fehler-Quadrik aus allen angrenzenden
                    //    Dreiecken aufsummieren.
                    // --------------------------------------------------------
                    std::vector<Quadric>
                        quadric(vertexCount);
                    for (size_t t = 0;
                        t < triangleCount;
                        ++t)
                    {
                        uint32_t i0, i1, i2;
                        if (!GetTriangleIndices(t, i0, i1, i2))
                            continue;
                        if (i0 >= vertexCount ||
                            i1 >= vertexCount ||
                            i2 >= vertexCount)
                            continue;
                        Quadric q =
                            PlaneQuadric(
                                m_vertices[i0].world(),
                                m_vertices[i1].world(),
                                m_vertices[i2].world());
                        quadric[i0] += q;
                        quadric[i1] += q;
                        quadric[i2] += q;
                    }

                    // --------------------------------------------------------
                    // 2) Eindeutige Kanten sammeln, Kollaps-Kosten bewerten.
                    //    Zielpunkt = der der beiden Endpunkte mit dem
                    //    niedrigeren Fehler (siehe Klassenkommentar oben).
                    // --------------------------------------------------------
                    struct EdgeCandidate
                    {
                        T cost;
                        uint32_t a;
                        uint32_t b;
                        bool targetIsA;
                    };
                    std::vector<EdgeCandidate> edges;
                    edges.reserve(m_indices.size());
                    std::unordered_set<uint64_t> seenEdges;
                    seenEdges.reserve(m_indices.size());

                    auto tryAddEdge =
                        [&](uint32_t a, uint32_t b)
                        {
                            if (a == b) return;
                            uint32_t lo = std::min(a, b);
                            uint32_t hi = std::max(a, b);
                            uint64_t key =
                                (static_cast<uint64_t>(lo) << 32) | hi;
                            if (!seenEdges.insert(key).second)
                                return;
                            Quadric qab =
                                quadric[lo] + quadric[hi];
                            T costA =
                                QuadricError(qab, m_vertices[lo].world());
                            T costB =
                                QuadricError(qab, m_vertices[hi].world());
                            bool targetIsA = costA <= costB;
                            edges.push_back(
                                EdgeCandidate{
                                    targetIsA ? costA : costB,
                                    lo, hi, targetIsA
                                });
                        };

                    for (size_t t = 0;
                        t < triangleCount;
                        ++t)
                    {
                        uint32_t i0, i1, i2;
                        if (!GetTriangleIndices(t, i0, i1, i2))
                            continue;
                        tryAddEdge(i0, i1);
                        tryAddEdge(i1, i2);
                        tryAddEdge(i2, i0);
                    }

                    std::sort(edges.begin(), edges.end(),
                        [](const EdgeCandidate& x, const EdgeCandidate& y)
                        {
                            return x.cost < y.cost;
                        });

                    // --------------------------------------------------------
                    // 3) Union-Find: guenstigste Kanten zuerst kollabieren,
                    //    bis die Ziel-Dreieckszahl erreicht ist. Der
                    //    resultierende Indexpuffer wird periodisch neu
                    //    aufgebaut, um den Fortschritt zu pruefen - guenstig
                    //    genug, da BuildLODs nur einmalig (z.B. beim Laden)
                    //    laeuft, nicht pro Frame.
                    // --------------------------------------------------------
                    std::vector<uint32_t> parent(vertexCount);
                    for (uint32_t i = 0; i < vertexCount; ++i)
                        parent[i] = i;
                    auto find =
                        [&](uint32_t x) -> uint32_t
                        {
                            while (parent[x] != x)
                            {
                                parent[x] = parent[parent[x]];
                                x = parent[x];
                            }
                            return x;
                        };

                    auto buildResult =
                        [&]() -> std::vector<uint32_t>
                        {
                            std::vector<uint32_t> out;
                            out.reserve(m_indices.size());
                            std::unordered_set<uint64_t> usedTriangles;
                            usedTriangles.reserve(triangleCount);
                            for (size_t t = 0;
                                t < triangleCount;
                                ++t)
                            {
                                uint32_t i0, i1, i2;
                                if (!GetTriangleIndices(t, i0, i1, i2))
                                    continue;
                                uint32_t a = find(i0);
                                uint32_t b = find(i1);
                                uint32_t c = find(i2);
                                if (a == b || b == c || a == c)
                                    continue; // entartet -> weglassen
                                uint32_t s0 = a, s1 = b, s2 = c;
                                if (s0 > s1) std::swap(s0, s1);
                                if (s1 > s2) std::swap(s1, s2);
                                if (s0 > s1) std::swap(s0, s1);
                                uint64_t key =
                                    (static_cast<uint64_t>(s0) << 42) |
                                    (static_cast<uint64_t>(s1) << 21) |
                                    static_cast<uint64_t>(s2);
                                if (!usedTriangles.insert(key).second)
                                    continue; // Duplikat -> weglassen
                                out.push_back(a);
                                out.push_back(b);
                                out.push_back(c);
                            }
                            return out;
                        };

                    std::vector<uint32_t> result = m_indices;
                    const size_t recheckInterval =
                        std::max<size_t>(
                            static_cast<size_t>(1),
                            edges.size() / 50);
                    for (size_t i = 0; i < edges.size(); ++i)
                    {
                        const EdgeCandidate& e = edges[i];
                        uint32_t ra = find(e.a);
                        uint32_t rb = find(e.b);
                        if (ra == rb)
                            continue;
                        uint32_t winner = e.targetIsA ? ra : rb;
                        uint32_t loser = e.targetIsA ? rb : ra;
                        parent[loser] = winner;

                        if ((i + 1) % recheckInterval == 0)
                        {
                            result = buildResult();
                            if (result.size() / 3 <= targetTriangles)
                                return result;
                        }
                    }
                    result = buildResult();
                    return result;
                }
                // ============================================================
                // Distanz Kamera
                // ============================================================
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
                // ============================================================
                // Aktive Indices
                // ============================================================
                const std::vector<uint32_t>&
                    GetRenderIndices(
                        const Vec3& cameraPosition) const
                {
                    size_t lod =
                        SelectLOD(
                            cameraPosition);
                    if (lod == 0)
                        return m_indices;
                    return
                        m_lods[
                            lod - 1].indices;
                }
                const std::vector<uint32_t>&
                    ActiveIndices(
                        const Vec3& cameraPosition) const
                {
                    return
                        GetRenderIndices(
                            cameraPosition);
                }
                size_t RenderTriangleCount(
                    const Vec3& cameraPosition) const
                {
                    return
                        GetRenderIndices(
                            cameraPosition).size() / 3;
                }
                // ============================================================
                // Aktive SubMeshes
                // ============================================================
                std::vector<SubMesh>
                    GetRenderSubMeshes(
                        const Vec3& cameraPosition) const
                {
                    size_t lod =
                        SelectLOD(
                            cameraPosition);
                    if (lod == 0)
                        return EffectiveSubMeshes();
                    const LODLevel& level =
                        m_lods[lod - 1];
                    if (!level.subMeshes.empty())
                        return level.subMeshes;
                    return
                    {
                        SubMesh{
                            0,
                            static_cast<uint32_t>(
                                level.indices.size()),
                            -1
                        }
                    };
                }
                // ============================================================
                // Aktives LOD Dreieck
                // ============================================================
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
                    i0 =
                        indices[base + 0];
                    i1 =
                        indices[base + 1];
                    i2 =
                        indices[base + 2];
                    return true;
                }
                size_t GetActiveLOD(
                    const Vec3& cameraPosition) const
                {
                    return
                        SelectLOD(
                            cameraPosition);
                }
                T GetActiveLODDistance(
                    const Vec3& cameraPosition) const
                {
                    size_t lod =
                        SelectLOD(
                            cameraPosition);
                    if (lod == 0)
                        return static_cast<T>(0);
                    return
                        m_lods[
                            lod - 1].distance;
                }
                // ============================================================
                // Smooth Normals
                // ============================================================
                void ComputeSmoothNormals()
                {
                    if (m_vertices.empty() ||
                        m_indices.empty())
                    {
                        return;
                    }
                    for (auto& v :
                        m_vertices)
                    {
                        v.normal() =
                            math::Matrix<3, 1, T>(
                                0, 0, 0);
                    }
                    for (size_t t = 0;
                        t < TriangleCount();
                        ++t)
                    {
                        uint32_t i0;
                        uint32_t i1;
                        uint32_t i2;
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
                        Vec3 e1 =
                            p1 - p0;
                        Vec3 e2 =
                            p2 - p0;
                        Vec3 n =
                            Cross(e1, e2);
                        m_vertices[i0].normal() += n;
                        m_vertices[i1].normal() += n;
                        m_vertices[i2].normal() += n;
                    }
                    for (auto& v :
                        m_vertices)
                    {
                        T len =
                            Length(
                                v.normal());
                        if (len >
                            static_cast<T>(1e-8))
                        {
                            v.normal() =
                                v.normal() *
                                (static_cast<T>(1) /
                                    len);
                        }
                    }
                    m_normalsDirty = false;
                    EnsureUVs();
                }
                // ============================================================
                // Flat Normals
                //
                // WICHTIG:
                // Hier werden die UVs zusammen mit den Vertices dupliziert.
                // ============================================================
                void ComputeFlatNormals()
                {
                    if (m_vertices.empty() ||
                        m_indices.empty())
                    {
                        return;
                    }
                    EnsureUVs();
                    std::vector<Vertex>
                        newVerts;
                    std::vector<Vec2>
                        newUVs;
                    std::vector<uint32_t>
                        newIndices;
                    newVerts.reserve(
                        m_indices.size());
                    newUVs.reserve(
                        m_indices.size());
                    newIndices.reserve(
                        m_indices.size());
                    for (size_t t = 0;
                        t < TriangleCount();
                        ++t)
                    {
                        uint32_t i0;
                        uint32_t i1;
                        uint32_t i2;
                        if (!GetTriangleIndices(
                            t,
                            i0,
                            i1,
                            i2))
                        {
                            break;
                        }
                        if (i0 >= m_vertices.size() ||
                            i1 >= m_vertices.size() ||
                            i2 >= m_vertices.size())
                        {
                            continue;
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
                                (static_cast<T>(1) /
                                    len);
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
                        // ----------------------------------------------------
                        // Die UVs müssen genauso dupliziert werden wie die
                        // Vertices, sonst laufen Index und UV auseinander.
                        // ----------------------------------------------------
                        newUVs.push_back(
                            GetUV(i0));
                        newUVs.push_back(
                            GetUV(i1));
                        newUVs.push_back(
                            GetUV(i2));
                        newIndices.push_back(
                            base + 0);
                        newIndices.push_back(
                            base + 1);
                        newIndices.push_back(
                            base + 2);
                    }
                    m_vertices =
                        std::move(newVerts);
                    m_uvs =
                        std::move(newUVs);
                    m_indices =
                        std::move(newIndices);
                    EnsureUVs();
                    m_normalsDirty = false;
                    // Neue Vertexnummerierung
                    // macht alte LODs ungültig.
                    m_lods.clear();
                    m_currentLOD = 0;
                    MarkGeometryDirty();
                }
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
                    return
                        rotated +
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
                // Normal Zugriff
                // ============================================================
                const Vec3&
                    GetNormal(size_t idx) const
                {
                    return
                        m_vertices[idx].normal();
                }
                Vec3&
                    GetNormal(size_t idx)
                {
                    return
                        m_vertices[idx].normal();
                }
                Vec3 GetWorldNormal(
                    size_t idx) const
                {
                    return
                        TransformNormal(
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
                    uint32_t i0;
                    uint32_t i1;
                    uint32_t i2;
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
                // Face Normale
                // ============================================================
                Vec3 ComputeFaceNormal(
                    size_t triIdx) const
                {
                    uint32_t i0;
                    uint32_t i1;
                    uint32_t i2;
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
                // Triangle Plane
                // ============================================================
                ng::math::RayPlaneLine<3, T>
                    GetTrianglePlane(
                        size_t triIdx) const
                {
                    uint32_t i0;
                    uint32_t i1;
                    uint32_t i2;
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
                        plane(
                            p0,
                            p1,
                            p2);
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
                    uint32_t i0;
                    uint32_t i1;
                    uint32_t i2;
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
                // LOD Sortierung
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
                    // Nach dem Sortieren koennte m_currentLOD auf ein
                    // anderes Level zeigen als vorher gemeint - beim
                    // naechsten SelectLOD()-Aufruf wird ohnehin neu
                    // bestimmt, hier nur zur Sicherheit zuruecksetzen,
                    // damit Hysterese nicht mit einem falschen alten
                    // Index rechnet.
                    m_currentLOD = 0;
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
                // ============================================================
                // Quadric Error Metric (Garland/Heckbert) - fuer
                // BuildLODIndices(). Speichert die symmetrische 4x4 Matrix
                // einer Summe von Ebenengleichungen als 10 unabhaengige
                // Werte (die Matrix ist symmetrisch, daher reichen die
                // obere Dreiecksmatrix + Diagonale).
                // ============================================================
                struct Quadric
                {
                    T a2 = 0, ab = 0, ac = 0, ad = 0;
                    T b2 = 0, bc = 0, bd = 0;
                    T c2 = 0, cd = 0;
                    T d2 = 0;
                    Quadric& operator+=(const Quadric& o)
                    {
                        a2 += o.a2; ab += o.ab; ac += o.ac; ad += o.ad;
                        b2 += o.b2; bc += o.bc; bd += o.bd;
                        c2 += o.c2; cd += o.cd;
                        d2 += o.d2;
                        return *this;
                    }
                    Quadric operator+(const Quadric& o) const
                    {
                        Quadric r = *this;
                        r += o;
                        return r;
                    }
                };
                // Ebenengleichung ax+by+cz+d=0 aus einem Dreieck ableiten
                // und daraus die Fehler-Quadrik K = p * p^T bilden, wobei
                // p = (a,b,c,d) der (normierte) Ebenenvektor ist.
                static Quadric PlaneQuadric(
                    const Vec3& p0,
                    const Vec3& p1,
                    const Vec3& p2)
                {
                    Vec3 n = Cross(p1 - p0, p2 - p0);
                    T len = Length(n);
                    if (len < static_cast<T>(1e-12))
                        return Quadric{}; // entartete Flaeche traegt nichts bei
                    n = n * (static_cast<T>(1) / len);
                    T d = -(n.x() * p0.x() + n.y() * p0.y() + n.z() * p0.z());
                    Quadric q;
                    q.a2 = n.x() * n.x(); q.ab = n.x() * n.y(); q.ac = n.x() * n.z(); q.ad = n.x() * d;
                    q.b2 = n.y() * n.y(); q.bc = n.y() * n.z(); q.bd = n.y() * d;
                    q.c2 = n.z() * n.z(); q.cd = n.z() * d;
                    q.d2 = d * d;
                    return q;
                }
                // Fehlerwert v^T * K * v fuer einen konkreten Punkt v.
                static T QuadricError(
                    const Quadric& q,
                    const Vec3& p)
                {
                    return
                        q.a2 * p.x() * p.x() +
                        q.b2 * p.y() * p.y() +
                        q.c2 * p.z() * p.z() +
                        q.d2 +
                        static_cast<T>(2) * q.ab * p.x() * p.y() +
                        static_cast<T>(2) * q.ac * p.x() * p.z() +
                        static_cast<T>(2) * q.ad * p.x() +
                        static_cast<T>(2) * q.bc * p.y() * p.z() +
                        static_cast<T>(2) * q.bd * p.y() +
                        static_cast<T>(2) * q.cd * p.z();
                }
                // ============================================================
                // Barycentric
                // ============================================================
                static void ComputeBarycentric(
                    const Vec3& p,
                    const Vec3& a,
                    const Vec3& b,
                    const Vec3& c,
                    T& outU,
                    T& outV,
                    T& outW)
                {
                    Vec3 v0 =
                        b - a;
                    Vec3 v1 =
                        c - a;
                    Vec3 v2 =
                        p - a;
                    T d00 =
                        v0 ^ v0;
                    T d01 =
                        v0 ^ v1;
                    T d11 =
                        v1 ^ v1;
                    T d20 =
                        v2 ^ v0;
                    T d21 =
                        v2 ^ v1;
                    T denom =
                        d00 * d11 -
                        d01 * d01;
                    if (std::abs(denom) <
                        static_cast<T>(1e-12))
                    {
                        outU =
                            static_cast<T>(1);
                        outV =
                            static_cast<T>(0);
                        outW =
                            static_cast<T>(0);
                        return;
                    }
                    T v =
                        (d11 * d20 -
                            d01 * d21) /
                        denom;
                    T w =
                        (d00 * d21 -
                            d01 * d20) /
                        denom;
                    outU =
                        static_cast<T>(1) -
                        v -
                        w;
                    outV = v;
                    outW = w;
                }
            private:
                // ============================================================
                // Geometrie
                // ============================================================
                std::vector<Vertex>
                    m_vertices;
                std::vector<uint32_t>
                    m_indices;
                std::vector<SubMesh>
                    m_subMeshes;
                // ============================================================
                // UVs
                //
                // IMMER:
                //
                // m_uvs.size() == m_vertices.size()
                //
                // ============================================================
                std::vector<Vec2>
                    m_uvs;
                // ============================================================
                // LOD
                // ============================================================
                std::vector<LODLevel>
                    m_lods;
                bool m_lodEnabled =
                    false;
                // NEU: merkt sich das zuletzt gewaehlte LOD fuer die
                // Hysterese in SelectLOD(). mutable, weil SelectLOD() const
                // ist (wird von vielen const-Methoden wie GetRenderIndices()
                // aufgerufen).
                mutable size_t m_currentLOD = 0;
                // ============================================================
                // Bounds
                // ============================================================
                mutable Vec3
                    m_localMin{
                        0, 0, 0
                };
                mutable Vec3
                    m_localMax{
                        0, 0, 0
                };
                mutable bool
                    m_localBoundsDirty =
                    true;
                // ============================================================
                // Normals
                // ============================================================
                bool m_normalsDirty =
                    true;
            };
        }
    }
}