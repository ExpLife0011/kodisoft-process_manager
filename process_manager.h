#pragma once

//defining unicode char set if not defined

#ifndef UNICODE
#define UNICODE 1
#endif
#ifndef _UNICODE
#define _UNICODE 1
#endif

//process manager STATES
#define PM_ENDED 0
#define PM_REBOOTING 1
#define PM_ACTIVE 2

#include <thread>
#include <mutex>
#include <functional>
#include <string>
#include <process.h>

#include "logger.h"
#include "my_win.h"
#include "excepts.h"


using namespace std;

typedef NTSTATUS(WINAPI *FUNC)(HANDLE, PROCESSINFOCLASS, PVOID, ULONG, PULONG);

//thread function
unsigned int _stdcall monitoring_function(void* ptr);

enum state {ended, rebooting, active};

class monitoring_thread{
	friend class process_manager;
public:
	monitoring_thread();
	~monitoring_thread();
	
	//methods
	void					load_path_args(const wstring& path, const wstring& args);

	void					resume_proc_main_thread();

	bool					start_thread();
	bool					terminate_thread();

	bool					start_process();
	bool					process_forse_stop(bool reset);
	bool					open_process(const DWORD& id);

	ofstream&				get_log(){ return log.get_log(); };

	DWORD					wait_for_event(){ return WaitForMultipleObjects(3, &terminate_thread_event, FALSE, INFINITE); };
	BOOL					get_exit_code_process(DWORD& end_result){ return GetExitCodeProcess(proc_info.hProcess, &end_result); };

	DWORD					get_last_exit_code() { return last_exit_code; };
	void					set_last_exit_code(DWORD& exit_code) { last_exit_code = exit_code; };

	void					log_time_message(const string& msg){ log.log_current_time_message(msg); };

	//events functions
	void					on_start();
	void					on_crash();
	void					on_end();
	void					on_force_stop();

	bool					is_process_up(){ return process_up; };
	
private:
	//variables
	bool					process_up;
	bool					rebooting;
	bool					monitoring;
	wstring					path;
	wstring					arguments;
	unsigned				thread_id;
	HANDLE					thread_handle;
	HANDLE					terminate_thread_event;
	HANDLE					force_stop_event;
	PROCESS_INFORMATION		proc_info;
	DWORD					last_exit_code;
	mutex					locker;
	mutex					func_locker;
	function<void()>		ProcStart;
	function<void()>		ProcCrash;
	function<void()>		ProcEnded;
	function<void()>		ProcManuallyStopped;
	logger log;

	bool					terminate_thread_unsafe();
	bool					terminate_thread_safe();

	//just for fun
	DWORD					wait_time;
};

class process_manager
{
public:
	process_manager();
	process_manager(const wstring& path, const wstring& arguments);
	~process_manager();

	static	unsigned		get_handlers() { return handlers; };
	unsigned				get_handler() const { return handler; };
	HANDLE					get_process_handle() const { return mon_thread.proc_info.hProcess; };
	DWORD					get_process_id() const { return mon_thread.proc_info.dwProcessId; };
	wstring					get_current_path() { return mon_thread.path; };
	wstring					get_current_arguments() { return mon_thread.arguments; };

	//use it carefully for monitoring mutex only
	mutex&					get_thread_mutex() { return mon_thread.locker;};

	bool					load_path_args(const wstring& path, const wstring& arguments, bool lock = true);

	
	bool					start(bool lock = true);
	BOOL					force_stop() { return SetEvent(mon_thread.force_stop_event); };
	int						get_process_state();
	void					clear_process_info(bool lock = true);
	void					full_clear(bool lock = true);

	bool					open_process(DWORD id);

	bool					load_on_proc_start_function(function<void()>& func, bool wait = true);
	bool					load_on_proc_crash_function(function<void()>& func, bool wait = true);
	bool					load_on_proc_end_function(function<void()>& func, bool wait = true);
	bool					load_on_proc_manual_stop_function(function<void()>& func, bool wait = true);

	bool					reset_on_proc_start_function(bool wait = true);
	bool					reset_on_proc_crash_function(bool wait = true);
	bool					reset_on_proc_end_function(bool wait = true);
	bool					reset_on_proc_manual_stop_function(bool wait = true);

	void					change_log(logger& l) { mon_thread.log = move(l); };

private:
	void					on_init();

	static mutex			global_locker;
	static unsigned			handlers;
	unsigned				handler;
	monitoring_thread		mon_thread;
	mutex					locker;
};