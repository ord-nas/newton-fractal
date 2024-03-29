#ifndef _CROW_FRACTAL_SERVER_FRACTAL_PARAMS_
#define _CROW_FRACTAL_SERVER_FRACTAL_PARAMS_

#include <string>
#include <optional>

#include <png++/png.hpp>
#include <crow.h>

#include "complex.h"

enum class Precision {
  SINGLE,
  DOUBLE,
};

enum class Strategy {
  NAIVE,
  DYNAMIC_BLOCK,
  DYNAMIC_BLOCK_THREADED,
  DYNAMIC_BLOCK_THREADED_INCREMENTAL,
};

enum class PngEncoder {
  PNGPP,
  FPNG,
};

enum class HandlerType {
  SYNCHRONOUS,
  PIPELINED,
  ASYNCHRONOUS,
};

bool ParseNonEmptyString(const crow::query_string& url_params,
			 const std::string& key,
			 std::string* output) {
  const char* value = url_params.get(key);
  if (value == nullptr) {
    return false;
  }
  std::string s(value);
  if (s.empty()) {
    return false;
  }
  *output = s;
  return true;
}

bool ToInt(const char* c_str,
	   int* output,
	   std::optional<int> min_value = std::nullopt,
	   std::optional<int> max_value = std::nullopt) {
  std::string s(c_str);

  int num = 0;
  size_t chars_processed = 0;
  try {
    num = std::stoi(s, &chars_processed);
  } catch (std::invalid_argument const& ex) {
    return false;
  } catch(std::out_of_range const& ex) {
    return false;
  }

  if (chars_processed != s.size()) {
    return false;
  }

  if (min_value.has_value() && num < *min_value) {
    return false;
  }
  if (max_value.has_value() && num > *max_value) {
    return false;
  }

  *output = num;
  return true;
}

bool ParsePositiveInt(const crow::query_string& url_params,
		      const std::string& key,
		      size_t* output) {
  const char* value = url_params.get(key);
  if (value == nullptr) {
    return false;
  }
  int int_value;
  if (!ToInt(value, &int_value, /*min_value=*/1)) {
    return false;
  }
  *output = static_cast<size_t>(int_value);
  return true;
}

bool ParseNonNegativeInt(const crow::query_string& url_params,
			 const std::string& key,
			 size_t* output) {
  const char* value = url_params.get(key);
  if (value == nullptr) {
    return false;
  }
  int int_value;
  if (!ToInt(value, &int_value, /*min_value=*/0)) {
    return false;
  }
  *output = static_cast<size_t>(int_value);
  return true;
}

bool ToFiniteDouble(const char* c_str, double* output) {
  const std::string s(c_str);

  double num = 0;
  size_t chars_processed = 0;
  try {
    num = std::stod(s, &chars_processed);
  } catch (std::invalid_argument const& ex) {
    return false;
  } catch(std::out_of_range const& ex) {
    return false;
  }

  if (chars_processed != s.size()) {
    return false;
  }

  if (!std::isfinite(num)) {
    return false;
  }

  *output = num;
  return true;
}

bool ParseFiniteDouble(const crow::query_string& url_params,
		       const std::string& key,
		       double* output) {
  const char* value = url_params.get(key);
  if (value == nullptr) {
    return false;
  }
  return ToFiniteDouble(value, output);
}

bool ParseComplexList(const crow::query_string& url_params,
		      const std::string& real_key,
		      const std::string& img_key,
		      std::vector<ComplexD>* output) {
  const std::vector<char*> rs = url_params.get_list(real_key, /*use_brackets=*/false);
  const std::vector<char*> is = url_params.get_list(img_key, /*use_brackets=*/false);
  if (rs.empty() || is.empty() || rs.size() != is.size()) {
    return false;
  }

  const size_t N = rs.size();
  std::vector<ComplexD> cs(N);
  for (size_t i = 0; i < N; ++i) {
    if (!ToFiniteDouble(rs[i], &cs[i].r) ||
	!ToFiniteDouble(is[i], &cs[i].i)) {
      return false;
    }
  }

  *output = cs;
  return true;
}

