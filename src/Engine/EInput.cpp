#include "EInput.h"
#include "Engine.h"
#include "EWorld.h"
#include "EInputComponent.h"
#include "../Objects/EObject.h"
#include "../Objects/EModel.h"
#include "../UI/UITypes.h"
#include "../UI/UIManager.h"
#include "../UI/UIComponent.h"

EInput::EInput() :
	uiManager(nullptr),
	bPlacingSelected(false)
{
}

void EInput::handleInput(const SDL_Event& sdlEvent)
{
	UIManagerEvent uiEvent = uiManager->handeInput(sdlEvent);

	if (uiEvent.spawned) {
		for (auto* obj : selectedObjects) obj->setSelected(false);
		selectedObjects.clear();
		selectedObjects.emplace(uiEvent.spawned);
		bPlacingSelected = true;
	}

	if (uiEvent.bConsumedInput) return;

	Engine& engine = Engine::instance();
	EWorld& world = engine.world();

	if (bPlacingSelected) {
		if (sdlEvent.type == SDL_MOUSEMOTION) {
			mat::vec3 hitLoc;
			if (EModel* model = engine.raycastScreen(sdlEvent.motion.x, sdlEvent.motion.y, hitLoc, selectedObjects)) {
				for (auto* obj : selectedObjects) obj->setPosition(hitLoc); // temp
			}
		}
		else if (sdlEvent.type == SDL_MOUSEBUTTONDOWN && sdlEvent.button.button == SDL_BUTTON_LEFT) {
			bPlacingSelected = false;
			for (auto* obj : selectedObjects) obj->setSelected(true);
		}
	}
	else {
		// Object selection
		if (sdlEvent.type == SDL_MOUSEBUTTONDOWN && sdlEvent.button.button == SDL_BUTTON_LEFT) {
			if (EModel* model = engine.raycastScreen(sdlEvent.motion.x, sdlEvent.motion.y)) {
				for (auto* obj : selectedObjects) obj->setSelected(false);
				selectedObjects.clear();
				
				model->setSelected(true);
				selectedObjects.emplace(model);
				auto* uiComp = model->uiComponent();
				uiManager->setActiveData(uiComp ? &uiComp->data : nullptr);
			}
			else {
				// clicked empty space
				for (auto* obj : selectedObjects) obj->setSelected(false);
				selectedObjects.clear();
				uiManager->setActiveData(nullptr);
			}
		}
	}

    for (const auto& object : world.allObjects()) {
        if (auto* inputComponent = object->inputComponent()) {
            inputComponent->processInput(sdlEvent);
        }
    }
}
