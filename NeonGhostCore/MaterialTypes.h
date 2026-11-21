
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <array>

namespace ng::material
{
    using NodeId = uint64_t;
    using PinId = uint64_t;
    using ConnectionId = uint64_t;
    using TextureId = uint64_t;

    // ------------------------------------------------------------
    // Datentypen der Pins
    // ------------------------------------------------------------

    enum class ValueType : uint8_t
    {
        Unknown,
        Float,
        Float2,
        Float3,
        Float4,
        Color,
        Boolean,
        Texture2D,

        // Numerischer Platzhaltertyp fuer Pins, deren konkreter Typ erst
        // beim Kompilieren aus den Verbindungen ermittelt wird
        // (Float, Float2, Float3, Float4 oder Color).
        Generic
    };

    enum class PinDirection : uint8_t
    {
        Input,
        Output
    };

    // ------------------------------------------------------------
    // Node-Kategorien
    // ------------------------------------------------------------

    enum class NodeType : uint16_t
    {
        Constant,
        Parameter,

        TextureCoordinate,
        VertexColor,
        VertexNormal,

        TextureSample,

        Add,
        Subtract,
        Multiply,
        Divide,
        Lerp,
        Clamp,
        Dot,
        Normalize,

        Diffuse,
        Specular,
        Emissive,

        MaterialOutput,

        // Am Ende angefuegt, damit sich bestehende Werte nicht verschieben.
        Split,
        Combine
    };

    // ------------------------------------------------------------
    // Universeller Pin-Wert
    // ------------------------------------------------------------

    struct MaterialValue
    {
        ValueType type = ValueType::Unknown;

        std::array<float, 4> value = { 0, 0, 0, 0 };

        TextureId texture = 0;

        static MaterialValue Float(float v)
        {
            MaterialValue result;
            result.type = ValueType::Float;
            result.value[0] = v;
            return result;
        }

        static MaterialValue Float2(float x, float y)
        {
            MaterialValue result;
            result.type = ValueType::Float2;
            result.value = { x, y, 0, 0 };
            return result;
        }

        static MaterialValue Texture(TextureId id)
        {
            MaterialValue result;
            result.type = ValueType::Texture2D;
            result.texture = id;
            return result;
        }

        static MaterialValue Float3(float x, float y, float z)
        {
            MaterialValue result;
            result.type = ValueType::Float3;
            result.value = { x, y, z, 0 };
            return result;
        }

        static MaterialValue Color(float r, float g, float b, float a = 1.0f)
        {
            MaterialValue result;
            result.type = ValueType::Color;
            result.value = { r, g, b, a };
            return result;
        }
    };

    // ------------------------------------------------------------
    // Pin
    // ------------------------------------------------------------

    struct MaterialPin
    {
        PinId id = 0;

        std::string name;

        ValueType type = ValueType::Unknown;

        PinDirection direction = PinDirection::Input;

        MaterialValue defaultValue;
    };

    // ------------------------------------------------------------
    // Node
    // ------------------------------------------------------------

    struct MaterialNode
    {
        NodeId id = 0;

        NodeType type = NodeType::Constant;

        std::string name;

        std::vector<MaterialPin> inputs;
        std::vector<MaterialPin> outputs;

        float editorX = 0.0f;
        float editorY = 0.0f;
    };

    enum class ConversionKind : uint8_t
    {
        None,
        FloatToFloat3,
        FloatToColor,
        Float3ToColor,
        Float4ToColor,
        ColorToFloat3,
        FloatToFloat2,
        FloatToFloat4,
        Float3ToFloat4,
        ColorToFloat4
    };

    // ------------------------------------------------------------
    // Verbindung zwischen zwei Pins
    // ------------------------------------------------------------


    struct MaterialConnection
    {
        ConnectionId id = 0;

        NodeId sourceNode = 0;
        PinId sourcePin = 0;

        NodeId targetNode = 0;
        PinId targetPin = 0;

        ConversionKind conversion = ConversionKind::None;
    };


  
  

}
