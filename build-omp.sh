#!/bin/bash -x

##############################################
# This is the OpenMP build for the RayTracer #
##############################################

# Put your favorite compiler here
export CXX=g++

# Output binary
export OUTDIR="./bin"
export OUTFILE="$OUTDIR/RayTracer_omp"

# Ensure the output directory exists
mkdir -p $OUTDIR

# Compiler flags. 
# You may need to alter them.
#
# OpenMP flag by compiler:
#  gcc: -fopenmp
#  icc: -openmp
#  clang: -fopenmp=libomp
export FLAGS="-O3 -fopenmp -o $OUTFILE"

# Source files (with the original main)
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
  src/main.cpp"

# Build the program
$CXX $FLAGS $SOURCE_FILES
