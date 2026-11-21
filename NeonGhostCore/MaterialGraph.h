
#pragma once

#include "MaterialTypes.h"

#include <algorithm>
#include <functional>
#include <string>
#include <unordered_set>
#include <vector>
#include <utility>
#include "MaterialNodeRegistry.h"
#include "MaterialTypeSystem.h"




namespace ng::material
{
    class MaterialGraph
    {
    public:

        using NodeList = std::vector<MaterialNode>;
        using ConnectionList = std::vector<MaterialConnection>;

    private:

        std::string m_name = "Material";

        NodeList m_nodes;
        ConnectionList m_connections;

        NodeId m_outputNode = 0;

        NodeId m_nextNodeId = 1;
        PinId m_nextPinId = 1;
        ConnectionId m_nextConnectionId = 1;

    private:

        const MaterialNodeRegistry* m_registry = nullptr;

        // --------------------------------------------------------
        // ID generation
        // --------------------------------------------------------

        NodeId GenerateNodeId()
        {
            return m_nextNodeId++;
        }

        PinId GeneratePinId()
        {
            return m_nextPinId++;
        }

        ConnectionId GenerateConnectionId()
        {
            return m_nextConnectionId++;
        }

        

        // --------------------------------------------------------
        // Pin lookup
        // --------------------------------------------------------

        MaterialPin* FindPinInternal(PinId id)
        {
            for (auto& node : m_nodes)
            {
                for (auto& pin : node.inputs)
                    if (pin.id == id)
                        return &pin;

                for (auto& pin : node.outputs)
                    if (pin.id == id)
                        return &pin;
            }

            return nullptr;
        }

        const MaterialPin* FindPinInternal(PinId id) const
        {
            for (const auto& node : m_nodes)
            {
                for (const auto& pin : node.inputs)
                    if (pin.id == id)
                        return &pin;

                for (const auto& pin : node.outputs)
                    if (pin.id == id)
                        return &pin;
            }

            return nullptr;
        }

        NodeId FindPinOwner(PinId id) const
        {
            for (const auto& node : m_nodes)
            {
                for (const auto& pin : node.inputs)
                    if (pin.id == id)
                        return node.id;

                for (const auto& pin : node.outputs)
                    if (pin.id == id)
                        return node.id;
            }

            return 0;
        }

        // --------------------------------------------------------
        // Cycle detection
        // --------------------------------------------------------

        bool HasPath(NodeId from, NodeId to) const
        {
            std::unordered_set<NodeId> visited;

            std::function<bool(NodeId)> visit =
                [&](NodeId current) -> bool
                {
                    if (current == to)
                        return true;

                    if (!visited.insert(current).second)
                        return false;

                    for (const auto& connection : m_connections)
                    {
                        if (connection.sourceNode != current)
                            continue;

                        if (visit(connection.targetNode))
                            return true;
                    }

                    return false;
                };

            return visit(from);
        }

    public:

        // --------------------------------------------------------
        // Construction
        // --------------------------------------------------------
        explicit MaterialGraph(
            const MaterialNodeRegistry& registry,
            std::string name = "Material")
            : m_name(std::move(name)),
            m_registry(&registry)
        {
        }

      
        // --------------------------------------------------------
        // Properties
        // --------------------------------------------------------

        const std::string& GetName() const
        {
            return m_name;
        }

        void SetName(const std::string& name)
        {
            m_name = name;
        }

        NodeId GetOutputNode() const
        {
            return m_outputNode;
        }

        bool SetOutputNode(NodeId id)
        {
            auto* node = FindNode(id);

            if (!node || node->type != NodeType::MaterialOutput)
                return false;

            m_outputNode = id;
            return true;
        }

        // --------------------------------------------------------
        // Nodes
        // --------------------------------------------------------

