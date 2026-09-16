#pragma once
#include <chrono>
namespace ng {
    namespace tools {
        class FPSCounter
        {
        public:
            FPSCounter()
            {
                Reset();
            }

            void Reset()
            {
                m_lastTime = Clock::now();
                m_lastFPSUpdate = m_lastTime;
                m_frames = 0;
                m_fps = 0.0;
                m_deltaTime = 0.0;
            }

            void Update()
            {
                auto now = Clock::now();

                // Zeit seit letztem Frame
                m_deltaTime = std::chrono::duration<double>(now - m_lastTime).count();
                m_lastTime = now;

                m_frames++;

                // FPS einmal pro Sekunde aktualisieren
                double elapsed = std::chrono::duration<double>(now - m_lastFPSUpdate).count();

                if (elapsed >= 1.0)
                {
                    m_fps = m_frames / elapsed;

                    m_frames = 0;
                    m_lastFPSUpdate = now;
                }
            }

            double FPS() const
            {
                return m_fps;
            }

            double DeltaTime() const
            {
                return m_deltaTime;
            }

        private:
            using Clock = std::chrono::high_resolution_clock;

            Clock::time_point m_lastTime;
            Clock::time_point m_lastFPSUpdate;

            int m_frames = 0;

            double m_fps = 0.0;
            double m_deltaTime = 0.0;
        };
    }
}