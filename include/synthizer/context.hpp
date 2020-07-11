#pragma once

#include "synthizer/base_object.hpp"
#include "synthizer/invokable.hpp"
#include "synthizer/panner_bank.hpp"
#include "synthizer/property_internals.hpp"
#include "synthizer/property_ring.hpp"
#include "synthizer/spatialization_math.hpp"
#include "synthizer/types.hpp"

#include "concurrentqueue.h"
#include "sema.h"

#include <atomic>
#include <array>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>

namespace synthizer {

class AudioOutput;
class Source;

/*
 * Infrastructure for deletion.
 * */
template<typename T>
void deletionCallback(void *p) {
	T *y = (T*) p;
	delete y;
}

/*
 * The context is the main entrypoint to Synthizer, holding the device, etc.
 * 
 *This has a few responsibilities:
 * - Dispatch callables to a high priority multimedia thread, with tagged priorities.
 * - Hold and orchestrate the lifetime of an audio device.
 * - Hold library-global parameters such as the listener's position and orientation, and configurable default values for things such as the distance model, speed of sound, etc.
 * - Handle memory allocation and freeing as necessary.
 * 
 * Users of synthizer will typically make one context per audio device they wish to use.
 * 
 * Unless otherwise noted, the functions of this class should only be called from the context-managed thread. External callers can submit invokables to run code on that thread, but 
 * since this is audio generation, the context needs full control over the priority of commands.
 * 
 * Later, if necessary, we'll extend synthizer to use atomics for some properties.
 * */
class Context: public BaseObject, public DistanceParamsMixin, public std::enable_shared_from_this<Context> {
	public:

	Context();
	/*
	 * Initialization occurs in two phases. The constructor does almost nothing, then this is called.
	 * 
	 * Why is because it is unfortunately necessary for the audio thread from miniaudio to hold a weak_ptr, which needs us to be able to use shared_from_this.
	 * */
	void initContext();

	~Context();

	std::shared_ptr<Context> getContext() override;
	Context *getContextRaw() override {
		return this;
	}


	/*
	 * Shut the context down.
	 * 
	 * This kills the audio thread.
	 * */
	void shutdown();
	void cDelete() override;

	/*
	 * Submit an invokable which will be invoked on the context thread.
	 * */
	void enqueueInvokable(Invokable *invokable);

	/*
	 * Call a callable in the audio thread.
	 * Convenience method to not have to make invokables everywhere.
	 * */
	template<typename C, typename... ARGS>
	auto call(C &&callable, ARGS&& ...args) {
		auto invokable = WaitableInvokable([&]() {
			return callable(args...);
		});
		this->enqueueInvokable(&invokable);
		return invokable.wait();
	}

	template<typename T, typename... ARGS>
	std::shared_ptr<T> createObject(ARGS&& ...args) {
		auto obj = new T(this->shared_from_this(), args...);
		std::shared_ptr<T> ret(obj, [] (T *ptr) {
			auto ctx = ptr->getContextRaw();
			if (ctx->delete_directly.load(std::memory_order_relaxed) == 1) ctx->enqueueDeletionRecord(&deletionCallback<T>, (void *)ptr);
			else delete ptr;
		});

		/* Do the second phase of initialization. */
		this->call([&] () {
			obj->initInAudioThread();
		});
		return ret;
	}

