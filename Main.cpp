#include "ProcessMonitor.h"
#include <tchar.h>	//TCHAR
#include <conio.h>	//_getch
#include <stdlib.h>	//rand, srand

// Stores thread handles for CrashTest
HANDLE* threads = NULL;
// Stores thread count for CrashTest
int cnt = 0;
// Synchronize console outpute line-by-line
CRITICAL_SECTION console_cs;

// Suspends all threads, while MonitorCrash thread trying to restart process
void OnProcCrash()
{
	wcout << L"crash" << endl;
	if (threads && cnt > 0)
		for (int i=0; i<cnt; i++)
			SuspendThread(*(threads + i));
}

// Resumes all threads, after MonitorCrash thread restart process
void OnProcStart()
{
	wcout << L"start" << endl;
	if (threads && cnt > 0)
		for (int i=0; i<cnt; i++)
			ResumeThread(*(threads + i));
}

// Calls different methods for ProcessMonitor or Terminate process. 
// ProcessMonitor must restart process after terminate.
DWORD WINAPI ThreadFunc(LPVOID lpParameter)
{
	ProcessMonitor* pm = (ProcessMonitor*)lpParameter;
	srand(time(NULL));
	for (int i = 0; i < 200; i++)
	{
		int x = rand() % 1000;
		if (x < 490)		// 0 .. 489
			pm->Suspend();
		else if (x < 980)	// 490 .. 979
			pm->Resume();
		else if (x < 990)	// 980 .. 989
		{
			EnterCriticalSection(&console_cs);
			wcout << L"ID = " << pm->GetProcessID() << endl;
			LeaveCriticalSection(&console_cs);
		}
		else if (x < 999)	// 990 .. 998
		{
			EnterCriticalSection(&console_cs);
			wcout << L"state = " << pm->GetProcessState() << endl;
			LeaveCriticalSection(&console_cs);
		}
		else				// 999
			TerminateProcess(pm->GetProcessHandle(), 0);
	}
	return 0;
}

// Test ProcessMonitor for thread-safe. 
// Starts thread_count ThreadFunc threads
void CrashTest(ProcessMonitor* pm, int thread_count)
{
	InitializeCriticalSection(&console_cs);
	threads = new HANDLE[cnt = thread_count];
	for (int i = 0; i < thread_count; i++)
		threads[i] = CreateThread(NULL, 0, ThreadFunc, pm, 0, NULL);
	WaitForMultipleObjects(thread_count, threads, TRUE, INFINITE);
	DeleteCriticalSection(&console_cs);
}

// Shows menu which allows to user s-suspend or r-resume process, or q-exit from menu
void ShowMenu(ProcessMonitor* pm)
{
	int ch;
	bool b = true;

	system("cls");
	wcout << L"s - Suspend" << endl << L"r - Resume" << endl << L"q - quit" << endl;
	do
	{
		switch (ch = _getch())
		{
		case 's':
			pm->Suspend();
			break;
		case 'r':
			pm->Resume();
			break;
		case 'q':
			b = false;
			break;
		}
	} while (b);
}

int _tmain(int argc, _TCHAR* argv[])
{
	// Starts CarshTest with 10 threads for calc.exe process and saves log-messages to file log.txt
	Log* flog = new FileLog(L"log.txt");
	ProcessMonitor* pm = new ProcessMonitor(L"C:\\Windows\\System32\\calc.exe", NULL, flog);
	pm->SetOnProcCrash(OnProcCrash);
	pm->SetOnProcStart(OnProcStart);
	CrashTest(pm, 10);
	delete pm;
	delete flog;

	// Shows menu for manage calc.exe process and shows log-messages on console.
	Log* clog = new ConsoleLog();
	ProcessMonitor* pm1 = new ProcessMonitor(L"C:\\Windows\\System32\\calc.exe", NULL, clog);
	ShowMenu(pm1);
	pm1->Suspend();

	// Finds already started calc.exe
	ProcessMonitor* pm2 = new ProcessMonitor(pm1->GetProcessID(), clog);
	pm2->Resume();	// pm1.state still will be SUSPENDED	!!!
	wcout << "pm2.ID = " << pm2->GetProcessID() << endl;
	wcout << "pm2.State = " << pm2->GetProcessState() << endl;
	Sleep(3000);
	//Terminate calc.exe. pm1 and pm2 restart calc.exe, so starts 2 instances
	if (TerminateProcess(pm2->GetProcessHandle(), 0))
		Sleep(3000);
	else
		wcout << "Fail to terminate process." << endl;
	pm2->Suspend();
	wcout << "pm1.State = " << pm1->GetProcessState() << endl;	//pm1.State = WORKING
	wcout << "pm2.State = " << pm2->GetProcessState() << endl;	//pm2.State = SUSPENDED
	system("PAUSE");
	//Terminates both calc.exe processes
	delete pm2;
	delete pm1;
	delete clog;
	system("PAUSE");
	return 0;
}