#pragma once
#include <d3d11.h>
#include "imgui.h"
#include <memory>
#include <PixelBuffer.h>
#include <ZBuffer.h>

class RenderTargetView
{
public:
	RenderTargetView(int width = -1, int height = -1);
	~RenderTargetView();
	void Resize(int width, int height);
	int Width() { return m_width; }
	int Height() { return m_height; }
	inline operator ImTextureID() { return (ImTextureID)srv; }
	inline ImTextureID GetImTextureID() { return (ImTextureID)srv; }

private:
	int m_width;
	int m_height;
protected:
	ID3D11Texture2D* texture = nullptr;
	ID3D11ShaderResourceView* srv = nullptr;

};


class RenderPixelbuffer :public RenderTargetView
{
public:
	RenderPixelbuffer(int width = -1, int height = -1);
	~RenderPixelbuffer();
	void Update(ng::renderer::buffers::PixelBufferRGBA pixelbuffer);
	void Update(uint32_t* buffer);
};

class RenderZBufferDouble :public RenderTargetView
{
public:
	RenderZBufferDouble(int width = -1, int height = -1);
	~RenderZBufferDouble();
	void Update(ng::renderer::buffers::ZBuffer<double> zbuffer);
	void Update(double* buffer);
	void Update(ng::renderer::buffers::ZBuffer<float> zbuffer);
	void Update(float* buffer);
};


class RenderZBufferFloat :public RenderTargetView
{
public:
	RenderZBufferFloat(int width = -1, int height = -1);
	~RenderZBufferFloat();
	void Update(ng::renderer::buffers::ZBuffer<float> zbuffer);
	void Update(float* buffer);
};