	/*
	 * Helpers for the C API. to get/set properties in the context's thread.
	 * These create and manage the invokables and can be called directly.
	 * 
	 * Eventually this will be extended to handle batched/deferred things as well.
	 * */
	int getIntProperty(std::shared_ptr<BaseObject> &obj, int property);
	void setIntProperty(std::shared_ptr<BaseObject> &obj, int property, int value);
	double getDoubleProperty(std::shared_ptr<BaseObject> &obj, int property);
	void setDoubleProperty(std::shared_ptr<BaseObject> &obj, int property, double value);
	std::shared_ptr<BaseObject> getObjectProperty(std::shared_ptr<BaseObject> &obj, int property);
	void setObjectProperty(std::shared_ptr<BaseObject> &obj, int property, std::shared_ptr<BaseObject> &object);
	std::array<double, 3> getDouble3Property(std::shared_ptr<BaseObject> &obj, int property);
	void setDouble3Property(std::shared_ptr<BaseObject> &obj, int property, std::array<double, 3> value);
	std::array<double, 6>  getDouble6Property(std::shared_ptr<BaseObject> &obj, int property);
	void setDouble6Property(std::shared_ptr<BaseObject> &obj, int property, std::array<double, 6> value);

	/*
	 * Ad a weak reference to the specified source.
	 * */
	void registerSource(std::shared_ptr<Source> &source);

	/*
	 * The properties for the listener.
	 * */
	std::array<double, 3> getPosition();
	void setPosition(std::array<double, 3> pos);
	std::array<double, 6> getOrientation();
	void setOrientation(std::array<double, 6> orientation);

	/* Helper methods used by various pieces of synthizer to grab global resources. */
	/* Allocate a panner lane intended to be used by a source. */
	std::shared_ptr<PannerLane> allocateSourcePannerLane(enum SYZ_PANNER_STRATEGY strategy);

	PROPERTY_METHODS
	private:
	/*
	 * Flush all pending roperty writes.
	 * */
	void flushPropertyWrites();

	/*
	 * Generate a block of audio output for the specified number of channels.
	 * 
	 * The number of channels shouldn't change for the duration of this context in most circumstances.
	 * */
	void generateAudio(unsigned int channels, AudioSample *output);

	/*
	 * The audio thread itself.
	 * */
	void audioThreadFunc();

	moodycamel::ConcurrentQueue<Invokable *> pending_invokables;
	std::thread context_thread;
	/*
	 * Wake the context thread, either because a command was submitted or a block of audio was removed.
	 * */
	Semaphore context_semaphore;
	std::atomic<int> running;
	std::shared_ptr<AudioOutput> audio_output;

	/*
	 * Deletion. This queue is read from when the semaphore for the context is incremented.
	 * 
	 * Objects are safe to delete when the iteration of the context at which the deletion was enqueued is greater.
	 * This means that all shared_ptr decremented in the previous iteration, and all weak_ptr were invalidated.
	 * */
	typedef void (*DeletionCallback)(void *);
	class DeletionRecord {
		public:
		uint64_t iteration;
		DeletionCallback callback;
		void *arg;
	};
	moodycamel::ConcurrentQueue<DeletionRecord> pending_deletes;

	std::atomic<int> delete_directly = 0;
	/*
	 * Used to signal that something is queueing a delete. This allows
	 * shutdown to synchronize by spin waiting, so that when it goes to drain the deletion queue, it can know that nothing else will appear in it.
	 * */
	std::atomic<int> deletes_in_progress = 0;

	void enqueueDeletionRecord(DeletionCallback cb, void *arg);
	/* Used by shutdown and the destructor only. Not safe to call elsewhere. */
	void drainDeletionQueues();

	PropertyRing<1024> property_ring;
	template<typename T>
	void propertySetter(const std::shared_ptr<BaseObject> &obj, int property, T &value);

	/* Collections of objects that require execution: sources, etc. all go here eventually. */

	/* The key is a raw pointer for easy lookup. */
	std::unordered_map<void *, std::weak_ptr<Source>> sources;
	std::shared_ptr<AbstractPannerBank> source_panners = nullptr;

	/* Parameters of the 3D environment: listener orientation/position, library-wide defaults for distance models, etc. */
	std::array<double, 3> position{ { 0, 0, 0 } };
	/* Default to facing positive y with positive x as east and positive z as up. */
	std::array<double, 6> orientation{ { 0, 1, 0, 0, 0, 1 } };
};

}