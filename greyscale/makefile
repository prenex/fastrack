# for gcc5.1+:
CC=g++
# for clang3.4+:
#CC=clang++
# for emscripten / webgl
#CC=em++

ifeq ($(CC),g++)
	# g++ (ver 5.1+ tested)
	# sadly we are having a lot of unused functions for tests so -Wall is undesirable...
# TODO: check -funroll-loops and -funroll-all-loops
	CFLAGS=-c -std=c++14 -g -O3 -ffast-math -funsafe-loop-optimizations -Wunsafe-loop-optimizations -freorder-blocks-and-partition #-fsanitize=address -fno-omit-frame-pointer # -O0 #-Wall
	LDFLAGS=-g -lpthread -lX11 -lGL -lX11 -O3 -ffast-math -funsafe-loop-optimizations -Wunsafe-loop-optimizations -freorder-blocks-and-partition #-fsanitize=address -fno-omit-frame-pointer #-lm # -O0
# else 
# ifeq ($(CC),em++)
# 	# TODO: This is just copy paste from old projects, please change accordingly
# 	# em++ (ver 1.3+ tested)
# 	# use 1y instead of 14 here, we need the LLVM libc so that streams are copyable (or a more recent gcc toolchain than the 4.9 at my work!)
# 	CFLAGS=-c -stdlib=libc++ -std=c++1y -O2
# 	LDFLAGS=--shell-file shell_minimal.html --preload-file models -lGL -lX11 -stdlib=libc++ -O2 
else
	# clang++ (ver 3.4+ tested)
	# clang uses 1y instead of 14 here, we need the LLVM libc so that streams are copyable (or a more recent gcc toolchain than the 4.9 at my work!)
	CFLAGS=-c -stdlib=libc++ -std=c++1y -g -O3 -ffast-math # -fsanitize=address -fno-omit-frame-pointer
	LDFLAGS=-g -stdlib=libc++ -lpthread -lX11 -v -O3 -lGL -lX11 -lm -stdlib=libc++ -ffast-math # -fsanitize=address -fsanitize-memory-track-origins
endif
#endif

FFLT_SOURCES=ffltest.cpp
FFLT_OBJECTS=$(FFLT_SOURCES:.cpp=.o)
FFLT_EXECUTABLE=ffltest

M1_SOURCES=marker1_gen.cpp #$(wildcard dxflib/*.cpp) $(wildcard ObjMaster/*.cpp)
M1_OBJECTS=$(M1_SOURCES:.cpp=.o)
M1_EXECUTABLE=marker1_gen

M2_SOURCES=marker2_gen.cpp #$(wildcard dxflib/*.cpp) $(wildcard ObjMaster/*.cpp)
M2_OBJECTS=$(M2_SOURCES:.cpp=.o)
M2_EXECUTABLE=marker2_gen

M1_EV_SOURCES=marker1_evaluator.cpp 
M1_EV_OBJECTS=$(M1_EV_SOURCES:.cpp=.o)
M1_EV_EXECUTABLE=marker1_eval

M1_MC_EV_SOURCES=marker1_mc_evaluator.cpp 
M1_MC_EV_OBJECTS=$(M1_MC_EV_SOURCES:.cpp=.o)
M1_MC_EV_EXECUTABLE=marker1_mc_eval

CAMAPP_SOURCES=marker_camapp.cpp
CAMAPP_OBJECTS=$(CAMAPP_SOURCES:.cpp=.o)
CAMAPP_EXECUTABLE=marker_camapp

CAMAPP_3D_SOURCES=marker3d_camapp.cpp
CAMAPP_3D_OBJECTS=$(CAMAPP_3D_SOURCES:.cpp=.o)
CAMAPP_3D_EXECUTABLE=marker3d_camapp

