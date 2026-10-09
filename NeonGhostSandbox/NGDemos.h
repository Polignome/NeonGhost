#pragma once

#include "imgui.h"
#include <NeonGhostCore.h>
#include <vector>
#include <string>
#include <filesystem>
#include <iostream>
#include <limits>
#include <random>
#include <cstdint>
#include "MapCompiler.h"
#include "RenderTargetView.h"

static ng::renderer::buffers::PixelBufferRGBA m_bixelbuffer(800, 600);
static ng::renderer::rasterizer::Rasterizer3D<NG_MATH_FLOAT> m_rasterizer_3d(800, 600);
static ng::renderer::rasterizer::RenderPipeline<NG_MATH_FLOAT> m_render_pipeline_3d(m_rasterizer_3d);
static ng::renderer::buffers::PixelBufferRGBA m_moc_buffer(800, 600);

static ng::scene::Camera<NG_MATH_FLOAT> m_camera;

static ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
static bool m_exit_program = false;
static bool m_tools_font_window = false;
static bool m_view_render_window = false;
static bool m_compiler_setting_window = false;
static bool m_view_render_fullscreen = true;
static bool m_masked_occlusion_buffer_window_visible = false;
static bool m_HZB_window_visible = false;
static bool m_2d_demo_window_visible = false;
static bool m_sprite_demo_window_visible = false;
static bool m_animatet_sprite_demo_window_visible = false;
static bool m_postfx_demo_window_visible = false;
static bool m_image_demo_window_visible = false;
static bool m_resource_window_visible = false;
static bool m_3d_info_window_visible = false;
static bool m_use_spanbuffer = false;
static bool m_use_lod = false;
static bool m_use_after_fx = false;
static bool m_use_tile_base_rendering = true;
static bool m_use_collision = false;
static bool m_use_lightmap = true;
static size_t m_num_visible_leafs = 0;
static size_t m_num_bsp_leafs = 0;
static bool m_build_lightmaps = false;
static bool m_build_radiosity = false;

static float m_max_fps = 0;
static std::vector<float> m_fps_count;
static float m_max_out_polys = 0;
static std::vector<float> m_out_polycount;
static float m_max_in_polys = 0;
static std::vector<float> m_in_polycount;
static float m_max_out_cells = 0;
static std::vector<float> m_out_cellscount;

static std::vector< ng::renderer::objects::Mesh<NG_MATH_FLOAT>> m_mesh_obj;

// Member der Anwendung
static ng::renderer::rasterizer::LightmapCorrection m_lmCorr;
static BspScene<NG_MATH_FLOAT> m_bsp_scene;

static ng::lightmap::LightmapOptions lmOpt;
static ng::lightmap::RadiosityOptions<NG_MATH_FLOAT> radOpt;
static ng::bsp::PVSOptions<NG_MATH_FLOAT> po;
static ng::bsp::VisCellOptions<NG_MATH_FLOAT> vo;





static ng::controller::PlayerController<NG_MATH_FLOAT> m_player(m_bsp_scene.m_bsp);

inline void Render2DWindow(bool& m_software_renderer_visible)
{
    static ng::renderer::buffers::PixelBufferRGBA m_soft_renderer_buffer(800, 600);
    static RenderPixelbuffer m_soft_renderer(m_soft_renderer_buffer.Width(), m_soft_renderer_buffer.Height());
    static bool clear_first = false;

    ng::renderer::gfx2d::Canvas2D canvas(m_soft_renderer_buffer);
    std::mt19937 rng(std::random_device{}());

    int w = m_soft_renderer_buffer.Width();
    int h = m_soft_renderer_buffer.Height();

    // Bewusst groﬂz¸giger Bereich, auch auﬂerhalb des Canvas,
    // damit Clipping an allen Kanten/Ecken getestet wird.
    std::uniform_int_distribution<int> distX(-w / 2, w + w / 2);
    std::uniform_int_distribution<int> distY(-h / 2, h + h / 2);
    std::uniform_int_distribution<int> distSize(-20, std::max(w, h)); // auch negative Werte testen!
    std::uniform_int_distribution<int> distByte(0, 255);
    std::uniform_int_distribution<int> distShape(0, 6);

    auto RandomColor = [&]() -> uint32_t
        {
            uint32_t a = 255; // meist deckend, damit man was sieht
            uint32_t r = distByte(rng);
            uint32_t g = distByte(rng);
            uint32_t b = distByte(rng);
            return (a << 24) | (r << 16) | (g << 8) | b;
        };





    if (clear_first)m_soft_renderer_buffer.Clear(0xff000000);

    // Pro Frame mehrere zuf‰llige Formen zeichnen
    int count = 50;
    for (int i = 0; i < count; ++i)
    {
        int shape = distShape(rng);
        uint32_t color = RandomColor();

        switch (shape)
        {
        case 0: // Line
        {
            int x0 = distX(rng), y0 = distY(rng);
            int x1 = distX(rng), y1 = distY(rng);
            canvas.DrawLine(x0, y0, x1, y1, color);
            break;
        }
        case 1: // LineGradient
        {
            int x0 = distX(rng), y0 = distY(rng);
            int x1 = distX(rng), y1 = distY(rng);
            canvas.DrawLine(x0, y0, x1, y1, RandomColor(), RandomColor());
            break;
        }
        case 2: // Rectangle (Outline)
        {
            int x = distX(rng), y = distY(rng);
            int rw = distSize(rng), rh = distSize(rng);
            canvas.DrawRectangle(x, y, rw, rh, color);
            break;
        }
        case 3: // Box (gef¸llt)
        {
            int x = distX(rng), y = distY(rng);
            int rw = distSize(rng), rh = distSize(rng);
            canvas.DrawBox(x, y, rw, rh, color);
            break;
        }
        case 4: // Circle
        {
            int cx = distX(rng), cy = distY(rng);
            int r = std::abs(distSize(rng));
            canvas.DrawCircle(cx, cy, r, color);
            break;
        }
        case 5: // CircleFilled
        {
            int cx = distX(rng), cy = distY(rng);
            int r = std::abs(distSize(rng));
            canvas.DrawCircleFilled(cx, cy, r, color);
            break;
        }
        case 6: // Ellipse / EllipseFilled abwechselnd
        {
            int cx = distX(rng), cy = distY(rng);
            int rx = std::abs(distSize(rng));
            int ry = std::abs(distSize(rng));
            if (i % 2 == 0)
                canvas.DrawEllipse(cx, cy, rx, ry, color);
            else
                canvas.DrawEllipseFilled(cx, cy, rx, ry, color);
            break;
        }
        }

    }




    //  printf("m_soft_renderer_buffer i \n", m_soft_renderer_buffer.GetRawBuffer());
    m_soft_renderer.Update(m_soft_renderer_buffer.GetRawBuffer());


    if (ImGui::Begin(
        "2D Primitives Demos ",
        &m_software_renderer_visible,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_MenuBar))
    {
        ImVec2 cpos1 = ImGui::GetCursorPos();

        ImVec2 rsize(
            (float)m_soft_renderer.Width(),
            (float)m_soft_renderer.Height());

        ImVec2 rr = ImGui::GetWindowContentRegionMin();


        ImGui::SetWindowSize(
            ImVec2(
                rsize.x + 20,
                rsize.y + 10 + cpos1.y));


        ImGui::Image(m_soft_renderer.GetImTextureID(), rsize);

        ImGui::SetCursorPos(cpos1);
        //        ImGui::Text("FPS: %.1 Frame Time: %.3f ms", fps.FPS(), fps.DeltaTime() * 1000.0);

    }
    ImGui::End();
}

