#include "Engine.h"
#include "../Audio/AudioEngine.h"
#include "../Graphics/Render.h"
#include "../Graphics/Matrix.h"
#include "../Graphics/GMesh.h"
#include "../UI/UIManager.h"
#include "../UI/UIObject.h"
#include "EModel.h"
#include "ECamera.h"
#include "EInput.h"
#include <SDL.h>

Engine& Engine::instance()
{
	static Engine instance;
	return instance;
}

Engine::Engine() :
	lastFrameTime(0.f),
	m_world(new EWorld),
	input(new EInput),
	renderer(new Render),
	uiManager(new UIManager),
	audioEngine(new AudioEngine)

{
	if (init()) {
		audioEngine->start();
		bInitialized = true;
	}
	else {
		bInitialized = false;
	}
}

Engine::~Engine()
{
	deinit();
}

bool Engine::init()
{
	// Initialize SDL
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
		printf("SDL could not initialize! SDL Error: %s\n", SDL_GetError());
		return false;
	}

	// Attributes must be set before the window is created
	renderer->setAttributes();

	// Create window
	window = SDL_CreateWindow("Sound Playground", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		1280, 720, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);

	if (!window) {
		printf("Window could not be created! SDL Error: %s\n", SDL_GetError());
		return false;
	}

	if (!renderer->init(window)) {
		printf("Warning: Renderer failed to initialize!\n");
		return false;
	}

	//if (!audioEngine->init()) {
	//	printf("Warning: AudioEngine failed to initialize!\n");
	//	return false;
	//}

	return true;
}

void Engine::deinit()
{
	bInitialized = false;
	audioEngine->deinit();
	renderer->deinit();
	SDL_DestroyWindow(window);
	window = nullptr;
	SDL_Quit();
}

EWorld& Engine::world()
{
	return *m_world;
}

AudioEngine& Engine::audio()
{
	return *audioEngine;
}

void Engine::run()
{
	if (!bInitialized) return;
	bool quit = false;
	Uint32 sdlTime = SDL_GetTicks();
	while (!quit) {
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT) {
				quit = true;
				break;
			}
			else {
				// UI input
				bool bConsumed = uiManager->handeInput(event, window);

				// Other input if not consumed by UI
				if (!bConsumed) input->handleInput(event);
			}
		}

		Uint32 newSdlTime = SDL_GetTicks();
		lastFrameTime = static_cast<float>(newSdlTime - sdlTime) * 0.001f;
		m_world->tick(lastFrameTime);
		sdlTime = newSdlTime;

		// Render
		const ECamera* camera = m_world->worldCamera();
		renderer->setCamera(camera->cameraPosition(), camera->cameraFocus());
		renderer->draw(meshes);
		renderer->drawUI(*uiManager->root, uiManager->screenBounds);
		renderer->swap(window);
	}
}

void Engine::registerModel(const std::shared_ptr<EModel>& model)
{
	// Assign mesh to model and save model reference
	auto mesh = makeMesh(model->getFilepath());
	model->registerWithMesh(mesh); // transfer mesh ownership to model
	mesh->registerModel(model.get()); // add weak pointer to model in mesh model list

	activeModel = model;
}

void Engine::unregisterModel(const std::shared_ptr<EModel>& model)
{
	model->getMesh()->unregisterModel(model.get());
	model->unregister();

	// Remove mesh from map if no more models reference this mesh
	std::string path = model->getFilepath();
	auto meshRef = meshes[path];
	if (meshRef.expired()) meshes.erase(path);
}

std::shared_ptr<GMesh> Engine::makeMesh(const std::string& filepath)
{
	if (auto existing = meshes[filepath].lock()) {
		return existing;
	}
	else {
		auto newMesh = std::make_shared<GMesh>(filepath);
		meshes[filepath] = newMesh;
		return newMesh;
	}
}

EModel* Engine::raycastScreen(int x, int y) {
	mat::vec3 hitLoc;
	return raycastScreen(x, y, hitLoc);
}

EModel* Engine::raycastScreen(int x, int y, mat::vec3& hitLoc)
{
	int width, height;
	SDL_GL_GetDrawableSize(window, &width, &height);

	mat::vec4 rayEndScreen{
		static_cast<float>(x - width / 2) / (width / 2),
		static_cast<float>(height / 2 - y) / (height / 2),
		1.f,
		1.f };
	mat::vec4 rayEndWorld(renderer->screenToWorldMatrix() * rayEndScreen);
	mat::vec3 rayEndWorldNormalized = mat::vec3(rayEndWorld) / rayEndWorld.w;

	const mat::vec3& rayStartWorld = m_world->worldCamera()->cameraPosition();

	return m_world->raycast(rayStartWorld, rayEndWorldNormalized - rayStartWorld, hitLoc);
}
