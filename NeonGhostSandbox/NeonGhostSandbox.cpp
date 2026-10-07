#include <cmath>

#include "imgui.h"
#include <vector>
#include <string>
#include <filesystem>
#include <iostream>
#include <limits>
#include <random>
#include <cstdint>
#include <Global.h>
#include "GuiStyle.h"
#include "ImGuiTools.h"
#include "InitImGui.h"
#include <NeonGhostCore.h>
#include "RenderTargetView.h"
#include "Logo800x600.h"
#include "Frustum.h"
#include "NGDemos.h"
#include <list>
#include "FPSCounter.h"
#include "HalfLife.h"
#include <Bspcollision.h>
#include <Map.h>
#include <Lightmap.h>
#include <Radiosity.h>
#include "MapCompiler.h"

#pragma comment(lib,"SDL2.lib")
#pragma comment(lib,"SDL2main.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "NeonGhostCore.lib")
#pragma comment(lib, "FreeImage.lib")

NG_BIND_MANAGER

#define NG_USE_STATIC_MESH_ONLY

const size_t MAX_POLY_COUNTS = 100;   

//https://lovable.dev/projects/ab425dac-45f9-4c5c-8222-af5664f071fb
//https://claude.ai/chat/8116e964-c2d9-40e2-bcfa-ed126458b56d

static ng::tools::FPSCounter m_fps_counter;

static ng::renderer::gfx::Texture m_bsp_lightmap;

static ng::renderer::postfx::PostFXManager<NG_MATH_FLOAT> mgr;

static auto* water = mgr.AddEffect<ng::renderer::postfx::UnderwaterWarpEffect<NG_MATH_FLOAT>>();

static auto* blur = mgr.AddEffect<ng::renderer::postfx::BlurEffect<NG_MATH_FLOAT>>(0.5f, 2);


// static auto* water = mgr.AddEffect<ng::renderer::postfx::DepthFogEffect<NG_MATH_FLOAT>>();

// static auto* fog = mgr.AddEffect<ng::renderer::postfx::DepthFogEffect<NG_MATH_FLOAT>>();
//static auto* snow =mgr.AddEffect < ng::renderer::postfx::SnowEffect<NG_MATH_FLOAT>>();

static MapCompiler<NG_MATH_FLOAT> m_map_compiler;

static std::vector < ng::map::BrushPoly<NG_MATH_FLOAT>> m_map_brush_polys;
std::string VFSFilePicker(bool& show, std::string wildcard) {
    if (!show) return "";
    static std::filesystem::path p = std::filesystem::current_path();
    static std::string filename = "";
    static std::string mount = "";
    std::string path = p.string();

    static std::string file_name = "";
    static std::string entry = "";
    ImGui::OpenPopup("VFSFilePicker");

    std::string change_drive;
    bool parent_dir = false;


    auto mount_points = ng::io::vfs::VFSFileManager::GetVFS().MountPoints();
    

    if (ImGui::BeginPopupModal("VFSFilePicker", &show))
    {
        ImVec2 wsize = ImGui::GetWindowSize();
        ImVec2 pos = ImGui::GetCursorPos();

        float width = (wsize.x / 3);
        if (width < 100) width = 100;
        float height = (wsize.y - pos.y - 50);
        if (ng::io::vfs::VFSFileManager::Exists("/"+mount)) ImGui::Text("Hallo");
//        ImGui::Text("Mount: %s  Etriys %i", mount.c_str(), ng::io::vfs::VFSFileManager::List(mount).size());

        ImGui::BeginChild("VFSDirList", ImVec2(width, height), ImGuiChildFlags_Borders);
        for (auto m : mount_points) {
            if (ImGui::Selectable(m.name.c_str(), mount == m.name)) mount = m.name;
        }
        ImGui::EndChild();
        
        

        if (ImGui::Button("Select")) {
            show = false;
        }

        ImGui::SameLine();
        if (ImGui::Button("Chancel")) show = false;
        ImGui::SameLine();


    }
    ImGui::EndPopup();

    return "";
}



