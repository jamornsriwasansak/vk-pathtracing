#ifndef BINDLESS_TABLE_H
#define BINDLESS_TABLE_H

#include "types.h"

// In bindless rendering, InstanceId, GeometryId and PrimitiveId are provided.
//
// To reference a geometry information, we need a geometry table.
// The correct geometry index can be referenced as follows:
// GeometryTableEntries[BaseInstanceTableEntries[InstanceId].geometry_entry_offset + GeometryId]
//
// There is a special case: DeviceBaseInstances[0] == 0
// We can use this knowledge to skip one dependency fetch

struct GeometryTableEntry
{
    uint32_t m_vertex_base_idx;
    uint32_t m_index_base_idx;
    uint32_t m_material_idx;
    uint32_t m_padding;
};

struct BaseInstanceTableEntry
{
    uint16_t m_geometry_entry_offset;
};

#endif // BINDLESS_TABLE_H