pm
==
Process manager - task for kodisoft internship. 

==

ProcessMonitor - class that launches and monitors win32 process. Class constructor accepts command line path and arguments. Constructor can also accept process ID and ProcessMonitor starts watching this process. Class instance launches a process and watches it`s state. 
In case of exit or crash ProcessMonitor, it starts process with exact same arguments as it was started before.
ProcessMonitor contains methods for stopping(Pause) process and start it again(Resume).
ProcessMonitor constructor can accept an instance of inheritor of Log class and writes messages into it.

Main.cpp contains example of usage. 
Function CrashTest creates few threads which calls different methods of ProcessMonitor instance at the same time. Function ShowMenu allows you Suspend or Resume process.


