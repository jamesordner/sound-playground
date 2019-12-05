#include "AudioComponent.h"
#include "../Engine/EObject.h"
#include "ADelayLine.h"

AudioComponent::AudioComponent() :
	bAcceptsInput(false),
	bAcceptsOutput(false),
	bDirtyTransform(false),
	sampleRate(0.f),
	channels(0)
{
}

void AudioComponent::init(float sampleRate, size_t channels, size_t bufferSize)
{
	this->sampleRate = sampleRate;
	this->channels = channels;
	indirectInputBuffer.resize(bufferSize);
	for (const auto& input : inputs) input->init(sampleRate);
	for (const auto& output : outputs) output->init(sampleRate);
}

const mat::vec3& AudioComponent::position() const
{
	return m_position;
}

const mat::vec3& AudioComponent::forward() const
{
	return m_forward;
}

void AudioComponent::transformUpdated()
{
	bDirtyTransform = true;
	for (const auto& output : outputs) {
		auto ptr = output->dest.lock();
		ptr->otherTransformUpdated(*output, true);
	}
	for (const auto& input : inputs) {
		auto ptr = input->dest.lock();
		ptr->otherTransformUpdated(*input, false);
	}
}

size_t AudioComponent::pullCount()
{
	size_t shortest = SIZE_MAX;
	for (const auto& input : inputs) {
		size_t count = input->readable();
		if (count < shortest) shortest = count;
	}
	return shortest;
}
