#include "GraphicsScene.h"
#include "GraphicsObject.h"
#include "Render.h"
#include "CameraGraphicsObject.h"
#include "UIGraphicsObject.h"
#include <algorithm>

GraphicsScene::GraphicsScene(const SystemInterface* system, const UScene* uscene) :
	SystemSceneInterface(system, uscene),
	activeCamera(nullptr)
{
}

GraphicsScene::~GraphicsScene()
{
	graphicsObjects.clear();
}

void GraphicsScene::drawScene(Render* render)
{
	if (activeCamera) render->setCamera(activeCamera->cameraPosition(), activeCamera->cameraForward());
	render->drawMeshes();
	for (auto* uiObject : uiObjects) render->drawUIElement(uiObject->uiData);
}

SystemObjectInterface* GraphicsScene::addSystemObject(SystemObjectInterface* object)
{
	graphicsObjects.emplace_back(static_cast<GraphicsObject*>(object));
	if (auto* uiObject = dynamic_cast<UIGraphicsObject*>(object)) {
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
