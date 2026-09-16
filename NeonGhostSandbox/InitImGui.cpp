#include <algorithm>

#include "InitImGui.h"
#include "GuiStyle.h"
#include "myfont.h"
#include "Font2.h"
#include "RenderTargetView.h"
// Data

static SDL_Window* window = nullptr;
static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;


// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();

SDL_Window* GetWindow() {
    return window;
}




RenderTargetView::RenderTargetView(int width, int height) : m_width(-1), m_height(-1)
{
    Resize(width, height);
}
void RenderTargetView::Resize(int width, int height)
{
    if (width == m_width && m_height == height) return;
    if (srv)
    {
        srv->Release();
        srv = nullptr;
    }
    if (texture)
    {
        texture->Release();
        texture = nullptr;
    }
    if (width <= 0 || height < 0) {
        m_width = 0;
        m_height = 0;
        return;
    }
    m_width = width;
    m_height = height;
    

    if (texture == nullptr && m_width > -1 && m_height > -1)
    {
        
        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = m_width;
        desc.Height = m_height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        //desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        HRESULT hr =
            g_pd3dDevice->CreateTexture2D(
                &desc,
                nullptr,
                &texture);

        if (FAILED(hr))
            return;

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = desc.Format;
        srvDesc.ViewDimension =
            D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;

        hr =
            g_pd3dDevice->CreateShaderResourceView(
                texture,
                &srvDesc,
                &srv);

        if (FAILED(hr))
            return;
    }

}
RenderTargetView::~RenderTargetView() {
    if (srv)
    {
        srv->Release();
        srv = nullptr;
    }
    if (texture)
    {
        texture->Release();
        texture = nullptr;
    }
}










RenderPixelbuffer::RenderPixelbuffer(int width, int height) :RenderTargetView(width, height)
{

}

RenderPixelbuffer::~RenderPixelbuffer()
{

}


void RenderPixelbuffer::Update(uint32_t *buffer)