void LoadMapReady(bool error, BspScene<NG_MATH_FLOAT>&& scene) {
    if (error) {
        std::cout << "Error loading map" << std::endl;
        return;
    }
    m_bsp_scene = std::move(scene);

    ng::resource::TextureResourceManager::LoadTextureFromPixelBuffer("__lightmap_atlas__", m_bsp_scene.m_lightmap);
    m_bsp_scene.m_lightmap.Resize(0, 0);
    m_bsp_lightmap = ng::resource::TextureResourceManager::GetResource("__lightmap_atlas__");

}

inline void Render() {
    static double totalTime = 0.0;
    static ng::math::Matrix<3, 1, NG_MATH_FLOAT> m_cam_old_pos;
    static ng::math::Matrix<3, 1, NG_MATH_FLOAT> m_cam_pos;

    double deltaTime = ImGui::GetIO().DeltaTime;
    totalTime += deltaTime;                      // <-- NEU: aufaddieren statt Delta direkt nutzen

//    blur->SetStrength(6.0f);   // z.B. fuer Pause-Menue oder Unterwasser
    
    // erwartet: ≈ (0,0,0,1)
     static bool first_time = true;
    if (first_time) {
        first_time = false;
        mgr.SetSize(m_rasterizer_3d.Width(), m_rasterizer_3d.Height());
        mgr.BindColor(m_rasterizer_3d.GetColorBuffer().GetRawBuffer());
        mgr.BindDepth(m_rasterizer_3d.GetDepthBuffer().GetRawBuffer());
        m_cam_old_pos =  m_camera.position();
    }


    m_rasterizer_3d.GetMOCBufferRef().Clear();  
    
    m_rasterizer_3d.State().detph = ng::renderer::rasterizer::DEPTHFUNC_LESS;
    m_rasterizer_3d.State().flatColor = RGB(255, 255, 0);
    m_rasterizer_3d.State().flags = ng::renderer::rasterizer::RASTER_DEPTH_TEST | ng::renderer::rasterizer::RASTER_DEPTH_WRITE | ng::renderer::rasterizer::RASTER_TEXTURE;// | ng::renderer::rasterizer::RASTER_VERTEXCOLOR;

    ng::renderer::gfx::Image test = ng::resource::ImageResourceManager::GetResource("Image1", true);
  
   
    m_rasterizer_3d.GetColorBuffer().Clear(RGB(0, 0, 0));

    m_render_pipeline_3d.ModelViewMatrix() = m_camera.ViewMatrix();
    m_render_pipeline_3d.ProjectionMatrix() = m_camera.ProjectionMatrix();
    m_render_pipeline_3d.SetCamera(m_camera);

    m_fps_counter.Update();
    m_render_pipeline_3d.Begin();
         
    
    m_rasterizer_3d.State().flags = ng::renderer::rasterizer::RASTER_DEPTH_TEST | ng::renderer::rasterizer::RASTER_DEPTH_WRITE /* | ng::renderer::rasterizer::RASTER_VERTEXCOLOR*/ | ng::renderer::rasterizer::RASTER_TEXTURE;
    if (m_use_lightmap) m_rasterizer_3d.State().flags |= ng::renderer::rasterizer::RASTER_LIGHTMAP;



    uint32_t medium=m_render_pipeline_3d.RenderWorld(m_bsp_scene.m_cells, m_bsp_scene.m_pvs, m_bsp_lightmap);
    
    m_rasterizer_3d.State().lmCorrection = &m_lmCorr;


    m_rasterizer_3d.State().flags = ng::renderer::rasterizer::RASTER_DEPTH_TEST | ng::renderer::rasterizer::RASTER_DEPTH_WRITE | ng::renderer::rasterizer::RASTER_VERTEXCOLOR;
    

    for (auto m : m_mesh_obj)
        m_render_pipeline_3d.RenderMesh(m);


    m_render_pipeline_3d.End();
       

//    if (m_use_after_fx)mgr.Apply(totalTime);
    if (medium & ng::map::BrushFace<NG_MATH_FLOAT>::LIQUID) mgr.Apply(totalTime);

  
    float inp = (float)m_render_pipeline_3d.GetStatistics().polys_in;
    float outp = (float)m_render_pipeline_3d.GetStatistics().polys_rendered;
    float fps = (float)m_fps_counter.FPS();
    float cc = (float)m_render_pipeline_3d.GetStatistics().cells_with_geometry;
    if (inp > m_max_in_polys) m_max_in_polys = inp;
    if (outp > m_max_out_polys) m_max_out_polys = outp;
    if (fps > m_max_fps) m_max_fps = fps;
    if (cc > m_max_out_cells) m_max_out_cells = cc;


    addValue(m_in_polycount, inp, MAX_POLY_COUNTS);
    addValue(m_out_polycount, outp, MAX_POLY_COUNTS);
    addValue(m_fps_count, (float)m_fps_counter.FPS(), MAX_POLY_COUNTS);
    addValue(m_out_cellscount, cc, m_render_pipeline_3d.GetStatistics().cells_in_pvs);
   
    

}


