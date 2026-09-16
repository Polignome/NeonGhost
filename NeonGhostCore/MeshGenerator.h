#pragma once
#include "Global.h"
#include "Mesh.h"
#include "HeightMapGenerator.h"
#include "RayPlaneLine.h"

namespace ng {
    namespace renderer {
        namespace objects {

            template<typename T>
            class MeshGenerator
            {
                static_assert(std::is_floating_point<T>::value,
                    "MeshGenerator<T> requires a floating point type (float/double).");

            public:
                using Vec3 = typename Mesh<T>::Vec3;
                using Vertex = typename Mesh<T>::Vertex;

                // ============================================================
                // Cube
                //
                // Erzeugt 24 Vertices (4 pro Seite, nicht geteilt), damit
                // jede Seite ihre eigene flache Normale/UVs bekommt.
                // ============================================================
                static Mesh<T> CreateCube(T size = static_cast<T>(1))
                {
                    return CreateBox(size, size, size);
                }
                
                
                static Mesh<T> CreateBox(T width, T height, T depth)
                {
                    Mesh<T> mesh;

                    T hx = width * static_cast<T>(0.5);
                    T hy = height * static_cast<T>(0.5);
                    T hz = depth * static_cast<T>(0.5);

                    // ------------------------------------------------------------
                    // 8 gemeinsame Eck-Vertices
                    //
                    //        7 -------- 6
                    //       /|         /|
                    //      4 -------- 5 |
                    //      | |        | |
                    //      | 3 -------|- 2
                    //      |/         |/
                    //      0 -------- 1
                    //
                    // ------------------------------------------------------------

                    Vec3 positions[8] =
                    {
                        Vec3(-hx, -hy, -hz), // 0
                        Vec3(hx, -hy, -hz), // 1
                        Vec3(hx,  hy, -hz), // 2
                        Vec3(-hx,  hy, -hz), // 3

                        Vec3(-hx, -hy,  hz), // 4
                        Vec3(hx, -hy,  hz), // 5
                        Vec3(hx,  hy,  hz), // 6
                        Vec3(-hx,  hy,  hz)  // 7
                    };

                    static const T uv[4][2] =
                    {
                        {0, 0},
                        {1, 0},
                        {1, 1},
                        {0, 1}
                    };

                    mesh.ReserveVertices(8);
                    mesh.ReserveIndices(36);

                    // ------------------------------------------------------------
                    // Vertices erzeugen
                    // ------------------------------------------------------------

                    for (int i = 0; i < 8; ++i)
                    {
                        Vertex v(positions[i]);

                        // Wird später berechnet
                        v.normal() = Vec3(0, 0, 0);

                        v.color() =
                            math::Matrix<4, 1, T>::NormalToColor(
                                v.normal(),
                                -1,
                                -1,
                                -1
                            );

                        // Bei gemeinsamen Vertices gibt es hier natürlich
                        // keine unterschiedlichen UVs pro Face.
                        v.texture() = math::Matrix<2, 1, T>(0, 0);

                        mesh.AddVertex(v);
                    }

                    // ------------------------------------------------------------
                    // 12 Dreiecke / 6 Flächen
                    // ------------------------------------------------------------

                    // -Z
                    mesh.AddTriangle(0, 1, 2);
                    mesh.AddTriangle(0, 2, 3);

                    // +Z
                    mesh.AddTriangle(4, 6, 5);
                    mesh.AddTriangle(4, 7, 6);

                    // -X
                    mesh.AddTriangle(0, 3, 7);
                    mesh.AddTriangle(0, 7, 4);

                    // +X
                    mesh.AddTriangle(1, 5, 6);
                    mesh.AddTriangle(1, 6, 2);

                    // -Y
                    mesh.AddTriangle(0, 4, 5);
                    mesh.AddTriangle(0, 5, 1);

                    // +Y
                    mesh.AddTriangle(3, 2, 6);
                    mesh.AddTriangle(3, 6, 7);

                    // ------------------------------------------------------------
                    // Vertex-Normalen aus den angrenzenden Flächen berechnen
                    // ------------------------------------------------------------

                    std::vector<Vec3> normalSum(mesh.VertexCount(), Vec3(0, 0, 0));

                    const auto& indices = mesh.Indices();

                    for (size_t i = 0; i < indices.size(); i += 3)
                    {
                        uint32_t i0 = indices[i + 0];
                        uint32_t i1 = indices[i + 1];
                        uint32_t i2 = indices[i + 2];

                        const Vec3& p0 = positions[i0];
                        const Vec3& p1 = positions[i1];
                        const Vec3& p2 = positions[i2];

                        // Flächennormale
                        Vec3 e1 = p1 - p0;
                        Vec3 e2 = p2 - p0;

                        Vec3 n = e1 % e2;

                        // Nicht normalisieren!
                        //
                        // Dadurch wird automatisch nach Fläche gewichtet:
                        // große Dreiecke haben mehr Einfluss als kleine.
                        normalSum[i0] += n;
                        normalSum[i1] += n;
                        normalSum[i2] += n;
                    }

                    // ------------------------------------------------------------
                    // Normalen normalisieren
                    // ------------------------------------------------------------

                    for (uint32_t i = 0; i < mesh.VertexCount(); ++i)
                    {
                        Vec3 n = normalSum[i];

                        T len = n.length();

                        if (len > static_cast<T>(0))
                            n /= len;
                        else
                            n = Vec3(0, 1, 0);

                        mesh.VerticesMutable()[i].normal() = n;
                        //mesh.Vertex(i).normal() = n;

                        mesh.VerticesMutable()[i].color() =
                            math::Matrix<4, 1, T>::NormalToColor(
                                n,
                                -1,
                                -1,
                                -1
                            );
                    }
                    mesh.GenerateUVsBoxMapped();
                    mesh.ComputeFlatNormals();
                    return mesh;
                }
                                
