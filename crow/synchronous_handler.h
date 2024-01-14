#ifndef _CROW_FRACTAL_SERVER_SYNCHRONOUS_HANDLER_
#define _CROW_FRACTAL_SERVER_SYNCHRONOUS_HANDLER_

#include <crow.h>
#include <fstream>
#include <iostream>
#include <dirent.h>
#include <string>
#include <vector>
#include <optional>

#include "handler.h"
#include "fractal_params.h"
#include "fractal_drawing.h"
#include "png_encoding.h"
#include "response.h"
#include "thread_pool.h"

static constexpr char image_directory[] = "/mnt/c/Users/young/Documents/Newton Fractal Saved Images/";
static constexpr char metadata_suffix[] = "_metadata.txt";

class SynchronousHandler : public Handler {
 public:
  explicit SynchronousHandler(ThreadPool* thread_pool) : thread_pool_(*thread_pool) {}

  crow::response HandleParamsRequest(const FractalParams& params) override {
    // Just black-hole the params and respond with an ack.
    crow::json::wvalue json({{"request_id", params.request_id}});
    return crow::response(json);
  }

  crow::response HandleFractalRequest(const FractalParams& params) override {
    std::string png = GeneratePng(params);
    return ImageWithMetadata(std::move(png),
			     {{"data_id", params.request_id},
			      {"viewport_id", params.request_id}});
  }

  crow::response HandleSaveRequest(const SaveParams& params) {
    // Render the fractal, potentially at increased scale.
    FractalParams render_params = params.fractal_params;
    render_params.width *= params.scale;
    render_params.height *= params.scale;
    std::string png = GeneratePng(render_params);

    // Construct the file path to write to.
    std::string path = std::string(image_directory) + params.filename;

    // Construct a default-successful response.
    crow::json::wvalue json({{"success", true}});
    crow::response save_response(json);

    // Try to write the PNG and metadata.
    if (!SavePng(png, path, &save_response)) {
      return save_response;
    }
    if (!SaveMetadata(params.metadata, path, &save_response)) {
      return save_response;
    }

    return save_response;
  }

  crow::response HandleLoadRequest(const LoadParams& params) {
    // Construct the file path to read from.
    std::string path = std::string(image_directory) + params.filename + std::string(metadata_suffix);

    // Try to open the metadata file.
    std::ifstream infile(path);
    if (!infile.good()) {
      crow::json::wvalue json({{"success", false},
			       {"error_message", "Could not open file: " + path}});
      return crow::response(json);
    }

    // Read the full contents of the file.
    std::ostringstream ss;
    ss << infile.rdbuf();
    const std::string metadata = ss.str();

    // Respond.
    crow::json::wvalue json({{"success", true},
			     {"metadata", metadata}});
    return crow::response(json);
  }

  crow::response HandleListImagesRequest() {
    const std::optional<std::vector<std::string>> files = GetDirectoryContents(image_directory);

    // If we failed to load files, return an error message.
    if (!files.has_value()) {
      crow::json::wvalue json({{"success", false},
			       {"error_message",
				"Can't list directory contents of " + std::string(image_directory)}});
      return crow::response(json);
    }

    // Filter to just the files that have metadata.
    std::vector<crow::json::wvalue> images_with_metadata;
    for (const std::string file : *files) {
      const std::optional<std::string> prefix = RemoveEnding(file, std::string(metadata_suffix));
      if (prefix.has_value()) {
	images_with_metadata.emplace_back(*prefix);
      }
    }

    // Construct a response.
    crow::json::wvalue json({{"success", true},
			     {"images", images_with_metadata}});
    return crow::response(json);
  }

 private:
  std::string GeneratePng(const FractalParams& params) {
    std::cout << Now() << ": Start generating PNG" << std::endl;
    const uint64_t start_time = Now();

    // Set up the image.
    auto image = std::make_unique<RGBImage>(params.width, params.height);

    // Draw the fractal.
    const size_t total_iters = DrawFractal({
	.params = params,
	.image = *image,
	.previous_params = previous_params_,
	.previous_image = previous_image_.get(),
	.thread_pool = thread_pool_,
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

  // Returns true on success; populates *response on error.
  bool SavePng(const std::string& png, const std::string& base_path, crow::response* response) {
    std::string path = base_path + ".png";

    // Check that the file doesn't already exist.
    std::ifstream infile(path, std::ios_base::binary);
    if (infile.good()) {
      crow::json::wvalue json({{"success", false},
			       {"error_message", "File already exists: " + path}});
      *response = crow::response(json);
      return false;
    }
    infile.close();

    // Try to write the PNG.
    std::ofstream outfile(path, std::ios_base::binary);
    if (!outfile.good()) {
      crow::json::wvalue json({{"success", false},
			       {"error_message", "Could not open file: " + path}});
      *response = crow::response(json);
      return false;
    }
    outfile.write(png.c_str(), png.size());
    if (!outfile.good()) {
      crow::json::wvalue json({{"success", false},
			       {"error_message", "Write failed"}});
      *response = crow::response(json);
      return false;
    }
    outfile.close();
    if (!outfile.good()) {
      crow::json::wvalue json({{"success", false},
			       {"error_message", "Close failed"}});
      *response = crow::response(json);
      return false;
    }

    return true;
  }

  // Returns true on success; populates *response on error.
  bool SaveMetadata(const std::string& metadata, const std::string& base_path, crow::response* response) {
    std::string path = base_path + std::string(metadata_suffix);

    // Try to write the metadata.
    std::ofstream outfile(path);
    if (!outfile.good()) {
      crow::json::wvalue json({{"success", false},
			       {"error_message", "Could not open metadata file: " + path}});
      *response = crow::response(json);
      return false;
    }
    outfile << metadata;
    if (!outfile.good()) {
      crow::json::wvalue json({{"success", false},
			       {"error_message", "Metadata write failed"}});
      *response = crow::response(json);
      return false;
    }
    outfile.close();
    if (!outfile.good()) {
      crow::json::wvalue json({{"success", false},
			       {"error_message", "Metadata close failed"}});
      *response = crow::response(json);
      return false;
    }

    return true;
  }

  std::optional<std::vector<std::string>> GetDirectoryContents(const char* directory) {
    DIR *dir = opendir(directory);
    if (dir == nullptr) {
      return std::nullopt;
    }

    std::vector<std::string> files;
    struct dirent *ent;
    while ((ent = readdir(dir)) != nullptr) {
      files.push_back(ent->d_name);
    }

    closedir(dir);
    return files;
  }

  // If `full` ends with `ending`, returns `full` with `ending` removed.
  // Otherwise, returns nullopt.
  static std::optional<std::string> RemoveEnding(const std::string& full, const std::string& ending) {
    if (full.size() < ending.size()) {
      return std::nullopt;
    }

    if (full.compare(full.size() - ending.size(), ending.size(), ending) != 0) {
      return std::nullopt;
    }

    return full.substr(0, full.size() - ending.size());
  }

  // Unowned.
  ThreadPool& thread_pool_;

  std::optional<FractalParams> previous_params_ = std::nullopt;
  std::unique_ptr<RGBImage> previous_image_ = nullptr;
};

#endif // _CROW_FRACTAL_SERVER_SYNCHRONOUS_HANDLER_
