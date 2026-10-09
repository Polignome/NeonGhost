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

						
			inline std::vector<Brush<T>> ExtractLiquidBrushes() {
				std::vector<Brush< T>> data;
				for (auto sb : GetWorldSpwan().GetNonSolidBrushes()) data.push_back(sb);
				return data;
			}

			inline std::vector<Brush<T>> ExtractPortalBrushes() {
					std::vector<Brush< T>> data;
					for (auto sb : GetWorldSpwan().GetPortaldBrushes()) data.push_back(sb);
					return data;

			}

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
			
			

			inline void StepLiquidEntitys(
				TEntity entity,
				uint32_t attrib = BrushFace<T>::LIQUID)
			{
				for (auto p : entity.parameter)
				{
					if (p.values.empty())
						continue;

					if (p.name == "skin")
					{
						switch (std::stoi(p.values[0]))
						{
						case -3:
							attrib |= BrushFace<T>::WATER;
							printf("[Map] Liquid-Entity mit Water-Flag\n");
							break;

						case -4:
							attrib |= BrushFace<T>::ACCID;
							printf("[Map] Liquid-Entity mit Acid-Flag\n");
							break;

						case -5:
							attrib |= BrushFace<T>::LAVA;
							printf("[Map] Liquid-Entity mit Lava-Flag\n");
							break;

						default:
							attrib |= BrushFace<T>::WATER;
							printf("[Map] Liquid-Entity mit Water-Flag\n");
							break;
						}
					}

					if (p.name == "rendermode")
					{
						switch (std::stoi(p.values[0]))
						{
						case 5:
							attrib |= BrushFace<T>::TRANS;
							printf("[Map] Liquid-Entity mit Transluzenz-Flag\n");
							break;
						}
					}
				}

				if (entity.name == "brush")
				{
					AddLiquidBrush(entity, attrib);
					return;
				}

				// WICHTIG:
				// Die bereits ermittelten Flags weiterreichen!
				for (auto e : entity.sub_entry)
					StepLiquidEntitys(e, attrib);
			}


			inline void StepPortalEntitys(
				TEntity entity,
				PortalBrushSettings settings= PortalBrushSettings())
			{
				
				for (auto p : entity.parameter)
				{

					if (p.name == "brush_mode")
					{
						uint32_t v = std::stoi(p.values[0]);
						bool on = v > 0;

						settings.visible = on;
					}

					if (p.name == "face_render")
					{
						uint32_t v = std::stoi(p.values[0]);
						bool on = v > 0;

					   settings.translucent = on;
					}
					if (p.name == "linked_to_id")
					{
						uint32_t v = std::stoi(p.values[0]);
						bool on = v > 0;
						settings.linked_id = v;
					}
					if (p.name == "portal_id")
					{
						uint32_t v = std::stoi(p.values[0]);
						bool on = v > 0;
						settings.id = v;
					}
					if (p.name == "spawnflags")
					{
						uint32_t v = std::stoi(p.values[0]);
						bool on = v > 0;

						settings.teleport = on;
					}
				}
				bool valid = true;
				if (settings.linked_id == settings.id) {
					valid = false;
					printf("[Map] Portal-Brush ist nicht gültig\n");
				}

				if (entity.name == "brush" && valid)
				{
					AddPortalBrush(entity,settings);
					return;
				}

				// WICHTIG:
				// Die bereits ermittelten Flags weiterreichen!
				for (auto e : entity.sub_entry)
					StepPortalEntitys(e, settings);
			}




			inline void StepWorldEntitys(TEntity entity) {
				for (auto p : entity.parameter)
				{
					if (p.name == "wad" && !p.values.empty()) {
						try {
							m_wad = texture::Wad3File(p.values[0]);
						}
						catch (const std::runtime_error& error) {
							printf("[Map] Fehler beim Laden der WAD-Datei: %s\n", error.what());
						}
					}
				}

				if (entity.name == "brush") AddSolidBrush(entity);

				for (auto e : entity.sub_entry) StepWorldEntitys(e);


			}


			inline void StepEntitys(TEntity entity) {

				if (entity.name == "world") {
					StepWorldEntitys(entity); 
					return;
				}
				if (entity.name == "liquid") {
					StepLiquidEntitys(entity);
					return;
				}
				if (entity.name == "portal") {
					StepPortalEntitys(entity);
					return;
				}


				// NEU: Licht-Entities einsammeln
				if (entity.name == "light" ||
					entity.name == "light_spot" ||
					entity.name == "light_environment")
				{
					AddLight(entity);
					return;
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
			


			
			inline void PrintBrushAttributes(uint32_t attrib)
			{
				printf("Brush Attributes: 0x%08X", attrib);

				if (attrib == 0)
				{
					printf(" -> NONE\n");
					return;
				}

				printf(" -> ");

				bool first = true;

				auto printFlag = [&](const char* name, uint32_t flag)
					{
						if (attrib & flag)
						{
							if (!first)
								printf(" | ");

							printf("%s", name);
							first = false;
						}
					};

				printFlag("SOLID", BrushFace<T>::SOLID);
				printFlag("LIQUID", BrushFace<T>::LIQUID);
				printFlag("WATER", BrushFace<T>::WATER);
				printFlag("LAVA", BrushFace<T>::LAVA);
				printFlag("ACCID", BrushFace<T>::ACCID);
				printFlag("GLASS", BrushFace<T>::GLASS);
				printFlag("GAS", BrushFace<T>::GAS);
				printFlag("TRANS", BrushFace<T>::TRANS);
				printFlag("SOLID_NO_SPLITTER", BrushFace<T>::SOLID_NO_SPLITTER);
				printFlag("EMPTY", BrushFace<T>::EMPTY);
				printFlag("WRONG", BrushFace<T>::WRONG);
				
				printf("\n");
			}


			inline void AddPortalBrush(TEntity entity, PortalBrushSettings settings)
			{
				if (entity.name != "brush") return;
				Brush<T> brush;

				brush.PortalSetting() = settings;

				for (unsigned int i = 0; i < entity.sub_entry.size(); i++)
				{
					if (entity.sub_entry[i].name == "side") AddBrushFace(entity.sub_entry[i], brush);
				}

				m_worldspawn.AddPortalBrush(brush);


			


			}



			inline void AddLiquidBrush(TEntity entity, const uint32_t attrib)
			{
				if (entity.name != "brush") return;
				Brush<T> brush;


				for (unsigned int i = 0; i < entity.sub_entry.size(); i++)
				{
					if (entity.sub_entry[i].name == "side") AddBrushFace(entity.sub_entry[i], brush);
				}

				brush.SetAllFaces(attrib);
				m_worldspawn.AddNonSolidBrush(brush);

				//				printf("AddLiquidBrush ATTRIB Soll:");
//				PrintBrushAttributes(attrib);
//				printf("AddLiquidBrush ATTRIB Ist:");

//				PrintBrushAttributes(brush.CheckAggregate());


			}



			inline void AddSolidBrush(TEntity entity)
			{

				if (entity.name != "brush") return;
				Brush<T> brush;


				for (unsigned int i = 0; i < entity.sub_entry.size(); i++)
				{
					if (entity.sub_entry[i].name == "side") AddBrushFace(entity.sub_entry[i], brush);
				}
				brush.SetAllFacesSolid();
                m_worldspawn.AddSolidBrush(brush);
				
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
					
				}

				brush.AddFacePlane(ray);
			}

		};

	}
}
