


#pragma once
#include <cstdio>
#include <memory>
#include <vector>
#include "BSP.h"
#include "PVS.h"
#include "VisCells.h"
#include "PixelBuffer.h"
#include "BspSceneIO.h"

namespace ng::io {

    template <class T>
    struct BspScene {
        ng::bsp::BSPTree<T> m_bsp;
        ng::bsp::PVS<T> m_pvs;
        ng::bsp::VisCells<T> m_cells;
        ng::renderer::buffers::PixelBufferRGBA m_lightmap;
        std::vector<ng::bsp::WarpPortal<T>> m_warpPortals;
        std::string m_wadname = "";
        std::vector<std::string> m_texture_names;
    };

} // namespace ng::io


// BSP-, PVS-, VisCells-, PixelBuffer-Header muessen ueber BspScene.h
// bzw. vorher eingebunden sein.

namespace ng::io {

    // ======================================================================
    // Freigeschaltete Rohdaten-Typen (reine Datenklassen: nur T-Werte,
    // keine Zeiger, keine std::vector, keine virtuellen Methoden).
    // MUSS vor der ersten Verwendung von Pod()/PodVector() stehen.
    // ======================================================================
    template <class T> struct IsRawSerializable<ng::math::Matrix<2, 1, T>> : std::true_type {};
    template <class T> struct IsRawSerializable<ng::math::Matrix<3, 1, T>> : std::true_type {};
    template <class T> struct IsRawSerializable<ng::math::Matrix<4, 1, T>> : std::true_type {};
    template <class T> struct IsRawSerializable<ng::math::RayPlaneLine<3, T>> : std::true_type {};

    // Sicherheitsnetz: schlaegt fehl, wenn Matrix versteckte Member hat.
    static_assert(sizeof(ng::math::Matrix<3, 1, double>) == 3 * sizeof(double),
        "Matrix<3,1,double> hat versteckte Member -> nicht roh kopierbar");

    enum : uint32_t {
        CHUNK_BSP = FourCC('B', 'S', 'P', ' '),
        CHUNK_PVS = FourCC('P', 'V', 'S', ' '),
        CHUNK_CELLS = FourCC('C', 'E', 'L', 'L'),
        CHUNK_LMAP = FourCC('L', 'M', 'A', 'P'),
        CHUNK_WARP = FourCC('W', 'R', 'P', 'P'),
        CHUNK_TEX = FourCC('T', 'E', 'X', 'N'),
    };

    // Pro Modul erhoehen, wenn sich dessen Format aendert.
    // In Read() kann mit r.Version() das alte Format weiter gelesen werden.
    constexpr uint32_t V_BSP = 2, V_PVS = 1, V_CELLS = 2, V_LMAP = 1, V_WARP = 1, V_TEX = 1;

    // ======================================================================
    // Vertex / Polygon (feldweise)
    // ======================================================================
    template <class T>
    void WriteVertex(ChunkWriter& w, const ng::geometry::Vertex<T>& v) {
        w.Pod(v.world());
        w.Pod(v.normal());
        w.Pod(v.texture());
        w.Pod(v.fxtexture());
        w.Pod(v.color());
        w.Pod(v.alpha());
    }
    template <class T>
    void ReadVertex(ChunkReader& r, ng::geometry::Vertex<T>& v) {
        v.world() = r.Pod<std::decay_t<decltype(v.world())>>();
        v.normal() = r.Pod<std::decay_t<decltype(v.normal())>>();
        v.texture() = r.Pod<std::decay_t<decltype(v.texture())>>();
        v.fxtexture() = r.Pod<std::decay_t<decltype(v.fxtexture())>>();
        v.color() = r.Pod<std::decay_t<decltype(v.color())>>();
        v.alpha() = r.Pod<std::decay_t<decltype(v.alpha())>>();
    }

