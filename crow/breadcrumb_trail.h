#ifndef _CROW_FRACTAL_SERVER_BREADCRUMB_TRAIL_
#define _CROW_FRACTAL_SERVER_BREADCRUMB_TRAIL_

#include <map>
#include <memory>
#include <optional>
#include <cmath>
#include <mutex>

#include "fractal_params.h"
#include "rgb_image.h"

class BreadcrumbTrail {
 public:
  using Element = std::pair<FractalParams, std::shared_ptr<RGBImage>>;

  BreadcrumbTrail(size_t max_elements, double bucket_size)
    : max_elements_(max_elements), bucket_size_(bucket_size) {}

  void Insert(const Element& element) {
    std::scoped_lock lock(m_);

    // Clear everything if the params have changed fundamentally.
    if (last_inserted_params_.has_value() &&
	!ParamsDifferOnlyByViewport(element.first, *last_inserted_params_)) {
      elements_.clear();
    }

    // Actually do the insert.
    const int bucket = GetBucket(element.first);
    auto [it, inserted] = elements_.insert_or_assign(bucket, element);
    last_inserted_params_ = element.first;

    // Discard unneeded elements.
    elements_.erase(elements_.begin(), it);

    // Shrink to be within max size.
    if (elements_.size() > max_elements_) {
      auto it = elements_.end();
      --it;
      elements_.erase(it);
    }
  }

  std::optional<Element> GetNextLargest(const FractalParams& query) {
    std::scoped_lock lock(m_);
    const int query_bucket = GetBucket(query);
    auto it = elements_.lower_bound(query_bucket);
    while (it != elements_.end() && it->second.first.r_range < query.r_range) {
      ++it;
    }
    if (it == elements_.end()) {
      return std::nullopt;
    }
    if (!ParamsDifferOnlyByViewport(query, it->second.first)) {
      return std::nullopt;
    }
    return it->second;
  }

  void Clear() {
    std::scoped_lock lock(m_);
    elements_.clear();
    last_inserted_params_.reset();
  }

 private:
  int GetBucket(const FractalParams& params) const {
    double scale = log(params.r_range) / log(bucket_size_);
    return static_cast<int>(scale >= 0 ? scale + 0.5 : scale - 0.5);
  }

  const size_t max_elements_;
  const double bucket_size_;

  std::mutex m_;
  std::map<int, Element> elements_;
  std::optional<FractalParams> last_inserted_params_;
};

#endif // _CROW_FRACTAL_SERVER_BREADCRUMB_TRAIL_
