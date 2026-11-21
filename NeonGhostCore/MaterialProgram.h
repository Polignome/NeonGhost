
#pragma once

#include "MaterialTypes.h"

#include <cstdint>
#include <vector>
#include <limits>

namespace ng::material
{
    constexpr uint32_t INVALID_INSTRUCTION =
        std::numeric_limits<uint32_t>::max();

    constexpr uint32_t INVALID_SLOT =
        std::numeric_limits<uint32_t>::max();

    struct MaterialInputBinding
    {
        PinId targetPin = 0;

        // Deklarierter Pin-Typ (kann Generic sein).
        ValueType type = ValueType::Unknown;

        // Vom Compiler aufgeloester, konkreter Typ, in dem der Node
        // den Wert erhaelt.
        ValueType resolvedType = ValueType::Unknown;

        uint32_t sourceInstruction = INVALID_INSTRUCTION;

        // Slot, aus dem der Wert gelesen wird. INVALID_SLOT = nicht
        // verbunden -> defaultValue (bereits auf resolvedType konvertiert).
        uint32_t sourceSlot = INVALID_SLOT;

        PinId sourcePin = 0;

        MaterialValue defaultValue;

        // Konvertierung Quelltyp -> resolvedType (zur Compile-Zeit bestimmt).
        ConversionKind conversion = ConversionKind::None;
    };

    struct MaterialOutputBinding
    {
        PinId id = 0;
        MaterialValue defaultValue;

        // Deklarierter Pin-Typ (kann Generic sein).
        ValueType type = ValueType::Unknown;

        // Konkreter Typ des gespeicherten Werts.
        ValueType resolvedType = ValueType::Unknown;

        // Index im Wertespeicher (siehe MaterialProgram::GetSlotCount()).
        uint32_t slot = INVALID_SLOT;
    };

    struct MaterialInstruction
    {
        NodeId nodeId = 0;
        NodeType type = NodeType::Constant;

        std::vector<MaterialOutputBinding> outputs;
        std::vector<MaterialInputBinding> inputs;
    };

    class MaterialProgram
    {
    public:

        void Clear()
        {
            m_instructions.clear();
            m_outputInstruction = INVALID_INSTRUCTION;
            m_slotCount = 0;
        }

        // Anzahl der Werte-Slots, die der Evaluator benoetigt.
        uint32_t GetSlotCount() const
        {
            return m_slotCount;
        }

        const std::vector<MaterialInstruction>& GetInstructions() const
        {
            return m_instructions;
        }

        uint32_t GetOutputInstruction() const
        {
            return m_outputInstruction;
        }

        bool IsValid() const
        {
            return m_outputInstruction != INVALID_INSTRUCTION &&
                m_outputInstruction < m_instructions.size();
        }

        std::size_t GetInstructionCount() const
        {
            return m_instructions.size();
        }

    private:

        friend class MaterialCompiler;

        std::vector<MaterialInstruction> m_instructions;

        uint32_t m_outputInstruction = INVALID_INSTRUCTION;

        uint32_t m_slotCount = 0;
    };
}

