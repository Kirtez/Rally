#include "model/World.h"
#include "model/PhysicsWorld.h"

#include <sstream>

namespace Rally { namespace Model {

    World::World() {
    }

    World::~World() {
    }

    void World::initialize(const std::string & bulletFile) {
        physicsWorld.initialize(bulletFile);
    }

    void World::update(float deltaTime) {
        physicsWorld.update(deltaTime);
    }
} }