    template <class T>
    void WritePoly(ChunkWriter& w, const ng::bsp::BSPPolygon<T>& p) {
        w.Pod<uint64_t>(uint64_t(p.verts.size()));
        for (const auto& v : p.verts) WriteVertex(w, v);
        w.Pod(p.plane);
        w.Pod(p.materialId);  w.Pod(p.objectId); w.Pod(p.flags);
        w.Pod(p.volumeId);    w.Pod(p.portalId);
        w.Pod(p.lmPosX);      w.Pod(p.lmPosY);
        w.Pod(p.lmWidth);     w.Pod(p.lmHeight);
        w.Pod(p.lmOrigin);    w.Pod(p.lmEdgeU);  w.Pod(p.lmEdgeV);
        w.Pod(p.textureId);
        // p.texture wird bewusst NICHT gespeichert -> RestoreTextures()
    }
    template <class T>
    void ReadPoly(ChunkReader& r, ng::bsp::BSPPolygon<T>& p) {
        p.verts.clear();
        p.verts.resize(size_t(r.Count(4)));
        for (auto& v : p.verts) ReadVertex(r, v);
        p.plane = r.Pod<std::decay_t<decltype(p.plane)>>();
        p.materialId = r.Pod<int>();       p.objectId = r.Pod<uint32_t>();
        p.flags = r.Pod<uint32_t>();
        p.volumeId = r.Pod<int>();       p.portalId = r.Pod<int32_t>();
        p.lmPosX = r.Pod<int32_t>();   p.lmPosY = r.Pod<int32_t>();
        p.lmWidth = r.Pod<int32_t>();   p.lmHeight = r.Pod<int32_t>();
        p.lmOrigin = r.Pod<std::decay_t<decltype(p.lmOrigin)>>();
        p.lmEdgeU = r.Pod<std::decay_t<decltype(p.lmEdgeU)>>();
        p.lmEdgeV = r.Pod<std::decay_t<decltype(p.lmEdgeV)>>();
        p.textureId = (r.Version() >= 2) ? r.Pod<int32_t>() : -1;
    }
    template <class T>
    void WritePolys(ChunkWriter& w, const std::vector<ng::bsp::BSPPolygon<T>>& v) {
        w.Pod<uint64_t>(uint64_t(v.size()));
        for (const auto& p : v) WritePoly(w, p);
    }
    template <class T>
    void ReadPolys(ChunkReader& r, std::vector<ng::bsp::BSPPolygon<T>>& v) {
        v.clear();
        v.resize(size_t(r.Count(8)));
        for (auto& p : v) ReadPoly(r, p);
    }

    template <class T>
    void WriteVolume(ChunkWriter& w, const ng::bsp::BSPVolumePiece<T>& vp) {
        w.Pod(vp.volumeId);
        w.Pod(vp.medium);
        WritePolys(w, vp.faces);
    }
    template <class T>
    void ReadVolume(ChunkReader& r, ng::bsp::BSPVolumePiece<T>& vp) {
        vp.volumeId = r.Pod<int>();
        vp.medium = r.Pod<uint32_t>();
        ReadPolys(r, vp.faces);
    }

    inline void WriteVecVecU32(ChunkWriter& w, const std::vector<std::vector<uint32_t>>& v) {
        w.Pod<uint64_t>(uint64_t(v.size()));
        for (const auto& x : v) w.PodVector(x);
    }
    inline void ReadVecVecU32(ChunkReader& r, std::vector<std::vector<uint32_t>>& v) {
        v.clear();
        v.resize(size_t(r.Count(8)));
        for (auto& x : v) x = r.PodVector<uint32_t>();
    }

    // ======================================================================
    // Zugriff auf die privaten Member
    // (friend struct ng::io::BspAccess in BSPNode, BSPTree, Portal, PVS, VisCells)
    // ======================================================================
    struct BspAccess {

        // ---------------- BSPTree ----------------
        template <class T>
        static void WriteTree(ChunkWriter& w, const ng::bsp::BSPTree<T>& t)
        {
            using Node = ng::bsp::BSPNode<T>;

            w.Pod(t.m_worldScale);
            w.Pod(t.m_minPortalWidth);
            w.Pod<int32_t>(int32_t(t.m_maxDepth));
            w.Pod(t.m_epsilon);
            // m_worldBox wird nur fuer BuildPortals() gebraucht -> nicht gespeichert.

            // Portale
            w.Pod<uint64_t>(uint64_t(t.m_portals.size()));
            for (const auto& p : t.m_portals) {
                w.Pod(p.m_plane);
                w.PodVector(p.m_verts);
                w.Pod(p.m_leafa);
                w.Pod(p.m_leafb);
            }

            // Knoten, Pre-Order, iterativ
            w.Pod<uint8_t>(t.m_root ? 1 : 0);
            std::vector<const Node*> st;
            if (t.m_root) st.push_back(t.m_root.get());

            while (!st.empty()) {
                const Node* n = st.back();
                st.pop_back();

                w.Pod(n->m_state);
                w.Pod(n->m_id);
                w.Pod(n->m_plane);

                if (n->isLeaf()) {
                    WritePolys(w, n->polys);
                    WritePolys(w, n->extraPolys);
                    w.Pod<uint64_t>(uint64_t(n->volumes.size()));
                    for (const auto& v : n->volumes) WriteVolume(w, v);
                }
                else {
                    const uint8_t flags = uint8_t((n->m_front_node ? 1 : 0) | (n->m_back_node ? 2 : 0));
                    w.Pod(flags);
                    if (n->m_back_node)  st.push_back(n->m_back_node.get());
                    if (n->m_front_node) st.push_back(n->m_front_node.get());   // Front wird zuerst geschrieben
                }
            }
        }

