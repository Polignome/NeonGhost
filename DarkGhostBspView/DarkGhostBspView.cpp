
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>

#include <cstdint>
#include <string>
#include <vector>
#include <iostream>
#include <chrono>
#include <algorithm>
#include <stdexcept>
#include <memory>

#include <NeonGhostCore.h>
#include <BspSceneSerialize.h>
#include <Frustum.h>
#include <Lightmap.h>
#include <Map.h>
#include <cmath>
#include <PlayerController.h>
#include <ViewMotionController.h>
// ------------------------------------------------------------
// Linker
// ------------------------------------------------------------

#pragma comment(lib, "NeonGhostCore.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

// ------------------------------------------------------------
// Types
// ------------------------------------------------------------

using MathType = NG_MATH_FLOAT;

struct AppConfig
{
    std::string filename;

    int width = 800;
    int height = 600;

    bool fullscreen = true;
};

struct InputState
{
    bool keys[256]{};

    bool mouseRight = false;
    bool mouseLeft = false;

    int mouseX = 0;
    int mouseY = 0;

    int mouseDeltaX = 0;
    int mouseDeltaY = 0;
};

// ------------------------------------------------------------
// Application
// ------------------------------------------------------------


class BspViewApplication
{

private:

    ng::renderer::postfx::PostFXManager<MathType> m_fx;

    ng::renderer::postfx::FXAAEffect<MathType>* m_fxaa = nullptr;
    ng::renderer::postfx::UnderwaterWarpEffect<MathType > * m_fxaa2 = nullptr;
public:
    NG_BIND_MANAGER


        BspViewApplication()
        : m_player(m_scene.m_bsp)
    {
        m_fxaa = m_fx.AddEffect<ng::renderer::postfx::FXAAEffect<MathType>>();
        m_fxaa2 = m_fx.AddEffect<ng::renderer::postfx::UnderwaterWarpEffect<MathType>>();
        m_fxaa->enabled = false;
        m_fxaa2->enabled = false;
    }

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


    bool Initialize(int argc, char** argv)
    {
        if (!ParseArguments(argc, argv))
            return false;

        if (!CreateWindowApp())
            return false;

        if (!CreateFramebuffer())
            return false;

        if (!InitializeRenderer(m_config.width, m_config.height))
            return false;

        m_fx.SetSize(m_config.width, m_config.height);

        

        if (!LoadScene(m_config.filename))
            return false;

        m_lastTime = Clock::now();

        return true;
    }

    int Run()
    {
        MSG msg{};

        while (m_running)
        {
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
            {
                if (msg.message == WM_QUIT)
                {
                    m_running = false;
                    break;
                }

                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }

            if (!m_running)
                break;

            UpdateTime();

            UpdateInput();

            UpdateCamera();

            RenderFrame();
            
            Present();
        }

        return 0;
    }

    void Shutdown()
    {
        m_scene = ng::io::BspScene<MathType>();
        m_lightmap = ng::renderer::gfx::Texture();

        ng::resource::Manager::Flush();

        if (m_window)
        {
            DestroyWindow(m_window);
            m_window = nullptr;
        }

        RestoreDisplayMode();
    }

private:

    ng::controller::CameraController<MathType> m_cameraController;
    ng::controller::ViewMotion<MathType> m_viewMotion;

    ng::controller::PlayerController<MathType> m_player;

    bool m_use_collision = false;
    bool m_collisionKeyWasDown = false;
    bool m_antialiasingKeyWasDown = false;
    using Clock = std::chrono::steady_clock;

    AppConfig m_config;
    InputState m_input;

    HWND m_window = nullptr;

    bool m_running = true;

    std::vector<uint32_t> m_framebuffer;

    BITMAPINFO m_bitmapInfo{};

    DEVMODEW m_oldDisplayMode{};
    bool m_displayModeChanged = false;

    float m_deltaTime = 0.016f;

    Clock::time_point m_lastTime;

    ng::io::BspScene<MathType> m_scene;

    ng::renderer::gfx::Texture m_lightmap;

