#pragma once
#include <map>
#include "Parser.h"
#include "WorldSpawn.h"
#include "Wad3File.h"

#define DDG_MAP_ID "DarkGhostMap"


namespace ng {
	namespace map {
		template < class T = float>

		class Data
		{
		public:

			Data() = default;

			~Data() = default;


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

		
				for (auto e : entity.sub_entry)	StepEntitys(e);
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
						GetEntity<T>(p, ray.vAxis(), ray.rotate());

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
