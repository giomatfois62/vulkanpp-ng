- load gltf material textures
- mesh constructor with vertices + compute AABB
- instances + lights octrees
- frustrum culling
- clustered/deferred shading
- pbr material + rendering
- skybox textures
- improved camera (+orthomode)
- stencil testing
- cascaded shadow maps / SSAO
- threadpool + async load
- vkformat checkers
- glm common include
- fix imgui framedrop
- postprocessing
- animations
- compute shaders
- clustered/deferred shading on gpu
- frustrum culling on gpu
- mesh lod
- collisions/physics
- ecs
- drawIndirect
- raytracing
- procgen
- move loadmodel/loadtexture functions to dedicated modules:
    allocator (create buffer/image)
    device (sampler+submit cmd)
    commandpool (create cmd)
    queue (submit cmd)

DONE
- draw indexed OK
- new pipeline (no renderpass) OK
- dynamic rendering OK
- imgui OK
- pipeline builder OK
- singleton vmaallocator OK
- remove singleton allocator
- use designated initializers OK
- improve image/buffer creation OK
- filename extension OK (texture)
- load texture from ktx OK
- texture (descriptors) OK
- camera (shader uniform data or pushconst) OK
- instanced rendering OK
- multitexture (bindless) OK
- pointlights, dirlights, spotlights OK
- load gltf mesh OK
- offscreen rendering OK
- remove bitangent OK