        template <class T>
        static void ReadTree(ChunkReader& r, ng::bsp::BSPTree<T>& t)
        {
            using Node = ng::bsp::BSPNode<T>;
            using Plane = ng::math::RayPlaneLine<3, T>;
            using Portal = ng::bsp::Portal<T>;
            using Vec3 = ng::math::Matrix<3, 1, T>;

            t.m_root.reset();
            t.m_leaflist.clear();
            t.m_portals.clear();

            t.m_worldScale = r.Pod<T>();
            t.m_minPortalWidth = r.Pod<T>();
            t.m_maxDepth = r.Pod<int32_t>();
            t.m_epsilon = r.Pod<T>();

            const uint64_t pc = r.Count(8);
            t.m_portals.reserve(size_t(pc));
            for (uint64_t i = 0; i < pc; ++i) {
                Plane pl = r.Pod<Plane>();
                std::vector<Vec3> verts = r.PodVector<Vec3>();
                Portal p(pl, verts);
                p.m_leafa = r.Pod<int32_t>();
                p.m_leafb = r.Pod<int32_t>();
                t.m_portals.push_back(std::move(p));
            }

            const bool hasRoot = r.Pod<uint8_t>() != 0;
            std::vector<std::shared_ptr<Node>*> slots;
            std::vector<std::shared_ptr<Node>> leaves;
            if (hasRoot) slots.push_back(&t.m_root);

            while (!slots.empty()) {
                std::shared_ptr<Node>* slot = slots.back();
                slots.pop_back();

                auto n = std::make_shared<Node>();
                *slot = n;

                n->m_state = r.Pod<uint32_t>();
                n->m_id = r.Pod<int32_t>();
                n->m_plane = r.Pod<Plane>();

                if (n->isLeaf()) {
                    ReadPolys(r, n->polys);
                    ReadPolys(r, n->extraPolys);
                    n->volumes.clear();
                    n->volumes.resize(size_t(r.Count(8)));
                    for (auto& v : n->volumes) ReadVolume(r, v);
                    leaves.push_back(n);
                }
                else {
                    const uint8_t flags = r.Pod<uint8_t>();
                    if (flags & 2) slots.push_back(&n->m_back_node);
                    if (flags & 1) slots.push_back(&n->m_front_node);
                }
            }

            // Leaf-Liste ueber die IDs wiederherstellen
            t.m_leaflist.assign(leaves.size(), nullptr);
            for (auto& l : leaves) {
                if (l->m_id < 0 || size_t(l->m_id) >= leaves.size())
                    throw std::runtime_error("BSP: invalid leaf id");
                t.m_leaflist[size_t(l->m_id)] = l;
            }
        }

        // ---------------- PVS ----------------
        template <class T>
        static void WritePVS(ChunkWriter& w, const ng::bsp::PVS<T>& p) {
            w.Pod<uint8_t>(p.m_built ? 1 : 0);
            w.Pod<uint64_t>(uint64_t(p.m_leafCount));
            w.PodVector(p.m_active);
            w.Pod<uint64_t>(uint64_t(p.m_rows.size()));
            for (const auto& row : p.m_rows) w.PodVector(row);
        }
        template <class T>
        static void ReadPVS(ChunkReader& r, ng::bsp::PVS<T>& p) {
            p.Clear();
            p.m_built = r.Pod<uint8_t>() != 0;
            p.m_leafCount = size_t(r.Pod<uint64_t>());
            p.m_active = r.PodVector<uint8_t>();
            p.m_rows.clear();
            p.m_rows.resize(size_t(r.Count(8)));
            for (auto& row : p.m_rows) row = r.PodVector<uint8_t>();
            if (p.m_active.size() != p.m_leafCount || p.m_rows.size() != p.m_leafCount)
                throw std::runtime_error("PVS: inconsistent sizes");
        }

