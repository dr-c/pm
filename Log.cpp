#include "Log.h"

StreamLog::StreamLog(wostream &outstream)
	: Log()
	, stream(outstream)
{
	InitializeCriticalSection(&cs);
}

StreamLog::~StreamLog()
{
	DeleteCriticalSection(&cs);
}

void StreamLog::PrintCurrentTime()
{
	WCHAR buf[9];
	time_t t = time(0);
	wcsftime(buf, sizeof(buf), L"%X", localtime(&t));	// Print only time, witout date
	stream << buf;
}

void StreamLog::WriteStartWatch(DWORD PID)
{
	EnterCriticalSection(&cs);
	PrintCurrentTime();
	stream << L" - Process " << PID << " found." << endl;
	LeaveCriticalSection(&cs);
}

void StreamLog::WriteStart(LPCWSTR appName, DWORD PID)
{
	EnterCriticalSection(&cs);
	stream << PID << " === " << appName << endl;
	PrintCurrentTime();
	stream << L" - Process " << PID << " started." << endl;
	LeaveCriticalSection(&cs);
}

void StreamLog::WriteCrash(DWORD PID)
{
	EnterCriticalSection(&cs);
	PrintCurrentTime();
	stream << L" - Process " << PID << " crashed." << endl << "Trying to restart..." << endl;
	LeaveCriticalSection(&cs);
}

void StreamLog::WriteExit(DWORD PID)
{
	EnterCriticalSection(&cs);
	PrintCurrentTime();
	stream << L" - Process " << PID << " exit." << endl;
	LeaveCriticalSection(&cs);
}

void StreamLog::WritePause(DWORD PID)
{
	EnterCriticalSection(&cs);
	PrintCurrentTime();
	stream << L" - Process " << PID << " paused." << endl;
	LeaveCriticalSection(&cs);
}

void StreamLog::WriteResume(DWORD PID)
{
	EnterCriticalSection(&cs);
	PrintCurrentTime();
	stream << L" - Process " << PID << " resumed." << endl;
	LeaveCriticalSection(&cs);
}

void StreamLog::WriteWarning(LPWSTR description, DWORD PID, DWORD warning_code)
{
	EnterCriticalSection(&cs);
	PrintCurrentTime();
	stream << L" - Warning on " << PID << endl << description << endl;
	if (warning_code)
		stream << L" Error code = " << warning_code << endl;
	LeaveCriticalSection(&cs);
}


void StreamLog::WriteError(LPWSTR description, DWORD PID, DWORD error_code)
{
	EnterCriticalSection(&cs);
	PrintCurrentTime();
	stream << L" - Error on " << PID << endl << description << endl;
	if (error_code)
		stream << L" Error code = " << error_code << endl;
	LeaveCriticalSection(&cs);
}

ConsoleLog::ConsoleLog() 
	: StreamLog(wcout)
{

}

FileLog::FileLog(LPWSTR fName)
	: fout(fName, ios_base::out),
	StreamLog(fout)
{

}

FileLog::~FileLog()
{
	fout.close();
}