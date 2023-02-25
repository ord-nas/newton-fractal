#ifndef _CROW_FRACTAL_SERVER_PIPELINED_HANDLER_
#define _CROW_FRACTAL_SERVER_PIPELINED_HANDLER_

#include <crow.h>

#include "handler.h"
#include "fractal_params.h"
#include "fractal_drawing.h"
#include "png_encoding.h"
#include "response.h"
#include "thread_pool.h"
#include "synchronized_resource.h"

class PipelinedHandler : public Handler {
 public:
  explicit PipelinedHandler(ThreadPool* thread_pool) : thread_pool_(*thread_pool) {
    Start();
  }

  ~PipelinedHandler() {
    Stop();
  }

  crow::response HandleParamsRequest(const FractalParams& params) override {
    SetSessionId(params.session_id);
    std::cout << "HandleParamsRequest putting version: " << params.request_id << std::endl;
    latest_params_.Set(/*version=*/params.request_id, params);
    crow::json::wvalue json({{"request_id", params.request_id}});
    return crow::response(json);
  }

  crow::response HandleFractalRequest(const FractalParams& params) {
    SetSessionId(params.session_id);
    std::cout << "HandleFractalRequest putting version: " << params.request_id << std::endl;
    latest_params_.Set(/*version=*/params.request_id, params);
    std::cout << "HandleFractalRequest waiting for above version: " << params.last_data_id << std::endl;
    auto png = latest_png_.GetAboveVersion(params.last_data_id);
    if (!png.has_value()) {
      std::cout << "PNG resource is dead :(" << std::endl;
      return crow::response(500);
    }

    return ImageWithMetadata(*png.value(),
			     {{"data_id", png.version()},
			      {"viewport_id", png.version()}});
  }

private:
  void Start() {
    latest_params_.Reset();
    latest_image_.Reset();
    latest_png_.Reset();
    computation_thread_ = std::make_unique<boost::thread>(&PipelinedHandler::ComputeLoop, this);
    encoding_thread_ = std::make_unique<boost::thread>(&PipelinedHandler::EncodeLoop, this);
  }

  void Stop() {
    latest_params_.Kill();
    latest_image_.Kill();
    latest_png_.Kill();
    computation_thread_->join();
    encoding_thread_->join();
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

  void ComputeLoop() {
    uint64_t latest_version = 0;
    std::optional<FractalParams> previous_params = std::nullopt;
    std::shared_ptr<RGBImage> previous_image = nullptr;

    while (true) {
      std::cout << "ComputeLoop start, waiting for above version: " << latest_version << std::endl;
      auto input = latest_params_.GetAboveVersion(latest_version);
      if (!input.has_value()) {
	std::cout << "Killing PipelinedHandler::ComputeLoop, params are dead" << std::endl;
	return;
      }
      std::cout << "ComputeLoop got version: " << input.version() << std::endl;

      std::cout << Now() << ": Start pipelined computation" << std::endl;
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
      latest_image_.Set(/*version=*/input.version(), std::make_pair(*input, image));
      previous_image = image;
      previous_params = *input;
      latest_version = input.version();
      std::cout << "ComputeLoop done" << std::endl;
    }
  }

  void EncodeLoop() {
    uint64_t latest_version = 0;

    while (true) {
      std::cout << "EncodeLoop start, waiting for above version: " << latest_version << std::endl;
      auto input = latest_image_.GetAboveVersion(latest_version);
      if (!input.has_value()) {
	std::cout << "Killing PipelinedHandler::EncodeLoop, image is dead" << std::endl;
	return;
      }
      const auto [params, image] = *input;
      std::cout << "EncodeLoop got version: " << input.version() << std::endl;

      std::cout << Now() << ": Start pipelined encoding" << std::endl;
      const uint64_t start_time = Now();

      // Prepare the output string.
      auto png = std::make_shared<std::string>();

      // Encode to PNG.
      *png = EncodePng(params, *image);
      const uint64_t end_time = Now();
      std::cout << "PNG encode time (ms): " << (end_time - start_time) << std::endl;

      // Push out the results.
      latest_png_.Set(/*version=*/input.version(), png);
      latest_version = input.version();
      std::cout << "EncodeLoop done" << std::endl;
    }
  }

  // Unowned.
  ThreadPool& thread_pool_;

  std::string session_id_;
  std::unique_ptr<boost::thread> computation_thread_;
  std::unique_ptr<boost::thread> encoding_thread_;

  SynchronizedResource<FractalParams> latest_params_;
  SynchronizedResource<std::pair<FractalParams,
				 std::shared_ptr<RGBImage>>> latest_image_;
  SynchronizedResource<std::shared_ptr<std::string>> latest_png_;
};

#endif // _CROW_FRACTAL_SERVER_PIPELINED_HANDLER_