        // ---------------- VisCells ----------------
        template <class T>
        static void WriteCells(ChunkWriter& w, const ng::bsp::VisCells<T>& c) {
            w.Pod(c.m_opt);
            WriteTree(w, c.m_tree);
            WritePolys(w, c.m_polys);
            WriteVecVecU32(w, c.m_cellPolys);
            WritePolys(w, c.m_extraPolys);
            WriteVecVecU32(w, c.m_cellExtraPolys);
        }
        template <class T>
        static void ReadCells(ChunkReader& r, ng::bsp::VisCells<T>& c) {
            c.m_opt = r.Pod<ng::bsp::VisCellOptions<T>>();
            ReadTree(r, c.m_tree);
            ReadPolys(r, c.m_polys);
            ReadVecVecU32(r, c.m_cellPolys);
            ReadPolys(r, c.m_extraPolys);
            ReadVecVecU32(r, c.m_cellExtraPolys);
            c.m_stamp.clear();      c.m_stampValue = 0;        // Zeitstempel neu aufbauen
            c.m_extraStamp.clear(); c.m_extraStampValue = 0;
        }

        // Alle Polygone von VisCells (fuer RestoreTextures)
        template <class T, class F>
        static void ForEachCellPoly(ng::bsp::VisCells<T>& c, F&& f) {
            for (auto& p : c.m_polys)      f(p);
            for (auto& p : c.m_extraPolys) f(p);
            std::vector<ng::bsp::BSPPolygon<T>*> out;
            c.m_tree.CollectPolygonPointers(c.m_tree.GetRoot(), out);
            for (auto* p : out) f(*p);
        }
    };

    // ======================================================================
    // Oeffentliche Write/Read-Overloads (von Save/Load benutzt)
    // ======================================================================
    template <class T> void Write(ChunkWriter& w, const ng::bsp::BSPTree<T>& x) { BspAccess::WriteTree(w, x); }
    template <class T> void Read(ChunkReader& r, ng::bsp::BSPTree<T>& x) { BspAccess::ReadTree(r, x); }
    template <class T> void Write(ChunkWriter& w, const ng::bsp::PVS<T>& x) { BspAccess::WritePVS(w, x); }
    template <class T> void Read(ChunkReader& r, ng::bsp::PVS<T>& x) { BspAccess::ReadPVS(r, x); }
    template <class T> void Write(ChunkWriter& w, const ng::bsp::VisCells<T>& x) { BspAccess::WriteCells(w, x); }
    template <class T> void Read(ChunkReader& r, ng::bsp::VisCells<T>& x) { BspAccess::ReadCells(r, x); }

    // ---- Warp-Portale ----
    template <class T>
    void Write(ChunkWriter& w, const std::vector<ng::bsp::WarpPortal<T>>& v) {
		printf("[Write] WarpPortals: %zu portals\n", v.size());
        w.Pod<uint64_t>(uint64_t(v.size()));
        for (const auto& p : v) {
            w.Pod(p.seeThrough); w.Pod(p.wobble);
            w.Pod(p.id); w.Pod(p.partner); w.Pod(p.flags);
            w.PodVector(p.verts);
            w.Pod(p.plane);
            w.Pod(p.origin); w.Pod(p.right); w.Pod(p.up); w.Pod(p.normal);
            w.PodVector(p.leaves);
            w.PodVector(p.visCells);
        }
    }
    template <class T>
    void Read(ChunkReader& r, std::vector<ng::bsp::WarpPortal<T>>& v) {
		printf("[Read] WarpPortals: %zu bytes remaining\n", r.Remaining());
        using WP = ng::bsp::WarpPortal<T>;
        using Vec3 = typename WP::Vec3;
        using Plane = typename WP::Plane;
        v.clear();
        v.resize(size_t(r.Count(8)));
        for (auto& p : v) {
            p.seeThrough = r.Pod<float>();
            p.wobble = r.Pod<float>();
            p.id = r.Pod<int32_t>();
            p.partner = r.Pod<int32_t>();
            p.flags = r.Pod<uint32_t>();
            p.verts = r.PodVector<Vec3>();
            p.plane = r.Pod<Plane>();
            p.origin = r.Pod<Vec3>();
            p.right = r.Pod<Vec3>();
            p.up = r.Pod<Vec3>();
            p.normal = r.Pod<Vec3>();
            p.leaves = r.PodVector<int32_t>();
            p.visCells = r.PodVector<int32_t>();
        }
		printf("[Read] WarpPortals: %zu portals\n", v.size());
    }

