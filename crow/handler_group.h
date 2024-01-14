#ifndef _CROW_FRACTAL_SERVER_HANDLER_GROUP_
#define _CROW_FRACTAL_SERVER_HANDLER_GROUP_

#include <crow.h>

#include "fractal_params.h"
#include "handler.h"
#include "synchronous_handler.h"
#include "pipelined_handler.h"
#include "async_handler.h"

class HandlerGroup : public Handler {
 public:
  explicit HandlerGroup(ThreadPool* thread_pool)
    : synchronous_handler_(thread_pool),
      pipelined_handler_(thread_pool),
      async_handler_(thread_pool) {}

  crow::response HandleParamsRequest(const FractalParams& params) override {
    return GetHandler(params).HandleParamsRequest(params);
  }

  crow::response HandleFractalRequest(const FractalParams& params) override {
    return GetHandler(params).HandleFractalRequest(params);
  }

  crow::response HandleSaveRequest(const SaveParams& params) {
    return synchronous_handler_.HandleSaveRequest(params);
  }

 private:
  Handler& GetHandler(const FractalParams& params) {
    switch (params.handler_type.value_or(HandlerType::SYNCHRONOUS)) {
      case HandlerType::SYNCHRONOUS:
      default:
	return synchronous_handler_;
      case HandlerType::PIPELINED:
	return pipelined_handler_;
      case HandlerType::ASYNCHRONOUS:
	return async_handler_;
    }
  }

  SynchronousHandler synchronous_handler_;
  PipelinedHandler pipelined_handler_;
  AsyncHandler async_handler_;
};

#endif // _CROW_FRACTAL_SERVER_HANDLER_GROUP_
