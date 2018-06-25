#!/bin/bash

############################################
# This is the OpenMP run for the RayTracer #
############################################

# You can either pass scenes individually as arguments or 
# leave it blank to run all scenes in the 'scenes' folder
SCENES="${@:1}"
SCENES="${SCENES:-`ls scenes/*.scn`}"

# Ray tracing parameters
export SUPER_SAMPLES=1
export DEPTH_COMPLEXITY=5

# Input binary
export PROGDIR="./bin"
export PROGRAM="$PROGDIR/RayTracer_omp"

# Output directory
export OUTDIR="./results/omp"

# Ensure the output directory exists
mkdir -p $OUTDIR

# Iterate through selected scenes and 
# run the ray tracer using time 
for SCENE in $SCENES
do
    FILENAME="$OUTDIR/`basename $SCENE`"
    FILENAME="${FILENAME%.*}"
    OUTFILE="$FILENAME.tga"
    OUTLOG="$FILENAME.log"
    echo $SCENE...
    (time $PROGRAM $SCENE $SUPER_SAMPLES $DEPTH_COMPLEXITY $OUTFILE) 2>&1 | tee $OUTLOG
done
