#include "pch.h"
#include "Canvas2D.h"
#include "Blend.h"
using namespace ng::renderer::gfx2d;

//////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                  //
//                                                                                                  //
//                                                                                                  //
//                                                                                                  //
//                                                                                                  //
//                                                                                                  //
//                                                                                                  //
//////////////////////////////////////////////////////////////////////////////////////////////////////

enum
{
	CS_INSIDE = 0,
	CS_LEFT = 1,
	CS_RIGHT = 2,
	CS_BOTTOM = 4,
	CS_TOP = 8
};



inline bool ClipRect(int& x, int& y, int& w, int& h, int canvasW, int canvasH)
{
	if (w <= 0 || h <= 0) return false;

	int x0 = x, y0 = y;
	int x1 = x + w; // exklusiv
	int y1 = y + h;

	if (x1 <= 0 || y1 <= 0 || x0 >= canvasW || y0 >= canvasH)
		return false; // komplett außerhalb

	x0 = std::max(x0, 0);
	y0 = std::max(y0, 0);
	x1 = std::min(x1, canvasW);
	y1 = std::min(y1, canvasH);

	x = x0; y = y0; w = x1 - x0; h = y1 - y0;
	return (w > 0 && h > 0);
}

inline int ComputeOutCode(int x, int y, int xmin, int ymin, int xmax, int ymax)
{
	int code = CS_INSIDE;

	if (x < xmin)      code |= CS_LEFT;
	else if (x > xmax) code |= CS_RIGHT;

	if (y < ymin)      code |= CS_TOP;
	else if (y > ymax) code |= CS_BOTTOM;

	return code;
}

inline uint32_t LerpColor(uint32_t c0, uint32_t c1, double t)
{
	t = std::clamp(t, 0.0, 1.0);
	auto a0 = (c0 >> 24) & 0xFF, r0 = (c0 >> 16) & 0xFF, g0 = (c0 >> 8) & 0xFF, b0 = c0 & 0xFF;
	auto a1 = (c1 >> 24) & 0xFF, r1 = (c1 >> 16) & 0xFF, g1 = (c1 >> 8) & 0xFF, b1 = c1 & 0xFF;

	uint32_t a = uint32_t(a0 + (a1 - a0) * t);
	uint32_t r = uint32_t(r0 + (r1 - r0) * t);
	uint32_t g = uint32_t(g0 + (g1 - g0) * t);
	uint32_t b = uint32_t(b0 + (b1 - b0) * t);

	return (a << 24) | (r << 16) | (g << 8) | b;
}

inline bool ClipLine(int& x0, int& y0, int& x1, int& y1, int xmin, int ymin, int xmax, int ymax)
{
	int out0 = ComputeOutCode(x0, y0, xmin, ymin, xmax, ymax);
	int out1 = ComputeOutCode(x1, y1, xmin, ymin, xmax, ymax);

	while (true)
	{
		// komplett sichtbar
		if (!(out0 | out1))
			return true;

		// komplett unsichtbar
		if (out0 & out1)
			return false;

		int out = out0 ? out0 : out1;

		double x = 0.0;
		double y = 0.0;

		if (out & CS_TOP)
		{
			x = x0 + (x1 - x0) * double(ymin - y0) / double(y1 - y0);
			y = ymin;
		}
		else if (out & CS_BOTTOM)
		{
			x = x0 + (x1 - x0) * double(ymax - y0) / double(y1 - y0);
			y = ymax;
		}
		else if (out & CS_RIGHT)
		{
			y = y0 + (y1 - y0) * double(xmax - x0) / double(x1 - x0);
			x = xmax;
		}
		else if (out & CS_LEFT)
		{
			y = y0 + (y1 - y0) * double(xmin - x0) / double(x1 - x0);
			x = xmin;
		}

		if (out == out0)
		{
			x0 = (int)x;
			y0 = (int)y;
			out0 = ComputeOutCode(x0, y0, xmin, ymin, xmax, ymax);
		}
		else
		{
			x1 = (int)x;
			y1 = (int)y;
			out1 = ComputeOutCode(x1, y1, xmin, ymin, xmax, ymax);
		}
	}
}

