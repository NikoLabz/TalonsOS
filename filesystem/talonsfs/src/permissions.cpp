#include "../include/talonsfs/permissions.h"

namespace TalonsFS {

namespace {

bool hasPermission(const Node& node, uint8_t bit)
{
    if (node.flags == 0) {
        // Legacy/pre-permissions node: treat as full rwx.
        return true;
    }

    if ((node.flags & PERMISSIONS_INITIALIZED) == 0) {
        // Nonzero flags without PERMISSIONS_INITIALIZED is not a
        // state this scheme produces, but fall back to the same
        // legacy-compatible behavior rather than denying access.
        return true;
    }

    return (node.flags & bit) != 0;
}

}

bool hasReadPermission(const Node& node)
{
    return hasPermission(node, PERMISSION_READ);
}

bool hasWritePermission(const Node& node)
{
    return hasPermission(node, PERMISSION_WRITE);
}

bool hasExecutePermission(const Node& node)
{
    return hasPermission(node, PERMISSION_EXECUTE);
}

}
