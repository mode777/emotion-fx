# Product Vision: EmotionFX - Oldschool 3d-Engine

I want to create an old-school 3d game engine that is capable of producing ps2-era graphics and is super lightweight and easy to use. 


Here are some properties I want in unspecified order:


- Fixed function pipeline - no programmable shaders
- 4 point lights, 1 directional light fixed
- 1 camera fixed
- Support for rendering to textures
- Support for additive and subtractive blending modes
- Support for simple post processing (color filtering, blur)
- Support for skinning and animations
- Support for rendering meshes
- Support for vertex colours
- Simple phong material system: Ambient, Diffuse, Specular, Emissive with support for maps for each of those channels
- Support for alpha masks
- Support for 2d drawing via quads
- Core written in C or C++
- Consumer API written in ECMA Script 6
- Cross platform compatibility for Win, Linux, MacOS, and EMscripten
- Compiles as a single binary
- Brings it’s own ES interpreter (quickjs) on non-browser platforms
- Comes with a emscripten javascript bridge so the browsers native js engine can use the API
- Uses Sokol library for rendering
- Immediate mode api does not mean immediate mode rendering. Rendering should create a display list that can be re-ordered by the renderer later
- Compiled binary is basically a runtime “player” for game resources (includes scripts)
- Compiled binary can be run in a console mode where you can interact with the ES API layer via a REPL like interface


## Consumer API Design
- Consumer API used with in ECMA Script 6
- Easy to use raylib-like immediate-mode API with high level abstractions
- Callbacks for update and rendering
- Low level functions written in C/C++
- Mid level functions implemented in C/C++ and exposed to JS API (e.g. drawQuad, drawMesh, setLight, setMaterial)
- High level functions implemented on top in pure JS (e.g. drawModel, drawText etc…)
- JS functions must not have any dependencies to browser APIs or NodeJS (also not transitively)
- Minimum number of memory managed resources exposed as handles 
- Try to manage as many resources (e.g. materials, meshes) as possible on JS level.
- Overall design must avoid memory leaks caused by using unmanaged resources in a memory-managed language
- Where memory leaks cannot be avoided expose handles on js level (e.g. deleteMaterial(1)) or use a pre-allocated number of resources (e.g. setMesh(0,data); useMesh(0))


## Packaging
- Resources are always loaded from a single folder (desktop only) or zip file
- This serves as resource root similar to godots res://
- Zip files are similar to Löve2Ds .love files
- Root has a main.js file where the runtime will pick-up the hooks


## Development
- Uses CMake as a build system 
- Purely developed by AI agents using OpenCode
- Uses OpenSpec as a SDD framework installed via NPM


## References
- Sokol lib: https://github.com/floooh/sokol
- Sokol samples: https://github.com/floooh/sokol-samples
- Quickjs: https://github.com/bellard/quickjs
- Rayjs - a javascript runtime with ray lib bindings for quickjs - can serve as example for quick js integration and also how to strip down quickjs for cross platform compatibility: https://github.com/mode777/rayjs
- 
