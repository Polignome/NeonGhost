#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <cstdint>
#include <utility>
#include <functional>

#include "Map.h"    
#include "HalfLife.h"
#include "BSP.h"
#include <PVS.h>
#include "Wad3File.h"
#include <Radiosity.h>
#include <WarpPortal.h>

template <class T>
struct BspScene {
    ng::bsp::BSPTree<T> m_bsp;
    ng::bsp::PVS<NG_MATH_FLOAT> m_pvs;
    ng::bsp::VisCells<NG_MATH_FLOAT> m_cells;
	ng::renderer::buffers::PixelBufferRGBA m_lightmap;
    std::vector<ng::bsp::WarpPortal<NG_MATH_FLOAT>> m_warps;
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
inline void Compile(const std::string& map_filename, const ng::bsp::PVSOptions<T>& po, const ng::bsp::VisCellOptions<T>& vo,  ng::lightmap::LightmapOptions& lmOpt, const ng::lightmap::RadiosityOptions<T>& radOpt, BspScene<T>& sscen)
{

    BspScene<T> scene;

    ng::map::HalfLife hl_map;
    if (hl_map.load(map_filename) == ng::map::Parser::NO_PARSER_ERROR) {

        ng::map::Data<T> data;
        data = hl_map;

        ng::map::texture::Wad3File m_wad_file = data.GetTextureWad();;

        


        for (auto n : data.m_texture_names) {
            ConvertfromWad(m_wad_file, n);
        }


        auto brush = data.ExtractBrushes();
        std::vector< ng::map::Brush<T>> liquit_brushes;
        for (auto b : brush) {
            
            for (auto f: b.faces()) {
                if (f.aggregate() & ng::map::BrushFace<T>::LIQUID) {
                    liquit_brushes.push_back(b);
                    break;
                }
                
            }
        }
        
        brush = ng::map::csg::MakeUnion(brush);




        std::vector<ng::bsp::BSPPolygon<T>> polys;
        std::vector<ng::bsp::BSPPolygon<T>> extra_polys;







        auto soild_faces = ng::map::extract_prims<T>(brush);
        for (auto p : soild_faces) {
            auto name = data.m_id_text[p.textureIndex()];
            ng::renderer::gfx::Texture texture = ng::resource::TextureResourceManager::GetResource(name);
            for (size_t i = 1; i < p.verts().size() - 1; ++i)
            {
                ng::bsp::BSPPolygon<T> poly = ng::bsp::BSPPolygon<T>::FromTriangle(p.verts()[0], p.verts()[i], p.verts()[i + 1], 1);
                poly.texture = texture;
                poly.FinalizeTextureUVs();
                polys.push_back(poly);
            }
        }


        auto liquit_faces = ng::map::extract_prims<T>(brush, ng::map::BrushFace<T>::LIQUID);
        for (auto p : liquit_faces) {
            auto name = data.m_id_text[p.textureIndex()];
            ng::renderer::gfx::Texture texture = ng::resource::TextureResourceManager::GetResource(name);


            for (size_t i = 1; i < p.verts().size() - 1; ++i)
            {
                ng::bsp::BSPPolygon<T> poly = ng::bsp::BSPPolygon<T>::FromTriangle(p.verts()[0], p.verts()[i], p.verts()[i + 1], 0);
                poly.texture = texture;
                poly.FinalizeTextureUVs();
                extra_polys.push_back(poly);

                ng::bsp::BSPPolygon<T> poly2 = ng::bsp::BSPPolygon<T>::FromTriangle(p.verts()[i + 1], p.verts()[i], p.verts()[0], 0);
                poly2.texture = texture;
                poly2.FinalizeTextureUVs();
                extra_polys.push_back(poly2);
            }
        }


        auto aabb = ng::bsp::ComputeSceneBounds(polys);
        auto sky = ng::bsp::GenerateSkyboxPolygons(aabb, (T)100);


        for (auto p : sky)polys.push_back(p);

        scene.m_bsp.Build(polys);
        scene.m_bsp.InsertExtraPolygons(extra_polys);


        scene.m_bsp.BuildPortals();

        auto reached = scene.m_bsp.FloodFromFlag();            // Seeds = HAS_ILLEGAL_GEOMETRY, dann ueber alle Portale
        scene.m_bsp.RemoveReachedPolygons(reached);     // Polygone in allen erreichten Leaves loeschen

        auto visible = scene.m_bsp.CollectRemainingPolygons(); // Schritt 9
        auto visibleextra = scene.m_bsp.CollectRemainingExtraPolygons(); // Schritt 9
        ///////////////////////////////////////////////////////////////////////////////////////////////////

        // ---- Lightmap ZUERST ----
        lmOpt.startAtlasSize = std::max(1024, ng::lightmap::EstimateAtlasSize(visible, visibleextra, lmOpt));
        auto lights = ng::map::ToLightSources<T>(data.GetLights());

        if (lmOpt.buildDirectLight)
        {
            auto atlas = ng::lightmap::BuildLightmapAtlas(visible, scene.m_bsp, lights, lmOpt);
            if (atlas)
            {
                if (radOpt.buildDirectRadiosity)
                    ng::lightmap::BakeRadiosity(*atlas, visible, radOpt);

                scene.m_lightmap = std::move(atlas->Buffer());

                ng::lightmap::BlurAllLightmaps(*atlas, visible, 4);
               // ng::lightmap::BlurAllLightmaps(*atlas, visibleextra, 4);


            }
        }





        scene.m_cells.Build(visible, vo);
        scene.m_cells.InsertExtraPolygons(visibleextra);

        const auto& cellPolys = scene.m_cells.Polygons();
        for (size_t i = 0; i < std::min<size_t>(3, cellPolys.size()); ++i)
            printf("[Lightmap] in m_cells.Polygons(): poly=%zu v0.fxtexture=(%.4f,%.4f)\n",
                i, (double)cellPolys[i].verts[0].fxtexture().x(), (double)cellPolys[i].verts[0].fxtexture().y());


        scene.m_pvs.Build(scene.m_cells.Tree(), po);            // PVS auf den groben Zellen
        
		uint32_t index = 0;
        for (auto b: liquit_brushes) {
			auto faces = b.primitives_org_copy();
            std::vector<ng::bsp::BSPPolygon<NG_MATH_FLOAT>> polys;
           
            for (auto p : faces) {
                for (size_t i = 1; i < p.verts().size() - 1; ++i)
                {
                    ng::bsp::BSPPolygon<NG_MATH_FLOAT> poly = ng::bsp::BSPPolygon<NG_MATH_FLOAT>::FromTriangle(p.verts()[0], p.verts()[i], p.verts()[i + 1], 1);
                    polys.push_back(poly);
                }
			}
            scene.m_cells.Tree().InsertVolume(polys, (++index), ng::map::BrushFace<NG_MATH_FLOAT>::LIQUID);
        }
        
        


        
     sscen=std::move(scene);
    }


}






template <class T>
class MapCompiler
{
public:
  




