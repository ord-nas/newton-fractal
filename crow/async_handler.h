#ifndef _CROW_FRACTAL_SERVER_ASYNC_HANDLER_
#define _CROW_FRACTAL_SERVER_ASYNC_HANDLER_

#include <chrono>

#include <crow.h>

#include "handler.h"
#include "fractal_params.h"
#include "fractal_drawing.h"
#include "png_encoding.h"
#include "response.h"
#include "thread_pool.h"
#include "synchronized_resource.h"
#include "image_regions.h"
#include "image_operations.h"

std::shared_ptr<RGBImage> LayoutImage(const RGBImage& input_image,
				      const FractalParams& image_params,
				      const FractalParams& viewport_params) {
  auto output_image = std::make_shared<RGBImage>(viewport_params.width, viewport_params.height);
  const std::optional<ImageOverlap> overlap = FindGeneralImageOverlap(image_params, viewport_params);
  if (overlap.has_value()) {
    ResizeBilinear(input_image, *output_image, *overlap);
  }
  return output_image;
}

class AsyncHandler : public Handler {
 public:
  explicit AsyncHandler(ThreadPool* thread_pool) : thread_pool_(*thread_pool) {
    Start();
  }

  ~AsyncHandler() {
    Stop();
  }

  crow::response HandleParamsRequest(const FractalParams& params) override {
    SetSessionId(params.session_id);
    std::cout << "HandleParamsRequest putting version: " << params.request_id << std::endl;
    latest_params().Set(params, /*version=*/params.request_id);
    crow::json::wvalue json({{"request_id", params.request_id}});
    return crow::response(json);
  }

  crow::response HandleFractalRequest(const FractalParams& params) {
    SetSessionId(params.session_id);
    std::cout << "HandleFractalRequest putting version: " << params.request_id << std::endl;
    latest_params().Set(params, /*version=*/params.request_id);
    std::cout << "HandleFractalRequest waiting for above version: ("
	      << params.last_data_id << ", "
	      << params.last_viewport_id << ")" << std::endl;
    ImageVersion last_version(params.last_data_id, params.last_viewport_id);
    auto png = latest_png_.GetAboveVersion(last_version);
    if (!png.has_value()) {
      std::cout << "PNG resource is dead :(" << std::endl;
      return crow::response(500);
    }

    return ImageWithMetadata(**png,
			     {{"data_id", png.version().first},
			      {"viewport_id", png.version().second}});
  }

 private:
  // first is data version, second is viewport version.
  using ImageVersion = std::pair<uint64_t, uint64_t>;

  struct EncodeInput {
    std::shared_ptr<RGBImage> image;
    FractalParams image_params;
    FractalParams viewport_params;
  };

  void Start() {
    latest_params_and_image_.Reset();
    latest_png_.Reset();
    computation_thread_ = std::make_unique<boost::thread>(&AsyncHandler::ComputeLoop, this);
    layout_thread_ = std::make_unique<boost::thread>(&AsyncHandler::LayoutLoop, this);
  }

  void Stop() {
    latest_params_and_image_.Kill();
    latest_png_.Kill();
    computation_thread_->join();
    layout_thread_->join();
  }

  void Reset() {
    Stop();
    Start();
  }

  void SetSessionId(const std::string& new_id) {
    if (!session_id_.empty() && new_id != session_id_) {
      Reset();
    }
    session_id_ = new_id;
  }

  SynchronizedResourceBase<FractalParams>& latest_params() {
    return latest_params_and_image_.first();
  }

  SynchronizedResourceBase<std::pair<FractalParams, std::shared_ptr<RGBImage>>>& latest_image() {
    return latest_params_and_image_.second();
  }

  void ComputeLoop() {
    uint64_t latest_version = 0;
    std::optional<FractalParams> previous_params = std::nullopt;
    std::shared_ptr<RGBImage> previous_image = nullptr;

    while (true) {
      std::cout << "ComputeLoop start, waiting for above version: " << latest_version << std::endl;
      auto input = latest_params().GetAboveVersion(latest_version);
      if (!input.has_value()) {
	std::cout << "Killing AsyncHandler::ComputeLoop, params are dead" << std::endl;
	return;
      }
      std::cout << "ComputeLoop got version: " << input.version() << std::endl;

      std::cout << Now() << ": Start async computation" << std::endl;
      const uint64_t start_time = Now();

      // Set up the image.
      auto image = std::make_shared<RGBImage>(input->width, input->height);

      // Draw the fractal.
      DrawFractalArgs args = {
	.params = *input,
	.image = *image,
	.previous_params = previous_params,
	.previous_image = previous_image.get(),
	.thread_pool = thread_pool_,
      };
      const size_t total_iters = DrawFractal(args);
      const uint64_t end_time = Now();
      std::cout << "Total iterations: " << total_iters << std::endl;
      std::cout << "Computation time (ms): " << (end_time - start_time) << std::endl;

      // Push out the results.
      latest_image().Set(std::make_pair(*input, image),
			 /*version=*/input.version());
      previous_image = image;
      previous_params = *input;
      latest_version = input.version();
      std::cout << "ComputeLoop done" << std::endl;
    }
  }