    template <class T>
    size_t CountMissingTextureIds(BspScene<T>& s, const char* where)
    {
        size_t total = 0, missing = 0;
        auto count = [&](auto& p) { ++total; if (p.textureId < 0) ++missing; };

        std::vector<ng::bsp::BSPPolygon<T>*> out;
        s.m_bsp.CollectPolygonPointers(s.m_bsp.GetRoot(), out);
        for (auto* p : out) count(*p);
        BspAccess::ForEachCellPoly(s.m_cells, count);

        printf("[TexId] %s: %zu Polygone, %zu ohne textureId\n", where, total, missing);
        return missing;
    }

    // ---- Lightmap (RGBA8, uint32 pro Pixel) ----
    inline void Write(ChunkWriter& w, const ng::renderer::buffers::PixelBufferRGBA& lm) {
        const bool valid = lm.GetRawBuffer() != nullptr && lm.Width() > 0 && lm.Height() > 0;
        printf("[Write] Lightmap: %dx%d, %zu bytes\n", lm.Width(), lm.Height(), size_t(lm.Width()) * size_t(lm.Height()) * sizeof(uint32_t));

        w.Pod<int32_t>(valid ? lm.Width() : 0);
        w.Pod<int32_t>(valid ? lm.Height() : 0);
        if (valid) w.Bytes(lm.GetRawBuffer(), size_t(lm.Width()) * size_t(lm.Height()) * sizeof(uint32_t));
    }

    inline void Read(ChunkReader& r, ng::renderer::buffers::PixelBufferRGBA& lm) {
		printf("[Read] Lightmap: %zu bytes remaining\n", r.Remaining());
        const int32_t width = r.Pod<int32_t>();
        const int32_t height = r.Pod<int32_t>();
        if (width < 0 || height < 0 ||
            uint64_t(width) * uint64_t(height) * 4 > r.Remaining())
            throw std::runtime_error("Lightmap: invalid size");
        lm.Resize(width, height);
        if (width > 0 && height > 0) {
            if (lm.Width() != width || lm.Height() != height)    // z.B. Buffer gelockt
                throw std::runtime_error("Lightmap: resize failed");
            r.Bytes(lm.GetRawBuffer(), lm.MemSize());
        }
		printf("[Read] Lightmap: %dx%d, %zu bytes\n", lm.Width(), lm.Height(), size_t(lm.Width()) * size_t(lm.Height()) * sizeof(uint32_t));
    }

    // ---- WAD / Texturnamen ----
    inline void WriteTextures(ChunkWriter& w, const std::string& wad,
        const std::vector<std::string>& names) {
        printf("[Write] Wadname %s\n", wad.c_str());
        w.String(wad);
        w.Pod<uint64_t>(uint64_t(names.size()));
        for (auto& n : names) {
            printf("[Write] Texturename %s\n", n.c_str());
            w.String(n);
        }
    }
    inline void ReadTextures(ChunkReader& r, std::string& wad, std::vector<std::string>& names) {
		printf("[Read] Wadname: %zu bytes remaining\n", r.Remaining());
        wad = r.String();
        printf("[Read] Wadname %s\n", wad.c_str());
        names.resize(size_t(r.Count(4)));
        for (auto& n : names) {
            n = r.String();
            printf("[Read] Texturename %s\n", n.c_str());
        }
    }

