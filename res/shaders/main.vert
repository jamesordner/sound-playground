#version 450
#extension GL_KHR_vulkan_glsl : enable

layout(binding = 0) uniform Dynamics {
    mat4 modelViewMatrix;
};

layout(binding = 1) uniform Constants {
    mat4 projectionMatrix;
};

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec3 lightDir;

void main()
{
    gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);
	
	mat4 modelViewRotationMatrix = transpose(inverse(modelViewMatrix));
	fragNormal = vec3(modelViewRotationMatrix * vec4(normal, 0));
	lightDir = vec3(modelViewRotationMatrix * vec4(normalize(vec3(.3, 1, .1)), 0));
}