                // ============================================================
                // Plane (in der XZ-Ebene, Normale +Y)
                // ============================================================
                static Mesh<T> CreatePlane(T width = static_cast<T>(1), T depth = static_cast<T>(1),
                    int segmentsX = 1, int segmentsZ = 1)
                {
                    Mesh<T> mesh;
                    segmentsX = std::max(1, segmentsX);
                    segmentsZ = std::max(1, segmentsZ);

                    T hx = width * static_cast<T>(0.5);
                    T hz = depth * static_cast<T>(0.5);

                    for (int z = 0; z <= segmentsZ; ++z)
                    {
                        T v = static_cast<T>(z) / static_cast<T>(segmentsZ);
                        T pz = -hz + v * depth;

                        for (int x = 0; x <= segmentsX; ++x)
                        {
                            T u = static_cast<T>(x) / static_cast<T>(segmentsX);
                            T px = -hx + u * width;

                            Vertex vert(Vec3(px, static_cast<T>(0), pz));
                            vert.normal() = Vec3(0, 1, 0);
                            vert.color() = math::Matrix<4, 1, T>::NormalToColor(vert.normal(), -1, -1, -1);
                            vert.texture() = math::Matrix<2, 1, T>(u, v);
                            mesh.AddVertex(vert);
                        }
                    }

                    int rowStride = segmentsX + 1;
                    for (int z = 0; z < segmentsZ; ++z)
                    {
                        for (int x = 0; x < segmentsX; ++x)
                        {
                            uint32_t i0 = static_cast<uint32_t>(z * rowStride + x);
                            uint32_t i1 = static_cast<uint32_t>(z * rowStride + x + 1);
                            uint32_t i2 = static_cast<uint32_t>((z + 1) * rowStride + x + 1);
                            uint32_t i3 = static_cast<uint32_t>((z + 1) * rowStride + x);

                            mesh.AddTriangle(i0, i1, i2);
                            mesh.AddTriangle(i0, i2, i3);
                        }
                    }

                    return mesh;
                }

