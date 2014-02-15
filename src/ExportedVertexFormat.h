#pragma once

#include <stdint.h>
#include <array>

// The exported vertex format.
// Update aiVertexToExportVertex(..) on changes
struct ExportVertex
{
	std::array<float, 3> position;
	std::array<float, 2> uv;
	std::array<float, 3> normal;
	std::array<float, 3> tangent;
	std::array<float, 3> bitangent;
};

using IndexType = uint32_t;