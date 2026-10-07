///////////////////////////////// HALF-LIFE MAP PARSER //////////////////////////////////////////

#include "pch.h"
#include <sstream> 
#include "HalfLife.h"
#define	SWAP_AXIS
using namespace ng::map;




long HalfLife::ReadBrushFace(
    const std::vector<std::string>& str,
    long index,
    bool liquit)
{
   

    if (index < 0 || index + 30 >= static_cast<long>(str.size())) {
        SetError("String zu kurz");
        return -1;
    }

    if (str[index] != "(") {
        SetError("BrushFace muss mit '(' beginnen");
        return -1;
    }

    // ------------------------------------------------------------
    // Plane
    // ------------------------------------------------------------

    if (str[index + 4] != ")" ||
        str[index + 5] != "(" ||
        str[index + 9] != ")" ||
        str[index + 10] != "(" ||
        str[index + 14] != ")")
    {
        SetError("Ungueltige BrushFace-Plane");
        return -1;
    }

    BeginEntity("side");

    std::string plane;

    plane += str[index + 0] + " ";
    plane += str[index + 2] + " ";
    plane += str[index + 3] + " ";
    plane += str[index + 1] + " ";
    plane += str[index + 4] + " ";

    plane += str[index + 5] + " ";
    plane += str[index + 7] + " ";
    plane += str[index + 8] + " ";
    plane += str[index + 6] + " ";
    plane += str[index + 9] + " ";

    plane += str[index + 10] + " ";
    plane += str[index + 12] + " ";
    plane += str[index + 13] + " ";
    plane += str[index + 11] + " ";
    plane += str[index + 14];

    AddParameter("plane", plane);

    // ------------------------------------------------------------
    // Material
    // ------------------------------------------------------------

    AddParameter("material", str[index + 15]);

    if (liquit)
        AddParameter("aggregate", "liquit");
    else
        AddParameter("aggregate", "solid");

    // ------------------------------------------------------------
    // U-Axis
    // ------------------------------------------------------------

    std::string uaxis;

    uaxis += str[index + 16] + " ";
    uaxis += str[index + 18] + " ";
    uaxis += str[index + 19] + " ";
    uaxis += str[index + 17] + " ";
    uaxis += str[index + 20] + " ";
    uaxis += str[index + 21] + " ";
    uaxis += str[index + 29];

    AddParameter("uaxis", uaxis);

    // ------------------------------------------------------------
    // V-Axis
    // ------------------------------------------------------------

    std::string vaxis;

    vaxis += str[index + 22] + " ";
    vaxis += str[index + 24] + " ";
    vaxis += str[index + 25] + " ";
    vaxis += str[index + 23] + " ";
    vaxis += str[index + 26] + " ";
    vaxis += str[index + 27] + " ";
    vaxis += str[index + 30];

    AddParameter("vaxis", vaxis);

    // ------------------------------------------------------------
    // Rotation
    // ------------------------------------------------------------

    AddParameter("rotate", str[index + 28]);

    // Falls du diese Parameter separat behalten möchtest:
    AddParameter("uscale", str[index + 29]);
    AddParameter("vscale", str[index + 30]);

    EndEntity();


    // 31 Tokens wurden verarbeitet: 0 ... 30
    if ((index + 31) >= str.size()) return -1;
    return index + 31;
}







unsigned long HalfLife::StepStrings(
    const std::vector<std::string>& str,
    long index)
{
    if (index < 0 || index >= static_cast<long>(str.size()))
        return -1;

    while (index < static_cast<long>(str.size()))
    {
        const std::string& line = str[index];

        // --------------------------------------------------------
        // Ende des aktuellen Blocks
        // --------------------------------------------------------

        if (line == "}")
        {
            EndEntity();
            return index + 1;
        }

        // --------------------------------------------------------
        // Neuer Block
        // --------------------------------------------------------

        if (line == "{")
        {
            BeginEntity("entity");

            index = StepStrings(str, index + 1);

            if (index == static_cast<unsigned long>(-1))
                return -1;

            continue;
        }

        // --------------------------------------------------------
        // Key / Value
        // --------------------------------------------------------

        if (line.size() >= 2 &&
            line.front() == '"' &&
            line.back() == '"')
        {
            if (index + 1 >= static_cast<long>(str.size()))
            {
                SetError("Key ohne Value");
                return -1;
            }

            std::string key = RemoveChars(line, "\"");
            std::string value = str[index + 1];

            if (value.size() < 2 ||
                value.front() != '"' ||
                value.back() != '"')
            {
                SetError("Value erwartet");
                return -1;
            }

            value = RemoveChars(value, "\"");

            // ----------------------------------------------------
            // classname
            // ----------------------------------------------------

            if (key == "classname")
            {
                

                if (value == "worldspawn")
                {
                    SetActiveEntityName("world");
                
                }
                else if (value == "func_water")
                {
                    SetActiveEntityName("liquid");
                
                }
                else if (value == "light")
                {
                    SetActiveEntityName("light");
                }
                else
                {
                    // Alle anderen Entities behalten ihren
                    // normalen Entity-Typ.
                    SetActiveEntityName("entity");
                }
            }
            else
            {
                AddParameter(key, value);
            }

            index += 2;
            continue;
        }

        // --------------------------------------------------------
        // Brush Face
        // --------------------------------------------------------
        /*
        if (line == "(")
        {
            auto& active = getActiveEntity();

            if (active.parent == nullptr)
            {
                SetError("BrushFace ohne Parent");
                return -1;
            }

            std::string parentName = active.parent->name;

            if (parentName != "world" &&
                parentName != "liquit")
            {
                SetError(
                    "BrushFace ausserhalb von world/liquit");
                return -1;
            }

            // Der aktuelle Block ist der Brush
            SetActiveEntityName("brush");

            bool isLiquit = (parentName == "liquit");

            index = ReadBrushFace(
                str,
                index,
                isLiquit);

            if (index < 0)
                return -1;

            continue;
        }
        */
        /*
        if (line == "(")
        {
            auto& active = getActiveEntity();

            if (active.parent == nullptr)
            {
                SetError("BrushFace ohne Parent");
                return -1;
            }

            std::string parentName = active.parent->name;

            if (parentName != "world" &&
                parentName != "liquid")
            {
                SetError(
                    "BrushFace ausserhalb von world/liquid");
                return -1;
            }

            // Der aktuelle Block ist der Brush
            SetActiveEntityName("brush");

            bool isLiquid = (parentName == "liquid");

            index = ReadBrushFace(
                str,
                index,
                isLiquid);

            if (index < 0)
                return -1;

            continue;
        }*/

        if (line == "(")
        {
            auto& active = getActiveEntity();

            if (active.parent == nullptr)
            {
                SetError("BrushFace ohne Parent");
                return -1;
            }

            const std::string& parentName = active.parent->name;
            const bool isLiquid = (parentName == "liquid");

            // Faces werden IMMER gelesen, auch bei func_wall/func_door usw.
            // Data::StepEntitys ignoriert sie später (name == "entity").
            SetActiveEntityName("brush");

            index = ReadBrushFace(str, index, isLiquid);
            if (index < 0)
                return -1;

            continue;
        }

        // --------------------------------------------------------
        // Unbekanntes Token
        // --------------------------------------------------------

        printf(
            "Unbekanntes Token [%s] bei Index %ld\n",
            line.c_str(),
            index);

        ++index;
    }

    return index;
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



    if (StepStrings(linea) == static_cast<unsigned long>(-1))
    {
        SetError("Parser error");
        return PARSER_ERROR;
    }

	

    
   




	return NO_PARSER_ERROR;
}
