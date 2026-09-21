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
#include <Map.h>

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



static ng::renderer::postfx::PostFXManager<NG_MATH_FLOAT> mgr;
//static auto* fog = mgr.AddEffect<ng::renderer::postfx::DepthFogEffect<NG_MATH_FLOAT>>();
static auto* snow =mgr.AddEffect < ng::renderer::postfx::SnowEffect<NG_MATH_FLOAT>>();

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
        /*
        ImGui::SameLine();
        
        
        
        auto entrys = ng::io::vfs::VFSFileManager::List(mount);

        ImGui::BeginChild("VFSFileList", ImVec2(width, height), ImGuiChildFlags_Borders);
        for (auto e : entrys) {
            if (ImGui::Selectable(e.name.c_str(), entry == e.name)) entry = e.name;
        }
        ImGui::EndChild();
        */

        

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


inline void RenderNode(const std::shared_ptr<ng::bsp::BSPNode<NG_MATH_FLOAT>> &node) {
    if (node == nullptr) return;

    
    if (node->isLeaf()) {
        for (auto p : node->GetPolygons()) {
            m_rasterizer_3d.State().flags = ng::renderer::rasterizer::RASTER_DEPTH_TEST | ng::renderer::rasterizer::RASTER_DEPTH_WRITE | ng::renderer::rasterizer::RASTER_VERTEXCOLOR;
            m_render_pipeline_3d.RenderPolygon(p.verts);
        }
    }
    



    if (node->GetFrontNode() != nullptr) RenderNode(node->GetFrontNode());
    if (node->GetBackNode() != nullptr) RenderNode(node->GetBackNode());

}


inline void Render() {
    static double totalTime = 0.0;
    static ng::math::Matrix<3, 1, NG_MATH_FLOAT> m_cam_old_pos;
    static ng::math::Matrix<3, 1, NG_MATH_FLOAT> m_cam_pos;
    static ng::math::Matrix<3, 1, NG_MATH_FLOAT> radius(160.0 * 10, 360.0 * 10, 360.0* 10);

    double deltaTime = ImGui::GetIO().DeltaTime;
    totalTime += deltaTime;                      // <-- NEU: aufaddieren statt Delta direkt nutzen

    static auto* water = mgr.AddEffect<ng::renderer::postfx::DepthFogEffect<NG_MATH_FLOAT>>();
      
    // erwartet: ≈ (0,0,0,1)

    static bool first_time = true;
    if (first_time) {
        first_time = false;
        mgr.SetSize(m_rasterizer_3d.Width(), m_rasterizer_3d.Height());
        mgr.BindColor(m_rasterizer_3d.GetColorBuffer().GetRawBuffer());
        mgr.BindDepth(m_rasterizer_3d.GetDepthBuffer().GetRawBuffer());
        m_cam_old_pos =  m_camera.position();
    }
	

    {
        m_cam_pos = m_camera.position();

        const auto delta =
            m_cam_pos - m_cam_old_pos;

        snow->SetCameraDelta(delta);
        snow->SetCamera(m_camera.forward(), m_camera.up());
        snow->SetFOV(
            ng::math::radiansToDegrees(m_camera.fov()));
        snow->SetAspect(
            m_camera.aspect());
    }
    
    m_rasterizer_3d.GetMOCBufferRef().Clear();  
    
    m_rasterizer_3d.State().detph = ng::renderer::rasterizer::DEPTHFUNC_LESS;
    m_rasterizer_3d.State().flatColor = RGB(255, 255, 0);
    m_rasterizer_3d.State().flags = ng::renderer::rasterizer::RASTER_DEPTH_TEST | ng::renderer::rasterizer::RASTER_DEPTH_WRITE | ng::renderer::rasterizer::RASTER_TEXTURE | ng::renderer::rasterizer::RASTER_VERTEXCOLOR;

    ng::renderer::gfx::Image test = ng::resource::ImageResourceManager::GetResource("Image1", true);
    ng::renderer::gfx::Texture testtexture = ng::resource::TextureResourceManager::GetResource("Texture1", true);

    ng::renderer::rasterizer::TextureView textview;
    textview.texture = testtexture;

    m_rasterizer_3d.State().diffuseTex = &textview;

    m_rasterizer_3d.GetColorBuffer().Clear(RGB(0, 0, 0));

    m_render_pipeline_3d.ModelViewMatrix() = m_camera.ViewMatrix();
    m_render_pipeline_3d.ProjectionMatrix() = m_camera.ProjectionMatrix();
    m_render_pipeline_3d.SetCamera(m_camera);

    m_fps_counter.Update();
    m_render_pipeline_3d.Begin();

    m_rasterizer_3d.State().flags = ng::renderer::rasterizer::RASTER_DEPTH_TEST | ng::renderer::rasterizer::RASTER_DEPTH_WRITE | ng::renderer::rasterizer::RASTER_VERTEXCOLOR | ng::renderer::rasterizer::RASTER_TEXTURE;
    //  for (auto bp : m_map_brush_polys) m_render_pipeline_3d.RenderPolygon(bp.verts());
    //  m_render_pipeline_3d.RenderMesh(m_mesh_obj);
    //  RenderBsp(m_bsp);

    m_render_pipeline_3d.RenderWorld(m_cells, m_pvs, [&](int mat)
        {
            m_rasterizer_3d.State().diffuseTex=&textview;

        });
    m_render_pipeline_3d.End();
        
   
    if (m_use_after_fx )mgr.Apply(totalTime);

  
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
    /*
    printf("Zellen: PVS %u, mit Geometrie %u, Frustum %u, MOC %u, HZB %u, gerendert %u\n",
        s.cells_in_pvs, s.cells_with_geometry, s.cells_frustum_culled,
        s.cells_culled_by_moc, s.cells_culled_by_hzb, s.cells_rendered);
        */

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
    

}


