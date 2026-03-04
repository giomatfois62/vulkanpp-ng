#include "vk_volume.hpp"

vke::Circle::Circle()
{

}

vke::Circle::Circle(const glm::vec2 &center, float radius) :
    center(center), radius(radius)
{

}

bool vke::Circle::intersect(const glm::vec2 &point)
{
    return lengthSq(point-center) <= radius*radius;
}

bool vke::Circle::intersect(const Circle &other)
{
    float dist = lengthSq(other.center-center);

    return dist <= (other.radius+radius)*(other.radius+radius) &&
        dist >= (other.radius-radius)*(other.radius-radius);
}

vke::Circle vke::Circle::translated(const glm::vec2 &pos)
{
    return Circle(center + pos, radius);
}

vke::Box::Box()
{

}

vke::Box::Box(const glm::vec2 &min, const glm::vec2 &max) :
    min(min), max(max)
{

}

vke::Box vke::Box::translated(const glm::vec2 &pos)
{
    return Box(min + pos, max + pos);
}

bool vke::Box::intersect(const glm::vec2 &point)
{
    return (point[0] >= min[0] && point.x <= max[0]) &&
        (point[1] >= min[1] && point.y <= max[1]);
}

bool vke::Box::intersect(const Box &other)
{
    return (min[0] <= other.max[0] && max[0] >= other.min[0]) &&
        (min[1] <= other.max[1] && max[1] >= other.min[1]);
}

bool vke::Box::intersect(const Circle &circle)
{
    glm::vec2 closestPoint = glm::clamp(circle.center, min, max);
    float dist = lengthSq(closestPoint - circle.center);

    return dist <= circle.radius * circle.radius;
}

std::vector<vke::Box> vke::Box::subdivide()
{
    glm::vec2 center = 0.5f * (min + max);

    std::vector<Box> sub;

    sub.push_back(Box(min,center));
    sub.push_back(Box(center, max));
    sub.push_back(Box(min + glm::vec2(0, center[1]-min[1]), center + glm::vec2(0, max[1]-center[1])));
    sub.push_back(Box(center - glm::vec2(0, center[1]-min[1]), max - glm::vec2(0, max[1]-center[1])));

    return sub;
}

vke::Volume::Volume()
{

}

vke::Volume::Volume(const glm::vec3 &min, const glm::vec3 &max) :
    min(min), max(max)
{

}

vke::Volume::Volume(const Sphere &sphere)
{
    min = sphere.center - glm::vec3(sphere.radius);
    max = sphere.center + glm::vec3(sphere.radius);
}

vke::Volume vke::Volume::transformed(const glm::mat4 &transform) const
{
    glm::vec3 scale(
        glm::length(glm::vec3(transform[0])), // Scale X
        glm::length(glm::vec3(transform[1])), // Scale Y
        glm::length(glm::vec3(transform[2]))  // Scale Z
    );

    glm::vec3 pos(transform[3]);

    return scaled(scale).translated(pos);
}

vke::Volume vke::Volume::scaled(const glm::vec3 &scale) const
{
    glm::vec3 center = 0.5f * (max+min);

    return Volume((min-center)*scale + center,
        (max-center)*scale + center);
}

bool vke::Volume::intersect(const glm::vec3 &point) const
{
    return (point[0] >= min[0] && point.x <= max[0]) &&
        (point[1] >= min[1] && point.y <= max[1]) &&
        (point[2] >= min[2] && point.z <= max[2]);
}

bool vke::Volume::intersect(const Volume &other) const
{
    return (min[0] <= other.max[0] && max[0] >= other.min[0]) &&
        (min[1] <= other.max[1] && max[1] >= other.min[1]) &&
        (min[2] <= other.max[2] && max[2] >= other.min[2]);
}

bool vke::Volume::intersect(const Frustum &f) const
{
    return f.intersect(min, max);
}

bool vke::Volume::intersect(const Ray &ray) const
{
    glm::vec3 center = 0.5f * (min + max);
    glm::vec3 closestPoint = glm::closestPointOnLine(center, ray.near, ray.far);

    return intersect(closestPoint);
}

bool vke::Volume::intersect(const Sphere &sphere) const
{
    glm::vec3 closestPoint = glm::clamp(sphere.center, min, max);
    float dist = lengthSq(closestPoint - sphere.center);

    return dist <= sphere.radius * sphere.radius;
}

vke::Volume vke::Volume::minimumBoundingBox(const Volume &other) const
{
    glm::vec3 _min = {
        std::min(min[0], other.min[0]),
        std::min(min[1], other.min[1]),
        std::min(min[2], other.min[2]),
    };

    glm::vec3 _max = {
        std::max(max[0], other.max[0]),
        std::max(max[1], other.max[1]),
        std::max(max[2], other.max[2]),
    };

    return Volume(_min,_max);
}

std::vector<vke::Volume> vke::Volume::subdivide()
{
    glm::vec3 center = 0.5f * (min + max);

    std::vector<Volume> sub;

    sub.push_back(Volume(min,center));
    sub.push_back(Volume(center, max));
    sub.push_back(Volume(glm::vec3(center[0], min[1], min[2]), glm::vec3(max[0], center[1], center[2])));
    sub.push_back(Volume(glm::vec3(center[0], min[1], center[2]), glm::vec3(max[0], center[1], max[2])));
    sub.push_back(Volume(glm::vec3(min[0], min[1], center[2]), glm::vec3(center[0], center[1], max[2])));
    sub.push_back(Volume(glm::vec3(min[0], center[1], min[2]), glm::vec3(center[0], max[1], center[2])));
    sub.push_back(Volume(glm::vec3(center[0], center[1], min[2]), glm::vec3(max[0], max[1], center[2])));
    sub.push_back(Volume(glm::vec3(min[0], center[1], center[2]), glm::vec3(center[0], max[1], max[2])));

    return sub;
}

vke::Volume vke::Volume::translated(const glm::vec3 &pos) const
{
    return Volume(min + pos, max + pos);
}

vke::Sphere::Sphere()
{

}

vke::Sphere::Sphere(const glm::vec3 &center, float radius) :
    center(center), radius(radius)
{

}

bool vke::Sphere::intersect(const glm::vec3 &point)
{
    return lengthSq(point-center) <= radius*radius;
}

bool vke::Sphere::intersect(const Sphere &other)
{
    float dist = lengthSq(other.center-center);

    return dist <= (other.radius+radius)*(other.radius+radius) &&
        dist >= (other.radius-radius)*(other.radius-radius);
}

vke::Sphere vke::Sphere::translated(const glm::vec3 &pos)
{
    return Sphere(center + pos, radius);
}

vke::Ray::Ray()
{

}

vke::Ray::Ray(const glm::vec3 &near, const glm::vec3 &far) :
    near(near), far(far)
{

}

vke::Ray::Ray(const std::pair<glm::vec3, glm::vec3> &points) :
    near(points.first), far(points.second)
{

}
