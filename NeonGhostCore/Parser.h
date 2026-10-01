///Parser.h
#pragma once

#include <vector>
#include <string>
#include "Matrix.h"
#include "RayPlaneLine.h"


namespace ng {
	namespace map {

		typedef struct {
			std::string name;
			std::vector<std::string> values;
			unsigned char type;
		} TEntityParam;

		typedef struct _TEntity {
			std::string name;
			std::vector<TEntityParam> parameter;
			std::vector<_TEntity> sub_entry;
			struct _TEntity* parent = nullptr;
		} TEntity;





		class Parser
		{
		public:

			enum {
				NO_PARSER_ERROR = -2,
				PARSER_ERROR = -1,
				FLAG_SWAP_NONE = 0,
				FLAG_SWAP_AXIS_XY = 1 << 0,
				FLAG_SWAP_AXIS_XZ = 1 << 1,
				FLAG_SWAP_AXIS_ZY = 1 << 2
			};


			Parser();
			~Parser();


			virtual int save(const std::string& filename, const unsigned long flags = Parser::FLAG_SWAP_NONE);
			virtual int load(const std::string& filename, const unsigned long flags = Parser::FLAG_SWAP_NONE);

			const TEntity& getEnitys() const { return m_entitys; }

			void clear();

			std::string EntitysToStr();
			inline std::string GetError() {
				return last_error;

			}
		protected:
			typedef std::vector<std::string> TMapLine;
			std::vector<TMapLine> m_map;
			std::vector<TMapLine> removeLineComments(std::string single, std::string multistart = "", std::string multiend = "");
			void BeginEntity(std::string name);
			void AddParameter(std::string name, std::string value);
			void EndEntity();

			int parse(const std::string& filename, const unsigned long flags = 0);

			const TEntity& getActiveEntity();
			TEntity getParentEntity();
			void SetActiveEntityName(std::string name);

			void SetError(std::string error);

			std::string RemoveChars(const std::string& source, const std::string& chars);
		private:
			//std::string EntitysToStr(TEntity steps,  std::string seperator="");
			void EntitysToStr(std::ostringstream& s, const TEntity& steps, const  std::string& seperator = "");
			unsigned long  StepStrings(const std::vector<std::string>& str, unsigned long index = 0);
			TEntity* current_entity;
			std::string last_error;
			TEntity m_entitys;
		};

		template < class T>
		inline int GetEntity(TEntityParam entity, math::Matrix<3, 1, T>& v) {
			// "0 1 2 3 4"
			// "( a b c )"

			if (entity.values.size() != 3) return -1;
			//if (entity.values[0] != "\"") return -2;
			//if (entity.values[4] != "\"") return -3;

			v.x() = (T)std::stod(entity.values[0]);
			v.y() = (T)std::stod(entity.values[1]);
			v.z() = (T)std::stod(entity.values[2]);
			return 0;

		}

		template < class T>
		inline int GetEntity(TEntityParam entity, math::Matrix<4, 1, T>& v) {
			if (entity.values.size() != 4) return -1;
			v.x() = (T)std::stod(entity.values[0]);
			v.y() = (T)std::stod(entity.values[1]);
			v.z() = (T)std::stod(entity.values[2]);
			v.w() = (T)std::stod(entity.values[3]);
			return 0;

		}
		/*
		template < class T>
		inline int GetEntity(TEntityParam entity, math::Matrix<4, 1, T>& v, T& a) {
			if (entity.values.size() != 5) return -1;

			if (entity.values[0] != "[") return -2;
			if (entity.values[4] != "]") return -3;

			v.x() = (T)std::stod(entity.values[0]);
			v.y() = (T)std::stod(entity.values[1]);
			v.z() = (T)std::stod(entity.values[2]);
			v.w() = (T)std::stod(entity.values[3]);
			a = (T)std::stod(entity.values[5]);
			return 0;

		}
		*/

		// values: [ "[", a, b, c, d, "]", scale ]
		template <class T>
		inline int GetEntity(TEntityParam entity, math::Matrix<4, 1, T>& v, T& a) {
			if (entity.values.size() != 7) return -1;
			if (entity.values[0] != "[" || entity.values[5] != "]") return -2;
			v.x() = (T)std::stod(entity.values[1]);
			v.y() = (T)std::stod(entity.values[2]);
			v.z() = (T)std::stod(entity.values[3]);
			v.w() = (T)std::stod(entity.values[4]);   // Offset in Texeln
			a = (T)std::stod(entity.values[6]);   // Scale
			return 0;
		}
		inline int GetEntity(TEntityParam entity, std::string& v) {
			if (entity.values.size() != 1) return -1;
			v = entity.values[0];
			return 0;

		}