inline void RenderSpriteWindow(bool& m_software_renderer_visible)
{
    static ng::renderer::buffers::PixelBufferRGBA m_soft_renderer_buffer(800, 600);
    static RenderPixelbuffer m_soft_renderer(m_soft_renderer_buffer.Width(), m_soft_renderer_buffer.Height());
    static ng::renderer::gfx2d::Canvas2D canvas(m_soft_renderer_buffer);

    static ng::renderer::buffers::PixelBufferRGBA sprite_buffer(64, 64);
    static bool first_time = true;
    static ng::math::Point2f pos(400, 300);
    static ng::math::Point2f direction(0.2f, -0.3f);;
    static ng::math::Point2f speed(0.5, 0.5);;
    static double s = 0;

    static bool rotation = false;
    static bool scale = false;
    static bool blend = false;
    static float color[3];
    static ng::renderer::gfx2d::blend::ColorKey color_key;
    static ng::renderer::gfx2d::Sprite<float> sprite(sprite_buffer);

    if (first_time) {
        first_time = false;
        double s = 255.0 / 64.0;
        for (int x = 0; x < 64; x++)
        {
            for (int y = 0; y < 64; y++) {
                if ((x > 64 / 3 && x < 64 - 64 / 3) && (y > 64 / 3 && y < 64 - 64 / 3))
                    sprite_buffer.PutPixel(x, y, RGB(255, 0, 0));
                else sprite_buffer.PutPixel(x, y, RGBA(uint8_t(x * s), uint8_t(y * s), (uint8_t(y * s) + uint8_t(x * s)) >> 1, 100));
            }
        }
        sprite.SetTexture(sprite_buffer);
        sprite.SetPosition(400.0f, 300.0f);
    }


    s += 0.01;

    m_soft_renderer_buffer.CopyFrom(m_bixelbuffer);

    if (scale) sprite.SetScale(10.0f - (10.0f * (float)sin(s)), 10.0f - (10.0f * (float)sin(s)));
    if (rotation) sprite.SetRotation(sprite.Rotation() + 0.01f);
    sprite.SetColorKey(color_key);
    canvas.DrawSprite(sprite);

    canvas.DrawRectangle((int)sprite.Bounds().Min().x(), (int)sprite.Bounds().Min().y(),
        (int)sprite.Bounds().Size().x(), (int)sprite.Bounds().Size().y(), RGB(255, 0, 255));


    //rite.Bounds().Max().x(), sprite.Bounds().Max().y(), RGB(255, 0, 255));




    m_soft_renderer.Update(m_soft_renderer_buffer.GetRawBuffer());


    if (ImGui::Begin(
        "Simple 2D Sprite Demo ",
        &m_software_renderer_visible,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_MenuBar))
    {




        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("View")) {
                if (ImGui::MenuItem("rotate", "", rotation)) rotation = !rotation;
                if (ImGui::MenuItem("scale", "", scale))     scale = !scale;

                if (ImGui::BeginMenu("blend"))
                {

                    if (ImGui::MenuItem("Opaque", "", sprite.GetBlendFunc() == ng::renderer::gfx2d::blend::Opaque))    sprite.SetBlendFunc(ng::renderer::gfx2d::blend::Opaque);
                    if (ImGui::MenuItem("Alpha", "", sprite.GetBlendFunc() == ng::renderer::gfx2d::blend::Alpha))      sprite.SetBlendFunc(ng::renderer::gfx2d::blend::Alpha);
                    if (ImGui::MenuItem("Additive", "", sprite.GetBlendFunc() == ng::renderer::gfx2d::blend::Additive))sprite.SetBlendFunc(ng::renderer::gfx2d::blend::Additive);
                    if (ImGui::MenuItem("Multiply", "", sprite.GetBlendFunc() == ng::renderer::gfx2d::blend::Multiply))sprite.SetBlendFunc(ng::renderer::gfx2d::blend::Multiply);
                    if (ImGui::MenuItem("Screen", "", sprite.GetBlendFunc() == ng::renderer::gfx2d::blend::Screen))    sprite.SetBlendFunc(ng::renderer::gfx2d::blend::Screen);
                    if (ImGui::MenuItem("Subtract", "", sprite.GetBlendFunc() == ng::renderer::gfx2d::blend::Subtract))sprite.SetBlendFunc(ng::renderer::gfx2d::blend::Subtract);

                    ImGui::EndMenu();

                }

                if (ImGui::BeginMenu("transbarent"))
                {
                    if (ImGui::MenuItem("Enable", "", color_key.enabled))    color_key.enabled = !color_key.enabled;
                    //if (ImGui::ColorPicker3("Color", color)) {
                      //  color_key.color = RGB(uint8_t(color[0] * 255), uint8_t(color[1] * 255), uint8_t(color[2] * 255));

                    //}
                    ImGui::EndMenu();

                }
                color_key.color = RGB(255, 0, 0);


                ImGui::EndMenu();
            }

            ImGui::EndMenuBar();
        }


        ImVec2 cpos1 = ImGui::GetCursorPos();

        ImVec2 rsize(
            (float)m_soft_renderer.Width(),
            (float)m_soft_renderer.Height());

        ImVec2 rr = ImGui::GetWindowContentRegionMin();


        ImGui::SetWindowSize(
            ImVec2(
                rsize.x + 20,
                rsize.y + 10 + cpos1.y));


        ImGui::Image(m_soft_renderer.GetImTextureID(), rsize);

        ImGui::SetCursorPos(cpos1);
        //        ImGui::Text("FPS: %.1 Frame Time: %.3f ms", fps.FPS(), fps.DeltaTime() * 1000.0);

    }
    ImGui::End();
}

