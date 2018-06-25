#!/bin/bash

# The MIT License (MIT)
#
# Copyright (c) 2015 Caian Benedicto <caian@ggaunicamp.com>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy 
# of this software and associated documentation files (the "Software"), to 
# deal in the Software without restriction, including without limitation the 
# rights to use, copy, modify, merge, publish, distribute, sublicense, 
# and/or sell copies of the Software, and to permit persons to whom the 
# Software is furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in 
# all copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL 
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS 
# IN THE SOFTWARE.

###############################################################################
# Get the location of the spitz scripts
###############################################################################
if [ -z "$SPITZDIR" ]
then
    SOURCE="${BASH_SOURCE[0]}"
    while [ -h "$SOURCE" ]; do
      DIR="$( cd -P "$( dirname "$SOURCE" )" && pwd )"
      SOURCE="$(readlink "$SOURCE")"
      [[ $SOURCE != /* ]] && SOURCE="$DIR/$SOURCE"
    done
    SPITZDIR="$( cd -P "$( dirname "$SOURCE" )" && pwd )"
fi

echo Using SPITZDIR=$SPITZDIR

###############################################################################
# Execution variables
###############################################################################

if [ "$#" -lt "2" ]
then
    echo "USAGE: run.sh module args"
    exit 1
fi

if [ -z "$SPITZARGS" ]
then
    SPITZARGS="--nw=0 --overfill=0 --announce=file --verbose=0 --jobid=123dasilva4"
fi

MODULE="$1"
MODULEARGS="${@:2}"

JMPATH="$SPITZDIR/jm.py"
TMPATH="$SPITZDIR/tm.py"
PYTHONCMD="python -u"

###############################################################################
# Generate a unique name to be used for the logs
###############################################################################

ADDR=$(hostname)
SPITZUID="$ADDR"-"$$"

echo Using $SPITZUID as UID

###############################################################################
# Set up nodes
###############################################################################
touch nodes.txt
mkdir -p nodes

###############################################################################
# Set up the logging
###############################################################################

mkdir -p log
JMSTZ="log/SPITZ-JM-"$SPITZUID
JMOUT="log/STDOUT-JM-"$SPITZUID
JMERR="log/STDERR-JM-"$SPITZUID
TMSTZ="log/SPITZ-TM-"$SPITZUID
TMOUT="log/STDOUT-TM-"$SPITZUID
TMERR="log/STDERR-TM-"$SPITZUID

###############################################################################
# Assemble the command line
###############################################################################
JMCMD="$PYTHONCMD $JMPATH --log=$JMSTZ $SPITZARGS $MODULE $MODULEARGS"
TMCMD="$PYTHONCMD $TMPATH --log=$TMSTZ --tmport=0 --announce=cat 
    $SPITZARGS $MODULE $MODULEARGS"

###############################################################################
# The following code automatically launches a Job Manager for the job and 
# prevents new Task Manager to run if the job is completed
# Be sure to remove 'jm.exists' and 'jm.finished' after each run
###############################################################################

JMEFILE="jm.exists"
JMFFILE="jm.finished"

# Kill subsequent task manager jobs
if [ -f $JMFFILE ]
then
    echo The Job inside this folder has already finished
    echo Please remove jm.exists and jm.finished
    exit 1
fi

echo Starting Job...

# Start a job manager if there is none 
if [ ! -f $JMEFILE ]
then
    echo $SPITZUID > $JMEFILE

    # Wait for an NFS sync
    sleep 10

    if [ "x$SPITZUID" = "x$(cat $JMEFILE)" ]
    then
        echo Launching Job Manager...
        echo Spitz log set to $JMSTZ
        echo Stdout log set to $JMOUT
        echo Stderr log set to $JMERR
        echo $JMCMD
        $JMCMD 2>$JMERR | tee $JMOUT && touch $JMFFILE &
    fi
fi

# Start a task manager
echo Launching Task Manager...
echo Stdout log set to $TMOUT
echo Stderr log set to $TMERR
echo $TMCMD
$TMCMD 2>$TMERR | tee $TMOUT

# Wait for the job manager, if it was launched
wait

exit 0
