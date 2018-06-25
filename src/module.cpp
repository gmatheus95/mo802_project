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

#include <iostream>

// This class creates tasks.
class job_manager : public spitz::job_manager
{
public:
    job_manager(int argc, const char *argv[], spitz::istream& jobinfo)
    {
        std::cout << "[JM] Job manager created." << std::endl;
    }
    
    bool next_task(const spitz::pusher& task)
    {
        spitz::ostream o;
                
        // Serialize the task into a binary stream
        // ...

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
public:
    worker(int argc, const char *argv[])
    {
        std::cout << "[WK] Worker created." << std::endl;
    }
    
    int run(spitz::istream& task, const spitz::pusher& result)
    {
        // Binary stream used to store the output
        spitz::ostream o;
        
        // Deserialize the task, process it and serialize the result
        // ...
        
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
public:
    committer(int argc, const char *argv[], spitz::istream& jobinfo)
    {
        std::cout << "[CO] Committer created." << std::endl;
    }
    
    int commit_task(spitz::istream& result)
    {
        // Deserialize the result from the task and use it 
        // to compose the final result
        // ...

        std::cout << "[CO] Result committed." << std::endl;

        return 0;
    }
    
    // Optional. If the result depends on receiving all of the task 
    // results, or if the final result must be serialized to the 
    // Spitz Main, then an additional Commit Job is called.

    // int commit_job(const spitz::pusher& final_result) 
    // {
    //     // Process the final result
    //
    //     // A result must be pushed even if the final 
    //     // result is not passed on
    //     final_result.push(NULL, 0);
    //     return 0;
    // }

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
