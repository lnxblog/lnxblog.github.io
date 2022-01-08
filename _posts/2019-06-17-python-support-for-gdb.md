---
layout: post
title: Python in GDB
---

One of the neat features of the GNU Debugger is the support for running python scripts in the GDB environment. This is helpful for logging and automating debug sessions.
In the gdb environment we run `pi` to launch an interactive python session.
```
(gdb) pi
>>> gdb.breakpoints()
(<gdb.Breakpoint object at 0x7f78b0c8e418>,)

>>> gdb.execute("info inferiors")
  Num  Description       Executable        
* 1    process 3850      /home/hema/C_progs/a.out 

>>> gdb.execute("info shared")
From                To                  Syms Read   Shared Object Library
0x00007ffff7ddab00  0x00007ffff7df55b0  Yes         /lib64/ld-linux-x86-64.so.2
0x00007ffff7bc19f0  0x00007ffff7bce471  Yes         /lib/x86_64-linux-gnu/libpthread.so.0
0x00007ffff7812520  0x00007ffff795ae03  Yes         /lib/x86_64-linux-gnu/libc.so.6

>>> gdb.solib_name(0x00007ffff78125ff)
'/lib/x86_64-linux-gnu/libc.so.6'
```
You can read more about it on their online docs. [ Python-GDB API ](https://sourceware.org/gdb/current/onlinedocs/gdb/Python-API.html#Python-API)

Now before diving into the example, I would like to mention about GDB's powerful utility to set thread-specific breakpoints, which means you can choose which thread is allowed to hit a breakpoint.
The syntax is simple
```
(gdb) b <breakpoint_location> thread <thread_number>
```
Now let's consider a simple multi threaded program.
```c
#include <stdio.h>
#include <pthread.h>

int thread_echo(int thread_num)
{
	//Nothing to do here
	return 100;
}

void* thread_func(void* num)
{
	int *echo_arg;
	echo_arg=(int*)num;
	while(1)
	thread_echo(*echo_arg);	
}

int main()
{
	pthread_t threads[5];
	int i,arg[5];
	
	for (i=0;i<5;i++)
	{
		arg[i]=i;
		pthread_create(&threads[i],NULL,thread_func,&arg[i]);
	}
	pthread_join(threads[0],NULL);
	return 0;
}
```
Now suppose we would like to log the entries to `thread_echo` function in a thread-specific manner. We can do this by setting a breakpoint at the function thread-wise and collect the arguments and return value whenever it is hit via a python script. 

GDB provides a general event facility so that Python code can be notified of various state changes, particularly changes that occur in the inferior. 

In order to be notified of an event, you must register an event handler with an event registry.
A stop event indicates that the inferior has stopped.
A stop event handler is used to provide the user an option to stop the logging and change the thread selection.
Here I have tested this script on a ARM v7 based board

```python
try:
	import gdb
except ImportError as e:
	raise ImportError("This script must be run in GDB: ", str(e))

logFileName = "./multithread_log.txt"
flag = 0
def stop_handler (event):
	global flag
	print("thread %s" %(event.inferior_thread))
	if isinstance(event,gdb.SignalEvent):
		print("event stop signal %s" %(event.stop_signal))
		if event.stop_signal == 'SIGINT':
			print("process got interrupted")
			resp = raw_input("continue logging ? (y/n)")
			if resp == 'n':
				flag = 0
			else:
				set_breakpoints()
				gdb.execute("c")

gdb.events.stop.connect (stop_handler)

class ConsoleColorCodes:
	RED = '\033[91m'
	BLUE = '\033[94m'
	YELLOW = '\033[93m'
	END = '\033[0m'

class Utility:
	@staticmethod
	def writeColorMessage(message, colorCode):
		print(colorCode + message + ConsoleColorCodes.END)

	@staticmethod
	def writeMessage(message):
		Utility.writeColorMessage(message, ConsoleColorCodes.BLUE)

	@staticmethod
	def writeErrorMessage(message):
		Utility.writeColorMessage(message, ConsoleColorCodes.RED)

	@staticmethod
	def logInfoMessage(message):
		global logFileName
		with open(str(logFileName), 'a') as logFile:
			logFile.write(str(message))

	@staticmethod
	def writeInfoMessage(message):
		Utility.logInfoMessage(message)
		Utility.writeColorMessage(message, ConsoleColorCodes.RED)

	@staticmethod
	def convertToHexString(val):
		return hex(int(val))

Functions = ["thread_echo"]
thread_echo_Args = ["thread number"]

max_callstack_depth = 16

def get_callstack():
	ret = []
	depth = 1
	frame = gdb.selected_frame()
	while True:
		if (frame) and ( depth <= max_callstack_depth ):
			current_frame_name = str(frame.name())
			ret.append(current_frame_name)
			frame = frame.older()
			depth += 1
		else:
			gdb.Frame.select ( gdb.newest_frame() )
			return ret

def get_return_value():
	ret = ""
	gdb.execute("finish")
	ret = str(gdb.parse_and_eval("$r0"))
	return ret

def get_argument (counter):
	index = counter - 1
	return str(gdb.parse_and_eval("$r" + str(index)))

def append_callstack(target, callstack):
	ret = str(target) + "\ncallstack : "
	for callstack_frame in callstack:
		ret += "\n\t"
		ret += str(callstack_frame)
	return ret

def set_breakpoints():
	global flag
	gdb.execute("info threads")
	print("choose thread to log")
	thread_num = raw_input()
	if thread_num == 'q':
		print("stop log")
		flag = 0
		return
	# Remove breakpoints
	gdb.execute("delete breakpoints")

	# Setup breakpoints for memory functions
	for Function in Functions:
		gdb.execute("b " + Function + " thread " + thread_num )
		print("breakpoints set")

gdb.execute("set confirm off")
gdb.execute("set pagination off")
gdb.execute("set breakpoint pending on")
backtrace_limit_command = "set backtrace limit " + str(max_callstack_depth)
gdb.execute(backtrace_limit_command)
flag = 1
gdb.execute("b main")
gdb.execute("r")
set_breakpoints()
gdb.execute("i b")
gdb.execute("c")

while flag == 1:
	print("entered while loop")
	try:
		frame = gdb.selected_frame()
	except:
		set_breakpoints()
		gdb.execute("c")
	print("current frame: " + str(frame.name()))
	current_frame_name = str(frame.name())

	##########################################################################
	#MAIN
	if "main" in current_frame_name:
		gdb.execute("cont")
	##########################################################################
	#EXIT
	if "exit" in current_frame_name:
		Utility.writeInfoMessage("\n")
		break
	##########################################################################

	if "thread_echo" in current_frame_name:
		message = "\ntype : thread_echo ,"

		counter = 1
		for arg in thread_echo_Args:
			message += " arg" + str(counter) + "  : "
			message += get_argument(counter)
			message += ","
			counter += 1

		callstack = get_callstack()

		address = Utility.convertToHexString(get_return_value())
		message += " return value : " + address
		message += ","
		# append callstack at the end
		message = append_callstack(message, callstack)
		# Continue
		Utility.writeInfoMessage(message)
		gdb.execute("cont")
Utility.writeMessage('DEBUGEE EXITING')
```

Here is a sample session.

```
Breakpoint 3, 0x0001050c in thread_echo ()
thread None
entered while loop
current frame: thread_echo
0x00010558 in thread_func ()
thread None

type : thread_echo , arg1  : 1, return value : 0x64,
callstack :
        thread_echo
        thread_func
        start_thread
        None

Breakpoint 3, 0x0001050c in thread_echo ()
thread None
entered while loop
current frame: thread_echo
0x00010558 in thread_func ()
thread None

type : thread_echo , arg1  : 1, return value : 0x64,
callstack :
        thread_echo
        thread_func
        start_thread
        None
		
^C
Program received signal SIGINT, Interrupt.
[Switching to Thread 0x75e3d460 (LWP 1367)]
0x0001050c in thread_echo ()

event stop signal SIGINT
process got interrupted
continue logging ? (y/n)y
  Id   Target Id         Frame
  6    Thread 0x74e3d460 (LWP 1370) "a.out" 0x0001050c in thread_echo ()
  5    Thread 0x7563d460 (LWP 1368) "a.out" 0x00010510 in thread_echo ()
* 4    Thread 0x75e3d460 (LWP 1367) "a.out" 0x0001050c in thread_echo ()
  3    Thread 0x7663d460 (LWP 1366) "a.out" 0x0001050c in thread_echo ()
  2    Thread 0x76e3d460 (LWP 1364) "a.out" 0x0001050c in thread_echo ()
  1    Thread 0x76ff7000 (LWP 1361) "a.out" 0x76f87274 in pthread_join (threadid=<optimized out>, thread_return=0x0) at pthread_join.c:92
choose thread to log
5
Breakpoint 4 at 0x1050c
breakpoints set
[Switching to Thread 0x7563d460 (LWP 1368)]

Breakpoint 4, 0x0001050c in thread_echo ()
thread None
entered while loop
current frame: thread_echo
0x00010558 in thread_func ()
thread None

type : thread_echo , arg1  : 3, return value : 0x64,
callstack :
        thread_echo
        thread_func
        start_thread
        None

Breakpoint 4, 0x0001050c in thread_echo ()
thread None
entered while loop
current frame: thread_echo
0x00010558 in thread_func ()
thread None

type : thread_echo , arg1  : 3, return value : 0x64,
callstack :
        thread_echo
        thread_func
        start_thread
        None
```

And finally the multithread_log.txt file
```
type : thread_echo , arg1  : 1, return value : 0x64,
callstack :
        thread_echo
        thread_func
        start_thread
        None
type : thread_echo , arg1  : 1, return value : 0x64,
callstack :
        thread_echo
        thread_func
        start_thread
        None
type : thread_echo , arg1  : 3, return value : 0x64,
callstack :
        thread_echo
        thread_func
        start_thread
        None
type : thread_echo , arg1  : 3, return value : 0x64,
callstack :
        thread_echo
        thread_func
        start_thread
        None
```

