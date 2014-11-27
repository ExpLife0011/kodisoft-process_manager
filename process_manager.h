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
#include <fstream>
#include <ctime>


#include <process.h>

#include "my_win.h"
#include "excepts.h"

using namespace std;

typedef NTSTATUS(WINAPI *FUNC)(HANDLE, PROCESSINFOCLASS, PVOID, ULONG, PULONG);

//thread function
unsigned int _stdcall monitoring_function(void* ptr);

enum state {ended, rebooting, active};

class logger{
	friend class monitoring_thread;
public:
	logger();
	logger(logger&& l);
	logger(const string& name);
	~logger();
	logger& operator=(logger&& l);
	friend void swap(logger& _left, logger& _right);

	bool		open(const string& name);
	bool		is_open();
	operator bool(){ return is_open(); };
	bool		close(bool lock = true);
	
	bool		log_current_time(bool lock = true, bool owned = false);
	bool		log_message(const string& message, bool lock = true);
	bool		log_current_time_message(const string& message, bool lock = true);

	//unstable
	void		lock_for_continious_writing(){ locker.lock(); };
	//unstable
	bool		try_lock_for_continious_writing(){ return locker.try_lock(); };
	//unstable
	void		unlock(){ locker.unlock(); };
	
	template<class T>
	logger& operator<<(const T& msg){
		swap_locker.lock();
		locker.lock();
		log << msg;
		locker.unlock();
		swap_locker.unlock();
		return *this;
	};
	
private:
	ofstream& get_log(){ return log; };
	//for swap
	ofstream	log;

	//no swap
	mutex		locker;
	mutex		swap_locker;
};

class monitoring_thread{
public:
	monitoring_thread();
	~monitoring_thread();
	
	//methods
	void					load_path_args(const wstring& path, const wstring& args);

	void					resume_proc_main_thread();
	void					on_start();
	void					on_crash();
	void					on_end();
	void					on_force_stop();

	bool					start_thread();
	bool					terminate_thread();

	bool					start_process();
	bool					process_forse_stop(bool reset);
	bool					open_process(const DWORD& id);

	ofstream&				get_log(){ return log.get_log(); };

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
private:
	bool					terminate_thread_unsafe();
	bool					terminate_thread_safe();
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

	bool					load_path_args(const wstring& path, const wstring& arguments);

	
	bool					start();
	BOOL					force_stop() { return SetEvent(mon_thread.force_stop_event); };
	int						get_process_state();
	void					full_clear();

	//testing
	int						open_process(DWORD id);

	bool					load_on_proc_start_function(function<void()>& func, bool wait = false);
	bool					load_on_proc_crash_function(function<void()>& func, bool wait = false);
	bool					load_on_proc_end_function(function<void()>& func, bool wait = false);
	bool					load_on_proc_manual_stop_function(function<void()>& func, bool wait = false);

	bool					reset_on_proc_start_function(bool wait = false);
	bool					reset_on_proc_crash_function(bool wait = false);
	bool					reset_on_proc_end_function(bool wait = false);
	bool					reset_on_proc_manual_stop_function(bool wait = false);

	void					change_log(logger& l) { mon_thread.log = move(l); };

private:
	void					on_init();

	static mutex			global_locker;
	static unsigned			handlers;
	unsigned				handler;
	monitoring_thread		mon_thread;
	mutex					locker;
};