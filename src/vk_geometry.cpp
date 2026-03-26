#include "vk_geometry.hpp"

using namespace vke;

GeometryData vke::createTriangle()
{
    std::vector<Vertex> vertices = {
        {{-1,-1,0}, {0,0,1}, {}, {0,0}, {1,0,0}},
        {{1,-1,0},  {0,0,1}, {}, {0,0}, {0,1,0}},
        {{0,1,0},   {0,0,1}, {}, {0,0}, {0,0,1}},
    };

    std::vector<uint32_t> indices = {0, 1, 2};

    return { vertices, indices };
}

GeometryData vke::createScreenQuad()
{
    std::vector<Vertex> vertices = {
        {{-1,-1,0}, {0,0,1}, {}, {0,0}, {1,0,0}},
        {{1,-1,0},  {0,0,1}, {}, {1,0}, {0,1,0}},
        {{1,1,0},   {0,0,1}, {}, {1,1}, {0,0,1}},
        {{-1,1,0},  {0,0,1}, {}, {0,1}, {1,0,0}},
    };

    std::vector<uint32_t> indices = {0, 1, 2, 2, 3, 0};

    return { vertices, indices };
}

GeometryData vke::createCube()
{
    std::vector<Vertex> vertices = {
        {{-.5,-.5,.5},  {0,0,1}, {},  {0,0}},
        {{.5,-.5,.5},   {0,0,1}, {},  {1,0}},
        {{.5,.5,.5},    {0,0,1}, {},  {1,1}},
        {{-.5,.5,.5},   {0,0,1}, {},  {0,1}},

        {{-.5,-.5,-.5}, {0,0,-1}, {}, {0,0}},
        {{.5,-.5,-.5},  {0,0,-1}, {}, {1,0}},
        {{.5,.5,-.5},   {0,0,-1}, {}, {1,1}},
        {{-.5,.5,-.5},  {0,0,-1}, {}, {0,1}},

        {{.5,-.5,.5},   {1,0,0}, {},  {0,0}},
        {{.5,-.5,-.5},  {1,0,0}, {},  {1,0}},
        {{.5,.5,-.5},   {1,0,0}, {},  {1,1}},
        {{.5,.5,.5},    {1,0,0}, {},  {0,1}},

        {{-.5,-.5,.5},  {-1,0,0}, {}, {0,0}},
        {{-.5,-.5,-.5}, {-1,0,0}, {}, {1,0}},
        {{-.5,.5,-.5},  {-1,0,0}, {}, {1,1}},
        {{-.5,.5,.5},   {-1,0,0}, {}, {0,1}},

        {{-.5,.5,.5},   {0,1,0}, {},  {0,0}},
        {{.5,.5,.5},    {0,1,0}, {},  {1,0}},
        {{.5,.5,-.5},   {0,1,0}, {},  {1,1}},
        {{-.5,.5,-.5},  {0,1,0}, {},  {0,1}},

        {{-.5,-.5,.5},  {0,-1,0}, {}, {0,0}},
        {{.5,-.5,.5},   {0,-1,0}, {}, {1,0}},
        {{.5,-.5,-.5},  {0,-1,0}, {}, {1,1}},
        {{-.5,-.5,-.5}, {0,-1,0}, {}, {0,1}},
    };

    std::vector<uint32_t> indices = {
        0, 1, 2, 2, 3, 0,
        4, 5, 6, 6, 7, 4,
        8, 9, 10, 10, 11, 8,
        12, 13, 14, 14, 15, 12,
        16, 17, 18, 18, 19, 16,
        20, 21, 22, 22, 23, 20
    };

    return { vertices, indices };
}