    // ======================================================================
    // Texture-Handles nach dem Laden wieder setzen.
    // resolve(name) -> ng::renderer::gfx::Texture
    // Annahme: materialId indiziert m_texture_names.
    // ======================================================================
    template <class T, class Resolver>
    size_t RestoreTextures(BspScene<T>& s, Resolver&& resolve)
    {
        printf("[RestoreTextures] %zu textures\n", s.m_texture_names.size());
        if (s.m_texture_names.empty()) return 0;

        std::vector<ng::renderer::gfx::Texture> cache(s.m_texture_names.size());
        std::vector<uint8_t> have(s.m_texture_names.size(), 0);

        auto get = [&](size_t i) -> ng::renderer::gfx::Texture& {
            if (!have[i]) { cache[i] = resolve(s.m_texture_names[i]); have[i] = 1; }
            return cache[i];
            };

        size_t total = 0, invalid = 0;

        auto fix = [&](ng::bsp::BSPPolygon<T>& p) {
            ++total;
            if (p.textureId < 0 || size_t(p.textureId) >= s.m_texture_names.size()) {
                ++invalid;
                p.texture = get(0);              // Ersatz: erste Textur, verdeckt den Fehler nur
                return;
            }
            p.texture = get(size_t(p.textureId));
            };

        std::vector<ng::bsp::BSPPolygon<T>*> out;
        s.m_bsp.CollectPolygonPointers(s.m_bsp.GetRoot(), out);
        for (auto* p : out) fix(*p);

        BspAccess::ForEachCellPoly(s.m_cells, fix);

        printf("[RestoreTextures] %zu Polygone, davon %zu ohne gueltige textureId\n", total, invalid);
        return invalid;
    }
    // ======================================================================
    // Speichern / Laden
    // ======================================================================
    template <class T>
    bool SaveBspScene(const std::string& path, const BspScene<T>& s) {
        std::ofstream os(path, std::ios::binary | std::ios::trunc);
        if (!os) return false;
        ChunkWriter w(os);
        w.Pod(kFileMagic);
        w.Pod(kFileVersion);

        auto chunk = [&](uint32_t id, uint32_t ver, auto&& fn) {
            w.BeginChunk(id, ver);
            fn();
            w.EndChunk();
            };
        chunk(CHUNK_BSP, V_BSP, [&] { Write(w, s.m_bsp); });
        chunk(CHUNK_PVS, V_PVS, [&] { Write(w, s.m_pvs); });
        chunk(CHUNK_CELLS, V_CELLS, [&] { Write(w, s.m_cells); });
        chunk(CHUNK_LMAP, V_LMAP, [&] { Write(w, s.m_lightmap); });
        chunk(CHUNK_WARP, V_WARP, [&] { Write(w, s.m_warpPortals); });
        chunk(CHUNK_TEX, V_TEX, [&] { WriteTextures(w, s.m_wadname, s.m_texture_names); });
        os.flush();
        return w.Good();
    }

    template <class T>
    bool LoadBspScene(const std::string& path, BspScene<T>& s) {
		printf("[BspScene] Loading %s\n", path.c_str());
        std::ifstream is(path, std::ios::binary | std::ios::ate);
        if (!is) return false;
        std::vector<uint8_t> file(size_t(is.tellg()));
        is.seekg(0);
        is.read(reinterpret_cast<char*>(file.data()), std::streamsize(file.size()));

        constexpr size_t kHeader = 8, kChunkHeader = 16;
        if (file.size() < kHeader) return false;
        uint32_t magic, fver;
        std::memcpy(&magic, file.data(), 4);
        std::memcpy(&fver, file.data() + 4, 4);
        if (magic != kFileMagic || fver > kFileVersion) return false;

        try {
            size_t pos = kHeader;
            while (pos + kChunkHeader <= file.size()) {
                uint32_t id, ver;
                uint64_t size;
                std::memcpy(&id, file.data() + pos, 4);
                std::memcpy(&ver, file.data() + pos + 4, 4);
                std::memcpy(&size, file.data() + pos + 8, 8);
                pos += kChunkHeader;
                if (size > file.size() - pos) return false;

                ChunkReader r(file.data() + pos, size_t(size), ver);
                switch (id) {
                case CHUNK_BSP:   Read(r, s.m_bsp); break;
                case CHUNK_PVS:   Read(r, s.m_pvs); break;
                case CHUNK_CELLS: Read(r, s.m_cells); break;
                case CHUNK_LMAP:  Read(r, s.m_lightmap); break;
                case CHUNK_WARP:  Read(r, s.m_warpPortals); break;
                case CHUNK_TEX:   ReadTextures(r, s.m_wadname, s.m_texture_names); break;
                default: break;   // unbekannter Chunk -> ueberspringen
                }
                pos += size_t(size);
            }
        }
        catch (const std::exception& e) {
            printf("[BspScene] Load error: %s\n", e.what());
            return false;
        }
        return true;
    }

} // namespace ng::io