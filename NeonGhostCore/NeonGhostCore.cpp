// NeonGhostCore.cpp : Hiermit werden die Funktionen für die statische Bibliothek definiert.
//

#include "pch.h"
#include "framework.h"
#include "MathUtils.h"
#include "matrix.h"
#include "RayPlaneLine.h"
#include "AABBox.h"
#include "BoundingSphere.h"
#include "Primitive.h"
#include "Bounding.h"
#include "ResourceManager.h"
// TODO: Dies ist ein Beispiel für eine Bibliotheksfunktion.


static ng::resource::Manager m_main_res_manager = ng::resource::Manager();


#if defined(__AVX512F__)
#pragma message("--- COMPILER HINWEIS: AVX-512-Unterstuetzung ist AKTIVIERT! ---")
#define NG_USE_AVX
#elif defined(__AVX2__)
#pragma message("--- COMPILER HINWEIS: AVX2-Unterstuetzung ist AKTIVIERT! ---")
#define NG_USE_AVX
#elif defined(__AVX__)
#pragma message("--- COMPILER HINWEIS: Klassisches AVX ist AKTIVIERT! ---")
#define NG_USE_AVX
#else
#pragma message("--- COMPILER HINWEIS: Kein AVX aktiv, verwende Standard-Code. ---")
#endif
#define NG_INIT_AVX