// https://www.songho.ca/opengl/gl_sphere.html#sphere
GeometryData vke::createSphere(float radius, int sectors, int stacks)
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    float x, y, z, xy;                              // vertex position
    float nx, ny, nz, lengthInv = 1.0f / radius;    // vertex normal
    float s, t;                                     // vertex texCoord

    float sectorStep = 2 * M_PI / sectors;
    float stackStep = M_PI / stacks;
    float sectorAngle, stackAngle;

    int k1, k2;

    // vertices
    for(int i = 0; i <= stacks; ++i) {
        stackAngle = M_PI / 2 - i * stackStep;        // starting from pi/2 to -pi/2
        xy = radius * cosf(stackAngle);             // r * cos(u)
        z = radius * sinf(stackAngle);              // r * sin(u)

        // add (sectors+1) vertices per stack
        // first and last vertices have same position and normal, but different tex coords
        for(int j = 0; j <= sectors; ++j) {
            Vertex v;

            sectorAngle = j * sectorStep;           // starting from 0 to 2pi

            // vertex position (x, y, z)
            x = xy * cosf(sectorAngle);             // r * cos(u) * cos(v)
            y = xy * sinf(sectorAngle);             // r * cos(u) * sin(v)
            v.pos = { x, y, z };

            // normalized vertex normal (nx, ny, nz)
            nx = x * lengthInv;
            ny = y * lengthInv;
            nz = z * lengthInv;
            v.normal = { nx, ny, nz };

            // vertex tex coord (s, t) range between [0, 1]
            s = (float)j / sectors;
            t = (float)i / stacks;
            v.uv = { s, t };

            vertices.push_back(v);
        }
    }

    // indices
    for(int i = 0; i < stacks; ++i) {
        k1 = i * (sectors + 1);     // beginning of current stack
        k2 = k1 + sectors + 1;      // beginning of next stack

        for(int j = 0; j < sectors; ++j, ++k1, ++k2) {
            // 2 triangles per sector excluding first and last stacks
            // k1 => k2 => k1+1
            if(i != 0) {
                indices.push_back(k1);
                indices.push_back(k2);
                indices.push_back(k1 + 1);
            }

            // k1+1 => k2 => k2+1
            if(i != (stacks-1)) {
                indices.push_back(k1 + 1);
                indices.push_back(k2);
                indices.push_back(k2 + 1);
            }
        }
    }

    return { vertices, indices };
}

// https://www.songho.ca/opengl/gl_cylinder.html
GeometryData vke::createCylinder(float radius, float height, int sectors)
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    // get unit circle vectors on XY-plane
    float sectorStep = 2 * M_PI / sectors;
    float sectorAngle;  // radian

    std::vector<float> unitVertices;

    for(int i = 0; i <= sectors; ++i) {
        sectorAngle = i * sectorStep;
        unitVertices.push_back(cos(sectorAngle)); // x
        unitVertices.push_back(sin(sectorAngle)); // y
        unitVertices.push_back(0);                // z
    }

    // put side vertices
    for(int i = 0; i < 2; ++i) {
        float h = -height / 2.0f + i * height;           // z value; -h/2 to h/2
        float t = 1.0f - i;                              // vertical tex coord; 1 to 0

        for(int j = 0, k = 0; j <= sectors; ++j, k += 3) {
            float ux = unitVertices[k];
            float uy = unitVertices[k+1];
            float uz = unitVertices[k+2];

            Vertex v;
            v.pos = { ux * radius, uy * radius, h };
            v.normal = { ux, uy, uz };
            v.uv = { (float)j / sectors, t };

            vertices.push_back(v);
        }
    }

    // the starting index for the base/top surface
    //NOTE: it is used for generating indices later
    int baseCenterIndex = (int)vertices.size();
    int topCenterIndex = baseCenterIndex + sectors + 1; // include center vertex

    // put base and top vertices
    for(int i = 0; i < 2; ++i) {
        float h = -height / 2.0f + i * height;           // z value; -h/2 to h/2
        float nz = -1 + i * 2;                           // z value of normal; -1 to 1

        // center point
        Vertex v;
        v.pos = { 0, 0, h };
        v.normal = { 0, 0, nz };
        v.uv = { 0.5f, 0.5f };

        vertices.push_back(v);

        for(int j = 0, k = 0; j < sectors; ++j, k += 3) {
            float ux = unitVertices[k];
            float uy = unitVertices[k+1];

            v.pos = { ux * radius, uy * radius, h };
            v.normal = { 0, 0, nz };
            v.uv = { -ux * 0.5f + 0.5f, -uy * 0.5f + 0.5f };

            vertices.push_back(v);
        }
    }

    int k1 = 0;                     // 1st vertex index at base
    int k2 = sectors + 1;           // 1st vertex index at top

    // indices for the side surface
    for(int i = 0; i < sectors; ++i, ++k1, ++k2) {
        // 2 triangles per sector
        // k1 => k1+1 => k2
        indices.push_back(k1);
        indices.push_back(k1 + 1);
        indices.push_back(k2);

        // k2 => k1+1 => k2+1
        indices.push_back(k2);
        indices.push_back(k1 + 1);
        indices.push_back(k2 + 1);
    }

    // indices for the base surface
    for(int i = 0, k = baseCenterIndex + 1; i < sectors; ++i, ++k) {
        if (i < sectors - 1) {
            indices.push_back(baseCenterIndex);
            indices.push_back(k + 1);
            indices.push_back(k);
        } else {
            indices.push_back(baseCenterIndex);
            indices.push_back(baseCenterIndex + 1);
            indices.push_back(k);
        }
    }

    // indices for the top surface
    for(int i = 0, k = topCenterIndex + 1; i < sectors; ++i, ++k) {
        if (i < sectors - 1) {
            indices.push_back(topCenterIndex);
            indices.push_back(k);
            indices.push_back(k + 1);
        } else {
            indices.push_back(topCenterIndex);
            indices.push_back(k);
            indices.push_back(topCenterIndex + 1);
        }
    }

    return { vertices, indices };
}

