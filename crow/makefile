# Use static linking to enable easily moving to other machines (or bash on windows).
# Link pthread and boost.
# Use --whole-archive for -lpthread to work around some errors with some required symbols not being statically linked otherwise.
fractal_server: fractal_server.cpp complex.h polynomial.h analyzed_polynomial.h development_utils.h fractal_params.h complex_array.h complex_array_eigen.h complex_array_hand_rolled.h thread_pool.h task_group.h image_regions.h pixel_iterator.h fpng/fpng.cpp fpng/fpng.h rgb_image.h fractal_drawing.h png_encoding.h response.h handler.h synchronous_handler.h handler_group.h
	g++-11 fractal_server.cpp fpng/fpng.cpp -msse4.1 -mpclmul -O3 --static -Wl,--whole-archive -lpthread -Wl,--no-whole-archive -lboost_system -lboost_thread -lpng16 -lz -o fractal_server

# To analyze loop vectorization, append: -fopt-info-vec-all  2>&1 | grep <filname_of_interest>