bool ConvertfromWad(const ng::map::texture::Wad3File& wad, const std::string& name) {

    printf("[WAD] Load Texture %s", name.c_str());
    if (ng::resource::TextureResourceManager::IsResourceInManager(name)) return true;
    
    try {

        ng::map::texture::Wad3File::Texture texture = wad.GetTexture(name);
        ng::resource::TextureResourceManager::LoadRawTextureFromMemory(name, (unsigned char*)texture.pixels.data(), texture.width, texture.height);
        printf("...OK\n");
    }
    catch (const std::runtime_error& error) {
        std::string help = error.what();
        printf("...ERROR\n");
        ng::resource::TextureResourceManager::LoadRawTextureFromMemory(name, (unsigned char*)nullptr,0,0);

    }
    return true;
}



int main(int argc, char** argv) {
    static bool m_pick_vfs_file = false;
    static bool m_load_obj = false;
    static bool m_load_hlm = false;
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
                if (ImGui::MenuItem("Load HalfLifeMap")) m_load_hlm = true;
                if (ImGui::MenuItem("Quit")) m_exit_program = true;
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


        if (m_load_hlm) {
            std::string filename = ng::gui::FilePicker(m_load_hlm, "*.map");
            if (!filename.empty()) {
                ng::map::HalfLife hl_map;
                hl_map.load(filename);
                ng::map::Data<NG_MATH_FLOAT> data;
                data = hl_map;                




                ng::map::texture::Wad3File m_wad_file = data.GetTextureWad();;


                for (auto n : data.m_texture_names) {
                    
                    ConvertfromWad(m_wad_file, n);
                }









                auto brush=data.ExtractBrushes();
                brush = ng::map::csg::MakeUnion(brush);

                std::vector<ng::bsp::BSPPolygon<NG_MATH_FLOAT>> polys;
                auto tnames = m_wad_file.GetTextureNames();

                for (auto b : brush) {
                    auto h=b.primitives();
                    for (auto p : h) {

                    
                        
                        for (size_t i = 1; i < p.verts().size() - 1; ++i)
                        {
                          
                            
                            ng::bsp::BSPPolygon<NG_MATH_FLOAT> poly= ng::bsp::BSPPolygon<NG_MATH_FLOAT>::FromTriangle(p.verts()[0], p.verts()[i], p.verts()[i + 1],-1);
                            
                            polys.push_back(poly);
                        }
                    }
                }
                auto aabb=ng::bsp::ComputeSceneBounds(polys);
                auto sky =ng::bsp::GenerateSkyboxPolygons(aabb,(NG_MATH_FLOAT) 100);

                for (auto p : sky)polys.push_back(p);
                m_bsp.Build(polys);
                m_num_bsp_leafs = m_bsp.GetLeaves().size();
                m_bsp.BuildPortals();
                 


                auto reached = m_bsp.FloodFromFlag();            // Seeds = HAS_ILLEGAL_GEOMETRY, dann ueber alle Portale
                m_bsp.RemoveReachedPolygons(reached);     // Polygone in allen erreichten Leaves loeschen
                
                auto visible = m_bsp.CollectRemainingPolygons(); // Schritt 9
           
                
                
                m_vo.minPlaneArea = 4096.0f;          // hier stellst du die Groebe ein


                
                m_cells.Build(visible, m_vo);
                ng::bsp::PVSOptions<NG_MATH_FLOAT> po;
                po.threads = 0;        // 0 = alle logischen Kerne (hardware_concurrency), sonst feste Zahl
                po.verbose = true;
                po.skipVoidLeaves = true;
                po.requirePolygonsInComponent = true;
                po.minPortalArea = 0.1;
                m_pvs.Build(m_cells.Tree(),po);            // PVS auf den groben Zellen

                // Rendering
            
            }
        }
                       
                
        ImGui::Render();
        const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
        SwapScreen(clear_color_with_alpha);
    }


    ng::resource::Manager::Flush();


	ShutdownImGui();
    return 0;
}




