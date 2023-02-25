#ifndef _CROW_FRACTAL_SERVER_SYNCHRONOUS_HANDLER_
#define _CROW_FRACTAL_SERVER_SYNCHRONOUS_HANDLER_

#include <crow.h>

#include "handler.h"
#include "fractal_params.h"
#include "fractal_drawing.h"
#include "png_encoding.h"
#include "response.h"

class SynchronousHandler : public Handler {
 public:
  explicit SynchronousHandler(size_t num_threads) : coordinator_(num_threads) {}

  crow::response HandleParamsRequest(const FractalParams& params) override {
    // Just black-hole the request and respond with an ack.
    crow::json::wvalue json({{"request_id", params.request_id}});
    return crow::response(json);
  }

  crow::response HandleFractalRequest(const FractalParams& params) {
    std::string png;
    switch (params.precision.value_or(Precision::SINGLE)) {
    case Precision::SINGLE:
      png = GeneratePng<float>(params);
      break;
    case Precision::DOUBLE:
      png = GeneratePng<double>(params);
      break;
    }

    return ImageWithMetadata(std::move(png),
			     {{"data_id", params.request_id},
			      {"viewport_id", params.request_id}});
  }

private:
  template <typename T>
  std::string GeneratePng(const FractalParams& params) {
    std::cout << Now() << ": Start generating PNG" << std::endl;
    const uint64_t start_time = Now();

    // Set up the image.
    auto image = std::make_unique<RGBImage>(params.width, params.height);

    // Draw the fractal.
    const size_t total_iters = DrawFractal<T>({
	.params = params,
	.image = *image,
	.previous_params = previous_params_,
	.previous_image = previous_image_.get(),
	.coordinator = coordinator_,
      });
    const uint64_t end_time = Now();
    std::cout << "Total iterations: " << total_iters << std::endl;
    std::cout << "Computation time (ms): " << (end_time - start_time) << std::endl;

    // Encode to PNG.
    std::string png = EncodePng(params, *image);
    const uint64_t encode_time = Now();
    std::cout << "PNG encode time (ms): " << (encode_time - end_time) << std::endl;
    std::cout << "Total time (ms): " << (encode_time - start_time) << std::endl;

    // Prepare for next request.
    previous_image_ = std::move(image);
    previous_params_ = params;

    std::cout << Now() << ": Done generating PNG" << std::endl;
    return png;
  }

  Coordinator coordinator_;
  std::optional<FractalParams> previous_params_ = std::nullopt;
  std::unique_ptr<RGBImage> previous_image_ = nullptr;
};

#endif // _CROW_FRACTAL_SERVER_SYNCHRONOUS_HANDLER_
