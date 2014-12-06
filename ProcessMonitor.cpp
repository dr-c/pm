#include "ProcessMonitor.h"

// Takes pointer to instance of ProcessMonitor.
// Waits while process executes. When process terminate - restarts it
// Exit when hSemaphore becomes on signal state
DWORD WINAPI ProcessMonitor::MonitorCrash(LPVOID lpParameter)
{
	ProcessMonitor* pm = (ProcessMonitor*)lpParameter;
	HANDLE handles[2] = { pm->pi.hProcess, pm->hSemaphore };
	DWORD index;

	do 
	{
		index = WaitForMultipleObjects(2, handles, FALSE, INFINITE) - WAIT_OBJECT_0;
		switch (index)
		{
		case 0:
			EnterCriticalSection(&pm->cs);
			pm->state = RESTARTING;
			if (pm->log) pm->log->WriteCrash(pm->pi.dwProcessId);
			if (pm->fOnProcCrash)
				pm->fOnProcCrash();
			LeaveCriticalSection(&pm->cs);
			pm->Start();
			handles[0] = pm->pi.hProcess;
			break;
		case 1:
			break;
		default:
			pm->log->WriteError(L"WaitForMultipleObjects returns WAIT_FAILED", pm->pi.dwProcessId, GetLastError());
		}
	} while (index == 0);

	return 0;
}

// Starts new process with lpAppName and lpCmdLine.
ProcessMonitor::ProcessMonitor(LPCWSTR lpAppName, LPWSTR lpCmdLine, Log* logger)
	: lpApplicationName(lpAppName), lpCommandLine(lpCmdLine), log(logger)
{
	InitLaunch();
}

// Starts new process with lpAppName and lpCmdLine.
ProcessMonitor::ProcessMonitor(LPCWSTR lpAppName, LPWSTR lpCmdLine)
	: lpApplicationName(lpAppName), lpCommandLine(lpCmdLine), log(NULL)
{
	InitLaunch();
}

// Used instead delegating construction (VS2012 not support)
// Initializes synchronization primitives and starts new process
void ProcessMonitor::InitLaunch()
{
	fOnProcStart = nullptr;
	fOnProcCrash = nullptr;
	fOnProcPause = nullptr;
	fOnProcResume = nullptr;
	hCrashMonitor = NULL;

	InitializeCriticalSection(&cs);
	hSemaphore = CreateSemaphore(NULL, 0, 1, NULL);
	
	Start();
}