    MapCompiler() = default;


    ~MapCompiler()
    {
        stop();
    }


    MapCompiler(const MapCompiler&) = delete;
    MapCompiler& operator=(const MapCompiler&) = delete;

    using CompleteCallback =
        std::function<void(bool, BspScene<T>&&)>;

    void setCompleteCallback(CompleteCallback callback)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        m_completeCallback = std::move(callback);
    }
    // ------------------------------------------------------------
    // Start
    // ------------------------------------------------------------

    void start(const std::string& map_filename,const ng::bsp::PVSOptions<T>&po,const ng::bsp::VisCellOptions<T>& vo, const ng::lightmap::LightmapOptions& lmOpt, const ng::lightmap::RadiosityOptions<T>& radOpt)
    {
        // Falls bereits ein Thread läuft:
        stop();

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_map_filename = map_filename;
            m_radOpt = radOpt;
            m_lmOpt = lmOpt;
			m_po = po;
			m_vo = vo;
            m_running = true;
            m_finished = false;
        }

        m_thread = std::thread(&MapCompiler::worker, this);
    }


    // ------------------------------------------------------------
    // Prüfen ob der Compiler noch läuft
    // ------------------------------------------------------------

    bool running() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_running;
    }


    // ------------------------------------------------------------
    // Prüfen ob Ergebnis vorhanden ist
    // ------------------------------------------------------------

    bool finished() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_finished;
    }


    // ------------------------------------------------------------
    // Warten bis fertig
    // ------------------------------------------------------------

    void wait()
    {
        std::unique_lock<std::mutex> lock(m_mutex);

        m_condition.wait(
            lock,
            [this]()
            {
                return m_finished;
            });

        lock.unlock();

        if (m_thread.joinable())
            m_thread.join();
    }


    // ------------------------------------------------------------
    // Ergebnis holen
    // ------------------------------------------------------------



    // ------------------------------------------------------------
    // Thread stoppen
    // ------------------------------------------------------------

    void stop()
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            m_stopRequested = true;
        }

        if (m_thread.joinable())
            m_thread.join();

        {
            std::lock_guard<std::mutex> lock(m_mutex);

            m_running = false;
            m_finished = false;
            m_stopRequested = false;
        }
    }


