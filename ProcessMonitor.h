#pragma once

#include "Log.h"
#include <functional>	//function<>
#include <winternl.h>	//NtQueryInformationProcess, ntdll.dll
#include <TlHelp32.h>	//CreateToolhelp32Snapshot

typedef NTSTATUS (NTAPI *pfnNtQueryInformationProcess)(
	IN	HANDLE ProcessHandle,
    IN	PROCESSINFOCLASS ProcessInformationClass,
    OUT	PVOID ProcessInformation,
    IN	ULONG ProcessInformationLength,
    OUT	PULONG ReturnLength	OPTIONAL
);

enum PROCESSSTATE { START_ERROR, WORKING, RESTARTING, SUSPENDED };

// Class to launct, monitor and manage win32 process
class ProcessMonitor
{
private:
	// Used to launch process after crash
	LPCWSTR lpApplicationName;
	LPWSTR lpCommandLine;

	Log* log;

	// Handle of thread that signalize where process crashed
	HANDLE hCrashMonitor;
	// Handle of semaphore that signalize thread MonitorCrash to exit
	HANDLE hSemaphore;
	CRITICAL_SECTION cs;
	PROCESS_INFORMATION pi;
	PROCESSSTATE state;
	
	// Callback functions
	function<void()> fOnProcStart;
	function<void()> fOnProcCrash;
	function<void()> fOnProcPause;
	function<void()> fOnProcResume;

	// Waits while process executes and restart it on terminate
	static DWORD WINAPI MonitorCrash(LPVOID lpParameter);
	// Used instead delegating construction (VS2012 not support)
	void InitLaunch();
	// Starts new process and thread MonitorCrash
	void Start();
	// Detects state of process. (SUSPENDED or RESUMED)
	void DetectState();

public:
	// Starts new process with lpAppName and lpCmdLine
	ProcessMonitor(LPCWSTR lpAppName, LPWSTR lpCmdLine, Log* logger);
	ProcessMonitor(LPCWSTR lpAppName, LPWSTR lpCmdLine);
	// Finds running process with PID
	ProcessMonitor(DWORD PID, Log* log);
	~ProcessMonitor();

	// Resumes all threads in process
	void Resume();
	// Suspends all threads in process
	void Suspend();

	// Sets Callback functions
	void SetOnProcStart(function<void()> func);
	void SetOnProcCrash(function<void()> func);
	void SetOnProcPause(function<void()> func);
	void SetOnProcResume(function<void()> func);

	// Retrives process information
	HANDLE GetProcessHandle();
	DWORD GetProcessID();
	PROCESSSTATE GetProcessState();
};