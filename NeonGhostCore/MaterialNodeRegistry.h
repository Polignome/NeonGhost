
#pragma once

#include "MaterialTypes.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <utility>

namespace ng::material
{
    // ------------------------------------------------------------
    // Node categories
    // ------------------------------------------------------------

    enum class NodeCategory : uint8_t
    {
        Input,
        Parameter,
        Texture,
        Math,
        Vector,
        Lighting,
        Output,
        Utility
    };

    // ------------------------------------------------------------
    // Pin definition
    // ------------------------------------------------------------

    struct MaterialPinDefinition
    {
        std::string name;

        ValueType type = ValueType::Unknown;

        PinDirection direction = PinDirection::Input;

        MaterialValue defaultValue;
    };

    // ------------------------------------------------------------
    // Node definition
    // ------------------------------------------------------------

    struct MaterialNodeDefinition
    {
        NodeType type = NodeType::Constant;

        std::string name;
        std::string description;

        NodeCategory category = NodeCategory::Utility;

        std::vector<MaterialPinDefinition> inputs;
        std::vector<MaterialPinDefinition> outputs;

        bool allowMultipleInstances = true;
    };

    // ------------------------------------------------------------
    // Node registry
    // ------------------------------------------------------------

    class MaterialNodeRegistry
    {
    public:

        using DefinitionMap =
            std::unordered_map<uint16_t, MaterialNodeDefinition>;

    private:

        DefinitionMap m_definitions;

    private:

        static uint16_t Key(NodeType type)
        {
            return static_cast<uint16_t>(type);
        }

    public:

        // --------------------------------------------------------
        // Registration
        // --------------------------------------------------------

        bool Register(MaterialNodeDefinition definition)
        {
            const uint16_t key = Key(definition.type);

            if (m_definitions.find(key) != m_definitions.end())
                return false;

            // Validate input pins.
            for (const auto& pin : definition.inputs)
            {
                if (pin.name.empty() ||
                    pin.type == ValueType::Unknown ||
                    pin.direction != PinDirection::Input)
                    return false;
            }

            // Validate output pins.
            for (const auto& pin : definition.outputs)
            {
                if (pin.name.empty() ||
                    pin.type == ValueType::Unknown ||
                    pin.direction != PinDirection::Output)
                    return false;
            }

            // Pin names must be unique within a node.
            auto validateNames = [](const auto& pins)
                {
                    for (size_t i = 0; i < pins.size(); ++i)
                    {
                        for (size_t j = i + 1; j < pins.size(); ++j)
                        {
                            if (pins[i].name == pins[j].name)
                                return false;
                        }
                    }

                    return true;
                };

            if (!validateNames(definition.inputs) ||
                !validateNames(definition.outputs))
                return false;

            m_definitions.emplace(key, std::move(definition));

            return true;
        }

        // --------------------------------------------------------
        // Lookup
        // --------------------------------------------------------

        const MaterialNodeDefinition* Find(NodeType type) const
        {
            auto it = m_definitions.find(Key(type));

            if (it == m_definitions.end())
                return nullptr;

            return &it->second;
        }

        bool Contains(NodeType type) const
        {
            return m_definitions.find(Key(type)) != m_definitions.end();
        }

        size_t Size() const
        {
            return m_definitions.size();
        }

        const DefinitionMap& GetDefinitions() const
        {
            return m_definitions;
        }

        // --------------------------------------------------------
        // Built-in registration
        // --------------------------------------------------------

