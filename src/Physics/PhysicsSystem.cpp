#include "PhysicsSystem.h"
#include "PhysicsScene.h"

PhysicsSystem::PhysicsSystem()
{
}

PhysicsSystem::~PhysicsSystem()
{
}

bool PhysicsSystem::init()
{
	return true;
}

void PhysicsSystem::deinit()
{
	physicsScenes.clear();
}

void PhysicsSystem::execute(float deltaTime)
{

}

SystemSceneInterface* PhysicsSystem::createSystemScene(const class UScene* uscene)
{
	return physicsScenes.emplace_back(std::make_unique<PhysicsScene>(this, uscene)).get();
}

SystemSceneInterface* PhysicsSystem::findSystemScene(const UScene* uscene)
{
	for (const auto& scene : physicsScenes) {
		if (scene->uscene == uscene) return scene.get();
	}
	return nullptr;
}

UObject* PhysicsSystem::raycast(const UScene* uscene, const mat::vec3& origin, const mat::vec3& direction, mat::vec3& hit)
{
	for (const auto& scene : physicsScenes) {
		if (scene->uscene == uscene) {
			return scene->raycast(origin, direction, hit);
		}
	}
	return nullptr;
}
