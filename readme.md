Exports a 3D model/scene using [Assimp](http://github.com/assimp/assimp). Produces a [Lua](http://www.lua.org)-parsable file with mesh and material information and a binary file with vertex- and index-data ready to be loaded directly into VBO/IBO buffers.

It is written assuming a compiler with support for C++11.

Example usage:
- git submodule init
- git submodule update
- use CMake to generate build files
- compile
- download [Crytek Sponza](http://graphics.cs.williams.edu/data/meshes.xml) .obj and .mtl
- run "SceneExporter sponza.obj" to get scene.lua and meshdata.bin