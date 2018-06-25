#!/usr/bin/env python

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

from libspitz import JobBinary, SimpleEndpoint
from libspitz import Listener, TaskPool
from libspitz import messaging, config
from libspitz import timeout as Timeout

import Args
import sys, os, socket, datetime, logging, multiprocessing, struct, time, traceback

from threading import Lock

try:
    import Queue as queue # Python 2
except:
    import queue # Python 3

# Global configuration parameters
tm_mode = None # Addressing mode
tm_addr = None # Bind address
tm_port = None # Bind port
tm_nw = None # Maximum number of workers
tm_overfill = 0 # Extra space in the task queue 
tm_announce = None # Mechanism used to broadcast TM address
tm_log_file = None # Output file for logging
tm_verbosity = None # Verbosity level for logging
tm_conn_timeout = None # Socket connect timeout
tm_recv_timeout = None # Socket receive timeout
tm_send_timeout = None # Socket send timeout
tm_timeout = None
tm_jobid = None

###############################################################################
# Parse global configuration
###############################################################################
def parse_global_config(argdict):
    global tm_mode, tm_addr, tm_port, tm_nw, tm_log_file, tm_verbosity, \
        tm_overfill, tm_announce, tm_conn_timeout, tm_recv_timeout, \
        tm_send_timeout, tm_timeout, tm_jobid

    def as_int(v):
        if v == None:
            return None
        return int(v)

    def as_float(v):
        if v == None:
            return None
        return int(v)

    tm_mode = argdict.get('tmmode', config.mode_tcp)
    tm_addr = argdict.get('tmaddr', '0.0.0.0')
    tm_port = int(argdict.get('tmport', config.spitz_tm_port))
    tm_nw = int(argdict.get('nw', multiprocessing.cpu_count()))
    if tm_nw <= 0:
        tm_nw = multiprocessing.cpu_count()
    tm_overfill = max(int(argdict.get('overfill', 0)), 0)
    tm_announce = argdict.get('announce', 'none')
    tm_log_file = argdict.get('log', None)
    tm_verbosity = as_int(argdict.get('verbose', logging.INFO // 10)) * 10
    tm_conn_timeout = as_float(argdict.get('ctimeout', config.conn_timeout))
    tm_recv_timeout = as_float(argdict.get('rtimeout', config.recv_timeout))
    tm_send_timeout = as_float(argdict.get('stimeout', config.send_timeout))
    tm_timeout = as_float(argdict.get('timeout', config.timeout))
    tm_jobid = argdict.get('jobid', '')

###############################################################################
# Configure the log output format
###############################################################################
def setup_log():
    root = logging.getLogger()
    root.setLevel(tm_verbosity)
    root.handlers = []
    if tm_log_file == None:
        ch = logging.StreamHandler(sys.stderr)
    else:
        ch = logging.StreamHandler(open(tm_log_file, 'wt'))
    ch.setLevel(logging.DEBUG)
    formatter = logging.Formatter('%(asctime)s - %(threadName)s - '+
        '%(levelname)s - %(message)s')
    ch.setFormatter(formatter)
    root.addHandler(ch)

###############################################################################
# Abort the aplication with message
###############################################################################
def abort(error):
    logging.critical(error)
    exit(1)
    
###############################################################################
# Append the node address to the nodes list
###############################################################################
def announce_cat(addr, filename = None):
    # Override the filename if it is empty
    if filename == None:
        nodefile = 'nodes.txt'
        filename = os.path.join('.', nodefile)

    logging.debug('Appending node %s to file %s...' % (addr, nodefile))
    
    try:
        f = open(filename, "a")
        f.write("node %s\n" % addr)
        f.close()
    except:
        logging.warning('Failed to write to %s!' % (nodefile,))
        
###############################################################################
# Add the node to a directory as a file
###############################################################################
def announce_file(addr, dirname = None):
    # Override the dirname if it is empty
    if dirname == None:
        dirname = 'nodes'
        try:
            os.makedirs(dirname)
        except:
            pass

    # Create a unique filename for the process
    pid = os.getpid()
    hostname = socket.gethostname()
    hostname = [c for c in hostname if c == ' ' or c == '-' or 
        (c >= 'a' and c <= 'z') or (c >= 'A' and c <= 'Z') or 
        (c >= '0' and c <= '9')]
    hostname = ''.join(hostname)
    filename = os.path.join('.', dirname, '%s-%s' % (hostname, pid))

    logging.debug('Adding node %s to directory %s...' % 
        (addr, dirname))
    
    try:
        f = open(filename, "w")
        f.write("node %s\n" % addr)
        f.close()
    except:
        logging.warning('Failed to write to %s!' % (nodefile,))

###############################################################################
# Server callback
###############################################################################
def server_callback(conn, addr, port, job, tpool, cqueue, timeout):
    logging.debug('Connected to %s:%d.', addr, port)

    try:
        # Send the job identifier
        conn.WriteString(tm_jobid)

        # Verify job id of the answer
        jobid = conn.ReadString(tm_recv_timeout)

        if tm_jobid != jobid:
            logging.error('Job Id mismatch from %s:%d! Self: %s, task manager: %s!',
                conn.address, conn.port, jm_jobid, jobid)
            conn.Close()
            return False

        # Read the type of message
        mtype = conn.ReadInt64(tm_recv_timeout)
        timeout.reset()

        # Termination signal
        if mtype == messaging.msg_terminate:
            logging.info('Received a kill signal from %s:%d.',
                addr, port)
            os._exit(0)

        # Job manager is sending heartbeats
        if mtype == messaging.msg_send_heart:
            logging.debug('Received heartbeat from %s:%d', addr, port)

        # Job manager is trying to send tasks to the task manager
        elif mtype == messaging.msg_send_task:
            # Two phase pull: test-try-pull
            while not tpool.Full():
                # Task pool is not full, start asking for data
                conn.WriteInt64(messaging.msg_send_more)
                taskid = conn.ReadInt64(tm_recv_timeout)
                runid = conn.ReadInt64(tm_recv_timeout)
                tasksz = conn.ReadInt64(tm_recv_timeout)
                task = conn.Read(tasksz, tm_recv_timeout)
                logging.info('Received task %d from %s:%d.',
                    taskid, addr, port)

                # Try enqueue the received task
                if not tpool.Put(taskid, runid, task):
                    # For some reason the pool got full in between
                    # (shouldn't happen)
                    logging.warning('Rejecting task %d because ' +
                        'the pool is ful!', taskid)
                    conn.WriteInt64(messaging.msg_send_rjct)

            # Task pool is full, stop receiving tasks
            conn.WriteInt64(messaging.msg_send_full)

        # Job manager is querying the results of the completed tasks
        elif mtype == messaging.msg_read_result:
            taskid = None
            try:
                # Dequeue completed tasks until cqueue fires
                # an Empty exception
                while True:
                    # Pop the task
                    taskid, runid, r, res = cqueue.get_nowait()

                    logging.info('Sending task %d to committer %s:%d...',
                        taskid, addr, port)

                    # Send the task
                    conn.WriteInt64(taskid)
                    conn.WriteInt64(runid)
                    conn.WriteInt64(r)
                    if res == None:
                        conn.WriteInt64(0)
                    else:
                        conn.WriteInt64(len(res))
                        conn.Write(res)

                    # Wait for the confirmation that the task has
                    # been received by the other side
                    ans = conn.ReadInt64(messaging.msg_read_result)
                    if ans != messaging.msg_read_result:
                        logging.warning('Unknown response received from '+
                            '%s:%d while committing task!', addr, port)
                        raise messaging.MessagingError()

                    taskid = None

            except queue.Empty:
                # Finish the response
                conn.WriteInt64(messaging.msg_read_empty)

            except:
                # Something went wrong while sending, put
                # the last task back in the queue
                if taskid != None:
                    cqueue.put((taskid, runid, r, res))
                    logging.info('Task %d put back in the queue.', taskid)
                pass

        # Unknow message received or a wrong sized packet could be trashing
        # the buffer, don't do anything
        else:
            logging.warning('Unknown message received \'%d\'!', mtype)

    except messaging.SocketClosed:
        logging.debug('Connection to %s:%d closed from the other side.',
            addr, port)

    except socket.timeout:
        logging.warning('Connection to %s:%d timed out!', addr, port)

    except:
        logging.warning('Error occurred while reading request from %s:%d!',
            addr, port)
        traceback.print_exc()

    conn.Close()
    logging.debug('Connection to %s:%d closed.', addr, port)

###############################################################################
# Initializer routine for the worker
###############################################################################
def initializer(cqueue, job, argv, active_workers, timeout):
    logging.info('Initializing worker...')
    return job.spits_worker_new(argv)

###############################################################################
# Worker routine
###############################################################################
def worker(state, taskid, runid, task, cqueue, job, argv, active_workers, timeout):
    timeout.reset()
    active_workers.inc()
    logging.info('Processing task %d from job %d...', taskid, runid)

    # Execute the task using the job module
    r, res, ctx = job.spits_worker_run(state, task, taskid)

    logging.info('Task %d processed.', taskid)

    if res == None:
        logging.error('Task %d did not push any result!', taskid)
        return

    if ctx != taskid:
        logging.error('Context verification failed for task %d!', taskid)
        return

    # Enqueue the result
    cqueue.put((taskid, runid, r, res[0]))
    active_workers.dec()

###############################################################################
# Run routine
###############################################################################


class AtomicInc(object):
    def __init__(self, value=0):
        self.value = value
        self.lock = Lock()

    def inc(self):
        self.lock.acquire()
        self.value += 1
        self.lock.release()

    def dec(self):
        self.lock.acquire()
        self.value -= 1
        self.lock.release()

    def get(self):
        return self.value


class App(object):
    def __init__(self, args):
        self.args = args
        self.timeout = Timeout(tm_timeout, self.timeout_exit)
        self.job = JobBinary(args.margs[0])
        self.cqueue = queue.Queue()
        self.active_workers = AtomicInc()
        data = (self.cqueue, self.job, self.args.margs, self.active_workers, self.timeout)
        self.tpool = TaskPool(tm_nw, tm_overfill, initializer, worker, data)
        self.server = Listener(tm_mode, tm_addr, tm_port, server_callback,
                               (self.job, self.tpool, self.cqueue, self.timeout))

    def run(self):
        argv = self.args.margs
        self.timeout.reset()
        logging.info('Starting workers...')
        self.tpool.start()
        logging.info('Starting network listener...')
        self.server.Start()
        addr = self.server.GetConnectableAddr()
        logging.info('ANNOUNCE %s' % addr)
        if tm_announce == config.announce_cat_nodes:
            announce_cat(addr)
        elif tm_announce == config.announce_file:
            announce_file(addr)
        logging.info('Waiting for work...')
        self.server.Join()

    def timeout_exit(self):
        if self.tpool.Empty() and self.active_workers.get() <= 0:
            logging.error('Task Manager exited due to timeout')
            os._exit(1)
            return False
        else:
            return True

###############################################################################
# Main routine
###############################################################################
def main(argv):
    if len(argv) <= 1:
        abort('USAGE: tm [args] module [module args]')
    args = Args.Args(argv[1:])
    parse_global_config(args.args)
    setup_log()
    logging.debug('Hello!')
    App(args).run()
    logging.debug('Bye!')

###############################################################################
# Entry point
###############################################################################
if __name__ == '__main__':
    main(sys.argv)