        static MaterialNodeRegistry CreateDefault()
        {
            MaterialNodeRegistry registry;

            using NT = NodeType;
            using VT = ValueType;
            using PD = PinDirection;
            using NC = NodeCategory;

            auto input = [](const char* name, VT type,
                MaterialValue value = {})
                {
                    return MaterialPinDefinition{
                        name, type, PD::Input, value
                    };
                };

            auto output = [](const char* name, VT type,
                MaterialValue value = {})
                {
                    return MaterialPinDefinition{
                        name, type, PD::Output, value
                    };
                };

            auto add = [&](NT type,
                const char* name,
                const char* description,
                NC category,
                std::vector<MaterialPinDefinition> inputs,
                std::vector<MaterialPinDefinition> outputs,
                bool multiple = true)
                {
                    MaterialNodeDefinition definition;

                    definition.type = type;
                    definition.name = name;
                    definition.description = description;
                    definition.category = category;
                    definition.inputs = std::move(inputs);
                    definition.outputs = std::move(outputs);
                    definition.allowMultipleInstances = multiple;

                    registry.Register(std::move(definition));
                };

            // ----------------------------------------------------
            // Input
            // ----------------------------------------------------

            add(NT::Constant, "Constant",
                "Constant value (Float, Float2, Float3, Float4 or Color).",
                NC::Input, {},
                { output("Value", VT::Generic,
                    MaterialValue::Float(1.0f)) });

            add(NT::Parameter, "Parameter",
                "Exposed material parameter.",
                NC::Parameter, {},
                { output("Value", VT::Generic,
                    MaterialValue::Float(1.0f)) });

            add(NT::TextureCoordinate, "Texture Coordinates",
                "Provides mesh texture coordinates.",
                NC::Input, {},
                { output("UV", VT::Float2) });

            add(NT::VertexColor, "Vertex Color",
                "Provides interpolated vertex color.",
                NC::Input, {},
                { output("Color", VT::Color) });

            add(NT::VertexNormal, "Vertex Normal",
                "Provides interpolated vertex normal.",
                NC::Input, {},
                { output("Normal", VT::Float3) });

            // ----------------------------------------------------
            // Texture
            // ----------------------------------------------------

            add(NT::TextureSample, "Texture Sample",
                "Samples a 2D texture.",
                NC::Texture,
                {
                    input("Texture", VT::Texture2D),
                    input("UV", VT::Float2)
                },
                {
                    output("Color", VT::Color)
                });

                // ----------------------------------------------------
                // Math (generic: Float, Float2, Float3, Float4, Color)
                //
                // Scalars are broadcast to the wider operand type.
                // Defaults of generic pins are always scalar Floats.
                // ----------------------------------------------------

                const MaterialValue f0 = MaterialValue::Float(0.0f);
                const MaterialValue f1 = MaterialValue::Float(1.0f);
                const MaterialValue fHalf = MaterialValue::Float(0.5f);

                add(NT::Add, "Add",
                    "Adds two values component-wise.",
                    NC::Math,
                    {
                        input("A", VT::Generic, f0),
                        input("B", VT::Generic, f0)
                    },
                    { output("Result", VT::Generic) });

                add(NT::Subtract, "Subtract",
                    "Subtracts B from A component-wise.",
                    NC::Math,
                    {
                        input("A", VT::Generic, f0),
                        input("B", VT::Generic, f0)
                    },
                    { output("Result", VT::Generic) });

                add(NT::Multiply, "Multiply",
                    "Multiplies two values component-wise.",
                    NC::Math,
                    {
                        input("A", VT::Generic, f1),
                        input("B", VT::Generic, f1)
                    },
                    { output("Result", VT::Generic) });

                add(NT::Divide, "Divide",
                    "Divides A by B component-wise.",
                    NC::Math,
                    {
                        input("A", VT::Generic, f1),
                        input("B", VT::Generic, f1)
                    },
                    { output("Result", VT::Generic) });

                add(NT::Lerp, "Linear Interpolation",
                    "Interpolates between A and B.",
                    NC::Math,
                    {
                        input("A", VT::Generic, f0),
                        input("B", VT::Generic, f1),
                        input("Alpha", VT::Generic, fHalf)
                    },
                    { output("Result", VT::Generic) });

                add(NT::Clamp, "Clamp",
                    "Clamps a value to a range component-wise.",
                    NC::Math,
                    {
                        input("Value", VT::Generic, f0),
                        input("Min", VT::Generic, f0),
                        input("Max", VT::Generic, f1)
                    },
                    { output("Result", VT::Generic) });

                add(NT::Dot, "Dot Product",
                    "Calculates the dot product (Color: RGB only).",
                    NC::Math,
                    {
                        input("A", VT::Generic, f0),
                        input("B", VT::Generic, f0)
                    },
                    { output("Result", VT::Float) });

                add(NT::Normalize, "Normalize",
                    "Normalizes a vector (Color: RGB only).",
                    NC::Vector,
                    {
                        input("Vector", VT::Generic, f0)
                    },
                    { output("Result", VT::Generic) });

                add(NT::Split, "Split",
                    "Splits a value into its R, G, B and A components.",
                    NC::Vector,
                    {
                        input("Value", VT::Generic, f0)
                    },
                    {
                        output("R", VT::Float),
                        output("G", VT::Float),
                        output("B", VT::Float),
                        output("A", VT::Float)
                    });

                add(NT::Combine, "Combine",
                    "Builds a color from R, G, B and A.",
                    NC::Vector,
                    {
                        input("R", VT::Float, MaterialValue::Float(0.0f)),
                        input("G", VT::Float, MaterialValue::Float(0.0f)),
                        input("B", VT::Float, MaterialValue::Float(0.0f)),
                        input("A", VT::Float, MaterialValue::Float(1.0f))
                    },
                    { output("Color", VT::Color) });

                // ----------------------------------------------------
                // Lighting
                // ----------------------------------------------------

                add(NT::Diffuse, "Diffuse",
                    "Calculates diffuse lighting.",
                    NC::Lighting,
                    {
                        input("Albedo", VT::Color,
                            MaterialValue::Color(1, 1, 1)),
                        input("Normal", VT::Float3),
                        input("LightDirection", VT::Float3)
                    },
                {
                    output("Color", VT::Color)
                });

                add(NT::Specular, "Specular",
                    "Calculates specular lighting.",
                    NC::Lighting,
                    {
                        input("Normal", VT::Float3),
                        input("ViewDirection", VT::Float3),
                        input("LightDirection", VT::Float3),
                        input("Power", VT::Float,
                            MaterialValue::Float(32.0f))
                    },
                {
                    output("Color", VT::Color)
                });

                add(NT::Emissive, "Emissive",
                    "Defines self illuminated color.",
                    NC::Lighting,
                    {
                        input("Color", VT::Color)
                    },
                {
                    output("Result", VT::Color)
                });

                // ----------------------------------------------------
                // Output
                // ----------------------------------------------------

                add(NT::MaterialOutput, "Material Output",
                    "Final material surface properties.",
                    NC::Output,
                    {
                        input("BaseColor", VT::Color,
                            MaterialValue::Color(1, 1, 1)),

                        input("Normal", VT::Float3),

                        input("Emissive", VT::Color,
                            MaterialValue::Color(0, 0, 0)),

                        input("Opacity", VT::Float,
                            MaterialValue::Float(1.0f)),

                        input("Roughness", VT::Float,
                            MaterialValue::Float(1.0f)),

                        input("Metallic", VT::Float,
                            MaterialValue::Float(0.0f))
                    },
                    {},
                    false);

                return registry;
        }
    };
}

