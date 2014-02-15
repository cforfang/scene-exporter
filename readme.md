Exports a 3D model/scene using [Assimp](http://github.com/assimp/assimp). Produces a [Lua](http://www.lua.org)-parsable file with mesh and material information and a binary file with vertex- and index-data ready to be loaded directly into VBO/IBO buffers.

It is written assuming a compiler with support for C++11.

Example usage:

1. git submodule init
2. git submodule update
3. use CMake to generate build files
4. compile
5. download e.g. [Crytek Sponza](http://graphics.cs.williams.edu/data/meshes.xml) .obj and .mtl
6. run "SceneExporter sponza.obj" to get scene.lua and meshdata.bin
