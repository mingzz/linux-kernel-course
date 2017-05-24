import Queue
import random
import time
import os
import subprocess

class Process:
    pid = 0
    cpu = 0
    mem = 0

    def __init__(self, pid, cpu, mem):
        self.pid = pid
        self.cpu = cpu
        self.mem = mem


class Sched:
    pq = Queue.Queue()
    wq = Queue.Queue()
    child_pool = Queue.Queue()
    cpu_sum = 0
    mem_sum = 0
    c = 3.0

    def __init__(self, wq):
        print ("Process Scheduler is running.")
        self.wq = wq

    def run(self):
        p = self.wq.get()
        child = subprocess.Popen(["./process", str(int(p.cpu*100)), str(int(p.mem*100))])
        print "RUN: Pid ", child.pid, "Process INFO - CPU:"+str(p.cpu)+"% MEMORY:"+str(p.mem)+"%"
        self.child_pool.put(child)
        self.pq.put(p)
        self.cpu_sum += p.cpu
        self.mem_sum += p.mem

    def drop(self):
        p = self.pq.get()
        child = self.child_pool.get()
        print "Switch: Pid ", child.pid, "is switched."
        child.kill()
        self.wq.put(p)
        self.cpu_sum -= p.cpu
        self.mem_sum -= p.mem


def get_cpu():
    load = os.popen("cat /proc/loadavg")
    loadavg = load.read()
    return loadavg.split(' ')[0]

def get_mem():
    free = os.popen("free -m")
    result = free.read().split('\n')[1].split()[2]
    return result


wq = Queue.Queue()
# create some processes
for i in range(0,20):
    a = Process(i, random.random(), random.random())
    wq.put(a)

s = Sched(wq);

count = 1 

while True:
    print "                    Round ", count
    print(" ")
    count += 1
    cpu_status = get_cpu()
    mem_status = get_mem()
    print ("CPU  Memory")
    print (cpu_status),
    print (" "),
    print (mem_status)
    print ("                   Running Process")

    for i in range(0, s.pq.qsize()):
        p = s.pq.get()
	child = s.child_pool.get()
        #print (p.pid),
	print (child.pid),
        s.pq.put(p)
	s.child_pool.put(child)
    print (" ")
#
#    print ("                   Waiting Process")
#    for i in range(0, s.wq.qsize()):
#        p = s.wq.get()
#        print (p.pid),
#        s.wq.put(p)
#    print (" ")

    #print "Schedule decision:"
    if float(cpu_status) < s.c and int(mem_status) < 1700:
        s.run()
    else:
        if not s.pq.empty():
            s.drop()

    time.sleep(2)

    print " "

