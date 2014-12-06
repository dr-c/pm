#pragma once

#ifndef UNICODE
#define UNICODE
#endif

#include <Windows.h>		
#include <ctime>	//time
#include <iostream>	//wcout
#include <fstream>	//wofstream

using namespace std;

// Interface for store/show log messages.
class Log
{
public:
	// Make record about already started process with PID has found
	virtual void WriteStartWatch(DWORD PID) = 0;
	// Make record about starting process appName with PID 
	virtual void WriteStart(LPCWSTR appName, DWORD PID) = 0;
	// Make record about crash process with PID
	virtual void WriteCrash(DWORD PID) = 0;
	// Make record about exit process with PID
	virtual void WriteExit(DWORD PID) = 0;
	// Make record about suspend process with PID
	virtual void WritePause(DWORD PID) = 0;
	// Make record about resume process with PID
	virtual void WriteResume(DWORD PID) = 0;
	// Make record about warning in the process with PID
	// warning_code usually not used or accept value GetLastError()
	virtual void WriteWarning(LPWSTR description, DWORD PID, DWORD warning_code = 0) = 0;
	// Make record about error in the process with PID
	// error_code usually not used or accept value GetLastError()
	virtual void WriteError(LPWSTR description, DWORD PID, DWORD error_code = 0) = 0;
};

// Abstract class for Log class based on wostrem, like snadard or file output stream
class StreamLog : public Log
{
private:
	// Used for line-by-line output on stream
	CRITICAL_SECTION cs;
	wostream &stream;
	// Print time to outpute stream. Use it before main part of log-messages
	void PrintCurrentTime();

public:
	StreamLog(wostream &outstream);
	~StreamLog();
	virtual void WriteStartWatch(DWORD PID);
	virtual void WriteStart(LPCWSTR appName, DWORD PID);
	virtual void WriteCrash(DWORD PID);
	virtual void WriteExit(DWORD PID);
	virtual void WritePause(DWORD PID);
	virtual void WriteResume(DWORD PID);
	virtual void WriteWarning(LPWSTR description, DWORD PID, DWORD warning_code = 0);
	virtual void WriteError(LPWSTR description, DWORD PID, DWORD error_code = 0);
};

// Class for output log messages on console
class ConsoleLog : public StreamLog
{
public:
	ConsoleLog();
};

// Class for output log messages on file.
class FileLog : public StreamLog
{
private:
	wofstream fout;
public:
	// Open file fName for output
	FileLog(LPWSTR fName);
	// Close file
	~FileLog();
};

/*class WindowLog : public Log
{
private:
	HWND hWnd;
public:
	WindowLog(HWND hwnd);
	virtual void WriteStartWatch(DWORD PID);
	virtual void WriteStart(LPCWSTR name, DWORD PID);
	virtual void WriteCrash(DWORD PID);
	virtual void WriteExit(DWORD PID);
	virtual void WritePause(DWORD PID);
	virtual void WriteResume(DWORD PID);
	virtual void WriteWarning(LPWSTR description, DWORD PID, DWORD warning_code = 0);
	virtual void WriteError(LPWSTR description, DWORD PID, DWORD error_code = 0);
};*/