private:

    // ------------------------------------------------------------
    // Worker Thread
    // ------------------------------------------------------------

    std::string m_map_filename;
    ng::lightmap::LightmapOptions m_lmOpt;
    ng::lightmap::RadiosityOptions<T> m_radOpt;
    ng::bsp::PVSOptions<T> m_po;
    ng::bsp::VisCellOptions<T> m_vo;

    void worker()
    {

        std::string map_filename;
        ng::lightmap::LightmapOptions lmOpt;
        ng::lightmap::RadiosityOptions<T> radOpt;
        ng::bsp::PVSOptions<T> po;
        ng::bsp::VisCellOptions<T> vo;
        bool error_occurred = false;

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            map_filename = m_map_filename;
            lmOpt = m_lmOpt;
            radOpt = m_radOpt;
            po = m_po;
            vo = m_vo;
        }

        BspScene<T> scene;

        ng::map::HalfLife hl_map;
        if (hl_map.load(map_filename) == ng::map::Parser::NO_PARSER_ERROR) {

            ng::map::Data<NG_MATH_FLOAT> data;
            data = hl_map;

            ng::map::texture::Wad3File m_wad_file = data.GetTextureWad();;


            for (auto n : data.m_texture_names) {
                ConvertfromWad(m_wad_file, n);
            }


            auto brush = data.ExtractBrushes();
            brush = ng::map::csg::MakeUnion(brush);

            std::vector<ng::bsp::BSPPolygon<NG_MATH_FLOAT>> polys;
            std::vector<ng::bsp::BSPPolygon<NG_MATH_FLOAT>> extra_polys;







            auto soild_faces = ng::map::extract_prims<NG_MATH_FLOAT>(brush);
            for (auto p : soild_faces) {
                auto name = data.m_id_text[p.textureIndex()];
                ng::renderer::gfx::Texture texture = ng::resource::TextureResourceManager::GetResource(name);
                for (size_t i = 1; i < p.verts().size() - 1; ++i)
                {
                    ng::bsp::BSPPolygon<NG_MATH_FLOAT> poly = ng::bsp::BSPPolygon<NG_MATH_FLOAT>::FromTriangle(p.verts()[0], p.verts()[i], p.verts()[i + 1], 1);
                    poly.texture = texture;
                    poly.FinalizeTextureUVs();
                    polys.push_back(poly);
                }
            }


            auto liquit_faces = ng::map::extract_prims<NG_MATH_FLOAT>(brush, ng::map::BrushFace<NG_MATH_FLOAT>::LIQUID);
            for (auto p : liquit_faces) {
                auto name = data.m_id_text[p.textureIndex()];
                ng::renderer::gfx::Texture texture = ng::resource::TextureResourceManager::GetResource(name);


                for (size_t i = 1; i < p.verts().size() - 1; ++i)
                {
                    ng::bsp::BSPPolygon<NG_MATH_FLOAT> poly = ng::bsp::BSPPolygon<NG_MATH_FLOAT>::FromTriangle(p.verts()[0], p.verts()[i], p.verts()[i + 1], 0);
                    poly.texture = texture;
                    extra_polys.push_back(poly);

                    ng::bsp::BSPPolygon<NG_MATH_FLOAT> poly2 = ng::bsp::BSPPolygon<NG_MATH_FLOAT>::FromTriangle(p.verts()[i + 1], p.verts()[i], p.verts()[0], 0);
                    poly2.texture = texture;
                    extra_polys.push_back(poly2);
                }
            }


            auto aabb = ng::bsp::ComputeSceneBounds(polys);
            auto sky = ng::bsp::GenerateSkyboxPolygons(aabb, (NG_MATH_FLOAT)100);


            for (auto p : sky)polys.push_back(p);

            scene.m_bsp.Build(polys);
            scene.m_bsp.InsertExtraPolygons(extra_polys);


            scene.m_bsp.BuildPortals();

            auto reached = scene.m_bsp.FloodFromFlag();            // Seeds = HAS_ILLEGAL_GEOMETRY, dann ueber alle Portale
            scene.m_bsp.RemoveReachedPolygons(reached);     // Polygone in allen erreichten Leaves loeschen

            auto visible = scene.m_bsp.CollectRemainingPolygons(); // Schritt 9
            auto visibleextra = scene.m_bsp.CollectRemainingExtraPolygons(); // Schritt 9
            ///////////////////////////////////////////////////////////////////////////////////////////////////







            // ---- Lightmap ZUERST ----
            lmOpt.startAtlasSize = std::max(1024, ng::lightmap::EstimateAtlasSize(visible, visibleextra, lmOpt));
            auto lights = ng::map::ToLightSources<T>(data.GetLights());

            if (lmOpt.buildDirectLight)
            {
                auto atlas = ng::lightmap::BuildLightmapAtlas(visible, scene.m_bsp, lights, lmOpt);
                if (atlas)
                {
                    if (radOpt.buildDirectRadiosity)
                        ng::lightmap::BakeRadiosity(*atlas, visible, radOpt);

                    scene.m_lightmap = std::move(atlas->Buffer());
                }
            }

















            scene.m_cells.Build(visible, vo);
            scene.m_cells.InsertExtraPolygons(visibleextra);

            const auto& cellPolys = scene.m_cells.Polygons();
            for (size_t i = 0; i < std::min<size_t>(3, cellPolys.size()); ++i)
                printf("[Lightmap] in m_cells.Polygons(): poly=%zu v0.fxtexture=(%.4f,%.4f)\n",
                    i, (double)cellPolys[i].verts[0].fxtexture().x(), (double)cellPolys[i].verts[0].fxtexture().y());


            po.threads = 0;        // 0 = alle logischen Kerne (hardware_concurrency), sonst feste Zahl
            po.verbose = true;
            po.skipVoidLeaves = true;
            po.requirePolygonsInComponent = true;
            po.minPortalArea = 0.1;
            scene.m_pvs.Build(scene.m_cells.Tree(), po);            // PVS auf den groben Zellen


            ///////////////////////////////////////////////////////////////////////////////////////////////////
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_running = false;
                m_finished = true;
            }

            m_condition.notify_all();
            if (m_completeCallback)
                m_completeCallback(error_occurred, std::move(scene));
        }

    }
private:

    std::thread m_thread;

    mutable std::mutex m_mutex;
    std::condition_variable m_condition;


    uint32_t m_flags = 0;

    std::string  m_filename;


    //	bsp::Tree<T> m_bsp_tree;    

    bool m_running = false;
    bool m_finished = false;
    bool m_stopRequested = false;
    CompleteCallback m_completeCallback = nullptr;

};
