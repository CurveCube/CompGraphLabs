#pragma once

#include <map>

#include "framework.h"
#include "Mesh.h"
#include "ShaderManagers.hpp"

struct Node {
    XMFLOAT4X4 M;
};

class Scene {
public:
private:
    std::map<unsigned int, RoughMetallicMaterial> materials;
    std::map<unsigned int, Object> objects;
    std::map<unsigned int, Node> nodes;
    PSManager psManager;
    VSManager vsManager;
};