template<typename T>
std::vector<ng::math::Matrix<3, 1, T>>
DistributeObjectsGrid(
    T size,
    size_t count,
    T y = static_cast<T>(0))
{
    std::vector<ng::math::Matrix<3, 1, T>> result;

    if (count == 0)
        return result;

    size_t dimension =
        static_cast<size_t>(
            std::ceil(std::sqrt(
                static_cast<double>(count)
            ))
            );

    result.reserve(count);

    T cellSize =
        size / static_cast<T>(dimension);

    T half = size * static_cast<T>(0.5);

    std::random_device rd;
    std::mt19937 gen(rd());

    std::uniform_real_distribution<T> jitter(
        -cellSize * static_cast<T>(0.35),
        cellSize * static_cast<T>(0.35)
    );

    for (size_t z = 0; z < dimension; ++z)
    {
        for (size_t x = 0; x < dimension; ++x)
        {
            if (result.size() >= count)
                return result;

            T px =
                -half +
                (static_cast<T>(x) + static_cast<T>(0.5))
                * cellSize;

            T pz =
                -half +
                (static_cast<T>(z) + static_cast<T>(0.5))
                * cellSize;

            ng::math::Matrix<3, 1, T> pos;

            pos(0) = px + jitter(gen);
            pos(1) = y;
            pos(2) = pz + jitter(gen);

            result.push_back(pos);
        }
    }

    return result;
}


template<typename T>
std::vector<ng::math::Matrix<3, 1, T>>
DistributeObjectsInCube(
    T size,
    size_t count)
{
    std::vector<ng::math::Matrix<3, 1, T>> result;
    result.reserve(count);

    if (count == 0 || size <= static_cast<T>(0))
        return result;

    std::random_device rd;
    std::mt19937 gen(rd());

    T half = size * static_cast<T>(0.5);

    std::uniform_real_distribution<T> dist(
        -half,
        half
    );

    for (size_t i = 0; i < count; ++i)
    {
        ng::math::Matrix<3, 1, T> pos;

        pos(0) = dist(gen); // X
        pos(1) = dist(gen); // Y
        pos(2) = dist(gen); // Z

        result.push_back(pos);
    }

    return result;
}

void InitStuff() {


    m_camera.aspect() = (double)m_rasterizer_3d.Width() / (double)m_rasterizer_3d.Height();
    m_camera.fov() = ng::math::degreesToRadians(64.0);
    m_camera.nearp() = 0.0001;
    m_camera.farp() = 18000.0;
    m_camera.position() = ng::math::Matrix<3, 1, NG_MATH_FLOAT>(0, 0, 0);
    m_camera.LookAt(ng::math::Matrix<3, 1, NG_MATH_FLOAT>(0, 0, 0));
    
    // einmalig
    m_player.Init(ng::math::Matrix<3, 1, NG_MATH_FLOAT>(0, 0, 0), /*eyeHeight*/ 28);
}




