#pragma once
#include "Parser.h"
#include "Brush.h"
#include <list>
namespace ng {
	namespace map {
		template < class T = float>
		class WorldSpawn
		{
		public:

			WorldSpawn()
			{
			}

			virtual ~WorldSpawn()
			{
			}

			inline void clear() {
				m_solid_brushs.clear();

			}

			inline void AddSolidBrush(const Brush<T>& brush) {
				Brush<T> b = brush;

				m_solid_brushs.push_back(brush);

			}
			inline void AddNonSolidBrush(const Brush<T>& brush) {
				Brush<T> b = brush;

				m_non_solid_brushs.push_back(brush);

			}


			inline       std::vector<Brush<T>>& GetSolidBrushes() { return m_solid_brushs; }
			inline const std::vector<Brush<T>>& GetSolidBrushes()	const { return m_solid_brushs; }

			inline       std::vector<Brush<T>>& GetNonSolidBrushes() { return m_non_solid_brushs; }
			inline const std::vector<Brush<T>>& GetNonSolidBrushes()	const { return m_non_solid_brushs; }

			//inline void setBrushSolidBrushList(std::vector<CDG_Brush<T>> l) { m_solid_brushs = l; }
			//inline void setBrushSolidBrushList(std::vector<CDG_Brush<T>> l) { m_solid_brushs = l; }

			inline       std::string& wadfilename() { return m_wad_file; }
			inline const std::string& wadfilename() const { return m_wad_file; }
		private:

			std::vector<Brush<T>> m_solid_brushs;
			std::vector<Brush<T>> m_non_solid_brushs;
			std::string m_wad_file = "";
		};

	}
}