inline bool ClipLine(int& x0, int& y0, int& x1, int& y1, int xmin, int ymin, int xmax, int ymax, double& t0, double& t1)
{
	const int origX0 = x0, origY0 = y0;
	const int origX1 = x1, origY1 = y1;

	int out0 = ComputeOutCode(x0, y0, xmin, ymin, xmax, ymax);
	int out1 = ComputeOutCode(x1, y1, xmin, ymin, xmax, ymax);

	t0 = 0.0;
	t1 = 1.0;

	while (true)
	{
		if (!(out0 | out1)) return true;
		if (out0 & out1)    return false;

		int out = out0 ? out0 : out1;
		double x = 0.0, y = 0.0, t = 0.0;

		if (out & CS_TOP)
		{
			t = double(ymin - origY0) / double(origY1 - origY0);
			x = origX0 + (origX1 - origX0) * t;
			y = ymin;
		}
		else if (out & CS_BOTTOM)
		{
			t = double(ymax - origY0) / double(origY1 - origY0);
			x = origX0 + (origX1 - origX0) * t;
			y = ymax;
		}
		else if (out & CS_RIGHT)
		{
			t = double(xmax - origX0) / double(origX1 - origX0);
			y = origY0 + (origY1 - origY0) * t;
			x = xmax;
		}
		else if (out & CS_LEFT)
		{
			t = double(xmin - origX0) / double(origX1 - origX0);
			y = origY0 + (origY1 - origY0) * t;
			x = xmin;
		}

		if (out == out0)
		{
			x0 = (int)x; y0 = (int)y;
			t0 = t;
			out0 = ComputeOutCode(x0, y0, xmin, ymin, xmax, ymax);
		}
		else
		{
			x1 = (int)x; y1 = (int)y;
			t1 = t;
			out1 = ComputeOutCode(x1, y1, xmin, ymin, xmax, ymax);
		}
	}
}


inline void BlitSpriteFast(ng::renderer::buffers::PixelBufferRGBA& target, ng::renderer::buffers::PixelBufferRGBA& sprite, int x, int y,
	const blend::ColorKey& key = {})
{
	int sw = sprite.Width();
	int sh = sprite.Height();
	int tw = target.Width();
	int th = target.Height();

	int srcX0 = 0, srcY0 = 0;
	int dstX0 = x, dstY0 = y;
	int w = sw, h = sh;

	if (dstX0 < 0) { srcX0 -= dstX0; w += dstX0; dstX0 = 0; }
	if (dstY0 < 0) { srcY0 -= dstY0; h += dstY0; dstY0 = 0; }
	if (dstX0 + w > tw) w = tw - dstX0;
	if (dstY0 + h > th) h = th - dstY0;
	if (w <= 0 || h <= 0) return;

	const uint32_t* srcBuf = sprite.GetRawBuffer();
	uint32_t* dstBuf = target.GetRawBuffer();

	if (!key.enabled)
	{
		// Ur-schneller Pfad: memcpy pro Zeile
		for (int row = 0; row < h; ++row)
		{
			const uint32_t* srcRow = srcBuf + (srcY0 + row) * sw + srcX0;
			uint32_t* dstRow = dstBuf + (dstY0 + row) * tw + dstX0;
			std::memcpy(dstRow, srcRow, w * sizeof(uint32_t));
		}
	}
	else
	{
		// Colorkey aktiv -> pro Pixel prüfen, kein memcpy möglich
		for (int row = 0; row < h; ++row)
		{
			const uint32_t* srcRow = srcBuf + (srcY0 + row) * sw + srcX0;
			uint32_t* dstRow = dstBuf + (dstY0 + row) * tw + dstX0;
			for (int col = 0; col < w; ++col)
			{
				uint32_t c = srcRow[col];
				if (c != key.color)
					dstRow[col] = c;
			}
		}
	}
}


