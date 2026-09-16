#pragma once
#include "imgui.h"

#include <vector>
#include <string>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <Matrix.h>
#include <filesystem>
#include "Frustum.h"
#include "CameraFrustum.h"
namespace ng {
    namespace gui {
        bool ExitPopup(bool& view_exit_dialog);
        void ShowFontViewer(bool& open);
        std::string FilePicker(bool& show, std::string wildcard);

        class FPSCounter
        {
        public:
            FPSCounter();

            void Reset();
            void Update();
            double FPS() const;
            double DeltaTime() const;

        private:
            using Clock = std::chrono::high_resolution_clock;

            Clock::time_point m_lastTime;
            Clock::time_point m_lastFPSUpdate;

            int m_frames = 0;

            double m_fps = 0.0;
            double m_deltaTime = 0.0;
        };
        inline ImVec2 ProjectIso(const ng::math::Vector3d& p)
        {

            return ImVec2((float)(p.x() - p.z() * 0.5), (float)(-p.y() + p.z() * 0.5));
        }

        inline ImVec2 ProjectIso(const ng::math::Vector3f& p)
        {

            return ImVec2((float)(p.x() - p.z() * 0.5), (float)(-p.y() + p.z() * 0.5));
        }

        template<typename T>
        static ImVec2 ProjectIso(
            const math::Matrix<3, 1, T>& p,
            const ImVec2& center,
            float scale)
        {
            float x = (float)(p.x() - p.z() * 0.5);
            float y = (float)(-p.y() + p.z() * 0.5);

            return ImVec2(
                center.x + x * scale,
                center.y + y * scale);
        }

