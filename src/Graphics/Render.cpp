#include "Render.h"
#include "GProgram.h"
#include "GMesh.h"
#include "GTexture.h"
#include "../UI/UIObject.h"
#include <SDL.h>
#include <GL/gl3w.h>
#include <stdio.h>

constexpr int glMajorVersion = 4;
constexpr int glMinorVersion = 6;

Render::Render() :
	glContext(nullptr),
	gObjects{}
{
}

Render::~Render() = default;

void Render::setAttributes()
{
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, glMajorVersion);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, glMinorVersion);
}

bool Render::init(SDL_Window* window)
{
	glContext = SDL_GL_CreateContext(window);

	// Init gl3w
	if (gl3wInit()) {
		fprintf(stderr, "failed to initialize OpenGL\n");
		return false;
	}
	if (!gl3wIsSupported(glMajorVersion, glMinorVersion)) {
		fprintf(stderr, "OpenGL %d.%d not supported\n", glMajorVersion, glMinorVersion);
		return false;
	}

	// Use Vsync
	if (SDL_GL_SetSwapInterval(1) < 0) {
		printf("Warning: Unable to set VSync! SDL Error: %s\n", SDL_GetError());
		return false;
	}

	glDepthFunc(GL_LESS);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	if (!initRectVAO()) {
		printf("Failed to initialize rectVAO.\n");
		deinit();
		return false;
	}

	if (!initDeferredPipeline()) {
		printf("Failed to initialize deferred pipeline.\n");
		deinit();
		return false;
	}

	if (!initShadow()) {
		printf("Failed to initialize shadow program.\n");
		deinit();
		return false;
	}

	if (!initUI()) {
		printf("Failed to initialize UI program.\n");
		deinit();
		return false;
	}

	return true;
}

void Render::deinit()
{
	// programs
	mainProgram.reset();
	shadowProgram.reset();

	gObjects.deinit();

	// context
	if (glContext) SDL_GL_DeleteContext(glContext);
	glContext = nullptr;
}

void Render::GObjects::deinit()
{
	if (shadowFBO) glDeleteFramebuffers(1, &shadowFBO);
	if (shadowTexture) glDeleteTextures(1, &shadowTexture);
	if (rectVAO) glDeleteVertexArrays(1, &rectVAO);
	if (gbuffer) glDeleteFramebuffers(1, &gbuffer);
	if (gbufferTextures[0]) glDeleteTextures(3, gbufferTextures);

	shadowFBO = 0;
	shadowTexture = 0;
	rectVAO = 0;
	gbuffer = 0;
	gbufferTextures[0] = gbufferTextures[1] = gbufferTextures[2] = 0;
}

void Render::setCamera(const mat::vec3& position, const mat::vec3& focus)
{
	float aspectRatio = 720.f / 1280.f;
	mat::mat4 view = lookAt(position, focus);
	mat::mat4 proj = mat::perspective(-0.05f, 0.05f, -0.05f * aspectRatio, 0.05f * aspectRatio, 0.1f, 15.f);
	gbuffersProgram->setUniform("viewMat", view);
	gbuffersProgram->setUniform("projMat", proj);
	compositeProgram->setUniform("viewMat", view);
	invProjectionViewMatrix = mat::inverse(proj * view);
}

void Render::drawMeshes()
{
	const auto& meshes = GMesh::sharedMeshes();

	// Synchronize transform data
	for (const auto& mesh : meshes) mesh.second->updateInstanceData();

	// shadow
	shadowProgram->use();
	for (const auto& mesh : meshes) mesh.second->draw();
	shadowProgram->release();

	// gbuffers
	gbuffersProgram->use();
	for (const auto& mesh : meshes) mesh.second->draw();
	gbuffersProgram->release();

	// composite
	compositeProgram->use();
	glDrawArrays(GL_TRIANGLES, 0, 6);
	compositeProgram->release();
}

void Render::drawUI(const UIObject& rootObject, const mat::vec2& virtualScreenBounds)
{
	uiProgram->use();
	drawUIRecursive(
		rootObject,
		virtualScreenBounds / 2.f,
		virtualScreenBounds,
		virtualScreenBounds);
	uiProgram->release();
}