bool ParseColorList(const crow::query_string& url_params,
		    const std::string& red_key,
		    const std::string& green_key,
		    const std::string& blue_key,
		    std::vector<png::rgb_pixel>* output) {
  const std::vector<char*> rs = url_params.get_list(red_key, /*use_brackets=*/false);
  const std::vector<char*> gs = url_params.get_list(green_key, /*use_brackets=*/false);
  const std::vector<char*> bs = url_params.get_list(blue_key, /*use_brackets=*/false);
  if (rs.empty() || gs.empty() || bs.empty() || rs.size() != gs.size() || rs.size() != bs.size()) {
    return false;
  }

  const size_t N = rs.size();
  std::vector<png::rgb_pixel> colors;
  for (size_t i = 0; i < N; ++i) {
    int red, green, blue;
    if (!ToInt(rs[i], &red, 0, 255) ||
	!ToInt(gs[i], &green, 0, 255) ||
	!ToInt(bs[i], &blue, 0, 255)) {
      return false;
    }
    colors.emplace_back(static_cast<char>(red),
			static_cast<char>(green),
			static_cast<char>(blue));
  }

  *output = colors;
  return true;
}

bool ParsePrecision(const crow::query_string& url_params,
		    const std::string& key,
		    std::optional<Precision>* output) {
  const char* c_str = url_params.get(key);
  if (c_str == nullptr) {
    return false;
  }
  const std::string s(c_str);
  if (s == "SINGLE") {
    *output = Precision::SINGLE;
    return true;
  } else if (s == "DOUBLE") {
    *output = Precision::DOUBLE;
    return true;
  }
  return false;
}

bool ParseStrategy(const crow::query_string& url_params,
		   const std::string& key,
		   std::optional<Strategy>* output) {
  const char* c_str = url_params.get(key);
  if (c_str == nullptr) {
    return false;
  }
  const std::string s(c_str);
  if (s == "NAIVE") {
    *output = Strategy::NAIVE;
    return true;
  } else if (s == "DYNAMIC_BLOCK") {
    *output = Strategy::DYNAMIC_BLOCK;
    return true;
  } else if (s == "DYNAMIC_BLOCK_THREADED") {
    *output = Strategy::DYNAMIC_BLOCK_THREADED;
    return true;
  } else if (s == "DYNAMIC_BLOCK_THREADED_INCREMENTAL") {
    *output = Strategy::DYNAMIC_BLOCK_THREADED_INCREMENTAL;
    return true;
  }
  return false;
}

bool ParsePngEncoder(const crow::query_string& url_params,
		     const std::string& key,
		     std::optional<PngEncoder>* output) {
  const char* c_str = url_params.get(key);
  if (c_str == nullptr) {
    return false;
  }
  const std::string s(c_str);
  if (s == "PNGPP") {
    *output = PngEncoder::PNGPP;
    return true;
  } else if (s == "FPNG") {
    *output = PngEncoder::FPNG;
    return true;
  }
  return false;
}

bool ParseHandlerType(const crow::query_string& url_params,
		      const std::string& key,
		      std::optional<HandlerType>* output) {
  const char* c_str = url_params.get(key);
  if (c_str == nullptr) {
    return false;
  }
  const std::string s(c_str);
  if (s == "SYNCHRONOUS") {
    *output = HandlerType::SYNCHRONOUS;
    return true;
  } else if (s == "PIPELINED") {
    *output = HandlerType::PIPELINED;
    return true;
  } else if (s == "ASYNCHRONOUS") {
    *output = HandlerType::ASYNCHRONOUS;
    return true;
  }
  return false;
}

