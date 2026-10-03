#pragma once
#include <map>
#include "Parser.h"
#include "WorldSpawn.h"
#include "Wad3File.h"
#include "Lightmap.h"
#include "HalfLife.h"
#define DDG_MAP_ID "DarkGhostMap"


namespace ng {
	namespace map {

		template<class T>
		struct LightEntity
		{
			math::Matrix<3, 1, T> origin;
			math::Matrix<3, 1, T> color = math::Matrix<3, 1, T>(T(255), T(255), T(255));
			T brightness = T(200);   // Half-Life-Standardwert, falls nicht angegeben
			T radius = T(0);         // wird beim Baken aus brightness abgeleitet, s. unten
		};

		template<class T>
		std::vector<ng::lightmap::LightSource<T>> ExtractLights(const std::vector<HalfLife::RawLight>& raw)
		{
			using Vec3 = ng::math::Matrix<3, 1, T>;
			std::vector<ng::lightmap::LightSource<T>> result;
			result.reserve(raw.size());

			for (const auto& rl : raw)
			{
				if (rl.origin.empty()) continue; // ohne Position unbrauchbar

				ng::lightmap::LightSource<T> src;

				// "x y z" -> Position
				{
					std::istringstream iss(rl.origin);
					double x = 0, y = 0, z = 0;
					iss >> x >> y >> z;
					src.pos = Vec3(T(x), T(y), T(z));
				}

				// "_light": "r g b brightness" | "r g b" | "grau" | leer
				double r = 255, g = 255, b = 255, brightness = 200;

				if (!rl.light.empty())
				{
					std::istringstream iss(rl.light);
					std::vector<double> vals;
					double v;
					while (iss >> v) vals.push_back(v);

					if (vals.size() >= 4) { r = vals[0]; g = vals[1]; b = vals[2]; brightness = vals[3]; }
					else if (vals.size() == 3) { r = vals[0]; g = vals[1]; b = vals[2]; }
					else if (vals.size() == 1) { r = g = b = vals[0]; }
				}

				src.color = Vec3(T(r), T(g), T(b));

				// Half-Life-Brightness ist kein direkter Radius/Intensitaets-Wert
				// in Weltunits - das Original nutzt eine quadratische Faltung
				// (Attenuation ~ brightness / dist^2). Fuer euren Falloff
				// (LightFalloff mit "radius") ist eine grobe Naeherung:
				// radius waechst mit sqrt(brightness), intensity bleibt 1
				// (die Farbe traegt bereits die Helligkeit). Kalibriert das
				// an eurer Szene - das ist nur ein plausibler Startwert.
				src.radius = T(std::sqrt(std::max(1.0, brightness)) * 8.0);
				src.intensity = T(1);

				result.push_back(src);
			}

			return result;
		}
		template<class T>
		std::vector<ng::lightmap::LightSource<T>> ToLightSources(const std::vector<ng::map::LightEntity<T>>& mapLights)
		{
			std::vector<ng::lightmap::LightSource<T>> result;
			result.reserve(mapLights.size());

			for (const auto& l : mapLights)
			{
				

				ng::lightmap::LightSource<T> src;
				src.pos = l.origin;
				src.color = l.color;
				src.intensity = T(1); // Helligkeit steckt bereits in radius/color
				src.radius = l.radius;
				result.push_back(src);
			}

			return result;
		}
		
		template < class T = float>
		class Data
		{
		public:

			Data() = default;

			~Data() = default;

			inline const std::vector<LightEntity<T>>& GetLights() const { return m_lights; }

			// ------------------------------------------------------------
			// Copy
			// ------------------------------------------------------------

			Data(const Data&) = default;
			Data& operator=(const Data&) = default;


			// ------------------------------------------------------------
			// Move
			// ------------------------------------------------------------

			Data(Data&&) noexcept = default;
			Data& operator=(Data&&) noexcept = default;

			inline		bool		operator =(Parser value) {
				m_text_id.clear();
				m_id_text.clear();
				m_worldspawn.clear();
				//std::cout << value.getEnitys().name << std::endl;
				TEntity entity = value.getEnitys();
				if (entity.name != DDG_MAP_ID) return false;
				StepEntitys(entity);
				return true;

			}

			inline const map::WorldSpawn<T>& GetWorldSpwan() const { return m_worldspawn; }
			inline       map::WorldSpawn<T>& GetWorldSpwan() { return m_worldspawn; }

						
			inline std::vector<Brush<T>> ExtractBrushes(bool solid = true) {
				std::vector<Brush< T>> data;

				for (auto sb : GetWorldSpwan().GetSolidBrushes()) data.push_back(sb);
				return data;
			}

			std::map<std::string, int32_t> m_text_id;
			std::map<int32_t, std::string> m_id_text;
			std::vector <std::string> m_texture_names;

			ng::map::texture::Wad3File GetTextureWad() { return m_wad; }


		private:
			WorldSpawn<T> m_worldspawn;
			ng::map::texture::Wad3File m_wad;
			std::map<std::string, int32_t> m_texturename_index;
			std::vector<LightEntity<T>> m_lights;

