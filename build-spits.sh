#!/bin/bash -x

#############################################
# This is the SPITS build for the RayTracer #
#############################################

# Put your favorite compiler here
export CXX=g++

# Output binary
export OUTDIR="./bin"
export OUTFILE="$OUTDIR/RayTracer_spits"

# Ensure the output directory exists
mkdir -p $OUTDIR

# SPITS specifics.

# This will add the include directory with the spitz headers
export SPITS_INCLUDE="-Iinclude/"
# Flags to build as a shared object
export SPITS_SO="-fPIC -shared"
# This creates a debugging layer that allows 
# spitz to run as executable if compiled as such
export SPITS_DEBUG="-DSPITZ_SERIAL_DEBUG"

# Compiler flags. 
# You may need to alter them.
#
# OpenMP flag by compiler:
#  gcc: -fopenmp
#  icc: -openmp
#  clang: -fopenmp=libomp
export FLAGS="-O3 -fopenmp"
export FLAGS_SO="$FLAGS -o $OUTFILE.so $SPITS_INCLUDE $SPITS_SO"
export FLAGS_EXE="$FLAGS -o $OUTFILE $SPITS_INCLUDE $SPITS_DEBUG"

# Source files (without the original main)
export SOURCE_FILES="
  src/Air.cpp \
  src/Boundaries.cpp \
  src/BSP.cpp \
  src/Camera.cpp \
  src/Checkerboard.cpp \
  src/Color.cpp \
  src/CrissCross.cpp \
  src/FlatColor.cpp \
  src/Glass.cpp \
  src/Image.cpp \
  src/Intersection.cpp \
  src/Light.cpp \
  src/Marble.cpp \
  src/Material.cpp \
  src/NormalMap.cpp \
  src/Object.cpp \
  src/PerlinNoise.cpp \
  src/Ray.cpp \
  src/RayTracer.cpp \
  src/ShinyColor.cpp \
  src/Sphere.cpp \
  src/Triangle.cpp \
  src/Turbulence.cpp \
  src/Vector.cpp \
  src/Wood.cpp \
  src/module.cpp"

# Build the program as a shared object
$CXX $FLAGS_SO $SOURCE_FILES

# Build the program as a test executable
$CXX $FLAGS_EXE $SOURCE_FILES