inline void RenderBouncingSpritesDemo(bool& visible)
{
    static ng::renderer::buffers::PixelBufferRGBA m_soft_renderer_buffer(800, 600);
    static RenderPixelbuffer m_soft_renderer(m_soft_renderer_buffer.Width(), m_soft_renderer_buffer.Height());
    static ng::renderer::gfx2d::Canvas2D canvas(m_soft_renderer_buffer);

    constexpr int SPRITE_COUNT = 135;
    constexpr int TEX_SIZE = 8 * 2;

    // Texturen m¸ssen die Sprites ¸berleben -> eigener statischer Speicher
    static std::vector<ng::renderer::buffers::PixelBufferRGBA> textures;
    static std::vector<ng::renderer::gfx2d::Sprite<float>> sprites;
    static ng::renderer::gfx2d::blend::ColorKey color_key{ true,0xff0000ff };

    static bool first_time = true;
    static ng::renderer::spatial::KDTree2D<float> tree;

    static bool render_sprites = true;
    static bool render_conections = false;
    static bool render_partions = false;

    m_soft_renderer_buffer.CopyFrom(m_bixelbuffer);

    if (first_time)
    {
        first_time = false;
        std::mt19937 rng(std::random_device{}());
        std::uniform_real_distribution<float> distPosX(TEX_SIZE, float(m_soft_renderer_buffer.Width() - TEX_SIZE));
        std::uniform_real_distribution<float> distPosY(TEX_SIZE, float(m_soft_renderer_buffer.Height() - TEX_SIZE));
        std::uniform_real_distribution<float> distVel(-150.0f, 150.0f);
        std::uniform_real_distribution<float> distScale(0.5f, 1.5f);
        std::uniform_real_distribution<float> distRot(0.0f, 6.2831853f);
        std::uniform_int_distribution<int> distByte(50, 255);

        textures.reserve(SPRITE_COUNT);
        sprites.reserve(SPRITE_COUNT);

        for (int i = 0; i < SPRITE_COUNT; ++i)
        {
            // Jede Sprite bekommt eine eigene, einfarbig eingef‰rbte Textur mit rundem Verlauf
            textures.emplace_back(TEX_SIZE, TEX_SIZE);
            auto& tex = textures.back();

            uint8_t r = (uint8_t)distByte(rng);
            uint8_t g = (uint8_t)distByte(rng);
            uint8_t b = (uint8_t)distByte(rng);

            float cx = TEX_SIZE * 0.5f;
            float cy = TEX_SIZE * 0.5f;
            float maxDist = TEX_SIZE * 0.5f;

            for (int y = 0; y < TEX_SIZE; ++y)
            {
                for (int x = 0; x < TEX_SIZE; ++x)
                {
                    float dx = x - cx;
                    float dy = y - cy;
                    float dist = std::sqrt(dx * dx + dy * dy);

                    if (dist > maxDist)
                    {
                        tex.PutPixel(x, y, 0xff0000ff); // transparent auﬂerhalb des Kreises
                    }
                    else
                    {
                        float fade = 1.0f - (dist / maxDist) * 0.5f; // Mitte heller
                        uint8_t rr = (uint8_t)(r * fade);
                        uint8_t gg = (uint8_t)(g * fade);
                        uint8_t bb = (uint8_t)(b * fade);
                        tex.PutPixel(x, y, RGBA(rr, gg, bb, 255));
                    }
                }
            }

            ng::renderer::gfx2d::Sprite<float> bs;
            color_key.color = RGB(255, 0, 0);
            color_key.enabled = true;
            bs.SetColorKey(color_key);
            bs.SetTexture(tex);
            bs.SetPosition(distPosX(rng), distPosY(rng));
            bs.SetScale(distScale(rng), distScale(rng));
            bs.SetRotation(distRot(rng));
            bs.SetVelocity(ng::math::Point2f(distVel(rng), distVel(rng)));

            sprites.push_back(bs);
        }
    }

    // ------------------------------------------------------------
    // Update
    // ------------------------------------------------------------
    static auto lastTime = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    float dt = std::chrono::duration<float>(now - lastTime).count();
    dt = std::min(dt, 0.05f); // Cap gegen Spiral-of-Death bei Lags
    lastTime = now;

    float canvasW = (float)m_soft_renderer_buffer.Width();
    float canvasH = (float)m_soft_renderer_buffer.Height();
    float scale = 1.0f / (float)RAND_MAX;
    // Bewegung + Wandkollision
    for (auto& bs : sprites)
    {
        float radius = bs.Circle().Radius(); // ggf. an deine BoundingCircle2D-API anpassen

        ng::math::Point2f pos = bs.Position();
        pos.x() += bs.Velocity().x() * dt;
        pos.y() += bs.Velocity().y() * dt;

        if (pos.x() - radius < 0.0f)
        {
            pos.x() = radius;
            bs.Velocity().x() = std::abs(bs.Velocity().x());
        }
        else if (pos.x() + radius > canvasW)
        {
            pos.x() = canvasW - radius;
            bs.Velocity().x() = -std::abs(bs.Velocity().x());
        }

        if (pos.y() - radius < 0.0f)
        {
            pos.y() = radius;
            bs.Velocity().y() = std::abs(bs.Velocity().y());
        }
        else if (pos.y() + radius > canvasH)
        {
            pos.y() = canvasH - radius;
            bs.Velocity().y() = -std::abs(bs.Velocity().y());
        }

        bs.SetPosition(pos);
        bs.SetRotation(bs.Rotation() + bs.AngularVelocity());
        const float su = 0.01f;
        if (bs.AngularVelocity() - su > 0) bs.SetAngularVelocity(bs.AngularVelocity() - su);
        else bs.SetAngularVelocity(0);
    }

    std::vector<ng::renderer::objects::Object2D<float>*> objectPtrs;
    for (auto& bs : sprites) objectPtrs.push_back(&bs);

    tree.Build(objectPtrs); // pro Frame neu aufbauen, da sich alle Positionen ‰ndern


    tree.ForEachPotentialPair([&](ng::renderer::objects::Object2D<float>* a, ng::renderer::objects::Object2D<float>* b)
        {

            ng::math::Point2f posA = a->Position();
            ng::math::Point2f posB = b->Position();

            float dx = posB.x() - posA.x();
            float dy = posB.y() - posA.y();
            float dist = std::sqrt(dx * dx + dy * dy);

            float rA = a->Circle().Radius();
            float rB = b->Circle().Radius();
            float minDist = rA + rB;
            a->SetAngularVelocity(a->AngularVelocity() + (float)std::rand() * 0.001f);
            b->SetAngularVelocity(a->AngularVelocity() + (float)std::rand() * 0.001f);



            if (dist < minDist && dist > 0.0001f)
            {
                // Normalisierte Kollisionsnormale
                float nx = dx / dist;
                float ny = dy / dist;

                // Sprites auseinanderschieben, damit sie nicht "kleben"
                float overlap = minDist - dist;
                posA.x() -= nx * overlap * 0.5f;
                posA.y() -= ny * overlap * 0.5f;
                posB.x() += nx * overlap * 0.5f;
                posB.y() += ny * overlap * 0.5f;
                a->SetPosition(posA);
                b->SetPosition(posB);

                // Geschwindigkeiten entlang der Normalen reflektieren (vereinfachte
                // elastische Kollision, gleiche Masse angenommen)
                auto& vA = a->Velocity();
                auto& vB = b->Velocity();

                float vAn = vA.x() * nx + vA.y() * ny;
                float vBn = vB.x() * nx + vB.y() * ny;

                // Impulsaustausch (gleiche Masse -> Normalkomponenten tauschen)
                float exchange = vBn - vAn;
                vA.x() += exchange * nx;
                vA.y() += exchange * ny;
                vB.x() -= exchange * nx;
                vB.y() -= exchange * ny;
            }
        });

    // ------------------------------------------------------------
    // Render
    // ------------------------------------------------------------
 //   canvas.Clear(RGBA(20, 20, 30, 255));

    if (render_sprites) for (auto& bs : sprites) canvas.DrawSprite(bs);



    uint32_t mode = 0;
    if (render_conections) mode |= ng::renderer::gfx2d::VISUALIZE_KDTREE2D_CONECTIONS;
    if (render_partions) mode |= ng::renderer::gfx2d::VISUALIZE_KDTREE2D_SPLITS;

    if (mode)ng::renderer::gfx2d::Visualize(canvas, tree, mode);


    m_soft_renderer.Update(m_soft_renderer_buffer.GetRawBuffer());


    /*
    bool open = true;



    if (ImGui::Begin("Bouncing Sprites Demo", &open,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_MenuBar))
    {

        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("Mode"))
            {
                 if (ImGui::MenuItem("Sprites", "", render_sprites)) render_sprites = !render_sprites;
                if (ImGui::MenuItem("KDTree Node conections", "", render_conections)) render_conections = !render_conections;
                if (ImGui::MenuItem("KDTree Node Partions ", "", render_partions)) render_partions = !render_partions;
                ImGui::EndMenu();

            }

            ImGui::EndMenuBar();
        }

        ImVec2 cpos1 = ImGui::GetCursorPos();
        ImVec2 rsize((float)m_soft_renderer.Width(), (float)m_soft_renderer.Height());
        ImGui::SetWindowSize(ImVec2(rsize.x + 20, rsize.y + 10 + cpos1.y));
        ImGui::Image(m_soft_renderer.GetImTextureID(), rsize);
        ImGui::SetCursorPos(cpos1);
  //      ImGui::Text("Sprites: %d  |  dt: %.3f ms", (int)sprites.size(), dt * 1000.0f);

  */

    float menuBarHeight = ImGui::GetFrameHeightWithSpacing();
    ImVec2 rsize((float)m_soft_renderer.Width(), (float)m_soft_renderer.Height());
    ImGui::SetNextWindowSize(ImVec2(rsize.x + 20, rsize.y + 10 + menuBarHeight), ImGuiCond_Once);


    if (ImGui::Begin("Bouncing Sprites Demo", &visible,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_MenuBar))
    {
        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("Mode"))
            {
                if (ImGui::MenuItem("Sprites", "", render_sprites)) render_sprites = !render_sprites;
                if (ImGui::MenuItem("KDTree Node conections", "", render_conections)) render_conections = !render_conections;
                if (ImGui::MenuItem("KDTree Node Partions ", "", render_partions)) render_partions = !render_partions;
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        ImGui::Image(m_soft_renderer.GetImTextureID(), rsize);
    }
    ImGui::End();



}

inline void RenderPostFxeWindow(bool& m_software_renderer_visible)
{
    static ng::renderer::buffers::PixelBufferRGBA m_soft_renderer_buffer(800, 600);
    static RenderPixelbuffer m_soft_renderer(m_soft_renderer_buffer.Width(), m_soft_renderer_buffer.Height());
    static ng::renderer::gfx2d::Canvas2D canvas(m_soft_renderer_buffer);
    static ng::renderer::postfx::PostFXManager<NG_MATH_FLOAT> mgr;
    static auto* gravity = mgr.AddEffect<ng::renderer::postfx::GravityWellEffect<NG_MATH_FLOAT>>();
    static auto* fog = mgr.AddEffect<ng::renderer::postfx::DepthFogEffect<NG_MATH_FLOAT>>();
    static auto* water = mgr.AddEffect<ng::renderer::postfx::UnderwaterWarpEffect<NG_MATH_FLOAT>>();


    static float totalTime = 0.0f;              // <-- NEU: akkumulierte Uhrzeit
    static bool first_time = true;
    if (first_time) {
        first_time = false;
        mgr.SetSize(m_soft_renderer_buffer.Width(), m_soft_renderer_buffer.Height());
        mgr.BindColor(m_soft_renderer_buffer.GetRawBuffer());
    }

    m_soft_renderer_buffer.CopyFrom(m_bixelbuffer);
    float deltaTime = ImGui::GetIO().DeltaTime;
    totalTime += deltaTime;                      // <-- NEU: aufaddieren statt Delta direkt nutzen

    mgr.Apply(totalTime);                        // <-- statt Apply(deltaTime)
    m_soft_renderer.Update(m_soft_renderer_buffer.GetRawBuffer());


    if (ImGui::Begin(
        "PostFx Demo ",
        &m_software_renderer_visible,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_MenuBar))
    {




        if (ImGui::BeginMenuBar()) {

            ImGui::EndMenuBar();
        }


        ImVec2 cpos1 = ImGui::GetCursorPos();

        ImVec2 rsize(
            (float)m_soft_renderer.Width(),
            (float)m_soft_renderer.Height());

        ImVec2 rr = ImGui::GetWindowContentRegionMin();


        ImGui::SetWindowSize(
            ImVec2(
                rsize.x + 20,
                rsize.y + 10 + cpos1.y));


        ImVec2 cpos2 = ImGui::GetCursorPos();
        ImVec2 wpos = ImGui::GetWindowPos();
        cpos2.x += wpos.x;
        cpos2.y += wpos.y;

        if (ImGui::IsWindowFocused() && gravity) {
            ng::bounding::AABBox2D<NG_MATH_FLOAT> bound(cpos2.x, cpos2.y, cpos2.x + rsize.x, cpos2.y + rsize.y);
            ImVec2 m_pos = ImGui::GetMousePos();
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && bound.Inside(m_pos.x, m_pos.y)) {

                gravity->Spawn((m_pos.x - cpos2.x), (m_pos.y - cpos2.y), totalTime, 180, 80);
            }

        }
        ImGui::Image(m_soft_renderer.GetImTextureID(), rsize);

        //ImGui::SetCursorPos(cpos1);
        //        ImGui::Text("FPS: %.1 Frame Time: %.3f ms", fps.FPS(), fps.DeltaTime() * 1000.0);

    }
    ImGui::End();
}