                // ============================================================
                // Sphere (UV-Sphere)
                //
                // stacks = horizontale Ringe (Breitengrade)
                // slices = vertikale Segmente (Längengrade)
                // ============================================================
                static Mesh<T> CreateSphere(T radius = static_cast<T>(1), int slices = 16, int stacks = 12)
                {
                    Mesh<T> mesh;
                    slices = std::max(3, slices);
                    stacks = std::max(2, stacks);

                    const T PI = static_cast<T>(3.14159265358979323846);

                    for (int stack = 0; stack <= stacks; ++stack)
                    {
                        T v = static_cast<T>(stack) / static_cast<T>(stacks);
                        T phi = v * PI; // 0 (Nordpol) .. PI (Südpol)
                        T sinPhi = std::sin(phi);
                        T cosPhi = std::cos(phi);

                        for (int slice = 0; slice <= slices; ++slice)
                        {
                            T u = static_cast<T>(slice) / static_cast<T>(slices);
                            T theta = u * static_cast<T>(2.0) * PI;
                            T sinTheta = std::sin(theta);
                            T cosTheta = std::cos(theta);

                            Vec3 n(
                                sinPhi * cosTheta,
                                cosPhi,
                                sinPhi * sinTheta);

                            Vertex vert(n * radius);
                            vert.normal() = n;
                            vert.color() = math::Matrix<4, 1, T>::NormalToColor(vert.normal(), -1, -1, -1);
                            vert.texture() = math::Matrix<2, 1, T>(u, v);
                            mesh.AddVertex(vert);
                        }
                    }

                    int rowStride = slices + 1;
                    for (int stack = 0; stack < stacks; ++stack)
                    {
                        for (int slice = 0; slice < slices; ++slice)
                        {
                            uint32_t i0 = static_cast<uint32_t>(stack * rowStride + slice);
                            uint32_t i1 = static_cast<uint32_t>(stack * rowStride + slice + 1);
                            uint32_t i2 = static_cast<uint32_t>((stack + 1) * rowStride + slice + 1);
                            uint32_t i3 = static_cast<uint32_t>((stack + 1) * rowStride + slice);

                            // An den Polen degeneriert je ein Dreieck zu einem
                            // Punkt (i0==i1 bzw. i2==i3 in der Position, nicht
                            // im Index) - AddTriangle trotzdem für beide Hälften
                            // aufrufen, degenerierte Fläche hat Fläche 0 und
                            if (stack != 0)
                                mesh.AddTriangle(i2, i1, i0);
                            if (stack != stacks - 1)
                                mesh.AddTriangle(i3, i2, i0);
                            // stört das Rendering nicht.

                            
                            /*
                            if (stack != 0)
                                mesh.AddTriangle(i0, i1, i2);
                            if (stack != stacks - 1)
                                mesh.AddTriangle(i0, i2, i3);
         */
                        }
                    }

                    return mesh;
                }

