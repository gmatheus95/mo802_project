/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Caian Benedicto <caian@ggaunicamp.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy 
 * of this software and associated documentation files (the "Software"), to 
 * deal in the Software without restriction, including without limitation the 
 * rights to use, copy, modify, merge, publish, distribute, sublicense, 
 * and/or sell copies of the Software, and to permit persons to whom the 
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in 
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL 
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS 
 * IN THE SOFTWARE.
 */


// This define enables the C++ wrapping code around the C API, it must be used
// only once per C++ module.
#define SPITZ_ENTRY_POINT

// Spitz serial debug enables a Spitz-compliant main function to allow 
// the module to run as an executable for testing purposes.
// #define SPITZ_SERIAL_DEBUG

#include <spitz/spitz.hpp>
#include <stdlib.h>
#include <string.h>
#include <fstream>
#include <ctime>
#include <iostream>
#include "RayTracer.h"
#include "Image.h"

using namespace std;

#define MAX_WIDTH 1920
#define MAX_HEIGHT 1080
#define MAX_REFLECTIONS 10

#define OFFSET_WIDTH 250
#define OFFSET_HEIGHT 250


typedef struct Dimensions{
    int curr_x;
    int curr_y;
};

typedef struct ImagePartition{
    int init_x;
    int init_y;
    int final_x;
    int final_y;
    Image imagePart;
};

// This class creates tasks.
class job_manager : public spitz::job_manager
{
private:
    Dimensions d;

public:
    job_manager(int argc, const char *argv[], spitz::istream& jobinfo)
    {
        std::cout << "[JM] Job manager created." << std::endl;
        d.curr_x = 0;
        d.curr_y = 0;
    }
    
    bool next_task(const spitz::pusher& task)
    {
        spitz::ostream o;

        
                
        // Serialize the task into a binary stream
        o.write_data(&d,sizeof(Dimensions));

        // Advance in Y and get X back to 0
        if (d.curr_x > MAX_WIDTH)
        {
            // Stop creating tasks
            if (d.curr_y > MAX_HEIGHT)
                return false;
            d.curr_y = std::min<int>(d.curr_y + OFFSET_HEIGHT, MAX_HEIGHT);
            d.curr_x = 0;
        }
        else
            d.curr_x += std::min<int>(d.curr_x + OFFSET_WIDTH, MAX_WIDTH);     

        std::cout << "[JM] Task generated." << std::endl;
        
        // Send the task to the Spitz runtime
        task.push(o);
        
        // Return true until finished creating tasks

        std::cout << "[JM] The module will run forever until "
            "you add a return false!" << std::endl;

        return true;
    }
    
    ~job_manager()
    {
    }
};

// This class executes tasks, preferably without modifying its internal state
// because this can lead to a break of idempotence between tasks. The 'run'
// method will not impose a 'const' behavior to allow libraries that rely 
// on changing its internal state, for instance, OpenCL (see clpi example).
class worker : public spitz::worker
{    
private:
    RayTracer* rayTracer;
    Dimensions dim;
    ImagePartition imgPart;
public:
    worker(int argc, const char *argv[])
    {
        std::cout << "[WK] Worker created." << std::endl;

        // Instanciating RayTracer
        rayTracer = new RayTracer(MAX_WIDTH,MAX_HEIGHT,MAX_REFLECTIONS,atoi(argv[2]),atoi(argv[3]));
        
        // Opening and reading scene from argv1
        char* inFile = (char*)argv[1];
        ifstream inFileStream;
        inFileStream.open(inFile, ifstream::in);

        if (inFileStream.fail()) {
            cerr << "Failed opening file" << endl;
            exit(EXIT_FAILURE);
        }
        rayTracer->readScene(inFileStream);
        inFileStream.close();
    }
    
    int run(spitz::istream& task, const spitz::pusher& result)
    {
        // Binary stream used to store the output
        spitz::ostream o;
        
        // Deserialize the task, process it and serialize the result
        result.read_data(&d,sizeof(Dimensions));
        //dim << o;

        int offset_x, offset_y;

        offset_x = ((dim.curr_x + OFFSET_WIDTH) > MAX_WIDTH) ? MAX_WIDTH : (dim.curr_x + OFFSET_WIDTH);
        offset_y = ((dim.curr_y + OFFSET_HEIGHT) > MAX_HEIGHT) ? MAX_HEIGHT : (dim.curr_y + OFFSET_HEIGHT);

        // Must finish implementation of this function!
        imgPart.init_x = dim.curr_x;
        imgPart.init_y = dim.curr_y;
        imgPart.final_x = offset_x;
        imgPart.final_y = offset_y;
        imgPart.imagePart = rayTracer->traceRaysMatrix(dim.curr_x, dim.curr_y, offset_x, offset_y);

        // Must find a way to take the relevant pixels from this imgPart and add it to final Image
        o.write_data(&imgPart,sizeof(ImagePartition));

        // Send the result to the Spitz runtime
        result.push(o);
        
        std::cout << "[WK] Task processed." << std::endl;

        return 0;
    }
};

// This class is responsible for merging the result of each individual task 
// and, if necessary, to produce the final result after all of the task 
// results have been received.
class committer : public spitz::committer
{
private: 
    Image* finalImg;
public:
    committer(int argc, const char *argv[], spitz::istream& jobinfo)
    {
        std::cout << "[CO] Committer created." << std::endl;
        
        finalImg = new Image(MAX_WIDTH, MAX_HEIGHT);
    }
    
    int commit_task(spitz::istream& result)
    {
        // Deserialize the result from the task and use it 
        // to compose the final result
        // ...
        ImagePartition imgPar;
        result.read_data(&imgPar,sizeof(ImagePartition));

        // this guy here sums the matrixes accordingly
        finalImg->SumColorFactor(&imgPar.imagePart);

        std::cout << "[CO] Result committed." << std::endl;

        return 0;
    }
    
    // Optional. If the result depends on receiving all of the task 
    // results, or if the final result must be serialized to the 
    // Spitz Main, then an additional Commit Job is called.

    int commit_job(const spitz::pusher& final_result) 
    {
        // Process the final result
        // A result must be pushed even if the final 
        // result is not passed on
        string outFile = "out.tga";
        finalImg->WriteTga(outFile.c_str(), false);
        //final_result.push(NULL, 0);
        return 0;
    }

    ~committer()
    {
        
    }
};

// The factory binds the user code with the Spitz C++ wrapper code.
class factory : public spitz::factory
{
public:
    spitz::job_manager *create_job_manager(int argc, const char *argv[],
        spitz::istream& jobinfo)
    {
        return new job_manager(argc, argv, jobinfo);
    }
    
    spitz::worker *create_worker(int argc, const char *argv[])
    {
        return new worker(argc, argv);
    }
    
    spitz::committer *create_committer(int argc, const char *argv[], 
        spitz::istream& jobinfo)
    {
        return new committer(argc, argv, jobinfo);
    }
};

// Creates a factory class.
spitz::factory *spitz_factory = new factory();