inline void RenderImageWindow(bool& m_software_renderer_visible)
{
    static ng::renderer::buffers::PixelBufferRGBA m_soft_renderer_buffer(800, 600);
    static RenderPixelbuffer m_soft_renderer(m_soft_renderer_buffer.Width(), m_soft_renderer_buffer.Height());
    static ng::renderer::gfx::_Image image;

    enum {
        NONE = 0,
        JPEG = 1,
        PNG = 2,
        BMP = 3,
        TGA = 4,
        GIF = 5,
        PSD = 6,
        HDR = 7,
        PIC = 8
    };

    static int mode = NONE;

    m_soft_renderer.Update(m_soft_renderer_buffer.GetRawBuffer());
    if (ImGui::Begin(
        "Image Demo ",
        &m_software_renderer_visible,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_MenuBar))
    {

        static bool show = false;
        static std::string type = "";
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("Load JPEG")) {
                    show = true; type = "*.jpg";
                }
                if (ImGui::MenuItem("Load PNG")) {
                    show = true; type = "*.png";
                }
                if (ImGui::MenuItem("Load BMP")) {
                    show = true; type = "*.BMP";
                }
                if (ImGui::MenuItem("Load TGA")) {
                    show = true; type = "*.tga";
                }
                if (ImGui::MenuItem("Load GIF")) {
                    show = true; type = "*.gif";
                }
                if (ImGui::MenuItem("Load PSD")) {
                    show = true; type = "*.psd";
                }
                if (ImGui::MenuItem("Load HDR")) {
                    show = true; type = "*.hdr";
                }
                if (ImGui::MenuItem("Load PIC")) {
                    show = true; type = "*.pic";
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        std::string file_name;
        file_name = ng::gui::FilePicker(show, "*");


        if (file_name != "") {
            image.Load(file_name);
        }

        m_soft_renderer.Update(image);


        ImVec2 rsize((float)image.Width(), (float)image.Height());
        ImGui::Image(m_soft_renderer.GetImTextureID(), rsize);

        //ImGui::SetCursorPos(cpos1);
        //        ImGui::Text("FPS: %.1 Frame Time: %.3f ms", fps.FPS(), fps.DeltaTime() * 1000.0);

    }
    ImGui::End();
}

inline void ViewImageRes(const std::string& res_name)
{
    if (res_name.empty()) return;

    static RenderPixelbuffer m_soft_renderer(80, 80);



    auto res = ng::resource::ImageResourceManager::GetResource(res_name, true);

    if (res) {
        m_soft_renderer.Resize(res->Width(), res->Height());
        m_soft_renderer.Update(*res);
        ImVec2 rsize((float)res->Width(), (float)res->Height());
        ImGui::Image(m_soft_renderer.GetImTextureID(), rsize);

    }

}

inline void ViewTextureRes(const std::string& res_name)
{
    if (res_name.empty()) return;
    static RenderPixelbuffer  m_soft_renderer(80,80);
    static int level = 0;
    static bool stretch = false;
    auto res = ng::resource::TextureResourceManager::GetResource(res_name, true);

    if (res) {
        if (level >= res->GetMipLevels()) level = 0;
        ImGui::Text("Size %i %i", res->Width(), res->Height());

        //ImGui::SetNextItemWidth(100);
        ImGui::SliderInt("MipLevel", &level, 0, int(res->GetMipLevels() - 1));
        ImGui::Checkbox("Stretch", &stretch);
        auto resl = res->GetMipLevel(level);
        ImGui::Text("Size %i %i", resl.Width(), resl.Height());

        ImGui::Separator();

        m_soft_renderer.Resize(resl.Width(), resl.Height());
        m_soft_renderer.Update(resl);
        ImVec2 rsize((float)resl.Width(), (float)resl.Height());
        if (!stretch) rsize = ImVec2((float)resl.Width(), (float)resl.Height());
        else rsize = ImVec2((float)res->Width(), (float)res->Height());
            
        ImGui::Image(m_soft_renderer.GetImTextureID(), rsize);
    }

}

inline void ResourcemanagerWindow(bool& m_software_renderer_visible)
{

    bool m_flush = false;
    if (ImGui::Begin(
        "ResourceManager ",
        &m_software_renderer_visible,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_MenuBar)) {

        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("Tools"))
            {
                if (ImGui::MenuItem("Flush")) m_flush = true;
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }
        ImGui::Text("Resources");

        ImVec2 cpos = ImGui::GetCursorPos();
        ImVec2 wsize = ImGui::GetWindowSize();
        ImVec2 size(200, wsize.y - cpos.y);
        static std::string mng_name = "";
        static std::string old_name = "";


        if (ImGui::BeginChild(0x0000f, size))
        {
            for (auto n : ng::resource::Manager::GetResourceManagerList())
            {
                bool selected = n->GetName() == mng_name;
                if (ImGui::Selectable(n->GetName().c_str(), &selected)) {
                    old_name = mng_name;
                    mng_name = n->GetName();
                }
            }
            ImGui::EndChild();
        }
        static std::string res_name = "";

        if (mng_name != old_name) {
            res_name = "";
            old_name = mng_name;
        }

        ImGui::SameLine();


        auto res = ng::resource::Manager::GetResourceManagerByName(mng_name);

        if (ImGui::BeginChild(0x0001f, size))
        {
            if (res) {
                for (auto n : res->GetResourceNameList())
                {
                    bool selected = (n == res_name);
                    if (ImGui::Selectable(n.c_str(), &selected)) res_name = n;
                
                }
            }
            ImGui::EndChild();
        }
        ImGui::SameLine();

       
        ImVec2 cpos2 = ImGui::GetCursorPos();
        wsize.x -= cpos2.x;

        if (ImGui::BeginChild(0x0002f, wsize)) {

           if (mng_name == "IMAGE_RESOURCE") ViewImageRes(res_name);
           if (mng_name == "TEXTURE_RESOURCE") ViewTextureRes(res_name);
           ImGui::EndChild();

        }
    }
    ImGui::End();


    if (m_flush) ng::resource::Manager::Flush();

}

inline void Noise(ng::renderer::buffers::PixelBufferRGBA& pixelbuff)
{
    static uint32_t seed = 123456789;
    uint32_t* buf = pixelbuff.GetRawBuffer();
    for (int i = 0; i < pixelbuff.Width() * pixelbuff.Height(); i++)
    {
        seed = seed * 1664525u + 1013904223u;

        uint8_t c = seed >> 24;

        buf[i] =
            0xFF000000 |
            (c << 16) |
            (c << 8) |
            c;

        //SetZBuffer(i, (TZbufferType)(std::rand()));
    }
}

inline void Noise(ng::renderer::buffers::HierarchicalZBuffer<double>& zbuffer)
{
    double* r = zbuffer.GetRawBuffer();
    for (int i = 0; i < zbuffer.Width() * zbuffer.Height(); i++)
    {

        r[i] = std::rand();
    }


}

inline void HierarchicalZBufferWindow(bool& HZB_window_visible)
{


    static RenderZBufferDouble m_hzb_renderer(m_rasterizer_3d.GetDepthBuffer().Width(), m_rasterizer_3d.GetDepthBuffer().Height());
    static int level = 0;
    static bool scale = true;

    ng::renderer::buffers::HierarchicalZBuffer<NG_MATH_FLOAT> m_hzb = m_rasterizer_3d.GetDepthBuffer();

    ng::renderer::buffers::ZBuffer zb = m_hzb.GetZBuffer(level);
    m_hzb_renderer.Update(zb);


    if (ImGui::Begin(
        "Hierarchical ZBuffer",
        &HZB_window_visible,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_MenuBar))
    {

        ImGui::Text("Level %i Size: %ix%i", level, zb.Width(), zb.Height());
        ImGui::SliderInt("Level", &level, 0, m_hzb.GetNumMipMapLevels() - 1);
        ImGui::Checkbox("Scale", &scale);
        ImVec2 cpos1 = ImGui::GetCursorPos();

        ImVec2 wsize((float)m_hzb.Width(), (float)m_hzb.Height());
        ImVec2 rsize((float)zb.Width(), (float)zb.Height());


        ImVec2 rr = ImGui::GetWindowContentRegionMin();

        if (scale) rsize = wsize;
        ImGui::SetWindowSize(
            ImVec2(
                wsize.x + 20,
                wsize.y + 10 + cpos1.y));


        ImGui::Image(m_hzb_renderer.GetImTextureID(), rsize);
        ImGui::End();

    }

}