			inline void StepEntitys(TEntity entity) {
				if (entity.name == "world") {
					for (auto p : entity.parameter)
					{
						if (p.name == "wad") {
							m_wad = texture::Wad3File(p.values[0]);
						}
					}
				}

				if (entity.name == "solid") AddBrush(entity);

				// NEU: Licht-Entities einsammeln
				if (entity.name == "light" ||
					entity.name == "light_spot" ||
					entity.name == "light_environment")
				{
					AddLight(entity);
				}

				for (auto e : entity.sub_entry) StepEntitys(e);
			}


		
			// ERSETZT die alte, kaputte AddLight()
			inline void AddLight(const TEntity& entity)
			{
				LightEntity<T> light;
				bool haveOrigin = false;

				for (const auto& p : entity.parameter)
				{
					if (p.name == "origin")
					{
						// "x y z" - drei Werte, exakt wie GetEntity<T>(..., Matrix<3,1,T>&) erwartet
						if (GetEntity<T>(p, light.origin) == 0)
							haveOrigin = true;
					}
					else if (p.name == "_light" || p.name == "light")
					{
						// Half-Life-Format: "r g b brightness" | "r g b" | "grau"
						// Variable Feldanzahl - die generischen GetEntity<T>-Overloads
						// (fest 3 oder 4 Werte) passen hier nicht, daher direkt aus
						// p.values lesen.
						const auto& v = p.values;

						if (v.size() >= 4)
						{
							light.color = math::Matrix<3, 1, T>(
								(T)std::stod(v[0]), (T)std::stod(v[1]), (T)std::stod(v[2]));
							light.brightness = (T)std::stod(v[3]);
						}
						else if (v.size() == 3)
						{
							light.color = math::Matrix<3, 1, T>(
								(T)std::stod(v[0]), (T)std::stod(v[1]), (T)std::stod(v[2]));
						}
						else if (v.size() == 1)
						{
							const T grey = (T)std::stod(v[0]);
							light.color = math::Matrix<3, 1, T>(grey, grey, grey);
						}
					}
				}

				if (!haveOrigin)
				{
					printf("[Map] Licht-Entity ohne 'origin' uebersprungen\n");
					return;
				}

				// Grobe Naeherung: Half-Life-Brightness ist keine direkte
				// Weltunit-Reichweite, sondern ein Attenuation-Faktor
				// (Intensitaet ~ brightness / dist^2). Fuer unseren
				// radius-basierten Falloff (siehe LightFalloff) reicht als
				// Startwert eine Wurzel-Skalierung - spaeter pro Szene
				// kalibrieren.
				light.radius =(T)std::sqrt(std::max((T)1, light.brightness)) * (T)8;

				m_lights.push_back(light);

				printf("[Map] Licht: pos=(%.1f, %.1f, %.1f) color=(%.0f,%.0f,%.0f) brightness=%.1f radius=%.1f\n",
					(double)light.origin.x(), (double)light.origin.y(), (double)light.origin.z(),
					(double)light.color.x(), (double)light.color.y(), (double)light.color.z(),
					(double)light.brightness, (double)light.radius);
			}
			

			inline void AddBrush(TEntity entity)
			{
				
				if (entity.name != "solid") return;
				Brush<T> brush;


				for (unsigned int i = 0; i < entity.sub_entry.size(); i++)
				{
					if (entity.sub_entry[i].name == "side") AddBrushFace(entity.sub_entry[i], brush);
				}
				unsigned long aggregate = brush.CheckAggregate();
				if (aggregate == BrushFace<T>::WRONG)
				{
					
					brush.SetAllFacesSolid();
					aggregate = BrushFace<T>::SOLID;
				}
				if (aggregate == BrushFace<T>::SOLID) m_worldspawn.AddSolidBrush(brush);
				else m_worldspawn.AddSolidBrush(brush);
			}

			inline void AddBrushFace(TEntity entity, Brush<T>& brush)
			{
				if (entity.name != "side") return;
				BrushFace<T> ray;
				ray.aggregate() = BrushFace<T>::SOLID;

				for (unsigned int i = 0; i < entity.parameter.size(); i++)
				{
					TEntityParam p = entity.parameter[i];

					if (p.name == "uaxis") {
						GetEntity<T>(p, ray.uAxis(), ray.uScale());
					}
					if (p.name == "vaxis") {
						GetEntity<T>(p, ray.vAxis(), ray.vScale());

					}
					if (p.name == "rotate") {
						
						GetEntity(p, ray.rotate());

					}

					if (p.name == "uscale") {
						GetEntity(p,  ray.uScale());

					}

					if (p.name == "vscale") {
						GetEntity(p, ray.vScale());
					}

					if (p.name == "material") {
						auto it = m_text_id.find(p.values[0]);
						if (it != m_text_id.end()) {
							ray.texture() = it->second;
						}
						else {

							int32_t index = (int32_t)m_texture_names.size();
							m_texture_names.push_back(p.values[0]);
							m_text_id.insert(std::pair<std::string, int32_t>(p.values[0], index));
							m_id_text.insert(std::pair<int32_t, std::string>(index, p.values[0]));
							ray.texture() = index;
						}
					}

					if (p.name == "plane")
					{
						GetEntity<T>(p, ray);
					}

					if (p.name == "aggregate")
					{
						std::string name;
						GetEntity(p, name);
						if (name == "liquid1") {
							ray.aggregate() = BrushFace<T>::TRANS | BrushFace<T>::WATER | BrushFace<T>::LIQUID;
						}
						if (name == "liquid2") {
							ray.aggregate() = BrushFace<T>::TRANS | BrushFace<T>::LAVA | BrushFace<T>::LIQUID;
						}
						if (name == "liquid3") {
							ray.aggregate() = BrushFace<T>::TRANS | BrushFace<T>::ACCID | BrushFace<T>::LIQUID;
						}
						if (name == "glass") {
							ray.aggregate() = BrushFace<T>::GLASS | BrushFace<T>::TRANS;
						}
						if (name == "glas") {
							ray.aggregate() = BrushFace<T>::GAS | BrushFace<T>::TRANS;
						}
						if (name == "solid_no_splitter") {
							ray.aggregate() = BrushFace<T>::SOLID;
							ray.no_splitter() = true;
						}

					}
				}

				brush.AddFacePlane(ray);
			}

		};

	}
}
