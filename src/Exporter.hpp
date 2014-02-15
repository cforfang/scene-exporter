#pragma once

#include "ExportedVertexFormat.h"
#include <string>

namespace ModelExporter
{
		enum class ExportOptions { MeshesCentered, MeshesNotCentered };
		extern bool Export(const std::string& file, ExportOptions options);
}