inline void RenderDebugInfoWindow(bool& m_software_renderer_visible)
{

    static ng::controller::CameraController<double> cam_controller;
    static RenderPixelbuffer  m_soft_renderer_3d(m_rasterizer_3d.Width(), m_rasterizer_3d.Height());


    m_soft_renderer_3d.Update(m_rasterizer_3d.GetColorBuffer().GetRawBuffer());
    ng::scene::CameraFrustum< NG_MATH_FLOAT> camera_frustum(m_camera);
    ng::renderer::occlusion::Frustum<NG_MATH_FLOAT> frustum = camera_frustum;


    //cam_controller.Turn(m_camera, 1.0, 0.0);
    //auto f = m_camera.forward();

    const float size = 150;
    if (ImGui::Begin(
        "Renderer Debug",
        &m_software_renderer_visible,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_MenuBar))
    {



        if (ImGui::CollapsingHeader("Memory")) {

            size_t mem = GetProcessMemoryUsage();
            ImGui::Text("Memory Usage: %zu B", mem);
            ImGui::Text("Memory Usage: %zu KB", mem / 1024);
            ImGui::Text("Memory Usage: %zu MB", mem / (1024 * 1024));

        }

        if (ImGui::CollapsingHeader("Camera")) {
            ng::gui::DrawDirectionIndicator3D(size, m_camera.forward());
            ImGui::SameLine();
            //if (ImGui::BeginChild(0xf000001))
            ImGui::BeginGroup();
            {
                if (ImGui::CollapsingHeader("General")) {
                    ng::gui::ViewVector3("Posision ", m_camera.position());
                    ng::gui::ViewVector3("Forward  ", m_camera.forward());
                    ng::gui::ViewVector3("Up       ", m_camera.up());
                    ng::gui::ViewVector3("Rssight  ", m_camera.right());
                }
                if (ImGui::CollapsingHeader("Matrix")) {
                    ng::gui::ViewMatrix4("ViewMatrix", m_camera.ViewMatrix());
                    ng::gui::ViewMatrix4("ProjectionMatrix", m_camera.ProjectionMatrix());
                }
            }
            ImGui::EndGroup();
            //ImGui::EndChild();
        }



        if (ImGui::CollapsingHeader("Frustume")) {

            ng::gui::DrawCameraFrustumWidget<NG_MATH_FLOAT>(size, camera_frustum);
            ng::gui::DrawFrustumWidget<NG_MATH_FLOAT>(size, camera_frustum);
            ng::gui::DrawFrustumWidget<NG_MATH_FLOAT>(size, m_render_pipeline_3d.Frustum());

            ImGui::SameLine();
            ImGui::BeginGroup();
            {
                if (ImGui::CollapsingHeader("General##2")) {

                    {
                        float data = (float)m_camera.nearp();
                        if (ImGui::DragFloat("Near Plane", &data, 0.5f, 0.0001f, 10000.0f)) m_camera.nearp() = (double)data;
                    }
                    {
                        float data = (float)m_camera.farp();
                        if (ImGui::DragFloat("Far Plane", &data, 0.5f, 0.0001f, 10000.0f)) m_camera.farp() = (double)data;
                    }
                    {

                        float data = (float)ng::math::radiansToDegrees(m_camera.fov());
                        if (ImGui::DragFloat("Fov ", &data, 1.0f, 16.0f, 128.0f)) m_camera.fov() = ng::math::degreesToRadians((double)data);;
                    }
                }
            }
            ImGui::EndGroup();
        }





        if (ImGui::CollapsingHeader("Pipeline"))
        {
            ImGui::Image(m_soft_renderer_3d.GetImTextureID(), ImVec2(size, size));
            ImGui::SameLine();
            ImGui::BeginGroup();




            if (ImGui::CollapsingHeader("General##3")) {
                ng::renderer::rasterizer::RenderStatistics stat = m_render_pipeline_3d.GetStatistics();
                ImGui::PlotLines("##FPS Counter", m_fps_count.data(), (int)m_out_polycount.size(), 0, "FPS", 0.0f, m_max_fps, ImVec2(0, 80));
                ImGui::PlotLines("##Polygons In", m_in_polycount.data(), (int)m_in_polycount.size(), 0, "Polygons in", 0.0f, m_max_in_polys, ImVec2(0, 80));
                ImGui::PlotLines("##Polygons Out", m_out_polycount.data(), (int)m_out_polycount.size(), 0, "Polygons out", 0.0f, m_max_out_polys, ImVec2(0, 80));

                ImGui::Text("Width....................................: %i", m_soft_renderer_3d.Width());
                ImGui::Text("Height...................................: %i", m_soft_renderer_3d.Height());

                ImGui::Text("Objects in...............................: %i", stat.objs_in);

                if (m_render_pipeline_3d.GetCullingMode() & ng::renderer::rasterizer::CULL_HZB)

                {
                    ImGui::Text("Objects culled by HZB....................: %i", stat.objs_culled_by_hzb);

                }
                else ImGui::TextColored(ImVec4(1, 0, 0, 1), "Objects culled by HZB.....................: %i OFF", stat.objs_culled_by_hzb);

                if (m_render_pipeline_3d.GetCullingMode() & ng::renderer::rasterizer::CULL_FRUSTUM)
                {
                    ImGui::Text("Objects culled by Frustum................: %i", stat.objs_frustum_culled);
                }
                else ImGui::TextColored(ImVec4(1, 0, 0, 1), "Objects culled by Frustum................: %i OFF", stat.objs_frustum_culled);

                if (m_render_pipeline_3d.GetCullingMode() & ng::renderer::rasterizer::CULL_MOC)
                {
                    ImGui::Text("Objects culled by MaskedOcclusionBuffer..: %i", stat.objs_culled_by_moc);


                }
                else ImGui::TextColored(ImVec4(1, 0, 0, 1), "Objects culled by MaskedOcclusionBuffer..: %i OFF", stat.objs_culled_by_moc);


                ImGui::Separator();


                ImGui::Text("Polygons in..............................: %i", stat.polys_in);



                if (m_render_pipeline_3d.GetCullingMode() & ng::renderer::rasterizer::CULL_BACKFACE || m_render_pipeline_3d.GetCullingMode() & ng::renderer::rasterizer::CULL_FRONTFACE)
                {
                    ImGui::Text("Polygons Faceculled......................: %i %s %s", stat.polys_face_culled,
                        (m_render_pipeline_3d.GetCullingMode() & ng::renderer::rasterizer::CULL_FRONTFACE) ? "FRONT" : "",
                        (m_render_pipeline_3d.GetCullingMode() & ng::renderer::rasterizer::CULL_BACKFACE) ? "BACK" : "");

                }
                else ImGui::TextColored(ImVec4(1, 0, 0, 1), "Polygons Faceculled......................: %i", stat.polys_face_culled);


                if (m_render_pipeline_3d.GetCullingMode() & ng::renderer::rasterizer::CULL_FRUSTUM)
                {
                    ImGui::Text("Polygons culled by Frustum...............: %i", stat.polys_frustum_culled);

                }
                else ImGui::TextColored(ImVec4(1, 0, 0, 1), "Polygons culled by Frustum................: %i OFF", stat.polys_frustum_culled);;

                if (m_render_pipeline_3d.GetCullingMode() & ng::renderer::rasterizer::CULL_TRANSFORM)
                {
                    ImGui::Text("Polygons transformculled.................: %i", stat.polys_culled_by_transform);
                }
                else ImGui::TextColored(ImVec4(1, 0, 0, 1), "Polygons transformculled..................: %i OFF", stat.polys_culled_by_transform);

                if (stat.polys_in > 0) {
                    double pro = (100.0 / (double)stat.polys_in) * (double)stat.polys_rendered;
                    ImGui::Text("Polygons rendered........................: %i %3.2f%%", stat.polys_rendered, pro);
                }
                else ImGui::Text("Polygons rendered........................: %i", stat.polys_rendered);

            }

            if (ImGui::CollapsingHeader("Posision/Matrix"))
            {
                ng::gui::ViewVector3("Posision ", m_render_pipeline_3d.CameraPos());
                ng::gui::ViewMatrix4("ViewMatrix", m_render_pipeline_3d.ModelViewMatrix());
                ng::gui::ViewMatrix4("ProjectionMatrix", m_render_pipeline_3d.ProjectionMatrix());
                ng::gui::ViewMatrix4("ComboMatrix", m_render_pipeline_3d.ComboMatrix());

            }
            ImGui::EndGroup();

        }


        ImGui::End();

    }
}

inline void RenderMocWindow(bool& m_software_renderer_visible)
{
    static RenderPixelbuffer  m_soft_renderer_3d(m_moc_buffer.Width(), m_moc_buffer.Height());
    static int mode = 0;

    if (mode == 0) m_rasterizer_3d.GetMOCBufferRef().VisualizeDepth(m_moc_buffer.GetRawBuffer());
    if (mode == 1) m_rasterizer_3d.GetMOCBufferRef().VisualizeMask(m_moc_buffer.GetRawBuffer());


   
    m_soft_renderer_3d.Update(m_moc_buffer.GetRawBuffer());


    if (ImGui::Begin(
        "MaskedOcclusionBuffer",
        &m_software_renderer_visible,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_MenuBar))
    {
        if (ImGui::BeginMenuBar()) {

            if (ImGui::BeginMenu("VisualizeMode")) {

                if (ImGui::MenuItem("Depth", NULL, mode == 0)) mode = 0;
                if (ImGui::MenuItem("Mask", NULL, mode == 1)) mode = 1;
                ImGui::EndMenu();
            }


            ImGui::EndMenuBar();
        }


        ImVec2 cpos1 = ImGui::GetCursorPos();

        ImVec2 rsize(
            (float)m_soft_renderer_3d.Width(),
            (float)m_soft_renderer_3d.Height());

        ImVec2 rr = ImGui::GetWindowContentRegionMin();


        ImGui::SetWindowSize(
            ImVec2(
                rsize.x + 20,
                rsize.y + 10 + cpos1.y));


        ImGui::Image(m_soft_renderer_3d.GetImTextureID(), rsize);

        ImGui::SetCursorPos(cpos1);


        ImGui::End();

    }
}

inline void addValue(std::vector<float>& data, float value, size_t maxSize = 100)
{
    data.insert(data.begin(), value);

    if (data.size() > maxSize)
        data.pop_back();
}

template <class T>
struct Object3D {
    std::vector<ng::geometry::Primitive<3, T>> polys;
    ng::bounding::AABBox<3, T> aabb;
};