    // --------------------------------------------------------
    // Renderer objects
    //
    // Hier müssen die tatsächlichen Konstruktoren und
    // Typdefinitionen aus deinem Global.h eingesetzt werden.
    // --------------------------------------------------------

    // Beispiel:
    //
    // ng::renderer::rasterizer::Rasterizer3D<MathType>
    //     m_rasterizer;
    //
    // ng::renderer::pipeline::RenderPipeline3D<MathType>
    //     m_pipeline;
    //
    // ng::scene::Camera<MathType> m_camera;

    // --------------------------------------------------------
    // Command line
    // --------------------------------------------------------

    bool ParseArguments(int argc, char** argv)
    {
        for (int i = 1; i < argc; ++i)
        {
            std::string arg = argv[i];

            if (arg == "-window")
            {
                m_config.fullscreen = false;
            }
            else if (arg == "-fullscreen")
            {
                m_config.fullscreen = true;
            }
            else if (arg == "-width" && i + 1 < argc)
            {
                m_config.width = std::stoi(argv[++i]);
            }
            else if (arg == "-height" && i + 1 < argc)
            {
                m_config.height = std::stoi(argv[++i]);
            }
            else if (arg == "-help" || arg == "--help")
            {
                PrintHelp();
                return false;
            }
            else if (!arg.empty() && arg[0] != '-')
            {
                if (m_config.filename.empty())
                    m_config.filename = arg;
                else
                    return false;
            }
            else
            {
                std::cerr << "Unknown argument: "
                    << arg << "\n";
                return false;
            }
        }

        if (m_config.filename.empty())
        {
            std::cerr << "Missing BSP filename.\n";
            PrintHelp();
            return false;
        }

        if (m_config.width <= 0 ||
            m_config.height <= 0 ||
            m_config.width > 16384 ||
            m_config.height > 16384)
        {
            std::cerr << "Invalid resolution.\n";
            return false;
        }

        return true;
    }

    void PrintHelp()
    {
        std::cout
            << "\nDarkGhostBspView\n\n"
            << "Usage:\n"
            << "  DarkGhostBspView.exe scene.ngbps [options]\n\n"
            << "Options:\n"
            << "  -width <pixels>\n"
            << "  -height <pixels>\n"
            << "  -window\n"
            << "  -fullscreen\n";
    }

    // --------------------------------------------------------
    // Display
    // --------------------------------------------------------

    bool SetFullscreen()
    {
        m_oldDisplayMode.dmSize = sizeof(DEVMODEW);

        if (!EnumDisplaySettingsW(
            nullptr,
            ENUM_CURRENT_SETTINGS,
            &m_oldDisplayMode))
        {
            return false;
        }

        DEVMODEW mode{};
        mode.dmSize = sizeof(DEVMODEW);

        mode.dmPelsWidth = m_config.width;
        mode.dmPelsHeight = m_config.height;
        mode.dmBitsPerPel = 32;

        mode.dmFields =
            DM_PELSWIDTH |
            DM_PELSHEIGHT |
            DM_BITSPERPEL;

        LONG result = ChangeDisplaySettingsW(
            &mode,
            CDS_FULLSCREEN
        );

        if (result != DISP_CHANGE_SUCCESSFUL)
            return false;

        m_displayModeChanged = true;

        return true;
    }

    void RestoreDisplayMode()
    {
        if (m_displayModeChanged)
        {
            ChangeDisplaySettingsW(nullptr, 0);
            m_displayModeChanged = false;
        }
    }

    // --------------------------------------------------------
    // Window
    // --------------------------------------------------------

    bool CreateWindowApp()
    {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);

        wc.style = CS_HREDRAW | CS_VREDRAW;

        wc.lpfnWndProc = StaticWindowProc;