{
    
    D3D11_MAPPED_SUBRESOURCE mapped;
  


    if (buffer == nullptr || texture == nullptr) return;
    if (SUCCEEDED(g_pd3dDeviceContext->Map(texture, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        
        for (int y = 0; y < Height(); y++)
        {
            memcpy(
                (uint8_t*)mapped.pData +
                y * mapped.RowPitch,
                buffer +
                y * Width(),
                Width() *
                sizeof(uint32_t));
        }

        g_pd3dDeviceContext->Unmap(
            texture,
            0);
    }

}
void RenderPixelbuffer::Update(ng::renderer::buffers::PixelBufferRGBA pixelbuffer)
{
    Resize(pixelbuffer.Width(), pixelbuffer.Height());
    Update(pixelbuffer.GetRawBuffer());
}





RenderZBufferDouble::RenderZBufferDouble(int width, int height) :RenderTargetView(width, height) {}

void RenderZBufferDouble::Update(ng::renderer::buffers::ZBuffer<double> zbuffer)
{
    Resize(zbuffer.Width(), zbuffer.Height());

    Update(zbuffer.GetRawBuffer());
}

void RenderZBufferDouble::Update(ng::renderer::buffers::ZBuffer<float> zbuffer)
{
    Resize(zbuffer.Width(), zbuffer.Height());

    Update(zbuffer.GetRawBuffer());
}


void RenderZBufferDouble::Update(double* buffer)
{

    //  printf("Update \n ");
      // if (!buffer) printf("Buffer empty\n");

    if (!buffer) return;
    double mmax = std::numeric_limits<double>::lowest();
    double mmin = std::numeric_limits<double>::max();
    size_t size = Width() * Height();

    for (int i = 0; i < size; i++)
    {
        double z = static_cast<double>(buffer[i]);
        mmax = std::max(mmax, z);
        mmin = std::min(mmin, z);
    }
    double range = mmax - mmin;
    if (range < 1e-9) range = 1.0; // Div/0 vermeiden bei konstantem Buffer
    D3D11_MAPPED_SUBRESOURCE mapped;
    if (SUCCEEDED(g_pd3dDeviceContext->Map(texture, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {


        uint8_t* dst = reinterpret_cast<uint8_t*>(mapped.pData);
        for (int y = 0; y < Height(); y++)
        {
            uint32_t* row =
                reinterpret_cast<uint32_t*>(
                    dst + y * mapped.RowPitch);
            for (int x = 0; x < Width(); x++)
            {
                double z = static_cast<double>(buffer[y * Width() + x]);


                double t = (z - mmin) / range; // -> [0,1]
                uint8_t d = static_cast<uint8_t>(
                    std::clamp(t, 0.0, 1.0) * 255.0);
                // BGRA
                row[x] = RGB(d, d, d);


            }
        }


        g_pd3dDeviceContext->Unmap(texture, 0);

    }


}

void RenderZBufferDouble::Update(float* buffer)
{

    //  printf("Update \n ");
      // if (!buffer) printf("Buffer empty\n");

    if (!buffer) return;
    double mmax = std::numeric_limits<double>::lowest();
    double mmin = std::numeric_limits<double>::max();
    size_t size = Width() * Height();

    for (int i = 0; i < size; i++)
    {
        double z = static_cast<double>(buffer[i]);
        mmax = std::max(mmax, z);
        mmin = std::min(mmin, z);
    }
    double range = mmax - mmin;
    if (range < 1e-9) range = 1.0; // Div/0 vermeiden bei konstantem Buffer
    D3D11_MAPPED_SUBRESOURCE mapped;
    if (SUCCEEDED(g_pd3dDeviceContext->Map(texture, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {


        uint8_t* dst = reinterpret_cast<uint8_t*>(mapped.pData);
        for (int y = 0; y < Height(); y++)
        {
            uint32_t* row =
                reinterpret_cast<uint32_t*>(
                    dst + y * mapped.RowPitch);
            for (int x = 0; x < Width(); x++)
            {
                double z = static_cast<double>(buffer[y * Width() + x]);


                double t = (z - mmin) / range; // -> [0,1]
                uint8_t d = static_cast<uint8_t>(
                    std::clamp(t, 0.0, 1.0) * 255.0);
                // BGRA
                row[x] = RGB(d, d, d);


            }
        }


        g_pd3dDeviceContext->Unmap(texture, 0);

    }


}






RenderZBufferDouble::~RenderZBufferDouble() {}



////////////////////////////////////////////////////////////////////////////////////////
//                                                                                   //
//                                                                                  //
//                                                                                 //
//                                                                                //
//                                                                               //
//                                                                              //
//                                                                             //
//                                                                            //
///////////////////////////////////////////////////////////////////////////////


RenderZBufferFloat::RenderZBufferFloat(int width, int height) :RenderTargetView(width, height) {}

void RenderZBufferFloat::Update(ng::renderer::buffers::ZBuffer<float> zbuffer)
{
    Resize(zbuffer.Width(), zbuffer.Height());

    Update(zbuffer.GetRawBuffer());
}

void RenderZBufferFloat::Update(float* buffer)
{

    //  printf("Update \n ");
      // if (!buffer) printf("Buffer empty\n");

    if (!buffer) return;
    float mmax = std::numeric_limits<float>::lowest();
    float mmin = std::numeric_limits<float>::max();
    size_t size = Width() * Height();

    for (int i = 0; i < size; i++)
    {
        float z = static_cast<float>(buffer[i]);
        mmax = std::max(mmax, z);
        mmin = std::min(mmin, z);
    }
    float range = mmax - mmin;
    if (range < 1e-9) range = 1.0; // Div/0 vermeiden bei konstantem Buffer
    D3D11_MAPPED_SUBRESOURCE mapped;
    if (SUCCEEDED(g_pd3dDeviceContext->Map(texture, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {


        uint8_t* dst = reinterpret_cast<uint8_t*>(mapped.pData);
        for (int y = 0; y < Height(); y++)
        {
            uint32_t* row =
                reinterpret_cast<uint32_t*>(
                    dst + y * mapped.RowPitch);
            for (int x = 0; x < Width(); x++)
            {
                double z = static_cast<float>(buffer[y * Width() + x]);


                float t = (float)(z - mmin) / range; // -> [0,1]
                uint8_t d = static_cast<uint8_t>(
                    std::clamp(t, 0.0f, 1.0f) * 255.0f);
                // BGRA
                row[x] = RGB(d, d, d);


            }
        }


        g_pd3dDeviceContext->Unmap(texture, 0);

    }


}



RenderZBufferFloat::~RenderZBufferFloat() {}


































// Helper functions to use DirectX11
bool CreateDeviceD3D(HWND hWnd)
{
    // Setup swap chain
    // This is a basic setup. Optimally could use e.g. DXGI_SWAP_EFFECT_FLIP_DISCARD and handle fullscreen mode differently. See #8979 for suggestions.
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    //createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED) // Try high-performance WARP software driver if hardware is not available.
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}


bool InitImGui() {

#ifdef _WIN32
    ::SetProcessDPIAware();
#endif
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
    {
        printf("Error: %s\n", SDL_GetError());
        return true;
    }

    // From 2.0.18: Enable native IME.
#ifdef SDL_HINT_IME_SHOW_UI
    SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
#endif

    // Setup window
    float main_scale = ImGui_ImplSDL2_GetContentScaleForDisplay(0);
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    window = SDL_CreateWindow("Dear ImGui SDL2+DirectX11 example", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, (int)(1280 * main_scale), (int)(800 * main_scale), window_flags);
    if (window == nullptr)
    {
        printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
        return true;
    }

    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    SDL_GetWindowWMInfo(window, &wmInfo);
    HWND hwnd = (HWND)wmInfo.info.win.window;

    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        return true;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    //ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();
    SetupImGuiStyle();
    // Setup scaling
    ImGuiStyle& style = ImGui::GetStyle();


    style.ScaleAllSizes(main_scale);        // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
    style.FontScaleDpi = main_scale;        // Set initial font scale. (in docking branch: using io.ConfigDpiScaleFonts=true automatically overrides this for every window depending on the current monitor)

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForD3D(window);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // Load Fonts
    // - If fonts are not explicitly loaded, Dear ImGui will select an embedded font: either AddFontDefaultVector() or AddFontDefaultBitmap().
    //   This selection is based on (style.FontSizeBase * style.FontScaleMain * style.FontScaleDpi) reaching a small threshold.
    // - You can load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - If a file cannot be loaded, AddFont functions will return a nullptr. Please handle those errors in your code (e.g. use an assertion, display an error and quit).
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use FreeType for higher quality font rendering.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //style.FontSizeBase = 20.0f;
    //io.Fonts->AddFontDefaultVector();
    //io.Fonts->AddFontDefaultBitmap();
    //io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf");
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf");
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf");
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf");
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf");
    //IM_ASSERT(font != nullptr);
    //ImFont* font = io.Fonts->AddFontFromMemoryTTF(ShureTechMonoNerdFontMono_Regular_ttf, ShureTechMonoNerdFontMono_Regular_ttf_len);
   
    style.FontSizeBase = 15.0f;
    ImFontConfig fontConfig;
    fontConfig.FontDataOwnedByAtlas = false;

    
    
    
        
        io.Fonts->AddFontFromMemoryTTF(
            (void*)ShureTechMonoNerdFontMono_Regular_ttf,
            ShureTechMonoNerdFontMono_Regular_ttf_len,
            16.0f,
            &fontConfig);



   

    // Our state
    bool show_demo_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    return false;

}




bool ShutdownImGui() {
    // Cleanup
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return false;
}


void SwapScreen(const float clear_color_with_alpha[4])
{
    g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
    g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    g_pSwapChain->Present(1, 0); // Present with vsync
    //g_pSwapChain->Present(0, 0); // Present without vsync

}

void ResizeSwapChainBuffer() {
    g_pSwapChain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);
}