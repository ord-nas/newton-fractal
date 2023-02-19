# Use static linking to enable easily moving to other machines (or bash on windows).
# Link pthread and boost.
# Use --whole-archive for -lpthread to work around some errors with some required symbols not being statically linked otherwise.
fractal_server: fractal_server.cpp
	g++-11 fractal_server.cpp --static -Wl,--whole-archive -lpthread -Wl,--no-whole-archive -lboost_system -lpng16 -lz -o fractal_server