// https://www.songho.ca/opengl/gl_cone.html
GeometryData vke::createCone(float baseRadius, float height, int sectors, int stacks)
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    float x, y, z;      // vertex pos
    float radius;       // radius for each stack

    // get normals for the sides
    std::vector<float> sideNormals;
    float sectorStep = 2 * M_PI / sectors;
    float sectorAngle;  // radian

    // compute the normal vector at 0 degree first
    // tanA = baseRadius / height
    float zAngle = atan2(baseRadius, height);
    float x0 = cos(zAngle);     // nx
    float z0 = sin(zAngle);     // nz

    // rotate (x0,y0,z0) per sector angle
    for(int i = 0; i <= sectors; ++i) {
        sectorAngle = i * sectorStep;
        sideNormals.push_back(cos(sectorAngle)*x0); // nx
        sideNormals.push_back(sin(sectorAngle)*x0); // ny
        sideNormals.push_back(z0);                  // nz
    }

    // get unit circle vectors on XY-plane
    std::vector<float> unitCircleVertices;

    for(int i = 0; i <= sectors; ++i) {
        sectorAngle = i * sectorStep;
        unitCircleVertices.push_back(cos(sectorAngle)); // x
        unitCircleVertices.push_back(sin(sectorAngle)); // y
        unitCircleVertices.push_back(0);                // z
    }

    // put vertices of side to array by scaling unit circle
    for(int i = 0; i <= stacks; ++i) {
        z = -(height * 0.5f) + (float)i / stacks * height;  // vertex pos z
        radius = baseRadius * (1.0f - (float)i / stacks);   // lerp
        float t = 1.0f - (float)i / stacks;                 // top-to-bottom

        for(int j = 0, k = 0; j <= sectors; ++j, k += 3)
        {
            x = unitCircleVertices[k];
            y = unitCircleVertices[k+1];

            Vertex v;
            v.pos = { x * radius, y * radius, z };
            v.normal = { sideNormals[k], sideNormals[k+1], sideNormals[k+2] };
            v.uv = { (float)j / sectors, t };

            vertices.push_back(v);
        }
    }

    // remember where the base vertices start
    unsigned int baseVertexIndex = (unsigned int)vertices.size();

    // put vertices of base of cone
    z = -height * 0.5f;

    Vertex v;
    v.pos = { 0, 0, z };
    v.normal = { 0, 0, -1 };
    v.uv = { 0.5f, 0.5f };

    vertices.push_back(v);

    for(int i = 0, j = 0; i < sectors; ++i, j += 3) {
        x = unitCircleVertices[j];
        y = unitCircleVertices[j+1];

        v.pos = { x * baseRadius, y * baseRadius, z };
        v.normal = { 0, 0, -1 };
        v.uv = { -x * 0.5f + 0.5f, -y * 0.5f + 0.5f };      // flip horizontal

        vertices.push_back(v);
    }

    // put indices for sides
    unsigned int k1, k2;
    for(int i = 0; i < stacks; ++i) {
        k1 = i * (sectors + 1);     // bebinning of current stack
        k2 = k1 + sectors + 1;      // beginning of next stack

        for(int j = 0; j < sectors; ++j, ++k1, ++k2) {
            // 2 trianles per sector
            indices.push_back(k1);
            indices.push_back(k1+1);
            indices.push_back(k2);

            indices.push_back(k2);
            indices.push_back(k1+1);
            indices.push_back(k2+1);
        }
    }

    // put indices for base
    for(int i = 0, k = baseVertexIndex + 1; i < sectors; ++i, ++k) {
        if(i < (sectors - 1)) {
            indices.push_back(baseVertexIndex);
            indices.push_back(k+1);
            indices.push_back(k);
        } else {    // last triangle
            indices.push_back(baseVertexIndex);
            indices.push_back(baseVertexIndex+1);
            indices.push_back(k);
        }
    }

    return { vertices, indices };
}

