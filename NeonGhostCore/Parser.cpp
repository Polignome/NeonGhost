#include "pch.h"

#include <fstream>
#include <sstream>
#include <iostream>
#include "Parser.h"
using namespace ng::map;




Parser::Parser() {}
Parser::~Parser() {}



const TEntity& Parser::getActiveEntity()  { return (*current_entity); }

TEntity Parser::getParentEntity()
{
	TEntity empty;
	if (current_entity = &m_entitys) return empty;
	return *current_entity->parent;
}

void Parser::SetActiveEntityName(std::string name)
{
	if ((current_entity == 0) || (current_entity->name==name)) return;
		current_entity->name = name;
}

std::string Parser::RemoveChars(const std::string& source, const std::string& chars) {
	std::string result = "";

  	for (const auto &i : source) {
	   bool foundany = false;
		for (unsigned int j = 0; j < chars.length() && !foundany; j++) {
			if ((foundany = (i == chars[j])) == true) continue;
		}
		if (!foundany) {
			result += i;
		}
	}
	return result;
}

void Parser::clear() {
	TEntity empty;
	empty.name = "DarkGhostMap";
	m_entitys = empty;

	current_entity = &m_entitys;
	m_map.clear();

}

int Parser::load(const std::string& filename, const unsigned long flags) {
	clear();
	

	parse(filename, flags);

	removeLineComments("//", "/*", "*/");

	std::vector<std::string> linea;
	for (const auto &i : m_map) {
		for (const auto &j : i)
		{
			linea.push_back(j);
		}

	}



	unsigned long steps = StepStrings(linea);

	if (steps > linea.size()) {
		SetError("Parser error " + std::to_string(steps) + " / " + std::to_string(linea.size()));
		return PARSER_ERROR;
	}

	

	return NO_PARSER_ERROR;
}

int Parser::save(const std::string& filename, const unsigned long flags)
{
	std::ofstream myfile(filename);


	if (myfile.is_open())
	{
		myfile << EntitysToStr();
	}
	else return PARSER_ERROR;

	return NO_PARSER_ERROR;
}

int Parser::parse(const std::string& filename, const unsigned long flags)
{

	std::string line;
	std::ifstream myfile(filename);

	clear();

	bool str = false;
	if (myfile.is_open())
	{
		char c;
		TMapLine l;
		std::string s;
		while (myfile.get(c))
		{
			if (c == '\n') {

				if (s.length()) { l.push_back(s); s.clear(); }
				if (l.size() > 0) { m_map.push_back(l); l.clear(); }
			}
			else if (c == '"') {
				str = !str;
				s += c;
				if (s.length() && !str) { l.push_back(s); s.clear(); }
			}
			else if (c == ' ' && s.length() > 0 && !str) { l.push_back(s); s.clear(); }
			else if (c == ' ') {
				if (str) s += c;
			}
			else {
				s += c;
			}
		}

		if (s.length()) { l.push_back(s); s.clear(); }
		if (l.size() > 0) { m_map.push_back(l); l.clear(); }

		if (str) {
			this->SetError(" \" wurde nicht geschlossen ");
		}
		myfile.close();
	}
	else {

		return PARSER_ERROR;
	}


	return NO_PARSER_ERROR;
}

std::vector<Parser::TMapLine> Parser::removeLineComments(std::string single, std::string multistart, std::string multiend) {
	std::vector<	TMapLine> temp;
	bool iscomment = false;

	for (const auto &lines : m_map)
	{
		TMapLine line;
		for (const auto &strings :lines)
		{
			std::string s = strings;
			bool help_flag = true;
			if (multistart != "" && multiend != "") {
				if (s == multistart) iscomment = true;
				if (s == multiend)
				{
					help_flag = false;
					if (!iscomment) {
						SetError("Fehler Kommentarende ohne Anfang");
						temp.clear();
						return temp;
					}
					iscomment = false;
				}
			}

			if (s == single && !iscomment) {
				break;
			}
			if (!iscomment && help_flag) line.push_back(s);

		}

		if (line.size()) temp.push_back(line);
	}

	if (iscomment) {
		temp.clear();

		SetError("Fehler Kommentar ohne Ende");
		temp.clear();
		return temp;
	}

	m_map = temp;
	return m_map;
}

void Parser::SetError(std::string name)
{
	//clear();
	
	last_error+= name+"\n";
}

void Parser::BeginEntity(std::string name)
{
	TEntity entity;

	entity.parent = current_entity;
	entity.name = name;

	current_entity->sub_entry.push_back(entity);
	current_entity = &current_entity->sub_entry[current_entity->sub_entry.size() - 1];
}

void Parser::AddParameter(std::string name, std::string value)
{
	if (current_entity == nullptr) {
		return;
	}



	if (name == "origin")
	{
		std::istringstream iss(value);
		double ox = 0.0, oy = 0.0, oz = 0.0;

		if (iss >> ox >> oy >> oz)
		{
			std::ostringstream oss;
			oss << oy << " " << oz << " " << ox; // dieselbe Permutation wie bei "plane"
			value = oss.str();
		}
		else
		{
			printf("[HalfLife] 'origin' konnte nicht geparst werden: '%s'\n", value.c_str());
		}
	}




	TEntityParam param;
	param.name = name;
	std::string s;

	for (const auto &i :value) {
		if (i == ' ') {
			if (s.length()) param.values.push_back(s);
			s.clear();
		}
		else s = s + i;
	}
	if (s.length()) param.values.push_back(s);


	current_entity->parameter.push_back(param);
}

void Parser::EndEntity()
{
	if (current_entity == &m_entitys) {
		SetError("Entity Ende ohne anfang");
		return;
	}

	current_entity = current_entity->parent;
}

std::string Parser::EntitysToStr()
{
	std::ostringstream  i;
	EntitysToStr(i, m_entitys);
	return i.str();
}

void Parser::EntitysToStr(std::ostringstream& s, const  TEntity &steps,const std::string &seperator)
{

	s << seperator + "#" + steps.name + "\n";
	s << seperator + "{\n";

	//for (unsigned int i = 0; i < steps.parameter.size(); i++)
	for (const auto &param : steps.parameter)
		{
		s << seperator + "     " + "\"" + param.name + "\"    ";
		s << "\"";
		for (unsigned int j = 0; j < param.values.size(); j++)
		{
			s << param.values[j];
			if (j < param.values.size() - 1) s << " ";
		}
		s << "\"\n";

	}

	for (const auto &i  : steps.sub_entry)  EntitysToStr(s, i, seperator + "    ");


	s << seperator + "} // " + steps.name + "\n\n";


}

unsigned long  Parser::StepStrings(const std::vector<std::string> &str, unsigned long index)
{


	if (index >= str.size()) return (unsigned long)str.size();

	while (index < str.size()) {


		std::string s = str[index];
		if (s.c_str()[0] == '#') {
			if (index + 1 >= str.size()) { SetError("Unerwartetes Dateinede nach " + s); return (unsigned long)str.size(); }
			index++;

			if (str[index] != "{") { SetError(" '{' nach " + s + " fehlt"); return (unsigned long)str.size(); }
			std::string name = RemoveChars(s, "#");
			if (name != "DarkGhostMap") BeginEntity(RemoveChars(s, "#"));
			else SetActiveEntityName("DarkGhostMap");
			index = StepStrings(str, index);
		}
		else
			if (s == "}" && this->getActiveEntity().name != "DarkGhostMap")
			{
				EndEntity();
				return  index;
			}
			else
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
					AddParameter(s, s2);
				}

		if (index < str.size())index++;
	}

	return index;
}