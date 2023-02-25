#ifndef _CROW_FRACTAL_SERVER_SYNCHRONIZED_RESOURCE_
#define _CROW_FRACTAL_SERVER_SYNCHRONIZED_RESOURCE_

#include <optional>
#include <mutex>
#include <condition_variable>

template <typename T, typename V = uint64_t>
struct VersionedResource {
  T value;
  V version;
};

// Tri-state: dead, unset, or has versioned value.
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

template <typename T, typename V = uint64_t>
class SynchronizedResource {
 public:
  MaybeResource<T, V> Get() {
    std::scoped_lock lock(m_);
    if (!still_alive) {
      return {.resource = std::nullopt, .still_alive = false};
    } else {
      return {.resource = resource_, .still_alive = true};
    }
  }

  MaybeResource<T, V> GetAboveVersion(V version) {
    std::unique_lock lock(m_);
    while (still_alive && (!resource_.has_value() ||
			   resource_->version <= version)) {
      cv_.wait(lock);
    }
    if (!still_alive) {
      return {.resource = std::nullopt, .still_alive = false};
    } else {
      return {.resource = resource_, .still_alive = true};
    }
  }

  bool Set(V version, const T& value) {
    bool changed = false;
    {
      std::scoped_lock lock(m_);
      if (still_alive && (!resource_.has_value() ||
			  version >= resource_->version)) {
	resource_ = VersionedResource<T, V>{.value = value,
					    .version = version};
	changed = true;
      }
    }
    if (changed) {
      cv_.notify_all();
    }
    return changed;
  }

  void Kill() {
    {
      std::scoped_lock lock(m_);
      still_alive = false;
    }
    cv_.notify_all();
  }

  void Reset() {
    std::scoped_lock lock(m_);
    resource_.reset();
    still_alive = true;
  }

 private:
  std::optional<VersionedResource<T, V>> resource_;
  std::mutex m_;
  std::condition_variable cv_;
  bool still_alive = true;
};

#endif // _CROW_FRACTAL_SERVER_SYNCHRONIZED_RESOURCE_