        wc.hInstance = GetModuleHandleW(nullptr);

        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);

        wc.lpszClassName = L"DarkGhostBspView";

        wc.hbrBackground = nullptr;

        if (!RegisterClassExW(&wc))
            return false;

        DWORD style;
        DWORD exStyle = 0;

        int x = CW_USEDEFAULT;
        int y = CW_USEDEFAULT;

        int width = m_config.width;
        int height = m_config.height;

        if (m_config.fullscreen)
        {
            if (!SetFullscreen())
                return false;

            style = WS_POPUP;

            x = 0;
            y = 0;
        }
        else
        {
            style = WS_OVERLAPPEDWINDOW;

            RECT rect{ 0, 0, width, height };

            AdjustWindowRectEx(
                &rect,
                style,
                FALSE,
                exStyle
            );

            width = rect.right - rect.left;
            height = rect.bottom - rect.top;
        }

        m_window = CreateWindowExW(
            exStyle,
            wc.lpszClassName,
            L"DarkGhostBspView",
            style,
            x, y,
            width, height,
            nullptr,
            nullptr,
            wc.hInstance,
            this
        );

        if (!m_window)
            return false;

        ShowWindow(m_window, SW_SHOW);
        UpdateWindow(m_window);

        return true;
    }

    static LRESULT CALLBACK StaticWindowProc(
        HWND hwnd,
        UINT msg,
        WPARAM wParam,
        LPARAM lParam)
    {
        BspViewApplication* app =
            reinterpret_cast<BspViewApplication*>(
                GetWindowLongPtrW(hwnd, GWLP_USERDATA)
                );

        if (msg == WM_NCCREATE)
        {
            auto* create =
                reinterpret_cast<CREATESTRUCTW*>(lParam);

            app = static_cast<BspViewApplication*>(
                create->lpCreateParams
                );

            SetWindowLongPtrW(
                hwnd,
                GWLP_USERDATA,
                reinterpret_cast<LONG_PTR>(app)
            );
        }

        if (app)
            return app->WindowProc(hwnd, msg, wParam, lParam);

        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    LRESULT WindowProc(
        HWND hwnd,
        UINT msg,
        WPARAM wParam,
        LPARAM lParam)
    {
        switch (msg)
        {
        case WM_CLOSE:
            m_running = false;
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        case WM_KEYDOWN:
            if (wParam < 256)
                m_input.keys[wParam] = true;

            return 0;

        case WM_KEYUP:
            if (wParam < 256)
                m_input.keys[wParam] = false;

            return 0;

        case WM_RBUTTONDOWN:
            m_input.mouseRight = true;
            SetCapture(hwnd);
            return 0;

        case WM_RBUTTONUP:
            m_input.mouseRight = false;
            ReleaseCapture();
            return 0;

        case WM_LBUTTONDOWN:
            m_input.mouseLeft = true;
            return 0;

        case WM_LBUTTONUP:
            m_input.mouseLeft = false;
            return 0;

        case WM_MOUSEMOVE:
        {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);

            m_input.mouseDeltaX += x - m_input.mouseX;
            m_input.mouseDeltaY += y - m_input.mouseY;

            m_input.mouseX = x;
            m_input.mouseY = y;

            return 0;
        }

        case WM_KILLFOCUS:
            m_input = {};
            return 0;

        }

        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    // --------------------------------------------------------
    // Framebuffer
    // --------------------------------------------------------

    bool CreateFramebuffer()
    {
        size_t count =
            static_cast<size_t>(m_config.width) *
            static_cast<size_t>(m_config.height);

        m_framebuffer.resize(count);

        m_bitmapInfo = {};

        m_bitmapInfo.bmiHeader.biSize =
            sizeof(BITMAPINFOHEADER);

        m_bitmapInfo.bmiHeader.biWidth =
            m_config.width;

        m_bitmapInfo.bmiHeader.biHeight =
            -m_config.height;

        m_bitmapInfo.bmiHeader.biPlanes = 1;
        m_bitmapInfo.bmiHeader.biBitCount = 32;
        m_bitmapInfo.bmiHeader.biCompression = BI_RGB;

        return true;
    }

    void Present()
    {
        if (!m_window || !m_rasterizer)
            return;

        HDC dc = GetDC(m_window);
        if (!dc)
            return;

        RECT rect{};
        GetClientRect(m_window, &rect);

        int width = rect.right - rect.left;
        int height = rect.bottom - rect.top;

        if (width > 0 && height > 0)
        {
            auto& target = m_rasterizer->GetTarget();

            const size_t pixelCount =
                static_cast<size_t>(target.width) *
                static_cast<size_t>(target.height);

            std::vector<uint32_t> bgra(pixelCount);

            const uint8_t* src =
                reinterpret_cast<const uint8_t*>(target.color);

            uint8_t* dst =
                reinterpret_cast<uint8_t*>(bgra.data());

            for (size_t i = 0; i < pixelCount; ++i)
            {
                dst[i * 4 + 0] = src[i * 4 + 2]; // B
                dst[i * 4 + 1] = src[i * 4 + 1]; // G
                dst[i * 4 + 2] = src[i * 4 + 0]; // R
                dst[i * 4 + 3] = 255;            // X
            }

            BITMAPINFO bmi{};
            bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bmi.bmiHeader.biWidth = target.width;
            bmi.bmiHeader.biHeight = -target.height;
            bmi.bmiHeader.biPlanes = 1;
            bmi.bmiHeader.biBitCount = 32;
            bmi.bmiHeader.biCompression = BI_RGB;

            StretchDIBits(
                dc,
                0, 0, width, height,
                0, 0, target.width, target.height,
                bgra.data(),
                &bmi,
                DIB_RGB_COLORS,
                SRCCOPY
            );
        }

        ReleaseDC(m_window, dc);
    }

    // --------------------------------------------------------
    // Timing
    // --------------------------------------------------------

    void UpdateTime()
    {
        auto now = Clock::now();
        
        m_deltaTime =
            std::chrono::duration<float>(
                now - m_lastTime
            ).count();

        m_lastTime = now;

        m_deltaTime = std::min(m_deltaTime, 0.05f);
        m_totalTime += m_deltaTime;
		m_fx.Apply(m_totalTime);
    }

    // --------------------------------------------------------
    // Input
    // --------------------------------------------------------

    void UpdateInput()
    {
        if (m_input.keys[VK_ESCAPE])
            m_running = false;

        const bool collisionKeyDown = m_input.keys['C'];
        const bool AntialiasingKeyDown = m_input.keys['I'];

        if (collisionKeyDown && !m_collisionKeyWasDown)
        {
            m_use_collision = !m_use_collision;

            std::cout << "[BSP VIEW] Collision: "
                << (m_use_collision ? "ON" : "OFF")
                << "\n";
        }
        if (AntialiasingKeyDown && !m_antialiasingKeyWasDown)
        {
			m_fxaa->enabled = !m_fxaa->enabled;
        } 


        m_collisionKeyWasDown = collisionKeyDown;
        m_antialiasingKeyWasDown = AntialiasingKeyDown;
    }

    // --------------------------------------------------------
    // Scene
    // --------------------------------------------------------

    bool LoadScene(const std::string& filename)
    {
        try
        {
            std::cout << "Loading BSP: "
                << filename << "\n";

            m_scene =
                ng::io::BspScene<MathType>();

            ng::io::LoadBspScene(filename, m_scene);

            std::cout << "BSP loaded.\n";

            if (!m_scene.m_wadname.empty())
            {
                ng::map::texture::Wad3File wad(
                    m_scene.m_wadname
                );

                for (const auto& name : m_scene.m_texture_names)
                    ConvertfromWad(wad, name);
            }

            ng::io::RestoreTextures(
                m_scene,
                [](const std::string& name)
                {
                    return ng::resource::TextureResourceManager
                        ::GetResource(name);
                }
            );

            ng::resource::TextureResourceManager
                ::LoadTextureFromPixelBuffer(
                    "__lightmap_atlas__",
                    m_scene.m_lightmap
                );

            m_lightmap =
                ng::resource::TextureResourceManager
                ::GetResource("__lightmap_atlas__");

            m_scene.m_lightmap.Resize(0, 0);

            std::cout << "Scene ready.\n";

            return true;
        }
        catch (const std::exception& e)
        {
            std::cerr << "BSP error: "
                << e.what() << "\n";

            return false;
        }
    }

    // --------------------------------------------------------
    // Renderer initialization
    // --------------------------------------------------------

    using Scalar = NG_MATH_FLOAT;
    using Rasterizer = ng::renderer::rasterizer::Rasterizer3D<Scalar>;
    using Pipeline = ng::renderer::rasterizer::RenderPipeline<Scalar>;

    std::unique_ptr<Rasterizer> m_rasterizer;
    std::unique_ptr<Pipeline>   m_pipeline;
    MathType m_totalTime = 0.0;
    ng::scene::Camera<Scalar> m_camera;

    bool InitializeRenderer(int width, int height)
    {
        m_rasterizer = std::make_unique<Rasterizer>(width, height);
        m_pipeline = std::make_unique<Pipeline>(*m_rasterizer);

        m_rasterizer->SetTiledRenderingEnabled(false);
        m_rasterizer->SetTiledSingleThreadedDebug(false);

        m_camera.aspect() =
            static_cast<Scalar>(width) / static_cast<Scalar>(height);

        m_camera.fov() =
            ng::math::degreesToRadians(64.0);

        m_camera.nearp() = 0.0001;
        m_camera.farp() = 18000.0;

        m_camera.position() =
            ng::math::Matrix<3, 1, Scalar>(0, 0, 0);

        m_camera.LookAt(
            ng::math::Matrix<3, 1, Scalar>(0, 0, 0));
        return true;
    }

    // --------------------------------------------------------
    // Camera
    // --------------------------------------------------------
    void UpdateCamera()
    {
        static bool e_up = false;

        float wishRight = 0.0f;
        float wishForward = 0.0f;

        bool speedup = false;
        bool wishJump = false;

        const float dt = m_deltaTime;

        // --------------------------------------------------------
        // ViewMotion-Offset entfernen
        // --------------------------------------------------------

        m_camera.SetPosition(
            m_camera.position() - m_viewMotion.Offset()
        );

        // --------------------------------------------------------
        // Mouse look
        // --------------------------------------------------------

        if (m_input.mouseRight)
        {
            const float speed = 0.25f;

            if (m_input.mouseDeltaX != 0 ||
                m_input.mouseDeltaY != 0)
            {
                m_cameraController.Turn(
                    m_camera,
                    m_input.mouseDeltaY * speed,
                    m_input.mouseDeltaX * speed
                );

                m_viewMotion.AddTurn(
                    m_input.mouseDeltaX * speed
                );
            }
        }

        // --------------------------------------------------------
        // Movement input
        // --------------------------------------------------------

        if (m_input.keys[VK_SHIFT])
            speedup = true;

        if (m_input.keys['W'])
            wishForward -= 1.0f;

        if (m_input.keys['S'])
            wishForward += 1.0f;

        if (m_input.keys['A'])
            wishRight -= 1.0f;

        if (m_input.keys['D'])
            wishRight += 1.0f;

        if (m_input.keys[VK_SPACE])
            wishJump = true;

        // --------------------------------------------------------
        // Player initialization
        // --------------------------------------------------------

        if (!m_player.IsInitialized())
        {
            m_player.Init(
                m_camera.position(),
                28.0f,
                m_player.Radius()
            );

            m_player.SetGravity(
                ng::math::Matrix<3, 1, MathType>(
                    0, -800, 0
                )
            );
        }

        // --------------------------------------------------------
        // Collision enabled
        // --------------------------------------------------------

        m_player.SetEnabled(m_use_collision);

        if (m_use_collision)
        {
            if (speedup)
            {
                wishRight *= 5.0f;
                wishForward *= 5.0f;
            }

            const auto forwardBefore = m_camera.forward();

            m_player.Update(
                m_camera,
                dt,
                wishRight,
                wishForward,
                wishJump
            );

            // ----------------------------------------------------
            // Warp portal rotation correction
            // ----------------------------------------------------

            if (m_player.Teleported())
            {
                const auto forwardAfter = m_player.LastLook();

                const float yaw0 = std::atan2(
                    static_cast<float>(forwardBefore(0)),
                    static_cast<float>(forwardBefore(2))
                );

                const float yaw1 = std::atan2(
                    static_cast<float>(forwardAfter(0)),
                    static_cast<float>(forwardAfter(2))
                );

                constexpr float PI = 3.14159265f;

                float delta = yaw1 - yaw0;

                while (delta > PI)
                    delta -= 2.0f * PI;

                while (delta < -PI)
                    delta += 2.0f * PI;

                m_cameraController.Turn(
                    m_camera,
                    0.0f,
                    delta * 57.2957795f
                );
            }
        }

        // --------------------------------------------------------
        // Collision disabled: Noclip
        // --------------------------------------------------------

        else
        {
            float speed = 5.0f;

            if (speedup)
                speed *= 2.0f;

            if (wishForward != 0.0f)
            {
                m_cameraController.Move(
                    m_camera,
                    0,
                    wishForward * speed,
                    0
                );
            }

            if (wishRight != 0.0f)
            {
                m_cameraController.Move(
                    m_camera,
                    wishRight * speed,
                    0,
                    0
                );
            }

            // Player-Position synchronisieren
            m_player.InitFromEye(
                m_camera.position(),
                28.0f
            );
        }

        // --------------------------------------------------------
        // ViewMotion
        // --------------------------------------------------------

        m_viewMotion.Update(
            dt,
            m_player,
            m_camera
        );

        m_viewMotion.Apply(
            m_camera,
            m_cameraController
        );

        // --------------------------------------------------------
        // Reset mouse delta
        // --------------------------------------------------------

        m_input.mouseDeltaX = 0;
        m_input.mouseDeltaY = 0;
    }

    // --------------------------------------------------------
    // Rendering
    // --------------------------------------------------------

    void RenderFrame()
    {
        if (!m_rasterizer || !m_pipeline)
            return;
        m_pipeline->SetTime(m_totalTime);
        // --------------------------------------------------------
        // Rasterizer state
        // --------------------------------------------------------

        m_rasterizer->State().detph =
            ng::renderer::rasterizer::DEPTHFUNC_LESS;

        m_rasterizer->State().flags =
            ng::renderer::rasterizer::RASTER_DEPTH_TEST |
            ng::renderer::rasterizer::RASTER_DEPTH_WRITE |
            ng::renderer::rasterizer::RASTER_TEXTURE |
            ng::renderer::rasterizer::RASTER_LIGHTMAP;


        // --------------------------------------------------------
        // Clear buffers
        // --------------------------------------------------------

        m_rasterizer->GetColorBuffer().Clear(RGB(0, 0, 0));
        m_rasterizer->GetNormalBuffer().Clear(RGB(0, 0, 0));
        m_rasterizer->GetMOCBufferRef().Clear();

        // --------------------------------------------------------
        // Camera matrices
        // --------------------------------------------------------

        m_pipeline->ModelViewMatrix() =
            m_camera.ViewMatrix();

        m_pipeline->ProjectionMatrix() =
            m_camera.ProjectionMatrix();

        m_pipeline->SetCamera(m_camera);

        // --------------------------------------------------------
        // Render BSP scene
        // --------------------------------------------------------

        m_pipeline->Begin();

        auto medium =m_pipeline->RenderWorld(
            m_scene.m_cells,
            m_scene.m_pvs,
            m_lightmap,
            m_scene.m_warpPortals,
            [](int /*materialId*/)
            {
                // Material callback
            }
        );
		
        m_pipeline->End();
        auto& target = m_rasterizer->GetTarget();
        m_fx.BindColor(reinterpret_cast<uint32_t*>(target.color));
        if (medium!=0) m_fxaa2->enabled = true;
		else m_fxaa2->enabled = false;
        m_fx.Apply(m_totalTime);
    }
};

// ------------------------------------------------------------
// Entry point
// ------------------------------------------------------------

int main(int argc, char** argv)
{
    SetProcessDPIAware();

    BspViewApplication app;

    try
    {
        if (!app.Initialize(argc, argv))
        {
            app.Shutdown();
            return 1;
        }

        int result = app.Run();

        app.Shutdown();

        return result;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Fatal error: "
            << e.what() << "\n";

        app.Shutdown();

        return 1;
    }
}