// https://www.songho.ca/opengl/gl_torus.html
GeometryData vke::createTorus(float majorRadius, float minorRadius, int sectors, int sides)
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    float x, y, z, xy;                              // vertex position
    float nx, ny, nz;                               // normal
    float lengthInv = 1.0f / minorRadius;           // to normalize normals
    float s, t;                                     // texCoord

    float sectorStep = 2 * M_PI / sectors;
    float sideStep = 2 * M_PI / sides;
    float sectorAngle, sideAngle;

    for(int i = 0; i <= sides; ++i) {
        // start the tube side from the inside where sideAngle = pi
        sideAngle = M_PI - i * sideStep;              // starting from pi to -pi
        xy = minorRadius * cosf(sideAngle);         // r * cos(u)
        z = minorRadius * sinf(sideAngle);          // r * sin(u)

        // add (sectors+1) vertices per side
        // the first and last vertices have same position and normal,
        // but different tex coords
        for(int j = 0; j <= sectors; ++j) {
            Vertex v;
            sectorAngle = j * sectorStep;           // starting from 0 to 2pi

            // tmp x and y to compute normal vector
            x = xy * cosf(sectorAngle);
            y = xy * sinf(sectorAngle);

            // add normalized vertex normal first
            nx = x * lengthInv;
            ny = y * lengthInv;
            nz = z * lengthInv;
            v.normal = { nx, ny, nz };

            // shift x & y, and vertex position
            x += majorRadius * cosf(sectorAngle);   // (R + r * cos(u)) * cos(v)
            y += majorRadius * sinf(sectorAngle);   // (R + r * cos(u)) * sin(v)
            v.pos = { x, y, z };

            // vertex tex coord between [0, 1]
            s = (float)j / sectors;
            t = (float)i / sides;
            v.uv = { s, t };

            vertices.push_back(v);
        }
    }

    // indices
    //  k1--k1+1
    //  |  / |
    //  | /  |
    //  k2--k2+1
    unsigned int k1, k2;

    for(int i = 0; i < sides; ++i) {
        k1 = i * (sectors + 1);     // beginning of current side
        k2 = k1 + sectors + 1;      // beginning of next side

        for(int j = 0; j < sectors; ++j, ++k1, ++k2) {
            // k1---k2---k1+1
            indices.push_back(k1);
            indices.push_back(k2);
            indices.push_back(k1+1);

            // k1+1---k2---k2+1
            indices.push_back(k1+1);
            indices.push_back(k2);
            indices.push_back(k2+1);
        }
    }

    return { vertices, indices };
}