		inline int GetEntity(TEntityParam entity, double& v) {
			if (entity.values.size() != 1) return -1;
			v = (double)std::stod(entity.values[0]);
			return 0;

		}

		inline int GetEntity(TEntityParam entity, float& v) {
			if (entity.values.size() != 1) return -1;
			v = (float)std::stod(entity.values[0]);
			return 0;

		}



		template < class T>
		inline int GetEntity(TEntityParam entity, math::RayPlaneLine<3, T>& v, unsigned long swap_exis = Parser::FLAG_SWAP_NONE) {
			// 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14
			// ( a b c ) ( a b c ) (  a  b  c  )

			if (entity.values.size() != 15) return -1;

			if (entity.values[0] != "(") return -2;
			if (entity.values[4] != ")") return -3;

			if (entity.values[5] != "(") return -4;
			if (entity.values[9] != ")") return -5;

			if (entity.values[10] != "(") return -6;
			if (entity.values[14] != ")") return -7;

			math::Matrix<3, 1, T> v1, v2, v3;

			v1.x() = (T)std::stod(entity.values[1]);
			v1.y() = (T)std::stod(entity.values[2]);
			v1.z() = (T)std::stod(entity.values[3]);

			v2.x() = (T)std::stod(entity.values[6]);
			v2.y() = (T)std::stod(entity.values[7]);
			v2.z() = (T)std::stod(entity.values[8]);

			v3.x() = (T)std::stod(entity.values[11]);
			v3.y() = (T)std::stod(entity.values[12]);
			v3.z() = (T)std::stod(entity.values[13]);


			v = math::RayPlaneLine<3, T>(v1, v2, v3);
			v.flip(); //!!!!!!!!!!!!!!!!!! Wichtig Die map erzeugtr brushes dessen faces nach innen gerichtet sind wir arbeiten aber mit nach aussen gerichtetn flächen
			return 0;
		}


		/*
		inline TEntityParam SetEntityParam(std::string name, std::string value)
		{

			TEntityParam param;
			param.name = name;
			std::string s;
			for (unsigned int i = 0; i < value.length(); i++) {
				if (value.c_str()[i] == ' ') {
					if (s.length()) param.values.push_back(s);
					s.clear();
				}
				else s = s + value.c_str()[i];
			}
			if (s.length()) param.values.push_back(s);
			return param;
		}


		inline std::string EntityToStr(TEntityParam param)
		{
			std::string s = "\"" + param.name + "\"" + " \"";
			for (unsigned int i = 0; i < param.values.size(); i++)
			{
				s += param.values[i];
				if (i < param.values.size() - 1) s += " ";
			}

			return s + "\"";

		}


		template < class T>
		inline TEntityParam SetEntity(std::string name, const math::matrix<3, 1, T>& v) {
			return SetEntityParam(name, std::to_string(v.x()) + " " + std::to_string(v.y()) + " " + std::to_string(v.z()));
		}

		template < class T>
		inline TEntityParam SetEntity(std::string name, const math::matrix<4, 1, T>& v) {
			return SetEntityParam(name, std::to_string(v.x()) + " " + std::to_string(v.y()) + " " + std::to_string(v.z()) + " " + std::to_string(v.w()));
		}

		template < class T>
		inline TEntityParam SetEntity(std::string name, const math::Primitive<3, T>& v) {
			primitive::Vertex<T> vv1 = v.vertices()[0];
			primitive::Vertex<T> vv2 = v.vertices()[1];
			primitive::Vertex<T> vv3 = v.vertices()[v.vertexCount() - 1];
			primitive::matrix<3, 1, T> v1 = vv1.world();
			primitive::matrix<3, 1, T> v2 = vv2.world();
			primitive::matrix<3, 1, T> v3 = vv3.world();

			std::string s1 = "( " + std::to_string(v1.x()) + " " + std::to_string(v1.y()) + " " + std::to_string(v1.z()) + " ) ";
			s1 += "( " + std::to_string(v2.x()) + " " + std::to_string(v2.y()) + " " + std::to_string(v2.z()) + " ) ";
			s1 += "( " + std::to_string(v3.x()) + " " + std::to_string(v3.y()) + " " + std::to_string(v3.z()) + " ) ";
			return SetEntityParam(name, s1);
		}
		*/




	}
}