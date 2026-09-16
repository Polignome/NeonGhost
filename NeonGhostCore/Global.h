#pragma once

#ifndef TemplateAssert

#ifdef _WIN32
#include <assert.h>
#include <iostream>
#include <string>
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>

#define	TemplateAssert(__a) static_assert (__a,__FUNCTION__ )

#else
#include <cassert>
#include <assert.h>
#define	TemplateAssert(__a) static_assert (__a,L"test " )
#endif
#endif

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

#include <algorithm>
#include <immintrin.h>


#ifndef NG_MATH_FLOAT
#define NG_MATH_FLOAT double
#endif

#ifndef _NG_USE_RES_THREADS
#define _NG_USE_RES_THREADS
#endif // !_NG_USE_RES_THREADS





#ifndef NG_INIT_AVX
#if defined(__AVX512F__)
#define NG_USE_AVX
#elif defined(__AVX2__)
#define NG_USE_AVX
#elif defined(__AVX__)
#define NG_USE_AVX
#else
#endif
#define NG_INIT_AVX
#endif



inline  uint8_t RGBToRed(uint32_t rgb) {
	return  (uint8_t)((rgb & 0x000000FF) >> 0);
}

inline  uint8_t RGBToGreen(uint32_t rgb) {
	return(uint8_t)((rgb & 0x0000FF00) >> 8);
}

inline  uint8_t RGBToBlue(uint32_t rgb) {
	return (uint8_t)((rgb & 0x00FF0000) >> 16);
}
inline  uint8_t RGBToAlpha(uint32_t rgb) {
	return (uint8_t)((rgb & 0xFF000000) >> 24) & 0x0ff;
}

inline  uint32_t RGBA(uint8_t r, uint8_t  g, uint8_t  b, uint8_t  a) {
	return (uint32_t)(((a & 0x0ff) << 24) | ((b & 0x0ff) << 16) | ((g & 0x0ff) << 8) | ((r & 0x0ff)));
}

#ifdef RGB
#undef RGB
#endif
inline  uint32_t RGB(uint8_t r, uint8_t  g, uint8_t  b)
{
	return RGBA(r, b, g, 0xff);
}


inline uint32_t RGBMix(uint32_t c1, uint32_t c2)
{
	return RGB((((c1 & 0x000000FF) >> 0) + ((c2 & 0x000000FF) >> 0)) >> 1,
		(((c1 & 0x0000FF00) >> 8) + ((c2 & 0x0000FF00) >> 8)) >> 1,
		(((c1 & 0x00FF0000) >> 16) + ((c2 & 0x00FF0000) >> 16)) >> 1);
}

inline double LUMA_REC709(uint8_t r, uint8_t  g, uint8_t  b) {
	return (0.2126 * r + 0.7152 * g + 0.0722 * b);
}
inline  uint32_t GREY(uint8_t r, uint8_t  g, uint8_t  b) {
	return (uint32_t)(LUMA_REC709(r, g, b) + 0.5);
}

inline uint32_t RGB565ToRGBA8888(uint16_t color)
{
	uint8_t r = (color >> 11) & 0x1F;
	uint8_t g = (color >> 5) & 0x3F;
	uint8_t b = color & 0x1F;

	// 5 Bit -> 8 Bit
	r = (r << 3) | (r >> 2);

	// 6 Bit -> 8 Bit
	g = (g << 2) | (g >> 4);

	// 5 Bit -> 8 Bit
	b = (b << 3) | (b >> 2);

	return
		(static_cast<uint32_t>(r) << 24) |
		(static_cast<uint32_t>(g) << 16) |
		(static_cast<uint32_t>(b) << 8) |
		0xFF;
}



// 
//    0..255   16       1.048.576
// |---------|----|------------------------|  
// |0000 0000|0000|0000 0000 0000 0000 0000|
//             24    20   16   12   8    4


const uint8_t POLY_INDEX = 1;
const uint8_t CELL_INDEX = 2;

inline uint32_t CalcIndex(uint32_t obj_type, uint32_t obj_attrib, uint32_t obj_index) {
	return (obj_type) << 24 | (obj_attrib) << 20 | obj_index;
}

inline uint32_t GetIndexObjType(uint32_t objid) {
	return (objid & 0xff000000) >> 24;
}
inline uint32_t GetIndexObjAttrib(uint32_t objid) {
	return (objid & 0x00f00000) >> 20;
}
inline uint32_t GetIndexObjIndex(uint32_t objid) {
	return (objid & 0x000fffff);
}