                // ============================================================
                // Cone (mit runder Grundfläche, Spitze nach +Y)
                //
                // segments = Anzahl der Segmente um die Grundfläche
                // Falls radiusTop > 0 wird ein Kegelstumpf erzeugt.
                // ============================================================
                static Mesh<T> CreateCone(T radiusBottom = static_cast<T>(1), T height = static_cast<T>(2),
                    int segments = 16, T radiusTop = static_cast<T>(0))
                {
                    Mesh<T> mesh;
                    segments = std::max(3, segments);

                    const T PI = static_cast<T>(3.14159265358979323846);
                    T halfHeight = height * static_cast<T>(0.5);

                    // Steigungswinkel der Mantelfläche für korrekte Normalen
                    T slope = std::atan2(radiusBottom - radiusTop, height);
                    T sinSlope = std::sin(static_cast<T>(1.57079632679) - slope); // cos(slope) alt.
                    // Einfacher: Normale = normalize(height, radiusBottom-radiusTop, 0) rotiert um Achse
                    T nyFlat = (radiusBottom - radiusTop);
                    T nLen = std::sqrt(height * height + nyFlat * nyFlat);
                    T nY = (nLen > static_cast<T>(1e-8)) ? (nyFlat / nLen) : static_cast<T>(0);
                    T nXZ = (nLen > static_cast<T>(1e-8)) ? (height / nLen) : static_cast<T>(1);

                    // ------------------------------------------------------------
                    // Mantelfläche (oberer + unterer Ring)
                    // ------------------------------------------------------------
                    for (int s = 0; s <= segments; ++s)
                    {
                        T u = static_cast<T>(s) / static_cast<T>(segments);
                        T theta = u * static_cast<T>(2.0) * PI;
                        T ct = std::cos(theta);
                        T st = std::sin(theta);

                        Vec3 normal(ct * nXZ, nY, st * nXZ);

                        // unterer Ring
                        Vertex vBottom(Vec3(radiusBottom * ct, -halfHeight, radiusBottom * st));
                        vBottom.normal() = normal;
                        vBottom.color() = math::Matrix<4, 1, T>::NormalToColor(normal, -1, -1, -1);

                        vBottom.texture() = math::Matrix<2, 1, T>(u, static_cast<T>(0));
                        mesh.AddVertex(vBottom);

                        // oberer Ring
                        Vertex vTop(Vec3(radiusTop * ct, halfHeight, radiusTop * st));
                        vTop.normal() = normal;
                        vTop.color() = math::Matrix<4, 1, T>::NormalToColor(normal, -1, -1, -1);
                        vTop.texture() = math::Matrix<2, 1, T>(u, static_cast<T>(1));
                        mesh.AddVertex(vTop);
                    }

                    int rowStride = 2; // bottom,top pro Segment-Spalte
                    for (int s = 0; s < segments; ++s)
                    {
                        uint32_t b0 = static_cast<uint32_t>(s * rowStride + 0);
                        uint32_t t0 = static_cast<uint32_t>(s * rowStride + 1);
                        uint32_t b1 = static_cast<uint32_t>((s + 1) * rowStride + 0);
                        uint32_t t1 = static_cast<uint32_t>((s + 1) * rowStride + 1);

                        mesh.AddTriangle(b0, b1, t1);
                        mesh.AddTriangle(b0, t1, t0);
                    }

                    // ------------------------------------------------------------
                    // Bodenscheibe (nur wenn radiusBottom > 0)
                    // ------------------------------------------------------------
                    if (radiusBottom > static_cast<T>(1e-8))
                    {
                        uint32_t centerIdx = static_cast<uint32_t>(mesh.VertexCount());
                        Vertex centerBottom(Vec3(0, -halfHeight, 0));
                        centerBottom.normal() = Vec3(0, -1, 0);
                        centerBottom.texture() = math::Matrix<2, 1, T>(static_cast<T>(0.5), static_cast<T>(0.5));
                        centerBottom.color() = math::Matrix<4, 1, T>::NormalToColor(centerBottom.normal(), -1, -1, -1   );
                        
                        mesh.AddVertex(centerBottom);

                        uint32_t ringStart = static_cast<uint32_t>(mesh.VertexCount());
                        for (int s = 0; s <= segments; ++s)
                        {
                            T u = static_cast<T>(s) / static_cast<T>(segments);
                            T theta = u * static_cast<T>(2.0) * PI;
                            Vertex v(Vec3(radiusBottom * std::cos(theta), -halfHeight, radiusBottom * std::sin(theta)));
                            v.normal() = Vec3(0, -1, 0);
                            v.color() = math::Matrix<4, 1, T>::NormalToColor(v.normal(), -1, -1, -1 );
                            v.texture() = math::Matrix<2, 1, T>(
                                static_cast<T>(0.5) + static_cast<T>(0.5) * std::cos(theta),
                                static_cast<T>(0.5) + static_cast<T>(0.5) * std::sin(theta));
                            mesh.AddVertex(v);
                        }

                        for (int s = 0; s < segments; ++s)
                        {
                            uint32_t i0 = ringStart + s;
                            uint32_t i1 = ringStart + s + 1;
                            // Blickrichtung nach unten -> Winding umkehren
                            mesh.AddTriangle(centerIdx, i1, i0);
                        }
                    }

                    // ------------------------------------------------------------
                    // Deckscheibe (nur wenn radiusTop > 0, sonst echte Spitze)
                    // ------------------------------------------------------------
                    if (radiusTop > static_cast<T>(1e-8))
                    {
                        uint32_t centerIdx = static_cast<uint32_t>(mesh.VertexCount());
                        Vertex centerTop(Vec3(0, halfHeight, 0));
                        centerTop.normal() = Vec3(0, 1, 0);
                        centerTop.texture() = math::Matrix<2, 1, T>(static_cast<T>(0.5), static_cast<T>(0.5));
                        centerTop.color() = math::Matrix<4, 1, T>::NormalToColor(centerTop.normal(), -1, -1, -1 );
                        mesh.AddVertex(centerTop);

                        uint32_t ringStart = static_cast<uint32_t>(mesh.VertexCount());
                        for (int s = 0; s <= segments; ++s)
                        {
                            T u = static_cast<T>(s) / static_cast<T>(segments);
                            T theta = u * static_cast<T>(2.0) * PI;
                            Vertex v(Vec3(radiusTop * std::cos(theta), halfHeight, radiusTop * std::sin(theta)));
                            v.normal() = Vec3(0, 1, 0);
                            v.color() = math::Matrix<4, 1, T>::NormalToColor(v.normal(), -1, -1, -1);
                            v.texture() = math::Matrix<2, 1, T>(
                                static_cast<T>(0.5) + static_cast<T>(0.5) * std::cos(theta),
                                static_cast<T>(0.5) + static_cast<T>(0.5) * std::sin(theta));
                            mesh.AddVertex(v);
                        }

                        for (int s = 0; s < segments; ++s)
                        {
                            uint32_t i0 = ringStart + s;
                            uint32_t i1 = ringStart + s + 1;
                            mesh.AddTriangle(centerIdx, i0, i1);
                        }
                    }

                    return mesh;
                }