void Render::drawUIRecursive(
	const UIObject& object,
	const mat::vec2& parentCenterAbs,
	const mat::vec2& parentBoundsAbs,
	const mat::vec2& screenBounds)
{
	mat::vec2 anchorOffset = (parentBoundsAbs - object.bounds) / 2.f * object.anchorPosition();
	mat::vec2 center = parentCenterAbs + anchorOffset + object.position; // virtual pixels

	mat::vec2 translation = center / screenBounds * 2.f - 1.f; // NDC
	mat::vec2 scale = object.bounds / screenBounds; // NDC
	mat::mat3 transform{
		{ scale.x,       0, translation.x },
		{       0, scale.y, translation.y },
		{       0,       0,             1 }
	};
	uiProgram->setUniform("transform", transform);

	mat::vec4 texCoords = object.textureCoords();
	const mat::vec2& texSize = uiTexture->textureSize();
	mat::vec4 texMapping{
		texCoords[0] / texSize.x,
		(texSize.y - texCoords[1] - texCoords[3]) / texSize.y,
		(texCoords[0] + texCoords[2]) / texSize.x,
		(texSize.y - texCoords[1]) / texSize.y,
	};
	uiProgram->setUniform("uiTexCoord", texMapping);

	glDrawArrays(GL_TRIANGLES, 0, 6);

	for (const auto& child : object.subobjects) {
		drawUIRecursive(child, center, object.bounds, screenBounds);
	}
}

void Render::swap(SDL_Window* window)
{
	SDL_GL_SwapWindow(window);
}

const mat::mat4& Render::screenToWorldMatrix()
{
	return invProjectionViewMatrix;
}

bool Render::initRectVAO()
{
	static const GLfloat g_vertex_data[] = {
		-1.f, -1.f, // LL
		-1.f,  1.f, // UL
		 1.f,  1.f, // UR
		-1.f, -1.f, // LL
		 1.f, -1.f, // LR
		 1.f,  1.f, // UR
	};

	static const GLfloat g_texcoord_data[] = {
		0.f, 0.f, // LL
		0.f, 1.f, // UL
		1.f, 1.f, // UR
		0.f, 0.f, // LL
		1.f, 0.f, // LR
		1.f, 1.f, // UR
	};

	GLuint vbo[2];
	glCreateVertexArrays(1, &gObjects.rectVAO);
	glCreateBuffers(2, vbo);

	glNamedBufferStorage(vbo[0], sizeof(g_vertex_data), g_vertex_data, 0);
	glVertexArrayVertexBuffer(gObjects.rectVAO, 0, vbo[0], 0, sizeof(mat::vec2));
	glVertexArrayAttribFormat(gObjects.rectVAO, 0, 2, GL_FLOAT, GL_FALSE, 0);
	glEnableVertexArrayAttrib(gObjects.rectVAO, 0);
	glVertexArrayAttribBinding(gObjects.rectVAO, 0, 0);

	glNamedBufferStorage(vbo[1], sizeof(g_texcoord_data), g_texcoord_data, 0);
	glVertexArrayVertexBuffer(gObjects.rectVAO, 1, vbo[1], 0, sizeof(mat::vec2));
	glVertexArrayAttribFormat(gObjects.rectVAO, 1, 2, GL_FLOAT, GL_FALSE, 0);
	glEnableVertexArrayAttrib(gObjects.rectVAO, 1);
	glVertexArrayAttribBinding(gObjects.rectVAO, 1, 1);
	
	glDeleteBuffers(2, vbo);
	return true;
}

