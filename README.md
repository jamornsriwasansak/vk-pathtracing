# Mortar - A fast (enough) physically based renderer

Mortar is a small physically based renderer with agnostic api. It can use either Vulkan or DX12 as a backend. One of it abilities is to utilizes ray-tracing api to generate the following images:
![The Toilet Scene](https://raw.githubusercontent.com/jamornsriwasansak/vulkan-mortar/master/readme/toilet.jpg)
![Fireplace Room](https://raw.githubusercontent.com/jamornsriwasansak/vulkan-mortar/master/readme/fireplace.jpg)
> Note: These images are generated by vulkan path tracing code that is now [deprecated](https://github.com/jamornsriwasansak/mortar/tree/master/deprecated-vk-pt-src).

### Input (Deprecated)
At this moment, the application accepts OBJ format for loading models and HDR format for environment map.
It guesses the ggx parameters from MTL file provided along with OBJ.
Roughness values are mapped from specular exponents (Ns values) using Blinn-Phong to microfacet mapping mentioned in [Brian Karis' blog](http://graphicrants.blogspot.com/2013/08/specular-brdf-reference.html).

As I do not have much time at this moment, changing the scene requires you to modify the code by yourself.
An example is in "src/main.cpp"

### Features (Deprecated)
In its current early stage, it supports:
* Ambient occlusion Integrator
* Path tracer with next event estimation utilizing MIS [Veach's thesis](https://graphics.stanford.edu/papers/veach_thesis) \[1997]
* Environment map importance sampling
* Alpha-testing
* GPU-based Low discreprancy sampler with blue-noise property ported from [Heitz et al.](https://eheitzresearch.wordpress.com/762-2/) \[2019]
* GPU-based PCG random number generator ported from [pcg-random.org](https://www.pcg-random.org/)
* Lambert diffuse BRDF
* GGX Microfacet BSDF (rough conductor and rough dielectric)
* FPS-style camera for traversing the scene

### Dependencies
The following dependencies aside from vulkan sdk are already included as submodules
* vulkan sdk >= 1.2
* D3D12MemoryAllocator
* DirectX-Headers
* DirectXShaderCompiler
* SPIRV-Reflect
* VulkanMemoryAllocator
* assimp
* glfw
* glm
* spdlog
* stb
