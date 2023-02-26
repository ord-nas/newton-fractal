#include "image_operations.h"

int main() {
  RGBImage from("images/a.png");
  RGBImage to(2000, 1000);

  // ImageOverlap overlap = {
  //   .a_region = {
  //     .x_min = from.get_width() / 2,
  //     .x_max = from.get_width(),
  //     .y_min = 0,
  //     .y_max = from.get_height() / 2,
  //   },
  //   .b_region = {
  //     .x_min = 500,
  //     .x_max = 1000,
  //     .y_min = 250,
  //     .y_max = 750,
  //   },
  // };

  ImageOverlap overlap = {
    .a_region = {
      .x_min = from.get_width() / 2,
      .x_max = from.get_width(),
      .y_min = 0,
      .y_max = from.get_height() / 2,
    },
    .b_region = {
      .x_min = 0,
      .x_max = to.get_width(),
      .y_min = 0,
      .y_max = to.get_height(),
    },
  };

  ResizeBilinear(from, to, overlap);
  to.write("resize_test_output.png");
}
