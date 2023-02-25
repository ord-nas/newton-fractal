#ifndef _CROW_FRACTAL_SERVER_HANDLER_
#define _CROW_FRACTAL_SERVER_HANDLER_

#include <string>
#include <utility>

#include <crow.h>

#include "fractal_params.h"

class Handler {
 public:
  virtual crow::response HandleParamsRequest(const FractalParams& params) = 0;
  virtual crow::response HandleFractalRequest(const FractalParams& params) = 0;

  virtual ~Handler() {}
};

#endif // _CROW_FRACTAL_SERVER_HANDLER_