struct FractalParams {
  static std::optional<FractalParams> Parse(const crow::query_string& url_params) {
    FractalParams fractal_params;

    // Required params.
    if (!ParseNonEmptyString(url_params, "session_id", &fractal_params.session_id) ||
	!ParsePositiveInt(url_params, "request_id", &fractal_params.request_id) ||
	!ParseNonNegativeInt(url_params, "last_data_id", &fractal_params.last_data_id) ||
	!ParseNonNegativeInt(url_params, "last_viewport_id", &fractal_params.last_viewport_id) ||
	!ParseFiniteDouble(url_params, "i_min", &fractal_params.i_min) ||
	!ParseFiniteDouble(url_params, "r_min", &fractal_params.r_min) ||
	!ParseFiniteDouble(url_params, "r_range", &fractal_params.r_range) ||
	!ParsePositiveInt(url_params, "width", &fractal_params.width) ||
	!ParsePositiveInt(url_params, "height", &fractal_params.height) ||
	!ParsePositiveInt(url_params, "max_iters", &fractal_params.max_iters) ||
	!ParseComplexList(url_params, "zero_rs", "zero_is", &fractal_params.zeros) ||
	!ParseColorList(url_params, "zero_reds", "zero_greens", "zero_blues", &fractal_params.colors) ||
	fractal_params.zeros.size() != fractal_params.colors.size()) {
      return std::nullopt;
    }

    // Optional params.
    ParsePrecision(url_params, "precision", &fractal_params.precision);
    ParseStrategy(url_params, "strategy", &fractal_params.strategy);
    ParsePngEncoder(url_params, "png_encoder", &fractal_params.png_encoder);
    ParseHandlerType(url_params, "handler", &fractal_params.handler_type);

    return fractal_params;
  }

  double i_range() const {
    return r_range / width * height;
  }

  // Request metadata.
  std::string session_id;
  size_t request_id;
  size_t last_data_id;
  size_t last_viewport_id;

  // Required args.
  double i_min;
  double r_min;
  double r_range;
  size_t width;
  size_t height;
  size_t max_iters;
  std::vector<ComplexD> zeros;
  std::vector<png::rgb_pixel> colors;

  // Optional args.
  std::optional<Precision> precision;
  std::optional<Strategy> strategy;
  std::optional<PngEncoder> png_encoder;
  std::optional<HandlerType> handler_type;
};

struct SaveParams {
  static std::optional<SaveParams> Parse(const crow::query_string& url_params) {
    SaveParams save_params;

    std::optional<FractalParams> fractal_params = FractalParams::Parse(url_params);
    if (!fractal_params.has_value()) {
      return std::nullopt;
    }
    save_params.fractal_params = *fractal_params;

    // Required params.
    if (!ParsePositiveInt(url_params, "save_scale", &save_params.scale) ||
	!ParseNonEmptyString(url_params, "save_file", &save_params.filename) ||
	!ParseNonEmptyString(url_params, "save_metadata", &save_params.metadata)) {
      return std::nullopt;
    }

    return save_params;
  }

  // Fractal generation params.
  FractalParams fractal_params;

  // Integer scale factor.
  size_t scale = 1;

  // Where to save the image.
  std::string filename;

  // Metadata about the image.
  std::string metadata;
};

struct LoadParams {
  static std::optional<LoadParams> Parse(const crow::query_string& url_params) {
    LoadParams load_params;

    // Required params.
    if (!ParseNonEmptyString(url_params, "load_file", &load_params.filename)) {
      return std::nullopt;
    }

    return load_params;
  }

  // Where to load the image.
  std::string filename;
};

bool operator==(const png::rgb_pixel& a,
		const png::rgb_pixel& b) {
  return (a.red == b.red && a.green == b.green && a.blue == b.blue);
}

bool operator!=(const png::rgb_pixel& a,
		const png::rgb_pixel& b) {
  return !(a == b);
}

template <typename T>
bool AllEqual(const std::vector<T>& a, const std::vector<T>& b) {
  if (a.size() != b.size()) {
    return false;
  }
  for (size_t i = 0; i < a.size(); ++i) {
    if (a[i] != b[i]) {
      return false;
    }
  }
  return true;
}

bool ParamsDifferOnlyByPanning(const FractalParams& a, const FractalParams& b) {
  return (a.r_range == b.r_range &&
	  a.width == b.width &&
	  a.height == b.height &&
	  a.max_iters == b.max_iters &&
	  AllEqual(a.zeros, b.zeros) &&
	  AllEqual(a.colors, b.colors) &&
	  a.precision == b.precision);
}

bool ParamsDifferOnlyByViewport(const FractalParams& a, const FractalParams& b) {
  return (a.width == b.width &&
	  a.height == b.height &&
	  a.max_iters == b.max_iters &&
	  AllEqual(a.zeros, b.zeros) &&
	  AllEqual(a.colors, b.colors) &&
	  a.precision == b.precision);
}

#endif // _CROW_FRACTAL_SERVER_FRACTAL_PARAMS_