inline void BlitSpriteTransformed(
	ng::renderer::buffers::PixelBufferRGBA& target,
	ng::renderer::buffers::PixelBufferRGBA& sprite,
	float x, float y,
	float scaleX, float scaleY, float rotationRad,
	ng::renderer::gfx2d::blend::BlendFunc blend = blend::Opaque,
	const blend::ColorKey& key = {})
{
	int sw = sprite.Width();
	int sh = sprite.Height();
	if (sw <= 0 || sh <= 0 || scaleX <= 0.0f || scaleY <= 0.0f) return;

	int tw = target.Width();
	int th = target.Height();

	float halfW = sw * 0.5f;
	float halfH = sh * 0.5f;

	float cosA = std::cos(-rotationRad); // invers, daher -rotationRad
	float sinA = std::sin(-rotationRad);

	// AABB der rotierten/skalierten Sprite-Ecken berechnen, um nicht das ganze Target zu scannen
	float corners[4][2] = {
		{ -halfW * scaleX, -halfH * scaleY },
		{  halfW * scaleX, -halfH * scaleY },
		{  halfW * scaleX,  halfH * scaleY },
		{ -halfW * scaleX,  halfH * scaleY }
	};
	float cosF = std::cos(rotationRad);
	float sinF = std::sin(rotationRad);

	float minX = 1e30f, maxX = -1e30f, minY = 1e30f, maxY = -1e30f;
	for (auto& c : corners)
	{
		float rx = c[0] * cosF - c[1] * sinF;
		float ry = c[0] * sinF + c[1] * cosF;
		minX = std::min(minX, rx); maxX = std::max(maxX, rx);
		minY = std::min(minY, ry); maxY = std::max(maxY, ry);
	}

	int destX0 = (int)std::floor(x + minX);
	int destY0 = (int)std::floor(y + minY);
	int destX1 = (int)std::ceil(x + maxX);
	int destY1 = (int)std::ceil(y + maxY);

	destX0 = std::max(destX0, 0);
	destY0 = std::max(destY0, 0);
	destX1 = std::min(destX1, tw);
	destY1 = std::min(destY1, th);
	if (destX0 >= destX1 || destY0 >= destY1) return;

	const uint32_t* srcBuf = sprite.GetRawBuffer();
	uint32_t* dstBuf = target.GetRawBuffer();

	float invScaleX = 1.0f / scaleX;
	float invScaleY = 1.0f / scaleY;

	for (int dy = destY0; dy < destY1; ++dy)
	{
		for (int dx = destX0; dx < destX1; ++dx)
		{
			float relX = dx + 0.5f - x;
			float relY = dy + 0.5f - y;

			// Erst rotieren, dann PRO ACHSE unterschiedlich zurückskalieren
			float rotX = (relX * cosA - relY * sinA);
			float rotY = (relX * sinA + relY * cosA);

			float localX = rotX * invScaleX;
			float localY = rotY * invScaleY;

			float srcXf = localX + halfW;
			float srcYf = localY + halfH;

			int srcX = (int)srcXf;
			int srcY = (int)srcYf;

			if (srcX < 0 || srcY < 0 || srcX >= sw || srcY >= sh)
				continue;

			uint32_t srcColor = srcBuf[srcY * sw + srcX];

			if (key.enabled && srcColor == key.color)
				continue; // Colorkey -> Pixel überspringen

			uint32_t& dstPixel = dstBuf[dy * tw + dx];
			dstPixel = blend(dstPixel, srcColor);
		}
	}
}


