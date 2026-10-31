#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <cstdint>
#include <utility>
#include <functional>

#include <Map.h>    
#include <HalfLife.h>
#include <BSP.h>
#include <PVS.h>
#include "Wad3File.h"
#include <Radiosity.h>
#include <WarpPortal.h>
#include <Portalcompile.h>

template <class T>
struct BspScene {
    ng::bsp::BSPTree<T> m_bsp;
    ng::bsp::PVS<T> m_pvs;
    ng::bsp::VisCells<T> m_cells;
	ng::renderer::buffers::PixelBufferRGBA m_lightmap;
    std::vector<ng::bsp::WarpPortal<T>> m_warpPortals;

};

inline bool ConvertfromWad(const ng::map::texture::Wad3File& wad, const std::string& name)
{

    printf("[WAD] Load Texture %s", name.c_str());
    if (ng::resource::TextureResourceManager::IsResourceInManager(name)) return true;

    try {
        ng::map::texture::Wad3File::Texture texture = wad.GetTexture(name);

        // texture.pixels: width*height Palettenindizes (1 Byte/Pixel)
        // texture.palette: 256 * 3 Bytes (RGB), siehe WAD3-Format
        std::vector<uint8_t> rgba(texture.width * texture.height * 4);



        ng::resource::TextureResourceManager::LoadRawTextureFromMemory(
            name, (unsigned char*)texture.pixels.data(), texture.width, texture.height);

        printf("...OK\n");
    }
    catch (const std::runtime_error& error) {
        std::string help = error.what();
        printf("...ERROR\n");
        ng::resource::TextureResourceManager::LoadRawTextureFromMemory(name, nullptr, 0, 0);
        return false;
    }
    return true;
}