                // ============================================================
                // Cylinder = Cone mit radiusTop == radiusBottom
                // ============================================================
                static Mesh<T> CreateCylinder(T radius = static_cast<T>(1), T height = static_cast<T>(2), int segments = 16)
                {
                    return CreateCone(radius, height, segments, radius);
                }


                static Mesh<T> CreateTerrain(
                    T width = static_cast<T>(1),
                    T depth = static_cast<T>(1),
                    int segmentsX = 1,
                    int segmentsZ = 1)
                {
                    Mesh<T> mesh;

                    segmentsX = std::max(1, segmentsX);
                    segmentsZ = std::max(1, segmentsZ);

                    T hx = width * static_cast<T>(0.5);
                    T hz = depth * static_cast<T>(0.5);

                    typename ng::tools::HeightMapGenerator<T>::Settings settings;

                    settings.edgeStrength =
                        static_cast<T>(5);

                    settings.edgeFalloff =
                        static_cast<T>(20);

                    settings.maxSlope =
                        static_cast<T>(0.8);

                    settings.erosionStrength =
                        static_cast<T>(0.25);

                    settings.erosionIterations =
                        13;

                    settings.randomStrength =
                        static_cast<T>(0.5);


                    auto height_map =
                        ng::tools::HeightMapGenerator<T>::Generate(
                            segmentsX,
                            segmentsZ,
                            settings
                        );


                    // ============================================================
                    // Vertices erzeugen
                    // ============================================================

                    for (int z = 0; z <= segmentsZ; ++z)
                    {
                        T v =
                            static_cast<T>(z) /
                            static_cast<T>(segmentsZ);

                        T pz =
                            -hz + v * depth;


                        for (int x = 0; x <= segmentsX; ++x)
                        {
                            T u =
                                static_cast<T>(x) /
                                static_cast<T>(segmentsX);

                            T px =
                                -hx + u * width;



                            T py = height_map[z][x]*10;


                            Vertex vert(
                                Vec3(px, py, pz)
                            );

                            vert.normal() =
                                Vec3(
                                    static_cast<T>(0),
                                    static_cast<T>(1),
                                    static_cast<T>(0)
                                );

                            vert.color() =
                                math::Matrix<4, 1, T>::
                                NormalToColor(vert.normal(), -1, -1, -1 );

                            vert.texture() =
                                math::Matrix<2, 1, T>(u, v);


                            mesh.AddVertex(vert);
                        }
                    }


                    // ============================================================
                    // Normalen berechnen
                    // ============================================================

                    const int rowStride = segmentsX + 1;

                    for (int z = 0; z <= segmentsZ; ++z)
                    {
                        for (int x = 0; x <= segmentsX; ++x)
                        {
                            /*
                             * Nachbar-X
                             */

                            int x0 = std::max(0, x - 1);
                            int x1 = std::min(segmentsX, x + 1);

                            /*
                             * Nachbar-Z
                             */

                            int z0 = std::max(0, z - 1);
                            int z1 = std::min(segmentsZ, z + 1);


                            /*
                             * Tatsächliche Weltpositionen.
                             */

                            T px0 =
                                -hx +
                                static_cast<T>(x0) /
                                static_cast<T>(segmentsX) *
                                width;

                            T px1 =
                                -hx +
                                static_cast<T>(x1) /
                                static_cast<T>(segmentsX) *
                                width;


                            T pz0 =
                                -hz +
                                static_cast<T>(z0) /
                                static_cast<T>(segmentsZ) *
                                depth;

                            T pz1 =
                                -hz +
                                static_cast<T>(z1) /
                                static_cast<T>(segmentsZ) *
                                depth;


                            /*
                             * Höhen.
                             */

                            T h0 = height_map[z][x0];
                            T h1 = height_map[z][x];

                            T h2 = height_map[z][x1];

                            T h3 = height_map[z0][x];
                            T h4 = height_map[z1][x];


                            /*
                             * Tangente in X-Richtung.
                             */

                            Vec3 tangentX(
                                px1 - px0,
                                h2 - h0,
                                static_cast<T>(0)
                            );


                            /*
                             * Tangente in Z-Richtung.
                             */

                            Vec3 tangentZ(
                                static_cast<T>(0),
                                h4 - h3,
                                pz1 - pz0
                            );


                            /*
                             * Normale.
                             *
                             * Achtung:
                             *
                             * tangentZ % tangentX
                             *
                             * oder
                             *
                             * tangentX % tangentZ
                             *
                             * hängt von deiner Kreuzprodukt-Konvention ab.
                             */

                            Vec3 normal =
                                tangentZ % tangentX;


                            /*
                             * Normalisieren.
                             */

                            T len = normal.length();

                            if (len > static_cast<T>(0.000001))
                            {
                                normal /= len;
                            }
                            else
                            {
                                normal =
                                    Vec3(
                                        static_cast<T>(0),
                                        static_cast<T>(1),
                                        static_cast<T>(0)
                                    );
                            }


                            /*
                             * Vertex holen und Normale setzen.
                             */

                            uint32_t index =
                                static_cast<uint32_t>(
                                    z * rowStride + x
                                    );

                            mesh.VerticesMutable()[index].normal() =
                                normal;

                            mesh.VerticesMutable()[index].color() =
                                math::Matrix<4, 1, T>::
                                NormalToColor(normal,-1,-1,-1);
                        }
                    }


                    // ============================================================
                    // Dreiecke
                    // ============================================================

                    for (int z = 0; z < segmentsZ; ++z)
                    {
                        for (int x = 0; x < segmentsX; ++x)
                        {
                            uint32_t i0 =
                                static_cast<uint32_t>(
                                    z * rowStride + x);

                            uint32_t i1 =
                                static_cast<uint32_t>(
                                    z * rowStride + x + 1);

                            uint32_t i2 =
                                static_cast<uint32_t>(
                                    (z + 1) * rowStride + x + 1);

                            uint32_t i3 =
                                static_cast<uint32_t>(
                                    (z + 1) * rowStride + x);


                            mesh.AddTriangle(i0, i1, i2);
                            mesh.AddTriangle(i0, i2, i3);
                        }
                    }

                    return mesh;
                }
                


            };
        }
    }
}