        NodeId AddNode(
            NodeType type,
            const std::string& name = "")
        {
            if (!m_registry)
                return 0;

            const auto* definition = m_registry->Find(type);

            if (!definition)
                return 0;

            // Check whether multiple instances are allowed.
            if (!definition->allowMultipleInstances)
            {
                for (const auto& existing : m_nodes)
                {
                    if (existing.type == type)
                        return 0;
                }
            }

            MaterialNode node;

            node.id = GenerateNodeId();
            node.type = type;

            node.name = name.empty()
                ? definition->name
                : name;

            // Create input pins.
            for (const auto& pinDefinition : definition->inputs)
            {
                MaterialPin pin;

                pin.id = GeneratePinId();
                pin.name = pinDefinition.name;
                pin.type = pinDefinition.type;
                pin.direction = PinDirection::Input;
                pin.defaultValue = pinDefinition.defaultValue;

                if (pin.defaultValue.type == ValueType::Unknown)
                    pin.defaultValue.type =
                        (pin.type == ValueType::Generic)
                        ? ValueType::Float
                        : pin.type;

                node.inputs.push_back(std::move(pin));
            }

            // Create output pins.
            for (const auto& pinDefinition : definition->outputs)
            {
                MaterialPin pin;

                pin.id = GeneratePinId();
                pin.name = pinDefinition.name;
                pin.type = pinDefinition.type;
                pin.direction = PinDirection::Output;
                pin.defaultValue = pinDefinition.defaultValue;

                if (pin.defaultValue.type == ValueType::Unknown)
                    pin.defaultValue.type =
                        (pin.type == ValueType::Generic)
                        ? ValueType::Float
                        : pin.type;

                node.outputs.push_back(std::move(pin));
            }

            NodeId id = node.id;

            m_nodes.push_back(std::move(node));

            if (type == NodeType::MaterialOutput &&
                m_outputNode == 0)
            {
                m_outputNode = id;
            }

            return id;
        }

        bool RemoveNode(NodeId id)
        {
            auto it = std::find_if(
                m_nodes.begin(),
                m_nodes.end(),
                [id](const MaterialNode& node)
                {
                    return node.id == id;
                });

            if (it == m_nodes.end())
                return false;

            m_connections.erase(
                std::remove_if(
                    m_connections.begin(),
                    m_connections.end(),
                    [id](const MaterialConnection& connection)
                    {
                        return connection.sourceNode == id ||
                            connection.targetNode == id;
                    }),
                m_connections.end());

            if (m_outputNode == id)
                m_outputNode = 0;

            m_nodes.erase(it);

            return true;
        }

        MaterialNode* FindNode(NodeId id)
        {
            for (auto& node : m_nodes)
                if (node.id == id)
                    return &node;

            return nullptr;
        }

        const MaterialNode* FindNode(NodeId id) const
        {
            for (const auto& node : m_nodes)
                if (node.id == id)
                    return &node;

            return nullptr;
        }

        const NodeList& GetNodes() const
        {
            return m_nodes;
        }

        // --------------------------------------------------------
        // Pins
        // --------------------------------------------------------

        MaterialPin* FindPin(PinId id)
        {
            return FindPinInternal(id);
        }

        const MaterialPin* FindPin(PinId id) const
        {
            return FindPinInternal(id);
        }

        MaterialPin* FindInput(NodeId nodeId, const std::string& name)
        {
            auto* node = FindNode(nodeId);

            if (!node)
                return nullptr;

            for (auto& pin : node->inputs)
                if (pin.name == name)
                    return &pin;

            return nullptr;
        }

        MaterialPin* FindOutput(NodeId nodeId, const std::string& name)
        {
            auto* node = FindNode(nodeId);

            if (!node)
                return nullptr;

            for (auto& pin : node->outputs)
                if (pin.name == name)
                    return &pin;

            return nullptr;
        }

        // --------------------------------------------------------
        // Connections
        // --------------------------------------------------------


        ConnectionId Connect(
            PinId sourcePinId,
            PinId targetPinId,
            std::string* error = nullptr)
        {
            auto fail = [&](const char* message) -> ConnectionId
                {
                    if (error)
                        *error = message;

                    return 0;
                };

            const auto* source = FindPinInternal(sourcePinId);
            const auto* target = FindPinInternal(targetPinId);

            if (!source || !target)
                return fail("Pin does not exist.");

            if (source->direction != PinDirection::Output)
                return fail("Source pin must be an output.");

            if (target->direction != PinDirection::Input)
                return fail("Target pin must be an input.");

            NodeId sourceNode = FindPinOwner(sourcePinId);
            NodeId targetNode = FindPinOwner(targetPinId);

            if (sourceNode == 0 || targetNode == 0)
                return fail("Pin owner does not exist.");

            if (sourceNode == targetNode)
                return fail("Cannot connect a node to itself.");

            // ------------------------------------------------------------
            // Typprüfung und automatische Konvertierung
            // ------------------------------------------------------------

            ConversionKind conversion = ConversionKind::None;

            if (!MaterialTypeSystem::GetConversion(
                source->type,
                target->type,
                conversion))
            {
                return fail("Incompatible pin types.");
            }

            // ------------------------------------------------------------
            // Vorhandene Verbindungen prüfen
            // ------------------------------------------------------------

            for (const auto& existing : m_connections)
            {
                if (existing.sourcePin == sourcePinId &&
                    existing.targetPin == targetPinId)
                {
                    return fail("Connection already exists.");
                }

                if (existing.targetPin == targetPinId)
                    return fail("Input pin already connected.");
            }

            // ------------------------------------------------------------
            // Zyklische Abhängigkeiten verhindern
            // ------------------------------------------------------------

            if (HasPath(targetNode, sourceNode))
                return fail("Connection would create a cycle.");

            // ------------------------------------------------------------
            // Verbindung erstellen
            // ------------------------------------------------------------

            MaterialConnection connection;

            connection.id = GenerateConnectionId();

            connection.sourceNode = sourceNode;
            connection.sourcePin = sourcePinId;

            connection.targetNode = targetNode;
            connection.targetPin = targetPinId;

            // Automatische Typkonvertierung speichern
            connection.conversion = conversion;

            const ConnectionId id = connection.id;

            m_connections.push_back(connection);

            if (error)
                error->clear();

            return id;
        }