        void ViewMatrix4(const std::string& name, const ng::math::Matrix4f& v);
        void ViewVector3(const std::string& name, const ng::math::Vector3f& v);
        void ViewVector3(const std::string& name, const ng::math::Vector3d& v);
        void ViewMatrix4(const std::string& name, const ng::math::Matrix4d& v);
        void DrawDirectionIndicator3D(float size, ng::math::Vector3d _direction);
        void DrawDirectionIndicator3D(float size, ng::math::Vector3f direction);
        

template<typename T>
void DrawCameraFrustumWidget(
    float size,
    const ng::scene::CameraFrustum<T>& frustum)
{
    ImVec2 pos = ImGui::GetCursorScreenPos();

    ImGui::Dummy(ImVec2(size, size));

    ImDrawList* draw = ImGui::GetWindowDrawList();

    ImVec2 center2d(
        pos.x + size * 0.5f,
        pos.y + size * 0.5f);


    //--------------------------------------------------
    // Eckpunkte
    //--------------------------------------------------

    std::vector<math::Matrix<3, 1, T>> corners =
    {
        frustum.NearTopLeft(),
        frustum.NearTopRight(),
        frustum.NearBottomLeft(),
        frustum.NearBottomRight(),

        frustum.FarTopLeft(),
        frustum.FarTopRight(),
        frustum.FarBottomLeft(),
        frustum.FarBottomRight()
    };


    //--------------------------------------------------
    // Bounding Box bestimmen
    //
    // Aber relativ zur Kamera!
    //--------------------------------------------------

    T maxExtent = (T)0;

    for (const auto& p : corners)
    {
        T x = std::abs(p.x());
        T y = std::abs(p.y());
        T z = std::abs(p.z());

        maxExtent = std::max(maxExtent, x);
        maxExtent = std::max(maxExtent, y);
        maxExtent = std::max(maxExtent, z);
    }


    if (maxExtent < (T)0.00001)
        maxExtent = (T)1;


    //--------------------------------------------------
    // Scale
    //--------------------------------------------------

    const float margin = size * 0.15f;

    float scale =
        (size - margin * 2.0f) /
        (float)(maxExtent * 2);


    //--------------------------------------------------
    // Projection
    //--------------------------------------------------

    auto Project =
        [&](const math::Matrix<3, 1, T>& p)
        {
            return ng::gui::ProjectIso(
                p,
                center2d,
                scale);
        };


    //--------------------------------------------------
    // Draw Edge
    //--------------------------------------------------

    auto DrawEdge =
        [&](const auto& a,
            const auto& b,
            ImU32 color,
            float thickness = 1.5f)
        {
            draw->AddLine(
                Project(a),
                Project(b),
                color,
                thickness);
        };


    //--------------------------------------------------
    // Farben
    //--------------------------------------------------

    ImU32 nearColor =
        IM_COL32(0, 255, 0, 255);

    ImU32 farColor =
        IM_COL32(255, 100, 0, 255);

    ImU32 sideColor =
        IM_COL32(255, 255, 255, 180);


    //--------------------------------------------------
    // Near
    //--------------------------------------------------

    DrawEdge(
        frustum.NearTopLeft(),
        frustum.NearTopRight(),
        nearColor);

    DrawEdge(
        frustum.NearTopRight(),
        frustum.NearBottomRight(),
        nearColor);

    DrawEdge(
        frustum.NearBottomRight(),
        frustum.NearBottomLeft(),
        nearColor);

    DrawEdge(
        frustum.NearBottomLeft(),
        frustum.NearTopLeft(),
        nearColor);


    //--------------------------------------------------
    // Far
    //--------------------------------------------------

    DrawEdge(
        frustum.FarTopLeft(),
        frustum.FarTopRight(),
        farColor);

    DrawEdge(
        frustum.FarTopRight(),
        frustum.FarBottomRight(),
        farColor);

    DrawEdge(
        frustum.FarBottomRight(),
        frustum.FarBottomLeft(),
        farColor);

    DrawEdge(
        frustum.FarBottomLeft(),
        frustum.FarTopLeft(),
        farColor);


    //--------------------------------------------------
    // Seiten
    //--------------------------------------------------

    DrawEdge(
        frustum.NearTopLeft(),
        frustum.FarTopLeft(),
        sideColor);

    DrawEdge(
        frustum.NearTopRight(),
        frustum.FarTopRight(),
        sideColor);

    DrawEdge(
        frustum.NearBottomLeft(),
        frustum.FarBottomLeft(),
        sideColor);

    DrawEdge(
        frustum.NearBottomRight(),
        frustum.FarBottomRight(),
        sideColor);


    //--------------------------------------------------
    // Kamera
    //--------------------------------------------------

    draw->AddCircleFilled(
        center2d,
        3.0f,
        IM_COL32(255, 255, 255, 255));
}


// ============================================================
// Generischer Wireframe-Renderer für ng::renderer::occlusion::Frustum<T>
//
// Im Gegensatz zu CameraFrustum<T> kennen wir hier weder die Anzahl
// noch die Bedeutung der Ebenen (kein Near/Far/Top/Bottom).
// Wir rekonstruieren die Flächen daher generisch:
//
//   1. Für jede Plane: alle Vertices einsammeln, die (näherungsweise)
//      auf dieser Plane liegen -> das sind die Eckpunkte der Fläche.
//   2. Diese Punkte um ihren Schwerpunkt herum winkelsortieren
//      (2D-Basis in der Ebene aufspannen), damit ein konvexes
//      Polygon entsteht.
//   3. Polygon als geschlossene Kette von Linien zeichnen.
//
// Voraussetzung: frustum.vertices() ist bereits befüllt, d.h.
// computeVerticesFromPlanes() wurde aufgerufen (passiert automatisch
// im Konstruktor Frustum(const std::vector<RayPlaneLine<3,T>>&)).
// ============================================================
template<typename T>
void DrawFrustumWidget(
    float size,
    const ng::renderer::occlusion::Frustum<T>& frustum,
    ImU32 wireColor = IM_COL32(255, 255, 255, 200),
    ImU32 vertexColor = IM_COL32(255, 255, 0, 255))
{
    const auto& verts = frustum.vertices();
    const auto& planes = frustum.planes();

    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(size, size));
    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImVec2 center2d(
        pos.x + size * 0.5f,
        pos.y + size * 0.5f);

    if (verts.size() < 4 || planes.empty())
    {
        // Nichts Sinnvolles darstellbar
        draw->AddCircleFilled(
            center2d,
            3.0f,
            IM_COL32(120, 120, 120, 255));
        return;
    }

    using Vec3 = math::Matrix<3, 1, T>;

    //--------------------------------------------------
    // Bounding Extent (wie beim CameraFrustum) - relativ
    // zum Ursprung/Kamera, kein Re-Zentrieren.
    //--------------------------------------------------
    T maxExtent = (T)0;
    for (const auto& p : verts)
    {
        maxExtent = std::max(maxExtent, std::abs(p.x()));
        maxExtent = std::max(maxExtent, std::abs(p.y()));
        maxExtent = std::max(maxExtent, std::abs(p.z()));
    }
    if (maxExtent < (T)0.00001)
        maxExtent = (T)1;

