
#pragma once
#include "MaterialTextureSampler.h"
#include "MaterialParameters.h"
#include "MaterialProgram.h"
#include "MaterialTypeSystem.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace ng::material
{
    // ------------------------------------------------------------
    // Eingabedaten fuer eine Materialauswertung
    // ------------------------------------------------------------

    struct MaterialEvaluationContext
    {
        std::array<float, 2> uv = { 0.0f, 0.0f };

        std::array<float, 4> vertexColor =
        { 1.0f, 1.0f, 1.0f, 1.0f };

        std::array<float, 3> vertexNormal =
        { 0.0f, 0.0f, 1.0f };

        // Zeigt zur Lichtquelle.
        std::array<float, 3> lightDirection =
        { 0.0f, 0.0f, 1.0f };

        // Zeigt zur Kamera.
        std::array<float, 3> viewDirection =
        { 0.0f, 0.0f, 1.0f };

        const MaterialParameterSet* parameters = nullptr;

        const IMaterialTextureSampler* textureSampler = nullptr;
    };

    // ------------------------------------------------------------
    // Ergebnis der Materialauswertung
    // ------------------------------------------------------------

    struct MaterialSurface
    {
        std::array<float, 4> baseColor =
        { 1.0f, 1.0f, 1.0f, 1.0f };

        std::array<float, 3> normal =
        { 0.0f, 0.0f, 1.0f };

        std::array<float, 4> emissive =
        { 0.0f, 0.0f, 0.0f, 1.0f };

        float opacity = 1.0f;
        float roughness = 1.0f;
        float metallic = 0.0f;
    };

    // ------------------------------------------------------------
    // Wiederverwendbarer Wertespeicher (keine Allokation pro Pixel)
    //
    // Pro Thread eine Instanz verwenden.
    // ------------------------------------------------------------

    class MaterialEvaluationScratch
    {
    public:

        void Prepare(const MaterialProgram& program)
        {
            const std::size_t count = program.GetSlotCount();

            if (m_slots.size() < count)
                m_slots.resize(count);
        }

        MaterialValue* Data()
        {
            return m_slots.data();
        }

    private:

        std::vector<MaterialValue> m_slots;
    };

    // ------------------------------------------------------------
    // MaterialEvaluator
    // ------------------------------------------------------------

    class MaterialEvaluator
    {
    private:

        using Slots = MaterialValue*;

        static MaterialValue MakeFloat(float value)
        {
            MaterialValue result;

            result.type = ValueType::Float;
            result.value = { value, 0.0f, 0.0f, 0.0f };

            return result;
        }

        static MaterialValue MakeFloat2(float x, float y)
        {
            MaterialValue result;

            result.type = ValueType::Float2;
            result.value = { x, y, 0.0f, 0.0f };

            return result;
        }

        static MaterialValue MakeFloat3(
            float x, float y, float z)
        {
            MaterialValue result;

            result.type = ValueType::Float3;
            result.value = { x, y, z, 0.0f };

            return result;
        }

        static MaterialValue MakeColor(
            float r, float g, float b, float a = 1.0f)
        {
            MaterialValue result;

            result.type = ValueType::Color;
            result.value = { r, g, b, a };

            return result;
        }

        static float Dot3(
            const std::array<float, 3>& a,
            const std::array<float, 3>& b)
        {
            return
                a[0] * b[0] +
                a[1] * b[1] +
                a[2] * b[2];
        }

        static std::array<float, 3> Normalize3(
            const std::array<float, 3>& v)
        {
            const float lengthSquared = Dot3(v, v);

            if (lengthSquared <= 1e-12f)
                return { 0.0f, 0.0f, 0.0f };

            const float invLength =
                1.0f / std::sqrt(lengthSquared);

            return
            {
                v[0] * invLength,
                v[1] * invLength,
                v[2] * invLength
            };
        }

        static float Saturate(float value)
        {
            return std::clamp(value, 0.0f, 1.0f);
        }

        // std::clamp ist bei lo > hi undefiniert.
        static float SafeClamp(float v, float lo, float hi)
        {
            return std::max(lo, std::min(v, hi));
        }

        static float GetFloat(const MaterialValue& value)
        {
            return value.value[0];
        }

        static std::array<float, 3> GetVector3(
            const MaterialValue& value)
        {
            return
            {
                value.value[0],
                value.value[1],
                value.value[2]
            };
        }

        static bool ConvertValue(
            const MaterialValue& source,
            ValueType target,
            MaterialValue& result)
        {
            return MaterialTypeSystem::Convert(
                source, target, result);
        }

        // ------------------------------------------------------------
        // Eingaben / Ausgaben (Slot-basiert, Typen sind bereits
        // vom Compiler aufgeloest)
        // ------------------------------------------------------------

        static bool GetInput(
            const MaterialInstruction& instruction,
            std::size_t index,
            const Slots slots,
            MaterialValue& result)
        {
            if (index >= instruction.inputs.size())
                return false;

            const auto& input = instruction.inputs[index];

            // Nicht verbunden: bereits konvertierter Default.
            if (input.sourceSlot == INVALID_SLOT)
            {
                result = input.defaultValue;
                return true;
            }

            const MaterialValue& source = slots[input.sourceSlot];

            if (input.conversion == ConversionKind::None)
            {
                result = source;
                return true;
            }

            return MaterialTypeSystem::Apply(
                input.conversion, source, result);
        }

        static bool IsConnected(
            const MaterialInstruction& instruction,
            std::size_t index)
        {
            return index < instruction.inputs.size() &&
                instruction.inputs[index].sourceSlot != INVALID_SLOT;
        }

        // Unverbundener Eingang -> Wert aus dem Kontext.
        static bool GetVector3OrContext(
            const MaterialInstruction& instruction,
            std::size_t index,
            const Slots slots,
            const std::array<float, 3>& fallback,
            MaterialValue& result)
        {
            if (index >= instruction.inputs.size())
                return false;

            if (!IsConnected(instruction, index))
            {
                result = MakeFloat3(fallback[0], fallback[1], fallback[2]);
                return true;
            }

            return GetInput(instruction, index, slots, result);
        }

        static bool StoreOutput(
            const MaterialInstruction& instruction,
            std::size_t index,
            const MaterialValue& value,
            Slots slots)
        {
            if (index >= instruction.outputs.size())
                return false;

            const auto& output = instruction.outputs[index];

            if (value.type == output.resolvedType)
            {
                slots[output.slot] = value;
                return true;
            }

            return ConvertValue(
                value, output.resolvedType, slots[output.slot]);
        }

        // ------------------------------------------------------------
        // Komponentenweise Mathe
        // ------------------------------------------------------------

        static float BinaryOp(NodeType type, float x, float y)
        {
            switch (type)
            {
            case NodeType::Add:      return x + y;
            case NodeType::Subtract: return x - y;
            case NodeType::Multiply: return x * y;
            case NodeType::Divide:
                return std::abs(y) < 1e-12f ? 0.0f : x / y;
            default:                 return 0.0f;
            }
        }

        static bool ExecuteNode(
            const MaterialInstruction& instruction,
            const MaterialEvaluationContext& context,
            Slots slots,
            MaterialSurface& surface,
            bool& hasSurface,
            std::string* error)
        {
            auto fail = [&](const char* message)
                {
                    if (error)
                        *error = message;

                    return false;
                };

            if (instruction.type != NodeType::MaterialOutput &&
                instruction.outputs.empty())
                return fail("Node has no output.");

            MaterialValue a, b, c, d;

            switch (instruction.type)
            {
                // ------------------------------------------------
                // Constant / Parameter
                // ------------------------------------------------
            case NodeType::Constant:
            {
                return StoreOutput(
                    instruction, 0,
                    instruction.outputs[0].defaultValue,
                    slots);
            }

            case NodeType::Parameter:
            {
                const auto& output = instruction.outputs[0];

                MaterialValue value = output.defaultValue;

                if (context.parameters)
                {
                    const MaterialValue* parameter =
                        context.parameters->Find(output.id);

                    if (parameter)
                    {
                        if (!ConvertValue(
                            *parameter,
                            output.resolvedType,
                            value))
                        {
                            return fail("Invalid parameter type.");
                        }
                    }
                }

                return StoreOutput(instruction, 0, value, slots);
            }

            // ------------------------------------------------
            // Vertex / Geometry Inputs
            // ------------------------------------------------

            case NodeType::TextureCoordinate:
            {
                return StoreOutput(
                    instruction, 0,
                    MakeFloat2(context.uv[0], context.uv[1]),
                    slots);
            }

            case NodeType::VertexColor:
            {
                return StoreOutput(
                    instruction, 0,
                    MakeColor(
                        context.vertexColor[0],
                        context.vertexColor[1],
                        context.vertexColor[2],
                        context.vertexColor[3]),
                    slots);
            }

            case NodeType::VertexNormal:
            {
                const auto n = Normalize3(context.vertexNormal);

                return StoreOutput(
                    instruction, 0,
                    MakeFloat3(n[0], n[1], n[2]),
                    slots);
            }

            // ------------------------------------------------
            // Texture Sample
            // ------------------------------------------------

            case NodeType::TextureSample:
            {
                MaterialValue textureValue;
                MaterialValue uvValue;

                if (!GetInput(instruction, 0, slots, textureValue))
                    return fail("TextureSample: Invalid texture input.");

                if (IsConnected(instruction, 1))
                {
                    if (!GetInput(instruction, 1, slots, uvValue))
                        return fail("TextureSample: Invalid UV input.");
                }
                else
                {
                    uvValue = MakeFloat2(context.uv[0], context.uv[1]);
                }

                if (textureValue.type != ValueType::Texture2D)
                    return fail("TextureSample: Input is not a texture.");

                if (uvValue.type != ValueType::Float2)
                    return fail("TextureSample: UV input must be Float2.");

                if (!context.textureSampler)
                    return fail("TextureSample: No texture sampler available.");

                MaterialValue sampled;

                if (!context.textureSampler->Sample(
                    textureValue.texture,
                    uvValue.value[0],
                    uvValue.value[1],
                    sampled))
                {
                    return fail("TextureSample: Texture sampling failed.");
                }

                MaterialValue color;

                if (!ConvertValue(sampled, ValueType::Color, color))
                    return fail("TextureSample: Invalid sampled color.");

                return StoreOutput(instruction, 0, color, slots);
            }

            // ------------------------------------------------
            // Math (komponentenweise, Typ vom Compiler aufgeloest)
            // ------------------------------------------------

            case NodeType::Add:
            case NodeType::Subtract:
            case NodeType::Multiply:
            case NodeType::Divide:
            {
                if (!GetInput(instruction, 0, slots, a) ||
                    !GetInput(instruction, 1, slots, b))
                    return fail("Invalid math input.");

                const ValueType type = instruction.outputs[0].resolvedType;
                const int n = MaterialTypeSystem::MathComponentCount(type);

                // Alpha (Color) bleibt von Operand A erhalten.
                MaterialValue r = a;
                r.type = type;
                r.texture = 0;

                for (int i = 0; i < n; ++i)
                    r.value[i] = BinaryOp(
                        instruction.type, a.value[i], b.value[i]);

                return StoreOutput(instruction, 0, r, slots);
            }

            case NodeType::Lerp:
            {
                if (!GetInput(instruction, 0, slots, a) ||
                    !GetInput(instruction, 1, slots, b) ||
                    !GetInput(instruction, 2, slots, c))
                    return fail("Invalid Lerp input.");

                const ValueType type = instruction.outputs[0].resolvedType;
                const int n = MaterialTypeSystem::MathComponentCount(type);

                MaterialValue r = a;
                r.type = type;
                r.texture = 0;

                for (int i = 0; i < n; ++i)
                    r.value[i] =
                    a.value[i] + (b.value[i] - a.value[i]) * c.value[i];

                return StoreOutput(instruction, 0, r, slots);
            }

            case NodeType::Clamp:
            {
                if (!GetInput(instruction, 0, slots, a) ||
                    !GetInput(instruction, 1, slots, b) ||
                    !GetInput(instruction, 2, slots, c))
                    return fail("Invalid Clamp input.");

                const ValueType type = instruction.outputs[0].resolvedType;
                const int n = MaterialTypeSystem::MathComponentCount(type);

                MaterialValue r = a;
                r.type = type;
                r.texture = 0;

                for (int i = 0; i < n; ++i)
                    r.value[i] = SafeClamp(
                        a.value[i], b.value[i], c.value[i]);

                return StoreOutput(instruction, 0, r, slots);
            }

            case NodeType::Dot:
            {
                if (!GetInput(instruction, 0, slots, a) ||
                    !GetInput(instruction, 1, slots, b))
                    return fail("Invalid Dot input.");

                const int n = MaterialTypeSystem::MathComponentCount(
                    instruction.inputs[0].resolvedType);

                float sum = 0.0f;

                for (int i = 0; i < n; ++i)
                    sum += a.value[i] * b.value[i];

                return StoreOutput(
                    instruction, 0, MakeFloat(sum), slots);
            }

            case NodeType::Normalize:
            {
                if (!GetInput(instruction, 0, slots, a))
                    return fail("Invalid Normalize input.");

                const ValueType type = instruction.outputs[0].resolvedType;
                const int n = MaterialTypeSystem::MathComponentCount(type);

                float lengthSquared = 0.0f;

                for (int i = 0; i < n; ++i)
                    lengthSquared += a.value[i] * a.value[i];

                MaterialValue r = a;
                r.type = type;
                r.texture = 0;

                if (lengthSquared <= 1e-12f)
                {
                    for (int i = 0; i < n; ++i)
                        r.value[i] = 0.0f;
                }
                else
                {
                    const float inv = 1.0f / std::sqrt(lengthSquared);

                    for (int i = 0; i < n; ++i)
                        r.value[i] *= inv;
                }

                return StoreOutput(instruction, 0, r, slots);
            }

            // ------------------------------------------------
            // Split / Combine
            // ------------------------------------------------

            case NodeType::Split:
            {
                if (!GetInput(instruction, 0, slots, a))
                    return fail("Invalid Split input.");

                float comp[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

                switch (a.type)
                {
                case ValueType::Float:
                    comp[0] = comp[1] = comp[2] = a.value[0];
                    break;

                case ValueType::Float2:
                    comp[0] = a.value[0];
                    comp[1] = a.value[1];
                    break;

                case ValueType::Float3:
                    comp[0] = a.value[0];
                    comp[1] = a.value[1];
                    comp[2] = a.value[2];
                    break;

                default: // Float4, Color
                    for (int i = 0; i < 4; ++i)
                        comp[i] = a.value[i];
                    break;
                }

                for (std::size_t i = 0;
                    i < 4 && i < instruction.outputs.size(); ++i)
                {
                    if (!StoreOutput(
                        instruction, i, MakeFloat(comp[i]), slots))
                        return fail("Split: Could not store output.");
                }

                return true;
            }

            case NodeType::Combine:
            {
                if (!GetInput(instruction, 0, slots, a) ||
                    !GetInput(instruction, 1, slots, b) ||
                    !GetInput(instruction, 2, slots, c) ||
                    !GetInput(instruction, 3, slots, d))
                    return fail("Invalid Combine input.");

                return StoreOutput(
                    instruction, 0,
                    MakeColor(
                        GetFloat(a), GetFloat(b),
                        GetFloat(c), GetFloat(d)),
                    slots);
            }

            // ------------------------------------------------
            // Diffuse
            // ------------------------------------------------

            case NodeType::Diffuse:
            {
                if (!GetInput(instruction, 0, slots, a) ||
                    !GetVector3OrContext(instruction, 1, slots,
                        context.vertexNormal, b) ||
                    !GetVector3OrContext(instruction, 2, slots,
                        context.lightDirection, c))
                    return fail("Invalid Diffuse input.");

                const auto normal = Normalize3(GetVector3(b));
                const auto light = Normalize3(GetVector3(c));

                const float intensity =
                    Saturate(Dot3(normal, light));

                return StoreOutput(
                    instruction, 0,
                    MakeColor(
                        a.value[0] * intensity,
                        a.value[1] * intensity,
                        a.value[2] * intensity,
                        a.value[3]),
                    slots);
            }

            // ------------------------------------------------
            // Specular
            // ------------------------------------------------

            case NodeType::Specular:
            {
                MaterialValue viewValue;
                MaterialValue lightValue;
                MaterialValue powerValue;

                if (!GetVector3OrContext(instruction, 0, slots,
                        context.vertexNormal, a) ||
                    !GetVector3OrContext(instruction, 1, slots,
                        context.viewDirection, viewValue) ||
                    !GetVector3OrContext(instruction, 2, slots,
                        context.lightDirection, lightValue) ||
                    !GetInput(instruction, 3, slots, powerValue))
                    return fail("Invalid Specular input.");

                const auto normal = Normalize3(GetVector3(a));
                const auto view = Normalize3(GetVector3(viewValue));
                const auto light = Normalize3(GetVector3(lightValue));

                const std::array<float, 3> halfVector = Normalize3({
                    view[0] + light[0],
                    view[1] + light[1],
                    view[2] + light[2] });

                const float ndoth =
                    Saturate(Dot3(normal, halfVector));

                const float power =
                    std::max(1.0f, GetFloat(powerValue));

                const float intensity = std::pow(ndoth, power);

                return StoreOutput(
                    instruction, 0,
                    MakeColor(intensity, intensity, intensity, 1.0f),
                    slots);
            }

            // ------------------------------------------------
            // Emissive
            // ------------------------------------------------

            case NodeType::Emissive:
            {
                if (!GetInput(instruction, 0, slots, a))
                    return fail("Invalid Emissive input.");

                return StoreOutput(instruction, 0, a, slots);
            }

            // ------------------------------------------------
            // Material Output
            // ------------------------------------------------

            case NodeType::MaterialOutput:
            {
                if (instruction.inputs.size() < 6)
                    return fail("MaterialOutput has invalid inputs.");

                MaterialValue baseColor;
                MaterialValue normal;
                MaterialValue emissive;
                MaterialValue opacity;
                MaterialValue roughness;
                MaterialValue metallic;

                if (!GetInput(instruction, 0, slots, baseColor) ||
                    !GetInput(instruction, 1, slots, normal) ||
                    !GetInput(instruction, 2, slots, emissive) ||
                    !GetInput(instruction, 3, slots, opacity) ||
                    !GetInput(instruction, 4, slots, roughness) ||
                    !GetInput(instruction, 5, slots, metallic))
                    return fail("Could not resolve material output.");

                surface.baseColor = baseColor.value;
                surface.emissive = emissive.value;

                auto n = GetVector3(normal);

                if (Dot3(n, n) <= 1e-12f)
                    n = Normalize3(context.vertexNormal);
                else
                    n = Normalize3(n);

                surface.normal = n;

                surface.opacity = Saturate(GetFloat(opacity));
                surface.roughness = Saturate(GetFloat(roughness));
                surface.metallic = Saturate(GetFloat(metallic));

                hasSurface = true;

                return true;
            }

            default:
                return fail("Unsupported material node type.");
            }
        }

    public:

        // Schnelle Variante: Wertespeicher wird vom Aufrufer gehalten
        // (pro Thread eine MaterialEvaluationScratch-Instanz).
        bool Evaluate(
            const MaterialProgram& program,
            const MaterialEvaluationContext& context,
            MaterialEvaluationScratch& scratch,
            MaterialSurface& surface,
            std::string* error = nullptr) const
        {
            auto fail = [&](const char* message)
                {
                    if (error)
                        *error = message;

                    return false;
                };

            surface = MaterialSurface{};

            if (!program.IsValid())
                return fail("Material program is invalid.");

            const auto& instructions =
                program.GetInstructions();

            const uint32_t outputIndex =
                program.GetOutputInstruction();

            if (instructions[outputIndex].type != NodeType::MaterialOutput)
                return fail("Output instruction is not MaterialOutput.");

            scratch.Prepare(program);

            Slots slots = scratch.Data();

            bool hasSurface = false;

            for (const auto& instruction : instructions)
            {
                if (!ExecuteNode(
                    instruction,
                    context,
                    slots,
                    surface,
                    hasSurface,
                    error))
                {
                    return false;
                }
            }

            if (!hasSurface)
                return fail("Material output was not evaluated.");

            if (error)
                error->clear();

            return true;
        }

        // Bequeme Variante: nutzt einen thread-lokalen Wertespeicher,
        // allokiert also nach dem ersten Aufruf nichts mehr.
        bool Evaluate(
            const MaterialProgram& program,
            const MaterialEvaluationContext& context,
            MaterialSurface& surface,
            std::string* error = nullptr) const
        {
            thread_local MaterialEvaluationScratch scratch;

            return Evaluate(program, context, scratch, surface, error);
        }
    };
}