default: marker1gen marker2gen marker1_ev ffl_test marker1_mc_ev camapp
# Rem.: The default make target is not "all" because it seems not good to rely on heavyweight libraries like Eigen3 or OpenGV
all: default camapp3d
ffl_test: $(FFLT_SOURCES) $(FFLT_EXECUTABLE)
marker1gen: $(M1_SOURCES) $(M1_EXECUTABLE)
marker2gen: $(M2_SOURCES) $(M2_EXECUTABLE)
camapp: $(CAMAPP_SOURCES) $(CAMAPP_EXECUTABLE)
camapp3d: $(CAMAPP_3D_SOURCES) $(CAMAPP_3D_EXECUTABLE)

marker1_mc_ev: $(M1_MC_EV_SOURCES) $(M1_MC_EV_EXECUTABLE)
$(M1_MC_EV_EXECUTABLE): $(M1_MC_EV_OBJECTS)
# In case of emscripten build, we make a html5/webgl output
ifeq ($(CC),em++)
	$(CC) $(M1_MC_EV_OBJECTS) -o $@.html $(LDFLAGS)
else
	$(CC) $(M1_MC_EV_OBJECTS) -o $@ $(LDFLAGS)
endif

marker1_ev: $(M1_EV_SOURCES) $(M1_EV_EXECUTABLE)
$(M1_EV_EXECUTABLE): $(M1_EV_OBJECTS)
# In case of emscripten build, we make a html5/webgl output
ifeq ($(CC),em++)
	$(CC) $(M1_EV_OBJECTS) -o $@.html $(LDFLAGS)
else
	$(CC) $(M1_EV_OBJECTS) -o $@ $(LDFLAGS)
endif

# The LDFLAGS should be after the files
# because of some lazyness in newer toolchains!
$(FFLT_EXECUTABLE): $(FFLT_OBJECTS) 
# In case of emscripten build, we make a html5/webgl output
ifeq ($(CC),em++)
	$(CC) $(FFLT_OBJECTS) -o $@.html $(LDFLAGS)
else
	$(CC) $(FFLT_OBJECTS) -o $@ $(LDFLAGS)
endif

$(CAMAPP_EXECUTABLE): $(CAMAPP_OBJECTS)
# In case of emscripten build, we make a html5/webgl output
ifeq ($(CC),em++)
	$(CC) $(CAMAPP_OBJECTS) -o $@.html $(LDFLAGS)
else
	$(CC) $(CAMAPP_OBJECTS) -o $@ $(LDFLAGS)
endif

$(CAMAPP_3D_EXECUTABLE): $(CAMAPP_3D_OBJECTS)
# In case of emscripten build, we make a html5/webgl output
ifeq ($(CC),em++)
	$(CC) $(CAMAPP_3D_OBJECTS) -o $@.html $(LDFLAGS)
else
	$(CC) $(CAMAPP_3D_OBJECTS) -o $@ $(LDFLAGS)
endif

# The LDFLAGS should be after the files
# because of some lazyness in newer toolchains!
$(M1_EXECUTABLE): $(M1_OBJECTS) 
# In case of emscripten build, we make a html5/webgl output
ifeq ($(CC),em++)
	$(CC) $(M1_OBJECTS) -o $@.html $(LDFLAGS)
else
	$(CC) $(M1_OBJECTS) -o $@ $(LDFLAGS)
endif

# The LDFLAGS should be after the files
# because of some lazyness in newer toolchains!
$(M2_EXECUTABLE): $(M2_OBJECTS) 
# In case of emscripten build, we make a html5/webgl output
ifeq ($(CC),em++)
	$(CC) $(M2_OBJECTS) -o $@.html $(LDFLAGS)
else
	$(CC) $(M2_OBJECTS) -o $@ $(LDFLAGS)
endif

.cpp.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f *.o $(M1_EXECUTABLE) $(M2_EXECUTABLE) $(M1_EV_EXECUTABLE) $(FFLT_EXECUTABLE) $(M1_MC_EV_EXECUTABLE) $(CAMAPP_EXECUTABLE) $(CAMAPP_3D_EXECUTABLE)

# vim: tabstop=4 noexpandtab shiftwidth=4 softtabstop=4
