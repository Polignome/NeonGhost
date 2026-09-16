#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include <random>


namespace ng{
	namespace tools {



        template<typename T>
        class HeightMapGenerator
        {
        public:

            using HeightMap = std::vector<std::vector<T>>;

            struct Settings
            {
                // Stärke des Kanten-Effekts
                T edgeStrength = (T)1.0;

                // Wie weit der Effekt ins Terrain reicht
                T edgeFalloff = (T)1.0;

                // Maximale erlaubte Steigung
                T maxSlope = (T)1.0;

                // Stärke des Abrutschens
                T erosionStrength = (T)0.5;

                // Anzahl der Erosionsdurchläufe
                int erosionIterations = 10;

                // Zufälligkeit
                T randomStrength = (T)0.2;

                // Seed
                unsigned int seed = 12345;
            };



            static void ScaleHeightMap(
                HeightMap& map,
                T maxHeight)
            {
                T currentMax = static_cast<T>(0);

                for (const auto& row : map)
                {
                    for (T h : row)
                    {
                        currentMax =
                            std::max(currentMax, h);
                    }
                }

                if (currentMax <= static_cast<T>(0))
                    return;

                T scale =
                    maxHeight / currentMax;

                for (auto& row : map)
                {
                    for (T& h : row)
                    {
                        h *= scale;
                    }
                }
            }


            static HeightMap Generate(
                int segmentsX,
                int segmentsZ,
                const Settings& settings = Settings())
            {
                HeightMap height_map(
                    segmentsZ + 1,
                    std::vector<T>(segmentsX + 1, (T)0)
                );

                /*
                 * 1. Großräumige Struktur erzeugen
                 */

                AddEdgeInfluence(
                    height_map,
                    segmentsX,
                    segmentsZ,
                    0, 0,
                    segmentsX, 0,
                    settings
                );

                AddEdgeInfluence(
                    height_map,
                    segmentsX,
                    segmentsZ,
                    segmentsX, 0,
                    segmentsX, segmentsZ,
                    settings
                );

                AddEdgeInfluence(
                    height_map,
                    segmentsX,
                    segmentsZ,
                    segmentsX, segmentsZ,
                    0, segmentsZ,
                    settings
                );

                AddEdgeInfluence(
                    height_map,
                    segmentsX,
                    segmentsZ,
                    0, segmentsZ,
                    0, 0,
                    settings
                );


                /*
                 * 2. Etwas Zufälligkeit hinzufügen
                 */

                AddNoise(
                    height_map,
                    settings.randomStrength,
                    settings.seed
                );


                /*
                 * 3. Sanduhr / Avalanche
                 */

                Erode(
                    height_map,
                    settings
                );


                return height_map;
            }


        private:

            // ------------------------------------------------------------
            // Punkt zu Linien-Distanz
            // ------------------------------------------------------------

            static T DistanceToLine(
                T px,
                T pz,
                T ax,
                T az,
                T bx,
                T bz)
            {
                const T dx = bx - ax;
                const T dz = bz - az;

                const T len2 = dx * dx + dz * dz;

                if (len2 <= (T)0)
                    return (T)0;

                T t =
                    ((px - ax) * dx +
                        (pz - az) * dz) / len2;

                t = std::clamp(t, (T)0, (T)1);

                const T cx = ax + t * dx;
                const T cz = az + t * dz;

                const T ex = px - cx;
                const T ez = pz - cz;

                return std::sqrt(ex * ex + ez * ez);
            }


            // ------------------------------------------------------------
            // Einfluss einer Kante
            // ------------------------------------------------------------

            static void AddEdgeInfluence(
                HeightMap& map,
                int sx,
                int sz,
                int ax,
                int az,
                int bx,
                int bz,
                const Settings& settings)
            {
                for (int z = 0; z <= sz; ++z)
                {
                    for (int x = 0; x <= sx; ++x)
                    {
                        T d = DistanceToLine(
                            (T)x,
                            (T)z,
                            (T)ax,
                            (T)az,
                            (T)bx,
                            (T)bz
                        );

                        /*
                         * Normalisierte Entfernung.
                         */

                        T influence =
                            (T)1 - d / settings.edgeFalloff;

                        influence =
                            std::clamp(influence, (T)0, (T)1);

                        /*
                         * Smoothstep.
                         */

                        influence =
                            influence * influence *
                            ((T)3 - (T)2 * influence);


                        /*
                         * Die Mitte wird angehoben.
                         */

                        map[z][x] +=
                            influence *
                            settings.edgeStrength;
                    }
                }
            }


            // ------------------------------------------------------------
            // Zufallsrauschen
            // ------------------------------------------------------------

            static void AddNoise(
                HeightMap& map,
                T strength,
                unsigned int seed)
            {
                std::mt19937 rng(seed);

                std::uniform_real_distribution<T> dist(
                    -strength,
                    strength
                );

                for (auto& row : map)
                {
                    for (auto& h : row)
                    {
                        h += dist(rng);
                    }
                }
            }


            // ------------------------------------------------------------
            // Sanduhr / Avalanche
            // ------------------------------------------------------------

            static void Erode(
                HeightMap& map,
                const Settings& settings)
            {
                const int sz = (int)map.size();

                if (sz == 0)
                    return;

                const int sx = (int)map[0].size();

                for (int iteration = 0;
                    iteration < settings.erosionIterations;
                    ++iteration)
                {
                    bool changed = false;

                    /*
                     * Wir verwenden mehrere Richtungen.
                     */

                    static const int dx[] =
                    {
                        -1, 1,
                         0, 0,
                        -1, 1,
                        -1, 1
                    };

                    static const int dz[] =
                    {
                         0, 0,
                        -1, 1,
                        -1,-1,
                         1, 1
                    };


                    for (int z = 0; z < sz; ++z)
                    {
                        for (int x = 0; x < sx; ++x)
                        {
                            T h = map[z][x];

                            for (int d = 0; d < 8; ++d)
                            {
                                const int nx = x + dx[d];
                                const int nz = z + dz[d];

                                if (nx < 0 || nx >= sx ||
                                    nz < 0 || nz >= sz)
                                    continue;

                                T nh = map[nz][nx];

                                T difference = h - nh;

                                /*
                                 * Stelle ist steiler als erlaubt.
                                 */

                                if (difference > settings.maxSlope)
                                {
                                    T excess =
                                        difference -
                                        settings.maxSlope;

                                    /*
                                     * Nur einen Teil abrutschen lassen.
                                     */

                                    T amount =
                                        excess *
                                        settings.erosionStrength;

                                    map[z][x] -= amount;
                                    map[nz][nx] += amount;

                                    h -= amount;

                                    changed = true;
                                }
                            }
                        }
                    }

                    if (!changed)
                        break;
                }
            }
        };


	}
}
