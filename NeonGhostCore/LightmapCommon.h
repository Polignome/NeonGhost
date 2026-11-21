#pragma once

// ----------------------------------------------------------------------
// LightmapCommon.h - gemeinsame Helfer fuer Lightmap.h und
// LightmapRadiosity.h (Threading, Hash).
// ----------------------------------------------------------------------

#include <thread>
#include <atomic>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <cstddef>

namespace ng::lightning
{
    // Deterministischer Seed pro (a, b, c) - unabhaengig von der Threadanzahl.
    inline uint32_t HashSeed(uint32_t a, uint32_t b, uint32_t c)
    {
        uint32_t h = a * 0x9E3779B1u;
        h ^= b + 0x85EBCA6Bu + (h << 6) + (h >> 2);
        h ^= c + 0xC2B2AE35u + (h << 6) + (h >> 2);
        h ^= h >> 16; h *= 0x7FEB352Du; h ^= h >> 15; h *= 0x846CA68Bu; h ^= h >> 16;
        return h;
    }

    // Verteilt [0,count) dynamisch in Chunks auf alle Kerne. Der Hauptthread arbeitet mit.
    template<class F>
    void ParallelFor(size_t count, F&& body, size_t chunk = 64)
    {
        if (count == 0) return;
        chunk = std::max<size_t>(1, chunk);

        const unsigned hw = std::max(1u, std::thread::hardware_concurrency());
        const unsigned nThreads = (unsigned)std::min<size_t>(hw, (count + chunk - 1) / chunk);

        std::atomic<size_t> next{ 0 };
        auto worker = [&]()
            {
                for (;;)
                {
                    const size_t begin = next.fetch_add(chunk);
                    if (begin >= count) break;
                    const size_t end = std::min(begin + chunk, count);
                    for (size_t i = begin; i < end; ++i) body(i);
                }
            };

        if (nThreads <= 1) { worker(); return; }

        std::vector<std::thread> threads;
        threads.reserve(nThreads - 1);
        for (unsigned t = 1; t < nThreads; ++t) threads.emplace_back(worker);
        worker();
        for (auto& th : threads) th.join();
    }
}