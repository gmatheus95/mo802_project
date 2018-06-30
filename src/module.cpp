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

using namespace std;

// Here are the global definitions
#define MAX_WIDTH 1920
#define MAX_HEIGHT 1080
#define MAX_REFLECTIONS 10

struct Dimensions{
    int curr_x;
    int curr_y;
};

// Pixel representing positions and respective Color
struct Pixel{
    int x;
    int y;
    Color color;
};

// This class creates tasks.
class job_manager : public spitz::job_manager
{
private:
    Dimensions d;
    int OFFSET_WIDTH;
    int OFFSET_HEIGHT;
public:
    job_manager(int argc, const char *argv[], spitz::istream& jobinfo)
    {
        std::cout << "[JM] Job manager created." << std::endl;
        d.curr_x = 0;
        d.curr_y = 0;
        OFFSET_WIDTH = atoi(argv[5]);
        OFFSET_HEIGHT = atoi(argv[6]);
    }
    
    bool next_task(const spitz::pusher& task)
    {
        spitz::ostream o;
                
        // Serialize the task into a binary stream
        o.write_data(&d,sizeof(Dimensions));

        if (d.curr_y >= MAX_HEIGHT)
        {
            return false;
        }

        d.curr_x = d.curr_x + OFFSET_WIDTH;     
        // Advance in Y and get X back to 0
        if (d.curr_x >= MAX_WIDTH)
        {
            d.curr_y = d.curr_y + OFFSET_HEIGHT;
            d.curr_x = 0;
            // Stop creating tasks
            if (d.curr_y >= MAX_HEIGHT)
            {
                return false;
            }
        }            

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
    int count;
    uint64_t raysCast;
    int OFFSET_WIDTH;
    int OFFSET_HEIGHT;    
public:
    worker(int argc, const char *argv[])
    {
        OFFSET_WIDTH = atoi(argv[5]);
        OFFSET_HEIGHT = atoi(argv[6]);

        raysCast = 0;
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
        rayTracer->initializeComponentsToTraceRays();
        inFileStream.close();
    }
    
    int run(spitz::istream& task, const spitz::pusher& result)
    {
        // Binary stream used to store the output
        spitz::ostream o;
        
        count = 0;

        Pixel* mPixel = (Pixel*)malloc(sizeof(Pixel));
        // Reading the area to be calculated
        task.read_data(&dim,sizeof(Dimensions));

        int offset_x, offset_y;

        // Here we define the offset of currentx + offset OR max size for a dimension
        offset_x = ((dim.curr_x + OFFSET_WIDTH) > MAX_WIDTH) ? MAX_WIDTH - 1: (dim.curr_x + OFFSET_WIDTH);
        offset_y = ((dim.curr_y + OFFSET_HEIGHT) > MAX_HEIGHT) ? MAX_HEIGHT - 1: (dim.curr_y + OFFSET_HEIGHT);

        std::cout << "[WK] Working on " << dim.curr_x << "x" << dim.curr_y << " to " << offset_x << "x" << offset_y << std::endl;

        // Cast ray for specific pixel and add it to ostream
        for (int i = dim.curr_x; i < offset_x; i++)
        {
            for (int j = dim.curr_y; j < offset_y; j++)
            {
                mPixel->x = i;
                mPixel->y = j;
                mPixel->color = rayTracer->castRayForPixel(i,j,raysCast);
                o.write_data(mPixel,sizeof(Pixel));
            }
        }

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
    Image finalImg;
    Pixel currPixel;
    string fileName;
public:
    // Initializing commiter alongisde with finalImg
    committer(int argc, const char *argv[], spitz::istream& jobinfo):finalImg(MAX_WIDTH, MAX_HEIGHT),fileName(argv[4])
    {
        std::cout << "[CO] Committer created." << std::endl;    
        
    }
    
    int commit_task(spitz::istream& result)
    {
        // Deserialize the result from the task and use it 
        // to compose the final result
        while(result.has_data()){
            // Read pixel from stream
            result.read_data(&currPixel,sizeof(Pixel));
            // Add processed pixel to finalImg
            finalImg.pixel(currPixel.x,currPixel.y,currPixel.color);
        }
        std::cout << "[CO] Result committed." << std::endl;

        return 0;
    }
    
    // Optional. If the result depends on receiving all of the task 
    // results, or if the final result must be serialized to the 
    // Spitz Main, then an additional Commit Job is called.

    int commit_job(const spitz::pusher& final_result) 
    {
        // Publish image in out.tga
        finalImg.WriteTga(fileName.c_str(), false);
        final_result.push(NULL, 0);
        return 0;
    }

    ~committer()
    {    }
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