template <class T>
inline ng::math::Matrix<4, 1, T> SetNormalColor(ng::math::Matrix<3, 1, T> normal, T red = 1, T green = 1, T blue = 1, T alpha = 1) {
    ng::math::Matrix<3, 1, T> color3 = (T)0.5 + normal * (T)0.5;
    ng::math::Matrix<4, 1, T> color4;


    T f = (T)((T)0.299 * color3.x() + (T)0.587 * (T)color3.y() + (T)0.114 * (T)color3.z());


    color4.x() = f * (T)red;
    color4.y() = f * (T)green;
    color4.z() = f * (T)blue;
    color4.w() = (T)alpha;
    return color4;
}

template <class T>
inline Object3D<T> CreateCube(T h = 10, const ng::math::Matrix<3, 1, T>& p = ng::math::Matrix<3, 1, T>(0, 0, 0), double alpha = 1) {

    Object3D<T> obj;
    std::vector<ng::math::Matrix<3, 1, T>> v =
    {
        {-h + p.x(), -h + p.y(), -h + p.z()}, // 0
        { h + p.x(), -h + p.y(), -h + p.z()}, // 1
        { h + p.x(),  h + p.y(), -h + p.z()}, // 2
        {-h + p.x(),  h + p.y(), -h + p.z()}, // 3

        {-h + p.x(), -h + p.y(),  h + p.z()}, // 4
        { h + p.x(), -h + p.y(),  h + p.z()}, // 5
        { h + p.x(),  h + p.y(),  h + p.z()}, // 6
        {-h + p.x(),  h + p.y(),  h + p.z()}  // 7
    };

    std::vector<std::vector<ng::math::Matrix<3, 1, T>>> cube =
    {
        { v[4], v[5], v[6], v[7] }, // Front
        { v[1], v[0], v[3], v[2] }, // Back
        { v[0], v[4], v[7], v[3] }, // Left
        { v[5], v[1], v[2], v[6] }, // Right
        { v[3], v[7], v[6], v[2] }, // Top
        { v[0], v[1], v[5], v[4] }  // Bottom
    };

    std::vector<std::vector<ng::math::Matrix<3, 1, T>>> cuber =
    {
        { v[7], v[6], v[5], v[4] }, // Front
        { v[2], v[3], v[0], v[1] }, // Back
        { v[3], v[7], v[4], v[0] }, // Left
        { v[6], v[2], v[1], v[5] }, // Right
        { v[2], v[6], v[7], v[3] }, // Top
        { v[4], v[5], v[1], v[0] }  // Bottom
    };


    std::vector<ng::geometry::Primitive<3, T>> polys;


    for (auto face : cuber) {

        ng::geometry::Primitive<3, T> p = ng::geometry::Primitive<3, T>(face);
        ng::math::Matrix<4, 1, T> color = SetNormalColor(p.Plane().normal());

        for (auto& v : p.Vertices())
        {
            v.color() = color;
        }
        polys.push_back(p);
    }

    obj.polys = polys;
    obj.aabb += polys;
    return obj;
}

inline void ToggleBit(uint32_t& value, uint32_t bit)
{
    if (value & bit) value &= ~bit; else value |= bit;

}

inline void SomeDebugStuff() {
    {
		bool first_start = true;
        static float gamma = 1;
        static float brightness = 2;

        if (first_start) {
            first_start = false;
            gamma = m_lmCorr.gamma;
            brightness = m_lmCorr.brightness;

        }

        int width = 400;
        float green = 1;
        ImVec2 start = ImGui::GetCursorScreenPos();
        start.x -= 20;
        start.y -= 20;
        int help;
        if (ImGui::BeginChild(0x1200ff)) {
            ng::renderer::rasterizer::RenderStatistics stat = m_render_pipeline_3d.GetStatistics();
            if (ImGui::CollapsingHeader("Visual")) {
            
                ImGui::PlotLines("##FPS Counter", m_fps_count.data(), (int)m_out_polycount.size(), 0, "FPS", 0.0f, m_max_fps, ImVec2((float)width, 40));
                ImGui::PlotLines("##Polygons In", m_in_polycount.data(), (int)m_in_polycount.size(), 0, "Polygons in", 0.0f, m_max_in_polys, ImVec2((float)width, 40));
                ImGui::PlotLines("##Polygons Out", m_out_polycount.data(), (int)m_out_polycount.size(), 0, "Polygons out", 0.0f, m_max_out_polys, ImVec2((float)width, 40));
                ImGui::PlotLines("##Cells", m_out_cellscount.data(), (int)m_out_cellscount.size(), 0, "Cells out", 0.0f, m_max_out_cells, ImVec2((float)width, 40));
            }

            if (ImGui::CollapsingHeader("Infio"))
            {
                ImGui::TextColored(ImVec4(1, green, 1, 1), "Width....................................: %i", m_rasterizer_3d.Width());
                ImGui::TextColored(ImVec4(1, green, 1, 1), "Height...................................: %i", m_rasterizer_3d.Height());

                ImGui::Separator();
                ImGui::TextColored(ImVec4(1, green, 1, 1), "Leaf in Bsp..............................: %i", m_num_bsp_leafs);
                ImGui::TextColored(ImVec4(1, green, 1, 1), "Cells....................................: %i", stat.cells_in_pvs);
                ImGui::TextColored(ImVec4(1, green, 1, 1), "Cells with Geometry......................: %i", stat.cells_with_geometry);
                ImGui::TextColored(ImVec4(1, green, 1, 1), "Cells Frustum culled.....................: %i", stat.cells_frustum_culled);
                ImGui::TextColored(ImVec4(1, green, 1, 1), "Cells culled by MOC......................: %i", stat.cells_culled_by_moc);
                ImGui::TextColored(ImVec4(1, green, 1, 1), "Cells culled by HZB......................: %i", stat.cells_culled_by_hzb);
                ImGui::TextColored(ImVec4(1, green, 1, 1), "Cells rendert............................: %i", stat.cells_rendered);
                ImGui::Separator();




                ImGui::TextColored(ImVec4(1, green, 1, 1), "Objects in...............................: %i", stat.objs_in);


                if (m_render_pipeline_3d.GetCullingMode() & ng::renderer::rasterizer::CULL_HZB)

                {
                    ImGui::TextColored(ImVec4(1, green, 1, 1), "Objects culled by HZB....................: %i", stat.objs_culled_by_hzb);

                }

                else ImGui::TextColored(ImVec4(1, 0, 0, 1), "Objects culled by HZB.....................: %i OFF", stat.objs_culled_by_hzb);

                if (m_render_pipeline_3d.GetCullingMode() & ng::renderer::rasterizer::CULL_FRUSTUM)
                {
                    ImGui::TextColored(ImVec4(1, green, 1, 1), "Objects culled by Frustum................: %i", stat.objs_frustum_culled);
                }
                else ImGui::TextColored(ImVec4(1, 0, 0, 1), "Objects culled by Frustum................: %i OFF", stat.objs_frustum_culled);

                if (m_render_pipeline_3d.GetCullingMode() & ng::renderer::rasterizer::CULL_MOC)
                {
                    ImGui::TextColored(ImVec4(1, green, 1, 1), "Objects culled by MaskedOcclusionBuffer..: %i", stat.objs_culled_by_moc);


                }
                else ImGui::TextColored(ImVec4(1, 0, 0, 1), "Objects culled by MaskedOcclusionBuffer..: %i OFF", stat.objs_culled_by_moc);





                ImGui::TextColored(ImVec4(1, green, 1, 1), "Polygons in..............................: %i", stat.polys_in);



                if (m_render_pipeline_3d.GetCullingMode() & ng::renderer::rasterizer::CULL_BACKFACE || m_render_pipeline_3d.GetCullingMode() & ng::renderer::rasterizer::CULL_FRONTFACE)
                {
                    ImGui::TextColored(ImVec4(1, green, 1, 1), "Polygons Faceculled......................: %i %s %s", stat.polys_face_culled,
                        (m_render_pipeline_3d.GetCullingMode() & ng::renderer::rasterizer::CULL_FRONTFACE) ? "FRONT" : "",
                        (m_render_pipeline_3d.GetCullingMode() & ng::renderer::rasterizer::CULL_BACKFACE) ? "BACK" : "");

                }
                else ImGui::TextColored(ImVec4(1, 0, 0, 1), "Polygons Faceculled......................: %i", stat.polys_face_culled);


                if (m_render_pipeline_3d.GetCullingMode() & ng::renderer::rasterizer::CULL_FRUSTUM)
                {
                    ImGui::TextColored(ImVec4(1, green, 1, 1), "Polygons culled by Frustum...............: %i", stat.polys_frustum_culled);

                }
                else ImGui::TextColored(ImVec4(1, 0, 0, 1), "Polygons culled by Frustum................: %i OFF", stat.polys_frustum_culled);;

                if (m_render_pipeline_3d.GetCullingMode() & ng::renderer::rasterizer::CULL_TRANSFORM)
                {
                    ImGui::TextColored(ImVec4(1, green, 1, 1), "Polygons transformculled.................: %i", stat.polys_culled_by_transform);
                }
                else ImGui::TextColored(ImVec4(1, 0, 0, 1), "Polygons transformculled..................: %i OFF", stat.polys_culled_by_transform);

                if (stat.polys_in > 0) {
                    double pro = (100.0 / (double)stat.polys_in) * (double)stat.polys_rendered;
                    ImGui::TextColored(ImVec4(1, green, 1, 1), "Polygons rendered........................: %i %3.2f%%", stat.polys_rendered, pro);
                }
                else ImGui::TextColored(ImVec4(1, green, 1, 1), "Polygons rendered........................: %i", stat.polys_rendered);
            }
            if (ImGui::CollapsingHeader("Lighting")) {

                ImGui::SliderFloat("Gamma", &gamma, 0, 2);
                ImGui::SliderFloat("Brightness", &brightness, 0, 2);
                if (m_lmCorr.brightness != brightness || m_lmCorr.gamma != gamma) {
                    m_lmCorr.brightness = brightness;
                    m_lmCorr.gamma = gamma;
                    m_lmCorr.Rebuild();

                }
                
            }


            help = (int)ImGui::GetCursorScreenPos().y;
            ImGui::EndChild();
            }
        ImVec2 end = ImGui::GetItemRectSize();
        end.y = float(help + 20);
        end.x += float(start.x+20);
        ImGui::GetWindowDrawList()->AddRectFilled(
            start,
            end,
            IM_COL32(30, 30, 30, 180),
            5.0f
        );
    
    }
}