bool Render::initDeferredPipeline()
{
	glCreateFramebuffers(1, &gObjects.gbuffer);
	glCreateTextures(GL_TEXTURE_2D, 3, gObjects.gbufferTextures);

	glTextureParameteri(gObjects.gbufferTextures[0], GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTextureParameteri(gObjects.gbufferTextures[0], GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTextureStorage2D(gObjects.gbufferTextures[0], 1, GL_RGBA32UI, 1280, 720);
	glNamedFramebufferTexture(gObjects.gbuffer, GL_COLOR_ATTACHMENT0, gObjects.gbufferTextures[0], 0);

	glTextureParameteri(gObjects.gbufferTextures[1], GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTextureParameteri(gObjects.gbufferTextures[1], GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTextureStorage2D(gObjects.gbufferTextures[1], 1, GL_RGBA32F, 1280, 720);
	glNamedFramebufferTexture(gObjects.gbuffer, GL_COLOR_ATTACHMENT1, gObjects.gbufferTextures[1], 0);

	glTextureStorage2D(gObjects.gbufferTextures[2], 1, GL_DEPTH_COMPONENT32F, 1280, 720);
	glNamedFramebufferTexture(gObjects.gbuffer, GL_DEPTH_ATTACHMENT , gObjects.gbufferTextures[2], 0);

	GLuint drawBuffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
	glNamedFramebufferDrawBuffers(gObjects.gbuffer, 2, drawBuffers);
	
	if (glCheckNamedFramebufferStatus(gObjects.gbuffer, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		return false;
	}
	
	gbuffersProgram = std::make_unique<GProgram>("gbuffers");
	gbuffersProgram->setFramebuffer(gObjects.gbuffer);
	gbuffersProgram->setPreDrawRoutine([this] {
		glViewport(0, 0, 1280, 720);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glEnable(GL_DEPTH_TEST);
		glBindTextures(0, 1, &gObjects.shadowTexture);
		}
	);
	gbuffersProgram->setReleaseRoutine([] {
		glBindTexture(GL_TEXTURE_2D, 0);
		}
	);

	compositeProgram = std::make_unique<GProgram>("composite");
	compositeProgram->setPreDrawRoutine([this] {
		glViewport(0, 0, 1280, 720);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glBindVertexArray(gObjects.rectVAO);
		glBindTextures(0, 2, gObjects.gbufferTextures);
		}
	);
	compositeProgram->setReleaseRoutine([] {
		glBindVertexArray(0);
		glBindTexture(GL_TEXTURE_2D, 0);
		}
	);

	// SSAO random directions
	std::vector<mat::vec3> dirs;
	dirs.reserve(64);
	for (size_t i = 0; i < 64; i++) {
		dirs.push_back(
			mat::normal(mat::vec3{
				static_cast<float>(rand()) / RAND_MAX * 2.f - 1.f,
				static_cast<float>(rand()) / RAND_MAX * 2.f - 1.f,
				static_cast<float>(rand()) / RAND_MAX * 2.f - 1.f
				}
			)
		);
	}
	compositeProgram->setUniform("samplePoints", dirs);
	
	return true;
}

bool Render::initShadow()
{
	constexpr int shadowMap_x = 2048;
	constexpr int shadowMap_y = 2048;

	glCreateFramebuffers(1, &gObjects.shadowFBO);

	glCreateTextures(GL_TEXTURE_2D, 1, &gObjects.shadowTexture);
	glTextureStorage2D(gObjects.shadowTexture, 1, GL_DEPTH_COMPONENT32, shadowMap_x, shadowMap_y);
	glTextureParameteri(gObjects.shadowTexture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTextureParameteri(gObjects.shadowTexture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTextureParameteri(gObjects.shadowTexture, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
	glTextureParameteri(gObjects.shadowTexture, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
	glNamedFramebufferTexture(gObjects.shadowFBO, GL_DEPTH_ATTACHMENT, gObjects.shadowTexture, 0);

	if (glCheckNamedFramebufferStatus(gObjects.shadowFBO, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		return false;
	}

	shadowProgram = std::make_unique<GProgram>("shadow");
	shadowProgram->setFramebuffer(gObjects.shadowFBO);
	shadowProgram->setPreDrawRoutine([shadowMap_x, shadowMap_y] {
		glViewport(0, 0, shadowMap_x, shadowMap_y);
		glClear(GL_DEPTH_BUFFER_BIT);
		glEnable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(2.f, 2.f);
		}
	);
	shadowProgram->setReleaseRoutine([] {
		glDisable(GL_POLYGON_OFFSET_FILL);
		}
	);

	mat::mat4 view = lookAt(mat::vec3{ 2.f, 3.f, -0.5f }, mat::vec3{ 0.f, 0.f, 0.f });
	mat::mat4 proj = mat::ortho(-3.f, 3.f, -3.f, 3.f, -10.f, 10.f);
	mat::mat4 projectionViewMatrix = proj * view;
	shadowProgram->setUniform("viewProj", projectionViewMatrix);

	mat::mat4 biasMatrix{
		{ 0.5, 0.0, 0.0, 0.5 },
		{ 0.0, 0.5, 0.0, 0.5 },
		{ 0.0, 0.0, 0.5, 0.5 },
		{ 0.0, 0.0, 0.0, 1.0 }
	};
	mat::mat4 depthBiasMVP = biasMatrix * projectionViewMatrix;
	gbuffersProgram->setUniform("shadowViewProj", depthBiasMVP);

	return true;
}

bool Render::initUI()
{
	uiTexture = std::make_unique<GTexture>("ui");
	uiProgram = std::make_unique<GProgram>("ui");
	uiProgram->setPreDrawRoutine([this] {
		glDisable(GL_DEPTH_TEST);
		glEnable(GL_BLEND);
		glBindVertexArray(gObjects.rectVAO);
		glBindTexture(GL_TEXTURE_2D, uiTexture->id());
		}
	);
	uiProgram->setReleaseRoutine([] {
		glEnable(GL_DEPTH_TEST);
		glDisable(GL_BLEND);
		glBindTexture(GL_TEXTURE_2D, 0);
		}
	);

	return true;
}