  void LayoutLoop() {
    uint64_t latest_data_version = 0;
    uint64_t latest_viewport_version = 0;

    while (true) {
      std::cout << "LayoutLoop start, waiting for viewport params above version: "
		<< latest_viewport_version << " or image above version "
		<< latest_data_version << std::endl;

      const auto encode_input = ComputeEncodeInput(latest_data_version, latest_viewport_version);
      if (!encode_input.has_value()) {
	// We've been killed.
	return;
      }

      // Encode to PNG.
      const uint64_t start_time = Now();
      auto png = std::make_shared<std::string>();
      *png = EncodePng(encode_input->viewport_params, *encode_input->image);
      const uint64_t end_time = Now();
      std::cout << "PNG encode time (ms): " << (end_time - start_time) << std::endl;

      // Push out the results.
      latest_data_version = encode_input->image_params.request_id;
      latest_viewport_version = encode_input->viewport_params.request_id;
      latest_png_.Set(png, /*version=*/std::make_pair(latest_data_version,
						      latest_viewport_version));
      std::cout << "LayoutLoop done" << std::endl;
    }
  }

  std::optional<EncodeInput> ComputeEncodeInput(uint64_t latest_data_version,
						uint64_t latest_viewport_version) {
    using namespace std::chrono_literals;

    // Get new params.
    auto [viewport_input, image_input] =
      latest_params_and_image_.GetBothWithAtLeastOneAboveVersion(latest_viewport_version,
								 latest_data_version);
    if (!viewport_input.has_value()) {
      std::cout << "Killing AsyncHandler::LayoutLoop, params are dead" << std::endl;
      return std::nullopt;
    }
    if (!image_input.has_value()) {
      std::cout << "Killing AsyncHandler::LayoutLoop, image is dead" << std::endl;
      return std::nullopt;
    }

    // Pull out the retrieved input.
    FractalParams viewport_params = *viewport_input;
    std::cout << "LayoutLoop got viewport params at version: "
	      << viewport_input.version() << std::endl;
    auto [image_params, image] = *image_input;
    std::cout << "LayoutLoop got image at version: "
	      << image_input.version() << std::endl;

    // If the image and viewport versions are identical, no need to do any work.
    if (image_params.request_id == viewport_params.request_id) {
      std::cout << "Versions identical, no layout required." << std::endl;
      return EncodeInput{
	.image = image,
	.image_params = image_params,
	.viewport_params = viewport_params,
      };
    }

    // If the viewport is only panned, then give the compute loop a bit of extra
    // time to catch up, since it might not be long.
    if (ParamsDifferOnlyByPanning(viewport_params, image_params)) {
      std::cout << "Versions differ only by panning, try wait." << std::endl;
      auto updated_image = latest_image().GetAtVersionWithTimeout(viewport_params.request_id, 50ms);
      if (updated_image.has_value()) {
	std::cout << "Wait success!" << std::endl;
	auto [new_params, new_image] = *updated_image;
	return EncodeInput{
	  .image = new_image,
	  .image_params = new_params,
	  .viewport_params = new_params,
	};
      }
    }

    // If the params differ only by viewport, take the latest image and just adjust for viewport change.
    if (ParamsDifferOnlyByViewport(viewport_params, image_params)) {
      std::cout << "Params differ only by viewport, performing layout." << std::endl;
      const uint64_t start_time = Now();
      auto stitched_image = LayoutImage(*image, image_params, viewport_params);
      const uint64_t end_time = Now();
      std::cout << "Layout time (ms): " << (end_time - start_time) << std::endl;
      return EncodeInput{
	.image = stitched_image,
	.image_params = image_params,
	.viewport_params = viewport_params,
      };
    }

    // Fall back on pipelined behaviour... wait for a usable image to be available.
    std::cout << "Fundamental param change, reverting to pipelined behavior." << std::endl;
    image_input = latest_image().GetAboveVersion(latest_data_version);
    if (!image_input.has_value()) {
      std::cout << "Killing AsyncHandler::LayoutLoop, image is dead" << std::endl;
      return std::nullopt;
    }
    auto [new_params, new_image] = *image_input;
    std::cout << "LayoutLoop got fallback image at version: "
	      << image_input.version() << std::endl;
    return EncodeInput{
      .image = new_image,
      .image_params = new_params,
      .viewport_params = new_params,
    };
  }

  // Unowned.
  ThreadPool& thread_pool_;

  std::string session_id_;
  std::unique_ptr<boost::thread> computation_thread_;
  std::unique_ptr<boost::thread> layout_thread_;

  SynchronizedResourcePair<FractalParams,
			   std::pair<FractalParams,
				     std::shared_ptr<RGBImage>>>
      latest_params_and_image_;
  SynchronizedResource<std::shared_ptr<std::string>, ImageVersion> latest_png_;
};

#endif // _CROW_FRACTAL_SERVER_ASYNC_HANDLER_