inline void AddRenderMenuSubMenue() {
    uint32_t mode = m_render_pipeline_3d.GetCullingMode();
    if (ImGui::BeginMenu("Culling")) {
        if (ImGui::BeginMenu("Face")) {
            if (ImGui::MenuItem("Front", "", mode & ng::renderer::rasterizer::CULL_FRONTFACE)) ToggleBit(mode, ng::renderer::rasterizer::CULL_FRONTFACE);
            if (ImGui::MenuItem("Back", "", mode & ng::renderer::rasterizer::CULL_BACKFACE))   ToggleBit(mode, ng::renderer::rasterizer::CULL_BACKFACE);
            if (ImGui::MenuItem("Frustum", "", mode & ng::renderer::rasterizer::CULL_FRUSTUM))  ToggleBit(mode, ng::renderer::rasterizer::CULL_FRUSTUM);
            if (ImGui::MenuItem("Transform", "", mode & ng::renderer::rasterizer::CULL_TRANSFORM))  ToggleBit(mode, ng::renderer::rasterizer::CULL_TRANSFORM);
            if (ImGui::MenuItem("HZB", "", mode & ng::renderer::rasterizer::CULL_HZB))  ToggleBit(mode, ng::renderer::rasterizer::CULL_HZB);
            if (ImGui::MenuItem("Masked Occlusion Buffer", "", mode & ng::renderer::rasterizer::CULL_MOC))  ToggleBit(mode, ng::renderer::rasterizer::CULL_MOC);
            ImGui::EndMenu();
        }

        ImGui::EndMenu();

    }
    if (ImGui::BeginMenu("Physics")) {
        if (ImGui::MenuItem("Use collision", "", m_use_collision)) m_use_collision = !m_use_collision;
        ImGui::EndMenu();
    }
    
    if (ImGui::BeginMenu("Renderer")) {
        if (ImGui::MenuItem("Pre sort Objects", "", m_render_pipeline_3d.IsPreSortObjsEnabled())) m_render_pipeline_3d.PreSortObjs(!m_render_pipeline_3d.IsPreSortObjsEnabled());
        if (ImGui::MenuItem("LOD", "", m_use_lod)) m_use_lod = !m_use_lod;
        if (ImGui::MenuItem("After FX", "", m_use_after_fx)) m_use_after_fx = !m_use_after_fx;
        if (ImGui::MenuItem("Use Tile based Rendering", "", m_rasterizer_3d.TiledRenderingEnabled())) m_use_tile_base_rendering = !m_use_tile_base_rendering;
        if (ImGui::MenuItem("Render Lightmaps", "", m_use_lightmap)) m_use_lightmap = !m_use_lightmap;
        ImGui::EndMenu();
    }
    m_render_pipeline_3d.SetCullingMode(mode);
    m_rasterizer_3d.SetTiledRenderingEnabled(m_use_tile_base_rendering);
}

inline void UpdateRenderMovement(bool reset_mouse = false) {

    static int m_mouse_x = -1;
    static int m_mouse_y = -1;
    static ImVec2 m_mouse_pos;
    static ImVec2 m_mouse_pos_old;
    static ImVec2 m_mouse_steps;
    static ng::controller::CameraController<NG_MATH_FLOAT> cam_controller;
    m_mouse_pos = ImGui::GetMousePos();
    m_mouse_steps.x = m_mouse_pos.x - m_mouse_pos_old.x;
    m_mouse_steps.y = m_mouse_pos.y - m_mouse_pos_old.y;
    m_mouse_pos_old = m_mouse_pos;

    // NEU: Wunsch-Eingabe fuer diesen Frame (wird unten je nach Kollisions-
    // Modus entweder direkt bewegt oder durch den Player-Controller geschickt).
    float wishRight = 0.0f;
    float wishForward = 0.0f;
    bool speedup = false;
    bool wishJump = false;

    if (ImGui::IsWindowFocused()) {
        m_mouse_pos.x = -1;
        m_mouse_pos.y = -1;



        if (reset_mouse) {
            m_mouse_x = -1;
            m_mouse_y = -1;
        }
        ImVec2 cpos1 = ImGui::GetCursorPos();

        ImVec2 cpos = ImGui::GetCursorPos();
        ImVec2 pos = ImGui::GetWindowPos();

        cpos.x += pos.x;
        cpos.y += pos.y;

        if (m_mouse_pos.x >= cpos.x && m_mouse_pos.x < cpos.x + m_rasterizer_3d.Width()) {
            m_mouse_x = (int)m_mouse_pos.x - (int)cpos.x;
        }
        else m_mouse_x = -1;

        if (m_mouse_pos.y >= cpos.y && m_mouse_pos.y < cpos.y + m_rasterizer_3d.Height()) {
            m_mouse_y = (int)m_mouse_pos.y - (int)cpos.y;

        }
        else m_mouse_y = -1;

        ImVec2 wmin = ImGui::GetWindowContentRegionMin();
        ImVec2 wmax = ImGui::GetWindowContentRegionMax();
        wmin.x += pos.x;
        wmin.y += pos.y;
        wmax.x += pos.x;
        wmax.y += pos.y;

        if (ImGui::IsMouseHoveringRect(wmin, wmax) && m_mouse_pos_old.x != -1 && m_mouse_pos_old.y != -1) {

           // float speed = 5.0;
            const float speed2 = 0.25;

            if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                cam_controller.Turn(m_camera, m_mouse_steps.y * speed2, m_mouse_steps.x * speed2);
            }

            // GEAENDERT: WASD bewegt nicht mehr direkt die Kamera, sondern setzt
            // nur noch die Wunschrichtung. Ausgefuehrt wird die Bewegung weiter
            // unten, je nachdem ob Kollision an oder aus ist.
            if (ImGui::IsKeyDown(ImGuiKey_LeftShift)) speedup = true;
            if (ImGui::IsKeyDown(ImGuiKey_W)) wishForward -= 1.0f;
            if (ImGui::IsKeyDown(ImGuiKey_S)) wishForward += 1.0f;
            if (ImGui::IsKeyDown(ImGuiKey_A)) wishRight -= 1.0f;
            if (ImGui::IsKeyDown(ImGuiKey_D)) wishRight += 1.0f;
            if (ImGui::IsKeyDown(ImGuiKey_Space)) wishJump = true;
        }
    }

    static auto lastTime = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    float dt = std::chrono::duration<float>(now - lastTime).count();
    dt = std::min(dt, 0.05f); // Cap gegen Spiral-of-Death bei Lags
    lastTime = now;

    // Einmalig: Player an der aktuellen Kameraposition "einklinken".
    if (!m_player.IsInitialized())
    {
        m_player.Init(m_camera.position(), /*eyeHeight*/ 28.0f);
        m_player.SetGravity(ng::math::Matrix<3,1, NG_MATH_FLOAT>(0, -800, 0));   // simulierte Erdanziehung zum Testen
    }

    m_player.SetEnabled(m_use_collision);

    if (m_use_collision)
    {
        // Kollision aktiv: Ellipsoid + Gravitationsvektor, Bewegung/Position
        // werden intern gegen den BSP getestet und setzen die Kamera selbst.
        if (speedup) {
            wishRight *= 5;
            wishForward *= 5;
        }
        m_player.Update(m_camera, dt, wishRight, wishForward, wishJump);
    }
    else
    {
        // Kollision aus: exakt dein bisheriges freies Fliegen.
        float speed = 5.0f;
        if (speedup) speed *= 2;
        if (wishForward != 0.0f) cam_controller.Move(m_camera, 0, wishForward * speed, 0);
        if (wishRight != 0.0f) cam_controller.Move(m_camera, wishRight * speed, 0, 0);

        // Player-Zustand synchron halten, damit beim Umschalten auf "Kollision an"
        // keine alte Position/Geschwindigkeit uebernommen wird.
        m_player.Init(m_camera.position(), 28.0f);
    }

    for (auto& m : m_mesh_obj) {
        m.Update(dt);
        m.EnableLOD(m_use_lod);
    }

}

