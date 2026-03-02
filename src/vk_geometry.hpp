#ifndef VK_GEOMETRY_HPP
#define VK_GEOMETRY_HPP

#include "vk_mesh.hpp"

namespace vke {

Mesh createTriangle();

Mesh createScreenQuad();

Mesh createCube();

Mesh createSphere(float radius, int sectors, int stacks);

Mesh createCylinder(float radius, float height, int sectors);

Mesh createCone(float baseRadius, float height, int sectors, int stacks);

Mesh createTorus(float majorRadius, float minorRadius, int sectors, int sides);

} // end namespace vke

#endif // VK_GEOMETRY_HPP
