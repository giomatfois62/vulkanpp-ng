#ifndef VK_GEOMETRY_HPP
#define VK_GEOMETRY_HPP

#include "vk_mesh.hpp"

namespace vke {

GeometryData createTriangle();
GeometryData createScreenQuad();
GeometryData createCube();
GeometryData createSphere(float radius, int sectors, int stacks);
GeometryData createCylinder(float radius, float height, int sectors);
GeometryData createCone(float baseRadius, float height, int sectors, int stacks);
GeometryData createTorus(float majorRadius, float minorRadius, int sectors, int sides);

} // end namespace vke

#endif // VK_GEOMETRY_HPP
