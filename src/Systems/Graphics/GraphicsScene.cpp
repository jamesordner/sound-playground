#include "GraphicsScene.h"
#include "GraphicsObject.h"
#include "CameraGraphicsObject.h"
#include "MeshGraphicsObject.h"
#include "UIGraphicsObject.h"
#include "Vulkan/VulkanInstance.h"
#include "Vulkan/VulkanScene.h"
#include <algorithm>

GraphicsScene::GraphicsScene(const SystemInterface* system, const UScene* uscene, VulkanScene* vulkanScene) :
	SystemSceneInterface(system, uscene),
	activeCamera(nullptr),
	vulkanScene(vulkanScene)
{
}

GraphicsScene::~GraphicsScene()
{
	for (const auto& graphicsObject : graphicsObjects) {
		if (auto* meshObject = dynamic_cast<MeshGraphicsObject*>(graphicsObject.get())) {
			vulkanScene->removeModel(meshObject->model);
		}
	}
	graphicsObjects.clear();
}

SystemObjectInterface* GraphicsScene::addSystemObject(SystemObjectInterface* object)
{
	graphicsObjects.emplace_back(static_cast<GraphicsObject*>(object));
	
	if (auto* meshObject = dynamic_cast<MeshGraphicsObject*>(object)) {
		meshObject->model = vulkanScene->createModel();
	}
	else if (auto* uiObject = dynamic_cast<UIGraphicsObject*>(object)) {
		uiObjects.push_back(uiObject);
		registerCallback(
			uiObject,
			EventType::UIDrawOrderUpdated,
			[this](const EventData& data, bool bEventFromParent) {
				std::sort(
					uiObjects.begin(),
					uiObjects.end(),
					[](UIGraphicsObject* l, UIGraphicsObject* r) { return l->uiData.drawOrder < r->uiData.drawOrder; }
				);
			}
		);
	}
	
	return object;
}

void GraphicsScene::deleteSystemObject(const UObject* uobject)
{
	for (const auto& graphicsObject : graphicsObjects) {
		if (graphicsObject->uobject == uobject) {
			graphicsObjects.remove(graphicsObject);
			if (auto* meshObject = dynamic_cast<MeshGraphicsObject*>(graphicsObject.get())) {
				vulkanScene->removeModel(meshObject->model);
			}
			break;
		}
	}
}

void GraphicsScene::drawScene(VulkanInstance* vulkan)
{
	if (activeCamera) {
		const auto& pos = activeCamera->cameraPosition();
		const auto& dir = activeCamera->cameraForward();
		vulkanScene->setViewMatrix(mat::lookAt(pos, pos + dir));
	}
	
	vulkan->renderScene(vulkanScene);
	// for (auto* uiObject : uiObjects) render->drawUIElement(uiObject->uiData);
}
