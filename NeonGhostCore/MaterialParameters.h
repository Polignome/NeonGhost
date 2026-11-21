
#pragma once

#include "MaterialTypes.h"

#include <unordered_map>

namespace ng::material
{
    class MaterialParameterSet
    {
    public:

        using ParameterMap =
            std::unordered_map<PinId, MaterialValue>;

        // --------------------------------------------------------
        // Parameter setzen
        // --------------------------------------------------------

        bool Set(PinId id, const MaterialValue& value)
        {
            if (id == 0 || value.type == ValueType::Unknown)
                return false;

            m_values[id] = value;
            return true;
        }

        // --------------------------------------------------------
        // Parameter entfernen
        // --------------------------------------------------------

        bool Remove(PinId id)
        {
            return m_values.erase(id) > 0;
        }

        // --------------------------------------------------------
        // Parameter abfragen
        // --------------------------------------------------------

        const MaterialValue* Find(PinId id) const
        {
            auto it = m_values.find(id);

            if (it == m_values.end())
                return nullptr;

            return &it->second;
        }

        bool Contains(PinId id) const
        {
            return m_values.find(id) != m_values.end();
        }

        void Clear()
        {
            m_values.clear();
        }

        std::size_t Size() const
        {
            return m_values.size();
        }

        const ParameterMap& GetValues() const
        {
            return m_values;
        }

    private:

        ParameterMap m_values;
    };
}

