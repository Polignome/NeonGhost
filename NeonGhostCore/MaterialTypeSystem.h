
#pragma once

#include "MaterialTypes.h"

namespace ng::material
{
    class MaterialTypeSystem
    {
    public:

        // --------------------------------------------------------
        // Typ-Eigenschaften
        // --------------------------------------------------------

        // Float, Float2, Float3, Float4, Color
        static bool IsNumeric(ValueType type)
        {
            switch (type)
            {
            case ValueType::Float:
            case ValueType::Float2:
            case ValueType::Float3:
            case ValueType::Float4:
            case ValueType::Color:
                return true;

            default:
                return false;
            }
        }

        // Anzahl gespeicherter Komponenten.
        static int ComponentCount(ValueType type)
        {
            switch (type)
            {
            case ValueType::Float:  return 1;
            case ValueType::Float2: return 2;
            case ValueType::Float3: return 3;
            case ValueType::Float4: return 4;
            case ValueType::Color:  return 4;
            default:                return 0;
            }
        }

        // Anzahl der Komponenten, auf die Mathe-Nodes wirken.
        // Bei Color nur RGB; Alpha bleibt unveraendert.
        static int MathComponentCount(ValueType type)
        {
            return type == ValueType::Color
                ? 3
                : ComponentCount(type);
        }

        // Gemeinsamer Typ zweier numerischer Operanden:
        //   Float + T      -> T          (Skalar wird gebroadcastet)
        //   T + T          -> T
        //   Float3 + Float4-> Float4
        //   Float3/Float4 + Color -> Color
        //   Float2 + Float3/Float4/Color -> nicht moeglich
        static bool ResolveCommonType(
            ValueType a,
            ValueType b,
            ValueType& result)
        {
            if (!IsNumeric(a) || !IsNumeric(b))
                return false;

            ValueType r;

            if (a == b)
                r = a;
            else if (a == ValueType::Float)
                r = b;
            else if (b == ValueType::Float)
                r = a;
            else if (a == ValueType::Float2 || b == ValueType::Float2)
                return false;
            else if (a == ValueType::Color || b == ValueType::Color)
                r = ValueType::Color;
            else
                r = ValueType::Float4;

            result = r;
            return true;
        }

        // --------------------------------------------------------
        // Konvertierung
        // --------------------------------------------------------

        static bool CanConvert(ValueType source, ValueType target)
        {
            ConversionKind conversion;
            return GetConversion(source, target, conversion);
        }

        static bool GetConversion(
            ValueType source,
            ValueType target,
            ConversionKind& conversion)
        {
            conversion = ConversionKind::None;

            if (source == target)
                return true;

            // Generic wird erst beim Kompilieren aufgeloest. Beim
            // Verbinden genuegt es, dass beide Seiten numerisch sind.
            if (source == ValueType::Generic ||
                target == ValueType::Generic)
            {
                const bool sourceOk =
                    source == ValueType::Generic || IsNumeric(source);

                const bool targetOk =
                    target == ValueType::Generic || IsNumeric(target);

                return sourceOk && targetOk;
            }

            switch (source)
            {
            case ValueType::Float:
                switch (target)
                {
                case ValueType::Float2:
                    conversion = ConversionKind::FloatToFloat2;
                    return true;
                case ValueType::Float3:
                    conversion = ConversionKind::FloatToFloat3;
                    return true;
                case ValueType::Float4:
                    conversion = ConversionKind::FloatToFloat4;
                    return true;
                case ValueType::Color:
                    conversion = ConversionKind::FloatToColor;
                    return true;
                default:
                    break;
                }
                break;

            case ValueType::Float3:
                if (target == ValueType::Color)
                {
                    conversion = ConversionKind::Float3ToColor;
                    return true;
                }
                if (target == ValueType::Float4)
                {
                    conversion = ConversionKind::Float3ToFloat4;
                    return true;
                }
                break;

            case ValueType::Float4:
                if (target == ValueType::Color)
                {
                    conversion = ConversionKind::Float4ToColor;
                    return true;
                }
                break;

            case ValueType::Color:
                if (target == ValueType::Float3)
                {
                    conversion = ConversionKind::ColorToFloat3;
                    return true;
                }
                if (target == ValueType::Float4)
                {
                    conversion = ConversionKind::ColorToFloat4;
                    return true;
                }
                break;

            default:
                break;
            }

            return false;
        }

        // Wendet eine bereits bestimmte Konvertierung an (schnell,
        // ohne erneute Typpruefung; fuer den Evaluator).
        static bool Apply(
            ConversionKind conversion,
            const MaterialValue& source,
            MaterialValue& result)
        {
            MaterialValue out;
            out.texture = 0;

            const float x = source.value[0];

            switch (conversion)
            {
            case ConversionKind::None:
                result = source;
                return true;

            case ConversionKind::FloatToFloat2:
                out.type = ValueType::Float2;
                out.value = { x, x, 0.0f, 0.0f };
                break;

            case ConversionKind::FloatToFloat3:
                out.type = ValueType::Float3;
                out.value = { x, x, x, 0.0f };
                break;

            case ConversionKind::FloatToFloat4:
                out.type = ValueType::Float4;
                out.value = { x, x, x, x };
                break;

            case ConversionKind::FloatToColor:
                out.type = ValueType::Color;
                out.value = { x, x, x, 1.0f };
                break;

            case ConversionKind::Float3ToColor:
                out.type = ValueType::Color;
                out.value = {
                    source.value[0], source.value[1], source.value[2], 1.0f };
                break;

            case ConversionKind::Float3ToFloat4:
                out.type = ValueType::Float4;
                out.value = {
                    source.value[0], source.value[1], source.value[2], 0.0f };
                break;

            case ConversionKind::Float4ToColor:
                out.type = ValueType::Color;
                out.value = source.value;
                break;

            case ConversionKind::ColorToFloat4:
                out.type = ValueType::Float4;
                out.value = source.value;
                break;

            case ConversionKind::ColorToFloat3:
                out.type = ValueType::Float3;
                out.value = {
                    source.value[0], source.value[1], source.value[2], 0.0f };
                break;

            default:
                return false;
            }

            result = out;
            return true;
        }

        static bool Convert(
            const MaterialValue& source,
            ValueType target,
            MaterialValue& result)
        {
            ConversionKind conversion;

            if (!GetConversion(source.type, target, conversion))
                return false;

            // Generic ist kein Laufzeittyp.
            if (source.type == ValueType::Generic ||
                target == ValueType::Generic)
                return false;

            return Apply(conversion, source, result);
        }
    };
}
