#pragma once

struct EngineSetting
{
    static const uint32_t MaxNumBindlessTextures         = 100;
    static const uint32_t MaxNumStandardMaterials        = 1000;
    static const uint32_t MaxNumVertices                 = 9000000;
    static const uint32_t MaxNumIndices                  = 27000000;
    static const uint32_t MaxNumGeometryOffsetTableEntry = 14000;
    static const uint32_t MaxNumGeometryTableEntry       = 32000;
};