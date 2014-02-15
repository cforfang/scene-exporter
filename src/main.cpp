#include <iostream>
#include <vector>

#include "Exporter.hpp"

int main(int argc, char* argv[])
{
	if (argc < 2)
	{
		std::cout << "Usage: <file>\n";
		return 0;
	}

	std::string file{ argv[1] };

	if (!ModelExporter::Export(file, ModelExporter::ExportOptions::MeshesCentered))
		return 1;

	return 0;
}