inline void BlitSpriteTransformedIncremental(
	ng::renderer::buffers::PixelBufferRGBA& target,
	ng::renderer::buffers::PixelBufferRGBA& sprite,
	float x, float y,
	float scaleX, float scaleY, float rotationRad,
	ng::renderer::gfx2d::blend::BlendFunc blend = blend::Opaque,
	const blend::ColorKey& key = {})
{
	int sw = sprite.Width();
	int sh = sprite.Height();
	if (sw <= 0 || sh <= 0 || scaleX <= 0.0f || scaleY <= 0.0f) return;

	int tw = target.Width();
	int th = target.Height();
	float halfW = sw * 0.5f;
	float halfH = sh * 0.5f;

	float cosA = std::cos(-rotationRad);
	float sinA = std::sin(-rotationRad);
	float invScaleX = 1.0f / scaleX;
	float invScaleY = 1.0f / scaleY;

	// AABB-Berechnung unverändert (wie bisher) ...
	float cosF = std::cos(rotationRad);
	float sinF = std::sin(rotationRad);
	float corners[4][2] = {
		{ -halfW * scaleX, -halfH * scaleY }, {  halfW * scaleX, -halfH * scaleY },
		{  halfW * scaleX,  halfH * scaleY }, { -halfW * scaleX,  halfH * scaleY }
	};
	float minX = 1e30f, maxX = -1e30f, minY = 1e30f, maxY = -1e30f;
	for (auto& c : corners)
	{
		float rx = c[0] * cosF - c[1] * sinF;
		float ry = c[0] * sinF + c[1] * cosF;
		minX = std::min(minX, rx); maxX = std::max(maxX, rx);
		minY = std::min(minY, ry); maxY = std::max(maxY, ry);
	}
	int destX0 = std::max((int)std::floor(x + minX), 0);
	int destY0 = std::max((int)std::floor(y + minY), 0);
	int destX1 = std::min((int)std::ceil(x + maxX), tw);
	int destY1 = std::min((int)std::ceil(y + maxY), th);
	if (destX0 >= destX1 || destY0 >= destY1) return;

	const uint32_t* srcBuf = sprite.GetRawBuffer();
	uint32_t* dstBuf = target.GetRawBuffer();

	// Schrittvektoren: wie ändert sich (localX, localY) pro +1 in dx bzw. dy?
	float stepXdx = cosA * invScaleX;
	float stepYdx = sinA * invScaleY;
	float stepXdy = -sinA * invScaleX;
	float stepYdy = cosA * invScaleY;

	// Startwert für die erste Zeile (destX0, destY0)
	float rowRelX = destX0 + 0.5f - x;
	float rowRelY = destY0 + 0.5f - y;
	float rowLocalX = (rowRelX * cosA - rowRelY * sinA) * invScaleX;
	float rowLocalY = (rowRelX * sinA + rowRelY * cosA) * invScaleY;

	for (int dy = destY0; dy < destY1; ++dy)
	{
		float localX = rowLocalX;
		float localY = rowLocalY;

		for (int dx = destX0; dx < destX1; ++dx)
		{
			int srcX = (int)(localX + halfW);
			int srcY = (int)(localY + halfH);

			if (srcX >= 0 && srcY >= 0 && srcX < sw && srcY < sh)
			{
				uint32_t srcColor = srcBuf[srcY * sw + srcX];
				if (!(key.enabled && srcColor == key.color))
				{
					uint32_t& dstPixel = dstBuf[dy * tw + dx];
					dstPixel = blend(dstPixel, srcColor);
				}
			}

			localX += stepXdx;   // nur noch ADDITION statt Multiplikation
			localY += stepYdx;
		}

		rowLocalX += stepXdy;
		rowLocalY += stepYdy;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                  //
//                                                                                                  //
//                                                                                                  //
//                                                                                                  //
//                                                                                                  //
//                                                                                                  //
//                                                                                                  //
//////////////////////////////////////////////////////////////////////////////////////////////////////








Canvas2D::Canvas2D(buffers::PixelBufferRGBA& target): m_target(target)
{

}


Canvas2D::~Canvas2D()
{

}

void Canvas2D::DrawLine(int x0, int y0, int x1, int y1, uint32_t color)
{
	int xmax = m_target.Width() - 1;
	int ymax = m_target.Height() - 1;

	if (!ClipLine(x0, y0, x1, y1, 0, 0, xmax, ymax))
		return; // komplett außerhalb

	int dx = std::abs(x1 - x0);
	int dy = -std::abs(y1 - y0);
	int sx = (x0 < x1) ? 1 : -1;
	int sy = (y0 < y1) ? 1 : -1;
	int err = dx + dy;

	while (true)
	{
		m_target.PutPixel(x0, y0, color);
		if (x0 == x1 && y0 == y1) break;
		int e2 = 2 * err;
		if (e2 >= dy) { err += dy; x0 += sx; }
		if (e2 <= dx) { err += dx; y0 += sy; }
	}
}

void Canvas2D::DrawLine(int x0, int y0, int x1, int y1, uint32_t color0, uint32_t color1)
{
	int xmax = m_target.Width() - 1;
	int ymax = m_target.Height() - 1;

	// Originalendpunkte für die t-Berechnung nach Clipping merken
	int cx0 = x0, cy0 = y0, cx1 = x1, cy1 = y1;
	double t0 = 0.0, t1 = 1.0;

	if (!ClipLine(cx0, cy0, cx1, cy1, 0, 0, xmax, ymax, t0, t1))
		return;

	// Farben an den geclippten Endpunkten neu bestimmen
	uint32_t clippedColor0 = LerpColor(color0, color1, t0);
	uint32_t clippedColor1 = LerpColor(color0, color1, t1);

	int dx = std::abs(cx1 - cx0);
	int dy = -std::abs(cy1 - cy0);
	int sx = (cx0 < cx1) ? 1 : -1;
	int sy = (cy0 < cy1) ? 1 : -1;
	int err = dx + dy;

	// Gesamtlänge (für lokale Interpolation innerhalb des geclippten Segments)
	double totalSteps = std::max(std::abs(cx1 - cx0), std::abs(cy1 - cy0));
	if (totalSteps < 1.0) totalSteps = 1.0;
	double step = 0.0;

	int px = cx0, py = cy0;
	while (true)
	{
		double localT = step / totalSteps;
		m_target.PutPixel(px, py, LerpColor(clippedColor0, clippedColor1, localT));

		if (px == cx1 && py == cy1) break;
		int e2 = 2 * err;
		if (e2 >= dy) { err += dy; px += sx; }
		if (e2 <= dx) { err += dx; py += sy; }
		step += 1.0;
	}
}

void Canvas2D::DrawRectangle( int x, int y, int w, int h, uint32_t color)
{
	if (w <= 0 || h <= 0) return;
	int x1 = x + w - 1;
	int y1 = y + h - 1;

	DrawLine( x, y, x1, y, color); // top
	DrawLine( x1, y, x1, y1, color); // right
	DrawLine( x1, y1, x, y1, color); // bottom
	DrawLine( x, y1, x, y, color); // left
}

void Canvas2D::DrawBox(int x, int y, int w, int h, uint32_t color)
{
	if (!ClipRect(x, y, w, h, m_target.Width(), m_target.Height()))
		return;

	for (int j = 0; j < h; ++j)
		for (int i = 0; i < w; ++i)
			m_target.PutPixel(x + i, y + j, color); // safe, da bereits geclippt
}

void Canvas2D::DrawCircle(int cx, int cy, int r, uint32_t color)
{
	if (r < 0) return;

	int x = r;
	int y = 0;
	int err = 0;

	while (x >= y)
	{
		PutPixelClipped( cx + x, cy + y, color);
		PutPixelClipped( cx + y, cy + x, color);
		PutPixelClipped( cx - y, cy + x, color);
		PutPixelClipped( cx - x, cy + y, color);
		PutPixelClipped( cx - x, cy - y, color);
		PutPixelClipped( cx - y, cy - x, color);
		PutPixelClipped( cx + y, cy - x, color);
		PutPixelClipped( cx + x, cy - y, color);

		y += 1;
		err += 1 + 2 * y;
		if (2 * (err - x) + 1 > 0)
		{
			x -= 1;
			err += 1 - 2 * x;
		}
	}
}

void Canvas2D::DrawCircleFilled(int cx, int cy, int r, uint32_t color)
{
	if (r < 0) return;

	auto HLine = [&](int xL, int xR, int yy)
		{
			if (yy < 0 || yy >= m_target.Height()) return;
			if (xL > xR) std::swap(xL, xR);
			xL = std::max(xL, 0);
			xR = std::min(xR, m_target.Width() - 1);
			if (xL > xR) return;
			FillSpan(xL, yy, xR - xL + 1, color); // je nach dem, wie Canvas2D auf den Buffer zugreift
		};


	int x = r;
	int y = 0;
	int err = 0;

	while (x >= y)
	{
		HLine(cx - x, cx + x, cy + y);
		HLine(cx - y, cx + y, cy + x);
		HLine(cx - x, cx + x, cy - y);
		HLine(cx - y, cx + y, cy - x);

		y += 1;
		err += 1 + 2 * y;
		if (2 * (err - x) + 1 > 0)
		{
			x -= 1;
			err += 1 - 2 * x;
		}
	}
}

void Canvas2D::DrawEllipse(int cx, int cy, int rx, int ry, uint32_t color)
{
	if (rx < 0 || ry < 0) return;

	long long rx2 = (long long)rx * rx;
	long long ry2 = (long long)ry * ry;
	long long twoRx2 = 2 * rx2;
	long long twoRy2 = 2 * ry2;

	long long x = 0;
	long long y = ry;
	long long px = 0;
	long long py = twoRx2 * y;

	// Region 1: Steigung < 1
	double p1 = ry2 - (double)rx2 * ry + 0.25 * rx2;
	while (px < py)
	{
		PutPixelClipped( cx + (int)x, cy + (int)y, color);
		PutPixelClipped( cx - (int)x, cy + (int)y, color);
		PutPixelClipped( cx + (int)x, cy - (int)y, color);
		PutPixelClipped( cx - (int)x, cy - (int)y, color);

		x++;
		px += twoRy2;
		if (p1 < 0)
		{
			p1 += ry2 + px;
		}
		else
		{
			y--;
			py -= twoRx2;
			p1 += ry2 + px - py;
		}
	}

	// Region 2: Steigung >= 1
	double p2 = ry2 * (x + 0.5) * (x + 0.5) + rx2 * (y - 1) * (y - 1) - (double)rx2 * ry2;
	while (y >= 0)
	{
		PutPixelClipped( cx + (int)x, cy + (int)y, color);
		PutPixelClipped( cx - (int)x, cy + (int)y, color);
		PutPixelClipped( cx + (int)x, cy - (int)y, color);
		PutPixelClipped( cx - (int)x, cy - (int)y, color);

		y--;
		py -= twoRx2;
		if (p2 > 0)
		{
			p2 += rx2 - py;
		}
		else
		{
			x++;
			px += twoRy2;
			p2 += rx2 - py + px;
		}
	}
}

void Canvas2D::DrawEllipseFilled(int cx, int cy, int rx, int ry, uint32_t color)
{
	if (rx < 0 || ry < 0) return;
	
	auto HLine = [&](int xL, int xR, int yy)
		{
			if (yy < 0 || yy >= m_target.Height()) return;
			if (xL > xR) std::swap(xL, xR);
			xL = std::max(xL, 0);
			xR = std::min(xR, m_target.Width() - 1);
			if (xL > xR) return;
			FillSpan(xL, yy, xR - xL + 1, color); // je nach dem, wie Canvas2D auf den Buffer zugreift
		};

	long long rx2 = (long long)rx * rx;
	long long ry2 = (long long)ry * ry;
	long long twoRx2 = 2 * rx2;
	long long twoRy2 = 2 * ry2;

	long long x = 0;
	long long y = ry;
	long long px = 0;
	long long py = twoRx2 * y;

	double p1 = ry2 - (double)rx2 * ry + 0.25 * rx2;
	while (px < py)
	{
		HLine(cx - (int)x, cx + (int)x, cy + (int)y);
		HLine(cx - (int)x, cx + (int)x, cy - (int)y);

		x++;
		px += twoRy2;
		if (p1 < 0)
		{
			p1 += ry2 + px;
		}
		else
		{
			y--;
			py -= twoRx2;
			p1 += ry2 + px - py;
		}
	}

	double p2 = ry2 * (x + 0.5) * (x + 0.5) + rx2 * (y - 1) * (y - 1) - (double)rx2 * ry2;
	while (y >= 0)
	{
		HLine(cx - (int)x, cx + (int)x, cy + (int)y);
		HLine(cx - (int)x, cx + (int)x, cy - (int)y);

		y--;
		py -= twoRx2;
		if (p2 > 0)
		{
			p2 += rx2 - py;
		}
		else
		{
			x++;
			px += twoRy2;
			p2 += rx2 - py + px;
		}
	}
}




void Canvas2D::DrawSprite(
	buffers::PixelBufferRGBA& sprite,
	float x, float y,
	float scalex, float scaley, float rotationRad,
	blend::BlendFunc blend,
	 const blend::ColorKey &key)
{
	bool isIdentity = (scalex == 1.0f) && (scaley == 1.0f) && (rotationRad == 0.0f) && (blend == nullptr);

	if (isIdentity)
	{
		BlitSpriteFast(m_target, sprite, (int)x, (int)y, key);
	}
	else
	{
		BlitSpriteTransformed(m_target, sprite, x, y, scalex, scaley, rotationRad,
			blend ? blend : blend::Opaque, key);
	}
}

void Canvas2D::DrawSprite(gfx2d::Sprite<double> sprite) {

	//, sprite.ColorKey()
	DrawSprite(sprite.Texture(), (float)sprite.Position().x(), (float)sprite.Position().y(), (float)sprite.Scale().x(), (float)sprite.Scale().y(), (float)sprite.Rotation(), sprite.GetBlendFunc(), sprite.GetColorKey());
}

void Canvas2D::DrawSprite(gfx2d::Sprite<float> sprite) {
	DrawSprite(sprite.Texture(), sprite.Position().x(), sprite.Position().y(), sprite.Scale().x(), sprite.Scale().y(), sprite.Rotation(), sprite.GetBlendFunc(), sprite.GetColorKey());

}