    const float margin = size * 0.15f;
    float scale =
        (size - margin * 2.0f) /
        (float)(maxExtent * 2);

    auto Project =
        [&](const Vec3& p)
        {
            return ng::gui::ProjectIso(
                p,
                center2d,
                scale);
        };

    const T eps = math::EPSILON<T> *(T)10; // etwas toleranter als bei exakten Tests

    //--------------------------------------------------
    // Für jede Plane: zugehörige Vertices sammeln
    //--------------------------------------------------
    for (const auto& plane : planes)
    {
        std::vector<Vec3> face;
        face.reserve(verts.size());

        for (const auto& v : verts)
        {
            T d =
                plane.normal().x() * v.x() +
                plane.normal().y() * v.y() +
                plane.normal().z() * v.z() +
                plane.D();

            if (std::abs(d) <= eps)
                face.push_back(v);
        }

        if (face.size() < 3)
            continue; // degenerierte/keine Fläche (z.B. Ebene ohne Beitrag zum Polyeder)

        //----------------------------------------------
        // Schwerpunkt der Fläche
        //----------------------------------------------
        Vec3 centroid((T)0, (T)0, (T)0);
        for (const auto& p : face)
            centroid = centroid + p;
        centroid = centroid * ((T)1 / (T)face.size());

        //----------------------------------------------
        // 2D-Basis in der Ebene aufspannen
        //----------------------------------------------
        Vec3 n = plane.normal();
        Vec3 arbitrary =
            (std::abs(n.x()) < (T)0.9)
            ? Vec3((T)1, (T)0, (T)0)
            : Vec3((T)0, (T)1, (T)0);

        Vec3 tangent1 = n % arbitrary;
        T tlen = tangent1.length();
        if (tlen <= math::EPSILON<T>)
            continue; // sollte praktisch nie passieren
        tangent1 /= tlen;
        Vec3 tangent2 = n % tangent1;

        //----------------------------------------------
        // Punkte nach Winkel um den Schwerpunkt sortieren
        //----------------------------------------------


        std::sort(
            face.begin(),
            face.end(),
            [&](const Vec3& a, const Vec3& b)
            {
                Vec3 ra = a - centroid;
                Vec3 rb = b - centroid;

                // Nutzen Sie den Typ T direkt, statt auf float zu casten (vermeidet Präzisionsverlust)
                T au = ra.x() * tangent1.x() + ra.y() * tangent1.y() + ra.z() * tangent1.z();
                T av = ra.x() * tangent2.x() + ra.y() * tangent2.y() + ra.z() * tangent2.z();
                T bu = rb.x() * tangent1.x() + rb.y() * tangent1.y() + rb.z() * tangent1.z();
                T bv = rb.x() * tangent2.x() + rb.y() * tangent2.y() + rb.z() * tangent2.z();

                double angleA = std::atan2((double)av, (double)au);
                double angleB = std::atan2((double)bv, (double)bu);

                // Epsilon-Vergleich für numerische Stabilität
                if (std::abs(angleA - angleB) > 1e-9) {
                    return angleA < angleB; // Korrigierter Operator
                }

                // Fallback bei gleichem Winkel: Sortierung nach Distanz zum Centroid
                // Dies garantiert Strict Weak Ordering und verhindert Abstürze!
                return (ra.x() * ra.x() + ra.y() * ra.y() + ra.z() * ra.z()) <
                    (rb.x() * rb.x() + rb.y() * rb.y() + rb.z() * rb.z());
            });
        //----------------------------------------------
        // Polygon zeichnen (geschlossene Kette)
        //----------------------------------------------
        for (size_t i = 0; i < face.size(); ++i)
        {
            const Vec3& a = face[i];
            const Vec3& b = face[(i + 1) % face.size()];
            draw->AddLine(
                Project(a),
                Project(b),
                wireColor,
                1.5f);
        }
    }

    //--------------------------------------------------
    // Vertices als Punkte markieren (optional, hilfreich zum Debuggen)
    //--------------------------------------------------
    for (const auto& v : verts)
    {
        draw->AddCircleFilled(
            Project(v),
            2.5f,
            vertexColor);
    }

    //--------------------------------------------------
    // Ursprung / Referenzpunkt
    //--------------------------------------------------
    draw->AddCircleFilled(
        center2d,
        3.0f,
        IM_COL32(255, 255, 255, 255));
}



    }



   

    namespace tools {
        std::string abs_exe_directory();
        std::filesystem::path abs_exe_path();

    }

    
}
