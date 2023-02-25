#ifndef _CROW_FRACTAL_SERVER_HANDLER_GROUP_
#define _CROW_FRACTAL_SERVER_HANDLER_GROUP_

#include <crow.h>

#include "fractal_params.h"
#include "handler.h"
#include "synchronous_handler.h"

class HandlerGroup : public Handler {
 public:
  explicit HandlerGroup(size_t num_threads)
    : synchronous_handler_(num_threads) {}

  crow::response HandleParamsRequest(const FractalParams& params) override {
    return GetHandler(params).HandleParamsRequest(params);
  }

  crow::response HandleFractalRequest(const FractalParams& params) override {
    return GetHandler(params).HandleFractalRequest(params);
  }

 private:
  Handler& GetHandler(const FractalParams& params) {
    // FIXME: actually check params.
    return synchronous_handler_;
  }

  SynchronousHandler synchronous_handler_;
};

#endif // _CROW_FRACTAL_SERVER_HANDLER_GROUP_
