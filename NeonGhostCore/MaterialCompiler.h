
#pragma once

#include "MaterialGraph.h"
#include "MaterialProgram.h"
#include "MaterialTypeSystem.h"

#include <algorithm>
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ng::material
{
    class MaterialCompiler
    {
    private:

        struct OutputInfo
        {
            uint32_t instruction = INVALID_INSTRUCTION;
            uint32_t slot = INVALID_SLOT;
            ValueType type = ValueType::Unknown;
        };

        static const char* TypeName(ValueType type)
        {
            switch (type)
            {
            case ValueType::Float:     return "Float";
            case ValueType::Float2:    return "Float2";
            case ValueType::Float3:    return "Float3";
            case ValueType::Float4:    return "Float4";
            case ValueType::Color:     return "Color";
            case ValueType::Boolean:   return "Boolean";
            case ValueType::Texture2D: return "Texture2D";
            case ValueType::Generic:   return "Generic";
            default:                   return "Unknown";
            }
        }

    public:

        bool Compile(
            const MaterialGraph& graph,
            MaterialProgram& program,
            std::string* error = nullptr) const
        {
            auto fail = [&](const std::string& message)
                {
                    program.Clear();

                    if (error)
                        *error = message;

                    return false;
                };

            program.Clear();

            // ----------------------------------------------------
            // Graph validieren
            // ----------------------------------------------------

            std::string validationError;

            if (!graph.Validate(&validationError))
                return fail(validationError);

            const NodeId outputNodeId = graph.GetOutputNode();

            if (outputNodeId == 0)
                return fail("Material has no output node.");

            const MaterialNode* outputNode =
                graph.FindNode(outputNodeId);

            if (!outputNode)
                return fail("Material output node does not exist.");

            if (outputNode->type != NodeType::MaterialOutput)
                return fail("Invalid material output node.");

            // ----------------------------------------------------
            // Verbindungen nach Ziel-Pin indizieren
            // ----------------------------------------------------

            std::unordered_map<PinId, const MaterialConnection*>
                connectionByTarget;

            for (const auto& connection : graph.GetConnections())
            {
                if (!connectionByTarget.emplace(
                    connection.targetPin,
                    &connection).second)
                {
                    return fail("Multiple connections found for an input pin.");
                }
            }

            // ----------------------------------------------------
            // Abhaengigkeiten sammeln und topologisch sortieren
            // ----------------------------------------------------

            std::vector<const MaterialNode*> sortedNodes;

            std::unordered_set<NodeId> visited;
            std::unordered_set<NodeId> visiting;

            std::function<bool(const MaterialNode*)> visit;

            visit = [&](const MaterialNode* node) -> bool
                {
                    if (!node)
                        return false;

                    if (visited.find(node->id) != visited.end())
                        return true;

                    if (visiting.find(node->id) != visiting.end())
                        return false;

                    visiting.insert(node->id);

                    for (const auto& input : node->inputs)
                    {
                        auto it = connectionByTarget.find(input.id);

                        if (it == connectionByTarget.end())
                            continue;

                        const MaterialNode* sourceNode =
                            graph.FindNode(it->second->sourceNode);

                        if (!sourceNode || !visit(sourceNode))
                            return false;
                    }

                    visiting.erase(node->id);
                    visited.insert(node->id);

                    sortedNodes.push_back(node);

                    return true;
                };

            if (!visit(outputNode))
                return fail("Could not sort material dependencies.");

            // ----------------------------------------------------
            // Programm erzeugen: Typen aufloesen, Slots vergeben
            // ----------------------------------------------------

            std::unordered_map<PinId, OutputInfo> outputInfo;
            uint32_t nextSlot = 0;

            program.m_instructions.reserve(sortedNodes.size());

            for (const MaterialNode* node : sortedNodes)
            {
                const uint32_t instructionIndex =
                    static_cast<uint32_t>(program.m_instructions.size());

                MaterialInstruction instruction;
                instruction.nodeId = node->id;
                instruction.type = node->type;

                const std::string where =
                    "Node '" + node->name + "'";

                // ------------------------------------------------
                // 1) Quelltypen sammeln und Generic aufloesen
                // ------------------------------------------------

                const std::size_t inputCount = node->inputs.size();

                std::vector<MaterialValue> defaults(inputCount);
                std::vector<ValueType> sourceTypes(inputCount);
                std::vector<const OutputInfo*> sources(inputCount, nullptr);

                bool hasGenericInput = false;
                ValueType domain = ValueType::Unknown;

                for (std::size_t i = 0; i < inputCount; ++i)
                {
                    const MaterialPin& input = node->inputs[i];

                    MaterialValue def = input.defaultValue;

                    if (def.type == ValueType::Unknown)
                        def.type = (input.type == ValueType::Generic)
                        ? ValueType::Float
                        : input.type;

                    defaults[i] = def;

                    auto connectionIt = connectionByTarget.find(input.id);

                    if (connectionIt != connectionByTarget.end())
                    {
                        auto infoIt = outputInfo.find(
                            connectionIt->second->sourcePin);

                        if (infoIt == outputInfo.end())
                            return fail("Source instruction was not compiled.");

                        sources[i] = &infoIt->second;
                        sourceTypes[i] = infoIt->second.type;
                    }
                    else
                    {
                        sourceTypes[i] = def.type;
                    }

                    if (input.type == ValueType::Generic)
                    {
                        if (!MaterialTypeSystem::IsNumeric(sourceTypes[i]))
                        {
                            return fail(where + ", input '" + input.name +
                                "': " + TypeName(sourceTypes[i]) +
                                " is not a numeric type.");
                        }

                        if (!hasGenericInput)
                        {
                            domain = sourceTypes[i];
                            hasGenericInput = true;
                        }
                        else
                        {
                            ValueType common;

                            if (!MaterialTypeSystem::ResolveCommonType(
                                domain, sourceTypes[i], common))
                            {
                                return fail(where + ", input '" + input.name +
                                    "': " + TypeName(sourceTypes[i]) +
                                    " cannot be combined with " +
                                    TypeName(domain) + ".");
                            }

                            domain = common;
                        }
                    }
                }

                // ------------------------------------------------
                // 2) Eingaben binden
                // ------------------------------------------------

                instruction.inputs.reserve(inputCount);

                for (std::size_t i = 0; i < inputCount; ++i)
                {
                    const MaterialPin& input = node->inputs[i];

                    MaterialInputBinding binding;
                    binding.targetPin = input.id;
                    binding.type = input.type;
                    binding.resolvedType =
                        (input.type == ValueType::Generic)
                        ? domain
                        : input.type;

                    if (sources[i])
                    {
                        const MaterialConnection& connection =
                            *connectionByTarget.find(input.id)->second;

                        binding.sourceInstruction = sources[i]->instruction;
                        binding.sourceSlot = sources[i]->slot;
                        binding.sourcePin = connection.sourcePin;

                        if (!MaterialTypeSystem::GetConversion(
                            sourceTypes[i],
                            binding.resolvedType,
                            binding.conversion))
                        {
                            return fail(where + ", input '" + input.name +
                                "': cannot convert " +
                                TypeName(sourceTypes[i]) + " to " +
                                TypeName(binding.resolvedType) + ".");
                        }
                    }
                    else
                    {
                        // Default einmalig zur Compile-Zeit konvertieren.
                        if (!MaterialTypeSystem::Convert(
                            defaults[i],
                            binding.resolvedType,
                            binding.defaultValue))
                        {
                            return fail(where + ", input '" + input.name +
                                "': default value of type " +
                                TypeName(defaults[i].type) +
                                " cannot be converted to " +
                                TypeName(binding.resolvedType) + ".");
                        }
                    }

                    instruction.inputs.push_back(std::move(binding));
                }

                // ------------------------------------------------
                // 3) Ausgaben binden, Slots vergeben
                // ------------------------------------------------

                instruction.outputs.reserve(node->outputs.size());

                for (const auto& output : node->outputs)
                {
                    MaterialOutputBinding binding;
                    binding.id = output.id;
                    binding.type = output.type;
                    binding.defaultValue = output.defaultValue;

                    if (binding.defaultValue.type == ValueType::Unknown)
                        binding.defaultValue.type =
                        (output.type == ValueType::Generic)
                        ? ValueType::Float
                        : output.type;

                    if (output.type == ValueType::Generic)
                    {
                        // Aus den Eingaben abgeleitet; ohne generische
                        // Eingaben (Constant, Parameter) aus dem Default.
                        binding.resolvedType = hasGenericInput
                            ? domain
                            : binding.defaultValue.type;

                        if (!MaterialTypeSystem::IsNumeric(
                            binding.resolvedType))
                        {
                            return fail(where + ", output '" + output.name +
                                "': value must be numeric.");
                        }
                    }
                    else
                    {
                        binding.resolvedType = output.type;
                    }

                    binding.slot = nextSlot++;

                    OutputInfo info;
                    info.instruction = instructionIndex;
                    info.slot = binding.slot;
                    info.type = binding.resolvedType;

                    outputInfo[output.id] = info;

                    instruction.outputs.push_back(std::move(binding));
                }

                program.m_instructions.push_back(std::move(instruction));
            }

            // ----------------------------------------------------
            // Output-Index speichern
            // ----------------------------------------------------

            // Der Output-Node haengt von allen anderen ab und wird
            // deshalb als letzter Node ausgegeben.
            if (program.m_instructions.empty() ||
                program.m_instructions.back().nodeId != outputNodeId)
                return fail("Material output was not compiled.");

            program.m_outputInstruction =
                static_cast<uint32_t>(program.m_instructions.size() - 1);

            program.m_slotCount = nextSlot;

            if (error)
                error->clear();

            return true;
        }
    };
}