        bool Disconnect(ConnectionId id)
        {
            auto it = std::find_if(
                m_connections.begin(),
                m_connections.end(),
                [id](const MaterialConnection& connection)
                {
                    return connection.id == id;
                });

            if (it == m_connections.end())
                return false;

            m_connections.erase(it);

            return true;
        }

        bool DisconnectInput(PinId inputPin)
        {
            auto it = std::find_if(
                m_connections.begin(),
                m_connections.end(),
                [inputPin](const MaterialConnection& connection)
                {
                    return connection.targetPin == inputPin;
                });

            if (it == m_connections.end())
                return false;

            m_connections.erase(it);

            return true;
        }

        const MaterialConnection* FindConnection(
            PinId inputPin) const
        {
            for (const auto& connection : m_connections)
                if (connection.targetPin == inputPin)
                    return &connection;

            return nullptr;
        }

        const ConnectionList& GetConnections() const
        {
            return m_connections;
        }

        // --------------------------------------------------------
        // Input values
        // --------------------------------------------------------

        bool SetDefaultValue(
            PinId pinId,
            const MaterialValue& value)
        {
            auto* pin = FindPinInternal(pinId);

            if (!pin)
                return false;

            // Output pins may only be edited on nodes whose value
            // is defined by their output (Constant, Parameter).
            if (pin->direction == PinDirection::Output)
            {
                const NodeId ownerId = FindPinOwner(pinId);
                const MaterialNode* owner = FindNode(ownerId);

                if (!owner ||
                    (owner->type != NodeType::Constant &&
                     owner->type != NodeType::Parameter))
                    return false;
            }

            if (pin->type == ValueType::Generic)
            {
                // Generic-Pins akzeptieren jeden numerischen Typ.
                if (!MaterialTypeSystem::IsNumeric(value.type))
                    return false;
            }
            else if (pin->type != value.type)
            {
                return false;
            }

            pin->defaultValue = value;

            return true;
        }

        // --------------------------------------------------------
        // Graph validation
        // --------------------------------------------------------

        bool Validate(std::string* error = nullptr) const
        {
            auto fail = [&](const char* message)
                {
                    if (error)
                        *error = message;

                    return false;
                };

            if (m_outputNode == 0)
                return fail("Material output node is missing.");

            const auto* output = FindNode(m_outputNode);

            if (!output || output->type != NodeType::MaterialOutput)
                return fail("Invalid material output node.");

            std::unordered_set<ConnectionId> connectionIds;
            std::unordered_set<PinId> connectedInputs;

            for (const auto& connection : m_connections)
            {
                if (!connectionIds.insert(connection.id).second)
                    return fail("Duplicate connection ID.");

                const auto* source = FindPinInternal(connection.sourcePin);
                const auto* target = FindPinInternal(connection.targetPin);

                if (!source || !target)
                    return fail("Connection references invalid pin.");

                if (source->direction != PinDirection::Output ||
                    target->direction != PinDirection::Input)
                    return fail("Invalid connection direction.");

                if (!MaterialTypeSystem::CanConvert(source->type, target->type))
                    return fail("Connection type mismatch.");

                if (!connectedInputs.insert(connection.targetPin).second)
                    return fail("Input has multiple connections.");
            }

            if (error)
                error->clear();

            return true;
        }

        // --------------------------------------------------------
        // Reset
        // --------------------------------------------------------

        void Clear()
        {
            m_nodes.clear();
            m_connections.clear();

            m_outputNode = 0;

            m_nextNodeId = 1;
            m_nextPinId = 1;
            m_nextConnectionId = 1;
        }
    };
}