// Initializes synchronization primitives and finds running process with PID
// Tries to load ntdll.dll library. Takes processes snapshot and looks for process with PID
// With NtQueryInformationProcess extracts PEB structure and gets command line from it
// If some step fails - state = START_ERROR. Else calls DetectState()
ProcessMonitor::ProcessMonitor(DWORD PID, Log* logger)
	: lpApplicationName(NULL),
	fOnProcStart(nullptr), fOnProcCrash(nullptr), fOnProcPause(nullptr), fOnProcResume(nullptr),
	hCrashMonitor(NULL),
	log(logger)
{
	InitializeCriticalSection(&cs);
	hSemaphore = CreateSemaphore(NULL, 0, 1, NULL);
	// If we cann`t get command line of process, then state = START_ERROR
	state = START_ERROR;
	// ntdll.dll contains NtQueryInformationProcess function that can read Command Line for process
	HMODULE hModule = LoadLibrary(L"ntdll.dll");
	if (hModule == NULL)
	{
		if (log) log->WriteError(L"Could not load ntdll.dll.", pi.dwProcessId);
		return;
	}
	
	pfnNtQueryInformationProcess NtQueryInformationProcess = (pfnNtQueryInformationProcess)GetProcAddress(hModule,"NtQueryInformationProcess");
	if (NtQueryInformationProcess == NULL)
	{
		FreeLibrary(hModule);
		if (log) log->WriteError(L"Could not retrieve the address of NtQueryInformationProcess function from ntdll.dll.", pi.dwProcessId, GetLastError());
		return;
	}

	PROCESSENTRY32 entry;
	entry.dwSize = sizeof(PROCESSENTRY32);
	PROCESS_BASIC_INFORMATION pbi;
	PEB pebCopy;
	RTL_USER_PROCESS_PARAMETERS RtlProcParamCopy;

	HANDLE snapshot  = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
	if (Process32First(snapshot, &entry))
	{
		ZeroMemory(&pi, sizeof(pi));
		pi.dwProcessId = 0;
		do
		{
			if (entry.th32ProcessID == PID)
			{
				pi.dwProcessId = entry.th32ProcessID;
				pi.hProcess = OpenProcess(PROCESS_VM_READ | PROCESS_TERMINATE | PROCESS_QUERY_INFORMATION | SYNCHRONIZE, FALSE, entry.th32ProcessID);
				if (!pi.hProcess)
				{
					if (log) log->WriteError(L"Could not open an existing local process object.", PID, GetLastError());
					break;
				}

				NTSTATUS status = NtQueryInformationProcess(pi.hProcess, ProcessBasicInformation, &pbi, sizeof(PROCESS_BASIC_INFORMATION), NULL);
				if (!NT_SUCCESS(status))
				{
					if (log) log->WriteError(L"Could not use NtQueryInformationProcess().", PID, status);
					break;
				}

				if (!ReadProcessMemory(pi.hProcess, pbi.PebBaseAddress, &pebCopy, sizeof(PEB), NULL))
				{
					if (log) log->WriteError(L"Could not read process memory().", PID, GetLastError());
					break;
				}

				if (!ReadProcessMemory(pi.hProcess, pebCopy.ProcessParameters, &RtlProcParamCopy, sizeof(RTL_USER_PROCESS_PARAMETERS), NULL))
				{
					if (log) log->WriteError(L"Could not read process memory().", PID, GetLastError());
					break;
				}
				
				//CommandLine stores length in bytes, not symbols. And CommandLine.Buffer declared as WCHAR
				int len = RtlProcParamCopy.CommandLine.MaximumLength / sizeof(WCHAR);
				LPWSTR lpCmdLine = new WCHAR[len];
				if (!ReadProcessMemory(pi.hProcess, RtlProcParamCopy.CommandLine.Buffer, lpCmdLine, RtlProcParamCopy.CommandLine.MaximumLength, NULL))
				{
					if (log) log->WriteError(L"Could not read process memory().", PID, GetLastError());
					delete []lpCmdLine;
					break;
				}
				//lpCmdLine starts and ends with \". Gets substring without quotes with \0 at end.
				lpCommandLine = new WCHAR[len - 2];
				wcsncpy(lpCommandLine, lpCmdLine + 1, len - 3);
				lpCommandLine[len - 3] = L'\0';
				delete []lpCmdLine;

				DetectState();
				if (state != START_ERROR && log)
				{
					log->WriteStartWatch(PID);
					if (!hCrashMonitor)
					{
						DWORD dwID;
						DWORD d = WaitForSingleObject(hSemaphore, 1);
						hCrashMonitor = CreateThread(NULL, 0, MonitorCrash, this, 0, &dwID);
					}
				}
				break;
			}
		} while (Process32Next(snapshot, &entry));

		if (!pi.dwProcessId && log)
			log->WriteError(L"Could not find process to start watch.", PID);
	}
	else
		if (log) log->WriteError(L"Process32First: The first entry of the process list hasn`t been copied to the buffer.", PID, GetLastError());

	CloseHandle(snapshot);

	FreeLibrary(hModule);
}

ProcessMonitor::~ProcessMonitor()
{
	if (hCrashMonitor)
	{
		ReleaseSemaphore(hSemaphore, 1 , NULL);
		CloseHandle(hCrashMonitor);
	}

	if (state == WORKING || state == SUSPENDED)
	{
		if (TerminateProcess(pi.hProcess, NO_ERROR))
		{
			if (log) log->WriteExit(pi.dwProcessId);
		}
		else
		{
			if (log) log->WriteWarning(L"Could not terminate process.", pi.dwProcessId, GetLastError());
		}
	}
	DeleteCriticalSection(&cs);
	CloseHandle(hSemaphore);
}

