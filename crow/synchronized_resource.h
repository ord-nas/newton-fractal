#ifndef _CROW_FRACTAL_SERVER_SYNCHRONIZED_RESOURCE_
#define _CROW_FRACTAL_SERVER_SYNCHRONIZED_RESOURCE_

#include <optional>
#include <mutex>
#include <condition_variable>
#include <chrono>

template <typename T, typename V = uint64_t>
struct VersionedResource {
  T value;
  V version;
};

// Tri-state: dead, unset, or has versioned value.
// TODO probably can simplify.
template <typename T, typename V>
struct MaybeResource {
  std::optional<VersionedResource<T, V>> resource;
  bool still_alive = true;

  bool has_value() const { return resource.has_value(); }
  bool is_alive() const { return still_alive; }
  T& value() { return resource->value; }
  const T& value() const { return resource->value; }
  V version() const { return resource->version; }
  T& operator* () { return resource->value; }
  const T& operator* () const { return resource->value; }
  T* operator->() { return &resource->value; }
  const T* operator->() const { return &resource->value; }
};

struct Synchronizer {
  std::mutex m;
  std::condition_variable cv;
};

template <typename T, typename V = uint64_t>
class SynchronizedResourceBase {
 public:
  explicit SynchronizedResourceBase(Synchronizer* sync)
    : sync_(*sync) {}

  MaybeResource<T, V> Get() {
    std::scoped_lock lock(sync_.m);
    if (!still_alive) {
      return {.resource = std::nullopt, .still_alive = false};
    } else {
      return {.resource = resource_, .still_alive = true};
    }
  }

  MaybeResource<T, V> GetInitialized() {
    std::unique_lock lock(sync_.m);
    while (still_alive && !resource_.has_value()) {
      sync_.cv.wait(lock);
    }
    if (!still_alive) {
      return {.resource = std::nullopt, .still_alive = false};
    } else {
      return {.resource = resource_, .still_alive = true};
    }
  }

  MaybeResource<T, V> GetAboveVersion(V version) {
    std::unique_lock lock(sync_.m);
    while (still_alive && (!resource_.has_value() ||
			   resource_->version <= version)) {
      sync_.cv.wait(lock);
    }
    if (!still_alive) {
      return {.resource = std::nullopt, .still_alive = false};
    } else {
      return {.resource = resource_, .still_alive = true};
    }
  }

  template<typename Rep, typename Period>
  MaybeResource<T, V> GetAtVersionWithTimeout(V version,
					      std::chrono::duration<Rep, Period> timeout) {
    const auto now = std::chrono::system_clock::now();
    const auto deadline = now + timeout;
    bool deadline_expired = false;
    std::unique_lock lock(sync_.m);
    while (still_alive && !deadline_expired && (!resource_.has_value() ||
						resource_->version < version)) {
      deadline_expired = (sync_.cv.wait_until(lock, deadline) == std::cv_status::timeout);
    }
    if (!still_alive) {
      return {.resource = std::nullopt, .still_alive = false};
    } else if (deadline_expired) {
      return {.resource = std::nullopt, .still_alive = true};
    } else {
      return {.resource = resource_, .still_alive = true};
    }
  }

  bool Set(const T& value, V version) {
    bool changed = false;
    {
      std::scoped_lock lock(sync_.m);
      if (still_alive && (!resource_.has_value() ||
			  version >= resource_->version)) {
	resource_ = VersionedResource<T, V>{.value = value,
					    .version = version};
	changed = true;
      }
    }
    if (changed) {
      sync_.cv.notify_all();
    }
    return changed;
  }

  void Kill() {
    {
      std::scoped_lock lock(sync_.m);
      still_alive = false;
    }
    sync_.cv.notify_all();
  }

  void Reset() {
    std::scoped_lock lock(sync_.m);
    resource_.reset();
    still_alive = true;
  }

  template <typename T1, typename T2, typename VOther>
  friend class SynchronizedResourcePair;

 private:
  // Unowned.
  Synchronizer& sync_;

  std::optional<VersionedResource<T, V>> resource_;
  bool still_alive = true;
};

template <typename T, typename V = uint64_t>
class SynchronizedResource : public SynchronizedResourceBase<T, V> {
 public:
  explicit SynchronizedResource()
    : SynchronizedResourceBase<T, V>(&owned_sync_) {}

 private:
  Synchronizer owned_sync_;
};

template <typename T1, typename T2, typename V = uint64_t>
class SynchronizedResourcePair {
 public:
  explicit SynchronizedResourcePair()
    : first_(&owned_sync_), second_(&owned_sync_) {}

  SynchronizedResourceBase<T1, V>& first() {
    return first_;
  }

  SynchronizedResourceBase<T2, V>& second() {
    return second_;
  }

  std::pair<MaybeResource<T1, V>, MaybeResource<T2, V>>
  GetBothWithAtLeastOneAboveVersion(V first_version, V second_version) {
    std::unique_lock lock(owned_sync_.m);
    while (first_.still_alive && second_.still_alive &&
	   (!first_.resource_.has_value() || !second_.resource_.has_value() ||
	    (first_.resource_->version <= first_version &&
	     second_.resource_->version <= second_version))) {
      owned_sync_.cv.wait(lock);
    }
    MaybeResource<T1, V> first_result;
    if (!first_.still_alive) {
      first_result = {.resource = std::nullopt, .still_alive = false};
    } else {
      first_result = {.resource = first_.resource_, .still_alive = true};
    }
    MaybeResource<T2, V> second_result;
    if (!second_.still_alive) {
      second_result = {.resource = std::nullopt, .still_alive = false};
    } else {
      second_result = {.resource = second_.resource_, .still_alive = true};
    }
    return std::make_pair(first_result, second_result);
  }

  void Kill() {
    first_.Kill();
    second_.Kill();
  }

  void Reset() {
    first_.Reset();
    second_.Reset();
  }

 private:
  Synchronizer owned_sync_;
  SynchronizedResourceBase<T1, V> first_;
  SynchronizedResourceBase<T2, V> second_;
};

#endif // _CROW_FRACTAL_SERVER_SYNCHRONIZED_RESOURCE_
