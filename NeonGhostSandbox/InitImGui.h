#pragma once
// Helper functions to use DirectX11
#include "imgui_impl_sdl2.h"
#include "imgui_impl_dx11.h"
#include <d3d11.h>
#include <stdio.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>



bool InitImGui();
bool ShutdownImGui();
void CreateRenderTarget();
void CleanupRenderTarget();

size_t GetProcessMemoryUsage();
void SwapScreen(const float clear_color_with_alpha[4]);
void ResizeSwapChainBuffer();
SDL_Window* GetWindow();