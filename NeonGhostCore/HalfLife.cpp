#include "pch.h"

#include "HalfLife.h"

using namespace ng::map;

unsigned long  HalfLife::StepStrings(const std::vector<std::string> &str, unsigned long index)
{
	if (index >= str.size()) return (unsigned long)str.size();
	
	static bool  m_brush = false;
	static bool  m_light = false;
	while (index < (unsigned long)str.size())
	{

		std::string s = str[index];
	      
		if (s == "{")
		{
			std::string name = "Noname";
			if (getActiveEntity().name == "world") {
				name = "solid"; m_brush = true;
			}
			if (getActiveEntity().name == "light") {
				m_light = true;
				name = "light";
			}


			BeginEntity(name);

			index = StepStrings(str, index + 1);
		}

		if (s == "}")
		{
			if (m_brush) m_brush = false;
			if (m_light) m_light = false;
			EndEntity();
			return index;
		}


		if (s == "(" && m_brush) {

			BeginEntity("side");



			if (index + 30 >= str.size()) {
				SetError("String zur Kurz");
				return (unsigned long)str.size() + 1;
			}
			if (str[index + 4] != ")") {
				SetError("( a b c >)< ( a b c ) ( a b c ) ");
				return (unsigned long)str.size() + 1;
			}
			if (str[index + 5] != "(") {
				SetError("( a b c ) >(< a b c ) ( a b c ) ");
				return (unsigned long)str.size() + 1;
			}
			if (str[index + 9] != ")") {
				SetError("( a b c ) ( a b c >)< ( a b c ) ");
				return (unsigned long)str.size() + 1;
			}
			if (str[index + 10] != "(") {

				SetError("( a b c ) ( a b c ) >(< a b c )! " + str[index + 10]);
				return (unsigned long)str.size() + 1;
			}
			if (str[index + 14] != ")") {
				SetError("( a b c ) ( a b c ) ( a b c >)< ");
				return (unsigned long)str.size() + 1;
			}

			{
				std::string v1 = "";
#define	SWAP_AXIS
#ifdef SWAP_AXIS
				v1 += str[index + 0]; v1 += " ";// ( 
				v1 += str[index + 2]; v1 += " ";// a
				v1 += str[index + 3]; v1 += " ";// b
				v1 += str[index + 1]; v1 += " ";// a
				v1 += str[index + 4]; v1 += " ";// )

				v1 += str[index + 5]; v1 += " ";// (
				v1 += str[index + 7]; v1 += " ";// a
				v1 += str[index + 8]; v1 += " ";// b
				v1 += str[index + 6]; v1 += " ";// a
				v1 += str[index + 9]; v1 += " ";// )

				v1 += str[index + 10]; v1 += " ";// (
				v1 += str[index + 12]; v1 += " ";// a
				v1 += str[index + 13]; v1 += " ";// b
				v1 += str[index + 11]; v1 += " ";// a
				v1 += str[index + 14]; v1 += " ";// )
#else /////////////////////////////////////////////////////////////////////////
				v1 += str[index + 0]; v1 += " ";// ( 
				v1 += str[index + 1]; v1 += " ";// a
				v1 += str[index + 2]; v1 += " ";// b
				v1 += str[index + 3]; v1 += " ";// a
				v1 += str[index + 4]; v1 += " ";// )

				v1 += str[index + 5]; v1 += " ";// (
				v1 += str[index + 6]; v1 += " ";// a
				v1 += str[index + 7]; v1 += " ";// b
				v1 += str[index + 8]; v1 += " ";// a
				v1 += str[index + 9]; v1 += " ";// )

				v1 += str[index + 10]; v1 += " ";// (
				v1 += str[index + 11]; v1 += " ";// a
				v1 += str[index + 12]; v1 += " ";// b
				v1 += str[index + 13]; v1 += " ";// a
				v1 += str[index + 14]; v1 += " ";// )

#endif




				this->AddParameter("plane", v1);

			}

			this->AddParameter("material", str[index + 15]);

			switch (str[index + 15][0]) {
			case '#': this->AddParameter("aggregate", "liquid1"); break;
			case '~': this->AddParameter("aggregate", "liquid2"); break;
			case '%': this->AddParameter("aggregate", "liquid3"); break;
			case '-': this->AddParameter("aggregate", "glas"); break;
			case '?': this->AddParameter("aggregate", "solid_no_splitter"); break;
			case '*': this->AddParameter("aggregate", "gas"); break;
			default: this->AddParameter("aggregate", "solid"); break;
			}


			{
				std::string v1 = "";
				//[0.0 0.0 0.0 0.0] 0.0

				v1 += str[index + 16]; v1 += " ";// [
				v1 += str[index + 17]; v1 += " ";// a
				v1 += str[index + 18]; v1 += " ";// b
				v1 += str[index + 19]; v1 += " ";// c
				v1 += str[index + 20]; v1 += " ";// d
				v1 += str[index + 21]; v1 += " ";// ]
				v1 += str[index + 29]; v1 += " ";// scaleu

				this->AddParameter("uaxis", v1);

			}

			{
				std::string v1 = "";
				//[0.0 0.0 0.0 0.0] 0.0

				v1 += str[index + 22]; v1 += " ";// [
				v1 += str[index + 23]; v1 += " ";// a
				v1 += str[index + 24]; v1 += " ";// b
				v1 += str[index + 25]; v1 += " ";// c
				v1 += str[index + 26]; v1 += " ";// d
				v1 += str[index + 27]; v1 += " ";// ]
				v1 += str[index + 30]; v1 += " ";// scalev

				this->AddParameter("vaxis", v1);
			}


			{
				std::string v1 = "";
				//[0.0 0.0 0.0 0.0] 0.0

				v1 += str[index + 28]; //rotate
				
				this->AddParameter("rotate", v1);
			}

		

			index += 30;



			EndEntity();
		}





		if (s.c_str()[0] == '\"' && s.c_str()[s.length() - 1] == '\"')
		{
			
			if (++index >= str.size())
			{
				return (unsigned long)str.size() + 1;
			}



			std::string s2 = str[index];

			if (s2.c_str()[0] != '\"' || s2.c_str()[s2.length() - 1] != '\"')
			{
				return (unsigned long)str.size() + 1;
			}

			s = RemoveChars(s, "\"");
			s2 = RemoveChars(s2, "\"");
			
			

			if (s == "classname" && s2 == "worldspawn") SetActiveEntityName("world");
			else if (s == "classname") SetActiveEntityName(s2);

			AddParameter(s, s2);
		}

		index++;

	}

	return (unsigned long)index;
}


int HalfLife::save(const std::string& filename, const unsigned long flags)
{
	return map::Parser::save(filename, flags);
}


int HalfLife::load(const std::string& filename, const unsigned long flags)
{
	
	if (parse(filename, flags) != Parser::NO_PARSER_ERROR)
	{

		return PARSER_ERROR;
	}

	this->removeLineComments("//", "/*", "*/");
	
	std::vector<std::string> linea;

	bool world_spawn = false;

	for (unsigned int i = 0; i < m_map.size(); i++) {
		for (unsigned int j = 0; j < m_map[i].size(); j++)
		{
			linea.push_back(m_map[i][j]);
		}

	}


	if (StepStrings(linea) != linea.size()) {



		SetError("Parser error");
		return PARSER_ERROR;
	}


	return NO_PARSER_ERROR;
}