inline void RenderWindow(bool& m_software_renderer_visible)
{

    static RenderPixelbuffer  m_soft_renderer_3d(m_rasterizer_3d.Width(), m_rasterizer_3d.Height());

    m_soft_renderer_3d.Update(m_rasterizer_3d.GetColorBuffer().GetRawBuffer());


    if (ImGui::Begin(
        "Renderer",
        &m_software_renderer_visible,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_MenuBar))
    {


        if (ImGui::BeginMenuBar()) {

            AddRenderMenuSubMenue();


            ImGui::EndMenuBar();
        }


        UpdateRenderMovement();

        ImVec2 cpos1 = ImGui::GetCursorPos();

        ImVec2 rsize(
            (float)m_soft_renderer_3d.Width(),
            (float)m_soft_renderer_3d.Height());

        ImVec2 rr = ImGui::GetWindowContentRegionMin();


        ImGui::SetWindowSize(
            ImVec2(
                rsize.x + 20,
                rsize.y + 10 + cpos1.y));


        ImGui::Image(m_soft_renderer_3d.GetImTextureID(), rsize);

        ImGui::SetCursorPos(cpos1);
        //        ImGui::Text("FPS: %.1 Frame Time: %.3f ms", fps.FPS(), fps.DeltaTime() * 1000.0);


        ImGui::End();

    }
}




template<class T>
inline void DrawLightingVisSetup(
    ng::lightmap::RadiosityOptions<T>& radiosity,
    ng::lightmap::LightmapOptions& lightmap,
    ng::bsp::VisCellOptions<T>& visCell,
    ng::bsp::PVSOptions<T>& pvs)
{
    // ================================================================
    // RADIOSITY
    // ================================================================
    if (ImGui::CollapsingHeader("Radiosity", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox(
            "Build Direct Radiosity",
            &radiosity.buildDirectRadiosity);

        ImGui::InputInt(
            "Bounces",
            &radiosity.bounces);

        ImGui::InputInt(
            "Samples per Patch",
            &radiosity.samplesPerPatch);

        ImGui::InputDouble(
            "Max Ray Distance",
            &radiosity.maxRayDistance,
            64.0,
            512.0,
            "%.3f");

        ImGui::InputDouble(
            "Epsilon",
            &radiosity.epsilon,
            0.001,
            0.01,
            "%.6f");

        int seed = static_cast<int>(radiosity.randomSeed);

        if (ImGui::InputInt(
            "Random Seed",
            &seed))
        {
            if (seed < 0)
                seed = 0;

            radiosity.randomSeed =
                static_cast<unsigned int>(seed);
        }

        ImGui::InputDouble(
            "Ambient Floor",
            &radiosity.ambientFloor,
            0.01,
            0.1,
            "%.4f");

        ImGui::InputDouble(
            "Indirect Gain",
            &radiosity.indirectGain,
            0.1,
            1.0,
            "%.3f");

        ImGui::InputInt(
            "Block Size",
            &radiosity.blockSize);

        ImGui::InputDouble(
            "Albedo Min",
            &radiosity.albedoMin,
            0.01,
            0.1,
            "%.3f");

        ImGui::InputDouble(
            "Albedo Max",
            &radiosity.albedoMax,
            0.01,
            0.1,
            "%.3f");

        // textureEmissionStrength kann T=float/double sein
        double emission =
            static_cast<double>(radiosity.textureEmissionStrength);

        if (ImGui::InputDouble(
            "Texture Emission Strength",
            &emission,
            0.01,
            0.1,
            "%.3f"))
        {
            radiosity.textureEmissionStrength =
                static_cast<T>(emission);
        }
    }


    // ================================================================
    // LIGHTMAP
    // ================================================================
    if (ImGui::CollapsingHeader("Lightmap", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox(
            "Build Direct Light",
            &lightmap.buildDirectLight);

        ImGui::InputInt(
            "Start Atlas Size",
            &lightmap.startAtlasSize);

        ImGui::InputInt(
            "Max Atlas Size",
            &lightmap.maxAtlasSize);

        ImGui::InputDouble(
            "Texel Density",
            &lightmap.texelDensity,
            0.5,
            2.0,
            "%.3f");

        ImGui::InputInt(
            "Max Lightmap Size",
            &lightmap.maxLightmapSize);

        ImGui::InputDouble(
            "Epsilon",
            &lightmap.epsilon,
            0.001,
            0.01,
            "%.6f");

        ImGui::InputDouble(
            "Pack Efficiency",
            &lightmap.packEfficiency,
            0.01,
            0.05,
            "%.3f");

        ImGui::InputInt(
            "Ambient Floor",
            &lightmap.kAmbientFloor);

        ImGui::Checkbox(
            "Only Covered Texels",
            &lightmap.onlyCoveredTexels);

        ImGui::InputDouble(
            "Coverage Margin",
            &lightmap.coverageMargin,
            0.1,
            0.5,
            "%.3f");
    }


    // ================================================================
    // VIS CELL
    // ================================================================
    if (ImGui::CollapsingHeader("Vis Cells", ImGuiTreeNodeFlags_DefaultOpen))
    {
        double minPlaneArea =
            static_cast<double>(visCell.minPlaneArea);

        if (ImGui::InputDouble(
            "Min Plane Area",
            &minPlaneArea,
            64.0,
            512.0,
            "%.3f"))
        {
            visCell.minPlaneArea =
                static_cast<T>(minPlaneArea);
        }

        int maxStructuralPlanes =
            static_cast<int>(visCell.maxStructuralPlanes);

        if (ImGui::InputInt(
            "Max Structural Planes",
            &maxStructuralPlanes))
        {
            if (maxStructuralPlanes < 0)
                maxStructuralPlanes = 0;

            visCell.maxStructuralPlanes =
                static_cast<size_t>(maxStructuralPlanes);
        }

        ImGui::Checkbox(
            "Merge Small Cells",
            &visCell.mergeSmallCells);

        double maxOccluderArea =
            static_cast<double>(visCell.maxOccluderArea);

        if (ImGui::InputDouble(
            "Max Occluder Area",
            &maxOccluderArea,
            64.0,
            512.0,
            "%.3f"))
        {
            visCell.maxOccluderArea =
                static_cast<T>(maxOccluderArea);
        }

        double epsilon =
            static_cast<double>(visCell.epsilon);

        if (ImGui::InputDouble(
            "Epsilon",
            &epsilon,
            0.001,
            0.01,
            "%.6f"))
        {
            visCell.epsilon =
                static_cast<T>(epsilon);
        }

        double minPortalWidth =
            static_cast<double>(visCell.minPortalWidth);

        if (ImGui::InputDouble(
            "Min Portal Width",
            &minPortalWidth,
            0.01,
            0.1,
            "%.4f"))
        {
            visCell.minPortalWidth =
                static_cast<T>(minPortalWidth);
        }

        ImGui::Checkbox(
            "Verbose",
            &visCell.verbose);
    }


    // ================================================================
    // PVS
    // ================================================================
    if (ImGui::CollapsingHeader("PVS", ImGuiTreeNodeFlags_DefaultOpen))
    {
        double epsilon =
            static_cast<double>(pvs.epsilon);

        if (ImGui::InputDouble(
            "Epsilon",
            &epsilon,
            0.01,
            0.1,
            "%.4f"))
        {
            pvs.epsilon =
                static_cast<T>(epsilon);
        }

        int threads =
            static_cast<int>(pvs.threads);

        if (ImGui::InputInt(
            "Threads",
            &threads))
        {
            if (threads < 0)
                threads = 0;

            pvs.threads =
                static_cast<unsigned int>(threads);
        }

        ImGui::Checkbox(
            "Verbose",
            &pvs.verbose);

        ImGui::Checkbox(
            "Skip Void Leaves",
            &pvs.skipVoidLeaves);

        ImGui::Checkbox(
            "Require Polygons In Component",
            &pvs.requirePolygonsInComponent);

        double minPortalArea =
            static_cast<double>(pvs.minPortalArea);

        if (ImGui::InputDouble(
            "Min Portal Area",
            &minPortalArea,
            0.01,
            0.1,
            "%.4f"))
        {
            pvs.minPortalArea =
                static_cast<T>(minPortalArea);
        }

        double clipTolerance =
            static_cast<double>(pvs.clipTolerance);

        if (ImGui::InputDouble(
            "Clip Tolerance",
            &clipTolerance,
            0.01,
            0.1,
            "%.4f"))
        {
            pvs.clipTolerance =
                static_cast<T>(clipTolerance);
        }
    }
}



template<class T>
inline void RenderCompilerSetupWindow(bool& m_compile_setup_visible, ng::lightmap::RadiosityOptions<T>& radiosity,
    ng::lightmap::LightmapOptions& lightmap,
    ng::bsp::VisCellOptions<T>& visCell,
    ng::bsp::PVSOptions<T>& pvs)
{
    if (ImGui::Begin("Compiler Setup", &m_compile_setup_visible))
    {
        
        DrawLightingVisSetup(radiosity, lightmap, visCell, pvs);
        
        ImGui::End();
    }


}