// Detects state of process. (SUSPENDED or RESUMED)
// Takes threads snapshot. Find all threads which belong for process. 
// If 1 or more threads performed - process working. If all threads suspended - process suspended
void ProcessMonitor::DetectState()
{
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, NULL);
	THREADENTRY32 te32;
	te32.dwSize = sizeof(THREADENTRY32);
	if (Thread32First(hSnapshot, &te32))
	{
		bool hasError = false;
		do
		{
			if (te32.th32OwnerProcessID == pi.dwProcessId)
			{
				HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, te32.th32ThreadID);
				if (hThread != NULL)
				{
					// Suspends and Resumes thread. Uses lastSuspend for detect current thread state (SUSPENDED or RESUMED)
					DWORD lastSuspend = SuspendThread(hThread);
					DWORD error_code = GetLastError();
					if (ResumeThread(hThread) == (DWORD)-1 || lastSuspend == (DWORD)-1)
					{
						hasError = true;
						if (log) log->WriteError(L"Could not suspend or resume thread on DetectState().", pi.dwProcessId, max(GetLastError(), error_code));
						break;
					}
					else if (lastSuspend == 0)
					{
						state = WORKING;
						break;
					}
					CloseHandle(hThread);
				}
				else
					if (log) log->WriteError(L"OpenThread on DetectState().", GetLastError());
			}
		} while (Thread32Next(hSnapshot, &te32));

		if (!hasError && state != WORKING)
			state = SUSPENDED;
	}
	else
		if (log) log->WriteError(L"Thread32First on DetectState(): The first entry of the thread list hasn`t been copied to the buffer.", pi.dwProcessId, GetLastError());

	CloseHandle(hSnapshot);
}

// Starts new process and thread MonitorCrash
void ProcessMonitor::Start()
{
	STARTUPINFO si;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	
	EnterCriticalSection(&cs);
	ZeroMemory(&pi, sizeof(pi));
	if (CreateProcess(lpApplicationName, lpCommandLine, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
	{
		if (log) log->WriteStart(lpApplicationName != NULL ? lpApplicationName : lpCommandLine, pi.dwProcessId);
		state = WORKING;
		if (!hCrashMonitor)
		{
			DWORD dwID;
			hCrashMonitor = CreateThread(NULL, 0, MonitorCrash, this, 0, &dwID);
		}
		if (fOnProcStart)
			fOnProcStart();
	}
	else
	{
		state = START_ERROR;
		if (log) log->WriteError(L"Could not START process.", pi.dwProcessId, GetLastError());
	}
	LeaveCriticalSection(&cs);
}

// Resumes process (Resumes all threads in process)
// Takes threads snapshot and resumes all threads which belong for process
void ProcessMonitor::Resume()
{
	EnterCriticalSection(&cs);
	switch (state)
	{
	case WORKING:
		if (log) log->WriteWarning(L"Could not RESUME process. It`s already working.", pi.dwProcessId);
		break;
	case START_ERROR:
		if (log) log->WriteWarning(L"Could not RESUME process. Starting error.", pi.dwProcessId);
		break;
	case RESTARTING:
		if (log) log->WriteWarning(L"Could not RESUME process. It`s terminate. Trying to restart.", pi.dwProcessId);
		break;
	case SUSPENDED:
		{
			HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, NULL);
			THREADENTRY32 te32;
			te32.dwSize = sizeof(THREADENTRY32);
			if (Thread32First(hSnapshot, &te32))
			{
				do
				{
					if (te32.th32OwnerProcessID == pi.dwProcessId)
					{
						HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, te32.th32ThreadID);
						if (hThread == NULL)
						{
							if (log) log->WriteError(L"OpenThread on Resume().", GetLastError());
							continue;
						}
						if (ResumeThread(hThread) != -1)
							state = WORKING;
						else
							if (log) log->WriteError(L"Could not resume thread on Resume().", GetLastError());
						CloseHandle(hThread);
					}
				} while (Thread32Next(hSnapshot, &te32));

				if (state == WORKING)
				{
					if (log) log->WriteResume(pi.dwProcessId);
					if (fOnProcResume)
						fOnProcResume();
				}
			}
			else
			{
				if (log) log->WriteError(L"Thread32First on Resume(): The first entry of the thread list hasn`t been copied to the buffer.", pi.dwProcessId, GetLastError());
			}

			CloseHandle(hSnapshot);
		}
		break;
	}
	LeaveCriticalSection(&cs);
}