int main(int argc, char** argv) {
    static bool m_pick_vfs_file = false;
    static bool m_load_obj = false;
    static bool m_load_hlm = false;
    static bool m_build_lightmaps = false;
    static bool m_build_radiosity = false;
    
    
    if (InitImGui()) return 1;
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGuiStyle& style = ImGui::GetStyle();
    InitStuff();
    
    for (size_t i = 0; i < 800*600; i++) {

        size_t h = i * 3;
        uint8_t r = Logo800x600_map[h+0];
        uint8_t g = Logo800x600_map[h+1];
        uint8_t b = Logo800x600_map[h+2];
           m_bixelbuffer.GetRawBuffer()[i] = RGB(b,r,g);
    }
    static RenderPixelbuffer m_logo_renderer(m_bixelbuffer.Width(), m_bixelbuffer.Height());


    m_rasterizer_3d.SetTiledRenderingEnabled(false);
    m_rasterizer_3d.SetTiledSingleThreadedDebug(false);  // erst so, gegen den alten Pfad vergleichen
    

    // Main loop

    bool done = false;
    while (!done)
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT) {
                m_exit_program = true; 
                continue;
            }
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(GetWindow()))
            {
                m_exit_program = true;
                continue;
            }
            ImGui_ImplSDL2_ProcessEvent(&event);

            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_RESIZED && event.window.windowID == SDL_GetWindowID(GetWindow()))
            {
                // Release all outstanding references to the swap chain's buffers before resizing.
                CleanupRenderTarget();
                ResizeSwapChainBuffer();
                    
                CreateRenderTarget();
            }
            
        }
        if (SDL_GetWindowFlags(GetWindow()) & SDL_WINDOW_MINIMIZED)
        {
            SDL_Delay(10);
            continue;
        }
                
        Render();
   


        // Start the Dear ImGui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        bool is_loading = m_map_compiler.running();
        
        {
            float x = ImGui::GetIO().DisplaySize.x + 20;
            float y = ImGui::GetIO().DisplaySize.y + 20;

            ImGui::SetNextWindowPos(ImVec2(-10, 10));
            ImGui::SetNextWindowSize(ImVec2(x, y));

            if (ImGui::Begin("DesktopOverlay",
                nullptr,
                ImGuiWindowFlags_NoDecoration |
                ImGuiWindowFlags_NoBackground |
                ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoBringToFrontOnFocus |
                ImGuiWindowFlags_NoMove)) {

                if (!m_view_render_window && m_view_render_fullscreen) {
                    static RenderPixelbuffer  m_soft_renderer_3d(m_rasterizer_3d.Width(), m_rasterizer_3d.Height());
                    m_soft_renderer_3d.Update(m_rasterizer_3d.GetColorBuffer().GetRawBuffer());

                    UpdateRenderMovement();
                    ImVec2 cpos = ImGui::GetCursorPos();
                    m_logo_renderer.Update(m_bixelbuffer);

                    ImGui::Image(m_soft_renderer_3d.GetImTextureID(), ImVec2(x, y));
                    cpos.x = x - 420;
                    cpos.y += 20;
                    ImGui::SetCursorPos(cpos);
                    SomeDebugStuff();

                } else
                ImGui::Image(m_logo_renderer.GetImTextureID(), ImVec2(x, y));
                ImGui::End();
            }
        }

        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Load Waveform Obj")) m_load_obj = true;
                if (ImGui::MenuItem("Load HalfLifeMap",0,false,!is_loading)) m_load_hlm = true;
                if (ImGui::MenuItem("Quit")) m_exit_program = true;
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Compiler")) {
                if (ImGui::MenuItem("Lighmaps", 0, m_build_lightmaps)) m_build_lightmaps = !m_build_lightmaps;
                if (!m_build_lightmaps)m_build_radiosity = false;
                if (ImGui::MenuItem("Radiosity", 0, m_build_radiosity, m_build_lightmaps)) m_build_radiosity = !m_build_radiosity;
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Tools")) {
                bool temp = m_tools_font_window;
                if (ImGui::MenuItem("View Font")) m_tools_font_window = true;
                temp = m_resource_window_visible;
                if (ImGui::MenuItem("Resourcemanager", 0, &temp)) m_resource_window_visible = !m_resource_window_visible;
                temp = m_pick_vfs_file;
                if (ImGui::MenuItem("Virtual Filesystem Browser", 0, &temp)) m_pick_vfs_file = true;

                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Demos")) {
                bool temp =false;
                if (ImGui::BeginMenu("2D")) {

                    if (ImGui::MenuItem("Basic 2D Demo", 0, &temp)) m_2d_demo_window_visible = !m_2d_demo_window_visible;
                    temp = m_sprite_demo_window_visible;
                    if (ImGui::MenuItem("Sprite 2D Demo", 0, &temp)) m_sprite_demo_window_visible = !m_sprite_demo_window_visible;
                    temp = m_animatet_sprite_demo_window_visible;
                    if (ImGui::MenuItem("Animatet Sprite 2D Demo", 0, &temp)) m_animatet_sprite_demo_window_visible = !m_animatet_sprite_demo_window_visible;
                    temp = m_postfx_demo_window_visible;
                    if (ImGui::MenuItem("PostFx Demo", 0, &temp)) m_postfx_demo_window_visible = !m_postfx_demo_window_visible;
                    temp = m_image_demo_window_visible;
                    if (ImGui::MenuItem("Image Demo", 0, &temp)) m_image_demo_window_visible = !m_image_demo_window_visible;
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("3D")) {
                    temp = m_view_render_window;
                    if (ImGui::MenuItem("Renderer Windowed", 0, &temp)) {m_view_render_window = !m_view_render_window; if (m_view_render_window) m_view_render_fullscreen = false;}
                    temp = m_view_render_fullscreen;
                    if (ImGui::MenuItem("Renderer Fullscreen", 0, &temp)) {m_view_render_fullscreen = !m_view_render_fullscreen; if (m_view_render_fullscreen) m_view_render_window = false;}
                    temp = m_HZB_window_visible;
                    if (ImGui::MenuItem("Hierarchical ZBuffer", 0, &temp)) m_HZB_window_visible = !m_HZB_window_visible;
                    temp = m_3d_info_window_visible;
                    if (ImGui::MenuItem("Renderer Debug", 0, &temp)) m_3d_info_window_visible = !m_3d_info_window_visible;
                    temp = m_masked_occlusion_buffer_window_visible;
                    if (ImGui::MenuItem("Masked Occlusion Buffer", 0, &temp)) m_masked_occlusion_buffer_window_visible = !m_masked_occlusion_buffer_window_visible;
                    ImGui::EndMenu();
                }
                ImGui::EndMenu();

            }

            if (m_view_render_fullscreen) AddRenderMenuSubMenue();


            int size = 100;
            ImGui::SameLine(
                ImGui::GetWindowWidth() - size - ImGui::GetStyle().ItemSpacing.x
            );
            std::string s = std::to_string((int)m_fps_counter.FPS()) + std::string("FPS");
            ImGui::PlotLines("##FPS Counter", m_fps_count.data(), (int)m_out_polycount.size(), 0,s.c_str(), 0.0f, m_max_fps, ImVec2((float)size, ImGui::GetWindowHeight()));
                        ImGui::EndMainMenuBar();
            
        }
                        
        if (m_pick_vfs_file) VFSFilePicker(m_pick_vfs_file, "");
        if (m_exit_program) done = ng::gui::ExitPopup(m_exit_program);
        if (m_tools_font_window) ng::gui::ShowFontViewer(m_tools_font_window);
        if (m_view_render_window) RenderWindow(m_view_render_window);
        if (m_HZB_window_visible) HierarchicalZBufferWindow(m_HZB_window_visible);
        if (m_2d_demo_window_visible) Render2DWindow(m_2d_demo_window_visible);
        if (m_sprite_demo_window_visible) RenderSpriteWindow(m_sprite_demo_window_visible);
        if (m_animatet_sprite_demo_window_visible) RenderBouncingSpritesDemo(m_animatet_sprite_demo_window_visible);
        if (m_animatet_sprite_demo_window_visible) RenderBouncingSpritesDemo(m_animatet_sprite_demo_window_visible);
        if (m_postfx_demo_window_visible) RenderPostFxeWindow(m_postfx_demo_window_visible);
        if (m_image_demo_window_visible) RenderImageWindow(m_image_demo_window_visible);
        if (m_resource_window_visible)ResourcemanagerWindow(m_resource_window_visible);
        if (m_3d_info_window_visible)RenderDebugInfoWindow(m_3d_info_window_visible);
		if (m_masked_occlusion_buffer_window_visible)RenderMocWindow(m_masked_occlusion_buffer_window_visible); 

        if (m_load_hlm & !is_loading) {
            std::string filename = ng::gui::FilePicker(m_load_hlm, "*.map");
            if (!filename.empty()) {
			

                ng::lightmap::LightmapOptions lmOpt;
                lmOpt.buildDirectLight = m_build_lightmaps;
                lmOpt.texelDensity = 2.0;        // feiner als der HL-Standard (16.0)
                lmOpt.maxLightmapSize = 128;     // groessere Patches erlauben
                lmOpt.maxAtlasSize = 8192;
                lmOpt.kAmbientFloor = 20;
                lmOpt.onlyCoveredTexels = true;
                lmOpt.coverageMargin = 1.0;

                ng::lightmap::RadiosityOptions<NG_MATH_FLOAT> radOpt;
                radOpt.buildDirectRadiosity = m_build_radiosity;
                radOpt.bounces = 4;
                radOpt.samplesPerPatch = 32;
                radOpt.blockSize = 2;
                radOpt.albedoMin = 0.15;                       // zum Testen heller (vorher 0.05)
                radOpt.albedoMax = 0.85;
                radOpt.textureEmissionStrength = 0;            // WICHTIG, siehe unten
                radOpt.ambientFloor = lmOpt.kAmbientFloor;     // = 20, gleicher Wert wie beim direkten Licht
                radOpt.indirectGain = 4.0;                     // zum Testen, spaeter runter
                radOpt.indirectGain = 20.0;
                
                ng::bsp::PVSOptions<NG_MATH_FLOAT> po;
                po.threads = 0;        // 0 = alle logischen Kerne (hardware_concurrency), sonst feste Zahl
                po.verbose = true;
                po.skipVoidLeaves = true;
                po.requirePolygonsInComponent = true;
                po.minPortalArea = 64;
               

                ng::bsp::VisCellOptions<NG_MATH_FLOAT> vo;
                vo.minPlaneArea = 4096.0f;          // hier stellst du die Groebe ein

                //m_map_compiler.setCompleteCallback(LoadMapReady);
                //m_map_compiler.start(filename,po,vo, lmOpt,radOpt );

                Compile(filename, po, vo, lmOpt, radOpt, m_bsp_scene);

                ng::resource::TextureResourceManager::LoadTextureFromPixelBuffer("__lightmap_atlas__", m_bsp_scene.m_lightmap);
                m_bsp_scene.m_lightmap.Resize(0, 0);
                m_bsp_lightmap = ng::resource::TextureResourceManager::GetResource("__lightmap_atlas__");
            }
            
        }
                
        if (is_loading) {
            ImGui::BeginTooltip();
			ImGui::Text("Compiling Map...");
            ImGui::EndTooltip();
          }

                
        ImGui::Render();
        const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
        SwapScreen(clear_color_with_alpha);
    }

    
	m_bsp_scene = BspScene<NG_MATH_FLOAT>();  // Reset BSP scene to free memory    
	m_bsp_lightmap = ng::renderer::gfx::Texture();  // Reset lightmap texture to free memory
    ng::resource::Manager::Flush();


	ShutdownImGui();
    return 0;
}