template <class T>
inline void Compile(const std::string& map_filename, const ng::bsp::PVSOptions<T>& po,
    const ng::bsp::VisCellOptions<T>& vo, ng::lightning::LightmapOptions& lmOpt,
    const ng::lightning::RadiosityOptions<T>& radOpt, BspScene<T>& sscen)
{
    using Vec3 = ng::math::Matrix<3, 1, T>;

    BspScene<T> scene;

    ng::map::HalfLife hl_map;
    if (hl_map.load(map_filename) == ng::map::Parser::NO_PARSER_ERROR) {
        ng::map::Data<T> data;
        data = hl_map;

        ng::map::texture::Wad3File m_wad_file = data.GetTextureWad();
        for (auto n : data.m_texture_names)
            ConvertfromWad(m_wad_file, n);

        auto brush = data.ExtractBrushes();
        auto liquit_brushes = data.ExtractLiquidBrushes();
        auto portal_brushes = data.ExtractPortalBrushes();

        printf("[MapCompiler] Extracted %zu solid brushes, %zu liquid brushes and %zu portal brushes\n",
            brush.size(), liquit_brushes.size(), portal_brushes.size());

        auto isTeleportTex = [&](int32_t t) {
            auto it = data.m_id_text.find(t);               // anpassen, falls m_id_text ein vector ist
            return it != data.m_id_text.end() && it->second == "~teleport";
            };

        // PORTAL: Portal-Face bestimmen + Portal-ID ins BrushPoly schreiben (VOR dem CSG)
        for (auto& pb : portal_brushes) { pb.ResolvePortalFace(isTeleportTex); pb.ApplyPortalId(); }

        // PORTAL: WarpPortals (Koordinatensystem + Partner). Kein Hull mehr noetig.
        std::vector<ng::bsp::BSPPolygon<T>> unused;
        scene.m_warpPortals = BuildWarpPortals<T>(portal_brushes, isTeleportTex, unused, Vec3(0, 1, 0));

        for (auto& pb : portal_brushes)
            printf("[Portal] id=%u translucent=%.2f\n", pb.PortalSetting().id, (double)pb.PortalSetting().translucent);
        for (auto& w : scene.m_warpPortals)
            printf("[Portal] warp id=%d see=%.2f flags=%u\n", w.id, (double)w.seeThrough, w.flags);


        // PORTAL: Map-ID -> Index in m_warpPortals
        std::unordered_map<int32_t, int32_t> idToIndex;
        for (size_t i = 0; i < scene.m_warpPortals.size(); ++i)
            idToIndex[scene.m_warpPortals[i].id] = (int32_t)i;

        // PORTAL: sichtbare Portal-Brushes laufen wie normale Brushes durch CSG/BSP
        for (auto& pb : portal_brushes)
            if (pb.PortalSetting().visible && pb.isPortal())
                brush.push_back(pb);

        brush.insert(brush.end(), liquit_brushes.begin(), liquit_brushes.end());
        auto origBrushes = brush;
        brush = ng::map::csg::MakeUnion(brush);

        std::vector<ng::bsp::BSPPolygon<T>> polys;
        std::vector<ng::bsp::BSPPolygon<T>> extra_polys;

        size_t portalPolyCount = 0;   // PORTAL

        auto soild_faces = ng::map::extract_prims<T>(brush);
        for (auto p : soild_faces) {
            auto name = data.m_id_text[p.textureIndex()];
            ng::renderer::gfx::Texture texture = ng::resource::TextureResourceManager::GetResource(name);

            // PORTAL: Fragment-Markierung bestimmen
            int32_t portalIndex = -1;
            if (p.portalId() >= 0)
            {
                auto it = idToIndex.find(p.portalId());
                if (it != idToIndex.end() &&
                    (scene.m_warpPortals[it->second].flags & ng::bsp::WARP_RENDER))
                    portalIndex = it->second;
            }

            for (size_t i = 1; i < p.verts().size() - 1; ++i)
            {
                auto poly = ng::bsp::BSPPolygon<T>::FromTriangle(p.verts()[0], p.verts()[i], p.verts()[i + 1], 1);
                poly.texture = texture;
                poly.FinalizeTextureUVs();

                if (portalIndex >= 0)   // PORTAL
                {
                    poly.flags |= ng::bsp::POLY_FLAG_PORTAL;
                    poly.portalId = portalIndex;
                    ++portalPolyCount;
                }
                polys.push_back(poly);
            }
        }

        auto liquit_faces = ng::map::extract_prims<T>(brush, ng::map::BrushFace<T>::LIQUID);
        for (auto p : liquit_faces) {
            auto name = data.m_id_text[p.textureIndex()];
            ng::renderer::gfx::Texture texture = ng::resource::TextureResourceManager::GetResource(name);
            for (size_t i = 1; i < p.verts().size() - 1; ++i)
            {
                auto poly = ng::bsp::BSPPolygon<T>::FromTriangle(p.verts()[0], p.verts()[i], p.verts()[i + 1], 0);
                poly.texture = texture;
                poly.FinalizeTextureUVs();
                extra_polys.push_back(poly);

                auto poly2 = ng::bsp::BSPPolygon<T>::FromTriangle(p.verts()[i + 1], p.verts()[i], p.verts()[0], 0);
                poly2.texture = texture;
                poly2.FinalizeTextureUVs();
                extra_polys.push_back(poly2);
            }
        }


        
        printf("[MapCompiler] %zu Warp-Portale, %zu Portal-Fragment-Dreiecke\n",
            scene.m_warpPortals.size(), portalPolyCount);
        for (size_t i = 0; i < scene.m_warpPortals.size(); ++i)
        {
            const auto& w = scene.m_warpPortals[i];
            printf("[MapCompiler] Portal[%zu] id=%d partner=%d flags=%u Durchlässigkeit %.2f%%\n", i, w.id, w.partner, w.flags, w.seeThrough);
        }
        printf("[MapCompiler] Extracted %zu solid faces and %zu liquid faces\n",
            soild_faces.size(), liquit_faces.size());

        auto aabb = ng::bsp::ComputeSceneBounds(polys);
        auto sky = ng::bsp::GenerateSkyboxPolygons(aabb, (T)100);
        for (auto p : sky) polys.push_back(p);


        ng::bsp::SmoothNormals(polys);

        
        scene.m_bsp.Build(polys);
        scene.m_bsp.FixTJunctions();
        scene.m_bsp.InsertExtraPolygons(extra_polys);
        scene.m_bsp.BuildPortals();

        auto reached = scene.m_bsp.FloodFromFlag();
        scene.m_bsp.RemoveReachedPolygons(reached);

        auto visible = scene.m_bsp.CollectRemainingPolygons();
        auto visibleextra = scene.m_bsp.CollectRemainingExtraPolygons();

        // ---- Lightmap (unveraendert) ----
        lmOpt.startAtlasSize = std::max(1024, ng::lightning::EstimateAtlasSize(visible, visibleextra, lmOpt));
        auto lights = ng::map::ToLightSources<T>(data.GetLights());

        if (lmOpt.buildDirectLight || radOpt.buildDirectRadiosity)
        {
            auto ll = lights;
            if (!lmOpt.buildDirectLight) ll.clear();

            auto atlas = ng::lightning::BuildLightmapAtlas(visible, scene.m_bsp, ll, lmOpt);
            if (atlas)
            {
                if (radOpt.buildDirectRadiosity)
                    ng::lightning::BakeRadiosity(*atlas, visible, radOpt);

                scene.m_lightmap = std::move(atlas->Buffer());
                ng::lightning::BlurAllLightmaps(*atlas, visible, 4);
            }
        }

        scene.m_cells.Build(visible, vo);
        scene.m_cells.InsertExtraPolygons(visibleextra);
        scene.m_pvs.Build(scene.m_cells.Tree(), po);
        
        // PORTAL: Sicht DURCH ein Portal = PVS der Zelle direkt vor dem Partner (B)
        for (auto& wp : scene.m_warpPortals)
        {
            wp.visCells.clear();
            if (wp.partner < 0) continue;

            const auto& B = scene.m_warpPortals[wp.partner];
            const Vec3 p = B.origin + B.normal * T(8);          // Punkt sicher im Raum vor B
            const int32_t cell = scene.m_cells.FindCell(p);

            if (scene.m_pvs.IsActiveLeaf(cell))
                wp.visCells = scene.m_pvs.VisibleLeaves(cell);
            else
                printf("[Portal] %d: Zelle vor Partner nicht aktiv (cell=%d)\n", wp.id, cell);
        }
        for (size_t i = 0; i < scene.m_warpPortals.size(); ++i)
            printf("Portal[%zu] visCells=%zu\n", i, scene.m_warpPortals[i].visCells.size());

        // Volumen (unveraendert)
        uint32_t index = 0;
        for (auto b : liquit_brushes) {
            auto faces = b.primitives_org_copy();
            std::vector<ng::bsp::BSPPolygon<NG_MATH_FLOAT>> vpolys;
            for (auto p : faces)
                for (size_t i = 1; i < p.verts().size() - 1; ++i)
                    vpolys.push_back(ng::bsp::BSPPolygon<NG_MATH_FLOAT>::FromTriangle(
                        p.verts()[0], p.verts()[i], p.verts()[i + 1], 1));
            scene.m_cells.Tree().InsertVolume(vpolys, (++index),
                ng::map::BrushFace<NG_MATH_FLOAT>::LIQUID);
        }

        sscen = std::move(scene);
    }
}