// Suspends process (Suspends all threads in process)
// Takes threads snapshot and suspends all threads which belong for process
void ProcessMonitor::Suspend()
{
	EnterCriticalSection(&cs);
	switch (state)
	{
	case SUSPENDED:
		if (log) log->WriteWarning(L"Could not SUSPEND process. It`s already paused.", pi.dwProcessId);
		break;
	case START_ERROR:
		if (log) log->WriteWarning(L"Could not SUSPEND process. Starting error.", pi.dwProcessId);
		break;
	case RESTARTING:
		if (log) log->WriteWarning(L"Could not SUSPEND process. It`s terminate. Trying to restart.", pi.dwProcessId);
		break;
	case WORKING:
		{
			HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, NULL);
			THREADENTRY32 te32;
			te32.dwSize = sizeof(THREADENTRY32);
			if (Thread32First(hSnapshot, &te32))
			{
				do
				{
					if (te32.th32OwnerProcessID == pi.dwProcessId)
					{
						HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, te32.th32ThreadID);
						if (hThread == NULL)
						{
							if (log) log->WriteError(L"OpenThread on Suspend().", pi.dwProcessId, GetLastError());
							continue;
						}
						if (SuspendThread(hThread) != -1)
							state = SUSPENDED;
						else
							if (log) log->WriteError(L"Could not suspend thread on Suspend().", pi.dwProcessId, GetLastError());
						CloseHandle(hThread);
					}
				} while (Thread32Next(hSnapshot, &te32));

				if (state == SUSPENDED)
				{
					if (fOnProcPause)
						fOnProcPause();
					if (log) log->WritePause(pi.dwProcessId);
				}
			}
			else
				if (log) log->WriteError(L"Thread32First on Suspend(): The first entry of the thread list hasn`t been copied to the buffer.", pi.dwProcessId, GetLastError());

			CloseHandle(hSnapshot);
		}
		break;
	}
	LeaveCriticalSection(&cs);
}

void ProcessMonitor::SetOnProcStart(function<void()> func)
{
	EnterCriticalSection(&cs);
	fOnProcStart = func;
	LeaveCriticalSection(&cs);
}

void ProcessMonitor::SetOnProcCrash(function<void()> func)
{
	EnterCriticalSection(&cs);
	fOnProcCrash = func;
	LeaveCriticalSection(&cs);
}

void ProcessMonitor::SetOnProcPause(function<void()> func)
{
	EnterCriticalSection(&cs);
	fOnProcPause = func;
	LeaveCriticalSection(&cs);
}

void ProcessMonitor::SetOnProcResume(function<void()> func)
{
	EnterCriticalSection(&cs);
	fOnProcResume = func;
	LeaveCriticalSection(&cs);
}

HANDLE ProcessMonitor::GetProcessHandle() 
{
	EnterCriticalSection(&cs);
	HANDLE hProcess = pi.hProcess;
	LeaveCriticalSection(&cs);
	return hProcess;
}

DWORD ProcessMonitor::GetProcessID() 
{
	EnterCriticalSection(&cs);
	DWORD dwProcessId = pi.dwProcessId;
	LeaveCriticalSection(&cs);
	return dwProcessId;
}

PROCESSSTATE ProcessMonitor::GetProcessState() 
{
	EnterCriticalSection(&cs);
	PROCESSSTATE st = state;
	LeaveCriticalSection(&cs);
	return st;
}