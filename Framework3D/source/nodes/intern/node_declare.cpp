#include "Nodes/node_declare.hpp"

#include "Nodes/all_socket_types.hpp"
#include "Nodes/node.hpp"
#include "Nodes/socket_types/geo_socket_types.hpp"
#include "Nodes/socket_types/render_socket_types.hpp"
#include "Nodes/socket_types/stage_socket_types.hpp"

USTC_CG_NAMESPACE_OPEN_SCOPE

void decl::Int::update_default_value(NodeSocket* socket) const
{
    if (!socket->dataField.default_value) {
        socket->dataField.min = soft_min;
        socket->dataField.max = soft_max;
        socket->dataField.default_value =
            std::max(std::min(default_value_, soft_max), soft_min);
    }
}

void decl::Float::update_default_value(NodeSocket* socket) const
{
    if (!socket->dataField.default_value) {
        // TODO: When shall we destroy these?
        auto* default_value = &(socket->dataField);
        default_value->min = soft_min;
        default_value->max = soft_max;
        default_value->default_value =
            std::max(std::min(default_value_, soft_max), soft_min);
    }
}

void decl::String::update_default_value(NodeSocket* socket) const
{
    if (!socket->dataField.default_value) {
        socket->dataField.default_value = std::string(256, 0);
        memcpy(
            socket->dataField.default_value.cast<std::string&>().data(),
            default_value_.data(),
            default_value_.size());
    }
}

#define BUILD_TYPE(NAME)                                             \
    NodeSocket* decl::NAME::build(NodeTree* ntree, Node* node) const \
    {                                                                \
        NodeSocket* socket = nodeAddSocket(                          \
            ntree,                                                   \
            node,                                                    \
            this->in_out,                                            \
            get_socket_typename(SocketType::NAME),                   \
            this->identifier.c_str(),                                \
            this->name.c_str());                                     \
        update_default_value(socket);                                \
                                                                     \
        return socket;                                               \
    }

MACRO_MAP(BUILD_TYPE, ALL_SOCKET_TYPES)

NodeDeclarationBuilder::NodeDeclarationBuilder(
    NodeDeclaration& declaration,
    const NodeTree* ntree,
    const Node* node)
    : declaration_(declaration),
      ntree_(ntree),
      node_(node)
{
}

void NodeDeclarationBuilder::finalize()
{
}

USTC_CG_NAMESPACE_CLOSE_SCOPE
