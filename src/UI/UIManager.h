#pragma once

#include "../Graphics/Matrix.h"
#include <memory>

// Forward declarations
struct UIObject;
struct SDL_Window;
union SDL_Event;

class UIManager
{
public:

	UIManager();

	~UIManager();

	// Handles input events. Returns true if the UI consumed the input.
	bool handeInput(const SDL_Event& event, SDL_Window* window);

	// Virtual screen bounds. All UI elements are arranged to this virtual resolution.
	// Scaling to the actual screen resolution is handled automatically during rendering.
	static mat::vec2 screenBounds;

	// This is the root of all drawn UIObjects. UIObjects are drawn in the order that they appear in the
	// UIObjects::subobjects array, and all of a UIObject's subobjects are drawn before the next UIObject in
	// the array. UIObjects draw on top of previously-drawn objects, so later-drawn objects will be on top.
	std::unique_ptr<UIObject> root;

private:

	// Currently hovered object. Objects must accept input to become hovered.
	UIObject* hoveredObject;

	mat::vec2 virtualMousePosition(SDL_Window* window);

	void setupMenuBar();

	// Returns the object at location, provided as a virtual screen coordinate
	UIObject* objectAt(const mat::vec2& location);

	UIObject* objectAtRecursive(
		UIObject& object,
		const mat::vec2& location,
		const mat::vec2& parentCenterAbs,
		const mat::vec2& parentBoundsAbs);
};