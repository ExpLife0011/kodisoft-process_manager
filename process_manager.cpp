#include "process_manager.h"

//thread function
unsigned int _stdcall monitoring_function(void* ptr){
	DWORD wait_result = 0;
	DWORD end_result = 0;
	monitoring_thread* mon_thr = reinterpret_cast<monitoring_thread*>(ptr);

	if (!mon_thr->process_up){
		if (!mon_thr->start_process()){
			mon_thr->log.log_message("cannot start process");
			mon_thr->monitoring = false;
			_endthreadex(1);
		}
	}

	while (mon_thr->process_up){
		wait_result = WaitForMultipleObjects(3, &mon_thr->terminate_thread_event, FALSE, INFINITE);
		switch (wait_result){
			case WAIT_OBJECT_0:
				mon_thr->log.log_current_time_message("thread terminated without process terminating");
				break;

			case WAIT_OBJECT_0+1:
				mon_thr->process_forse_stop(true);
				mon_thr->on_force_stop();
				mon_thr->log.log_current_time_message("forcing to stop process");
				break;

			case WAIT_OBJECT_0+2:
				if (!GetExitCodeProcess(mon_thr->proc_info.hProcess, &end_result)){
					mon_thr->log.log_current_time_message("GetExitCodeProcess failed");
					//temporary solution
					throw(excepts("GetExitCodeProcess fail"));
					break;
				}
				mon_thr->last_exit_code = end_result;
				mon_thr->process_forse_stop(false);
				switch(end_result){
					case 0:
						mon_thr->on_end();
						mon_thr->log.log_current_time_message("process ended with code 0");
						break;
					default:
						mon_thr->on_crash();
						mon_thr->log.log_current_time_message("process crashed with code - " + to_string(end_result));
						mon_thr->rebooting = true;
						if (!mon_thr->start_process()){
							mon_thr->rebooting = false;
							mon_thr->log.log_current_time_message("cannot start process");
							mon_thr->monitoring = false;
							_endthreadex(1);
						}
						mon_thr->rebooting = false;
				}
				break;
			case WAIT_FAILED:
				mon_thr->log.log_current_time_message("WaitForMultipleObjects failed");
				break;
		}
	}
	mon_thr->monitoring = false;
	_endthreadex(0);
	return 0;
}

//static variables initialisation
unsigned int process_manager::handlers = 0;
mutex process_manager::global_locker;

//############
//logger class
//############
logger::logger(){

}

logger::logger(logger&& l){
	l.swap_locker.lock();

	std::swap(log, l.log);

	l.swap_locker.unlock();
}

logger::logger(const string& name){
	log.open(name);
}

logger::~logger(){
	if (log.is_open())
		log.close();
}

logger& logger::operator=(logger&& l){
	swap_locker.lock();
	l.swap_locker.lock();

	log.close();
	std::swap(log, l.log);

	l.swap_locker.unlock();
	swap_locker.unlock();
	return *this;
}

bool logger::open(const string& name){
	swap_locker.lock();
	locker.lock();
	if (log.is_open()){
		locker.unlock();
		swap_locker.lock();
		return false;
	}
	log.open(name);
	locker.unlock();
	swap_locker.unlock();
	return true;
}

bool logger::is_open(){
	swap_locker.lock();
	locker.lock();
	bool result = log.is_open();
	locker.unlock();
	swap_locker.unlock();
	return result;
}

bool logger::close(bool lock){
	swap_locker.lock();
	if (lock)
		locker.lock();
	else if (!locker.try_lock()){
		swap_locker.unlock();
		return false;
	}

	if (log.is_open())
		log.close();

	locker.unlock();
	swap_locker.unlock();
	return true;
}
bool logger::log_current_time(bool lock, bool owned){
	if (!owned)
		if (lock)
			locker.lock();
		else if (!locker.try_lock())
			return false;
	swap_locker.lock();

	time_t current_time = time(nullptr);
	tm* t = localtime(&current_time);
	log << t->tm_mday << "." << t->tm_mon + 1 << "." << t->tm_year + 1900 << " - " << t->tm_hour << ":" << t->tm_min << ":" << t->tm_sec << endl;

	swap_locker.unlock();
	if (!owned)
		locker.unlock();
	return true;
}

bool logger::log_message(const string& message, bool lock){
	if (lock)
		locker.lock();
	else if (!locker.try_lock())
		return false;
	swap_locker.lock();

	log << message;
	if (message[message.length() - 1] != '\n')
		log << endl;

	swap_locker.unlock();
	locker.unlock();
	return true;
}

bool logger::log_current_time_message(const string& message, bool lock){
	if (lock)
		locker.lock();
	else if (!locker.try_lock())
		return false;
	swap_locker.lock();

	time_t current_time = time(nullptr);
	tm* t = localtime(&current_time);
	log << t->tm_mday << "." << t->tm_mon + 1 << "." << t->tm_year + 1900 << " - " << t->tm_hour << ":" << t->tm_min << ":" << t->tm_sec << endl;
	log << message;
	if (message[message.length() - 1] != '\n')
		log << endl;

	swap_locker.unlock();
	locker.unlock();
	return true;
}

void swap(logger& _left, logger& _right){
	_left.swap_locker.lock();
	_right.swap_locker.lock();

	std::swap(_left.log, _right.log);

	_right.swap_locker.unlock();
	_left.swap_locker.unlock();
}


//#######################
//monitoring_thread class
//#######################
monitoring_thread::monitoring_thread() : process_up(false), monitoring(false), rebooting(false), last_exit_code(0), thread_id(0), thread_handle(nullptr), wait_time(100){
	ZeroMemory(&proc_info, sizeof(PROCESS_INFORMATION));
	force_stop_event = HANDLE(CreateEvent(NULL, TRUE, FALSE, NULL));
	
	if (force_stop_event == NULL)
		throw(excepts("cannot create event for force_stop mechanism"));

	terminate_thread_event = HANDLE(CreateEvent(NULL, TRUE, FALSE, NULL));
	if (terminate_thread_event == NULL)
		throw(excepts("cannot create event for safe_terminating_thread mechanism"));
}

monitoring_thread::~monitoring_thread(){
	if (proc_info.hProcess != nullptr){
		TerminateProcess(proc_info.hProcess, 0);
		CloseHandle(proc_info.hProcess);
		proc_info.hProcess = nullptr;
	}
	if (thread_handle != nullptr)
		terminate_thread();
	CloseHandle(terminate_thread_event);
	CloseHandle(force_stop_event);
}

void monitoring_thread::load_path_args(const wstring& path, const wstring& args){
	this->path = path;
	this->arguments = args;

}

void monitoring_thread::on_start(){
	if (bool(ProcStart)){
		func_locker.lock();
		ProcStart();
		func_locker.unlock();
	}
}
void monitoring_thread::on_crash(){
	if (bool(ProcCrash)){
		func_locker.lock();
		ProcCrash();
		func_locker.unlock();
	}
}
void monitoring_thread::on_end() {
	if (bool(ProcEnded)){
		func_locker.lock();
		ProcEnded();
		func_locker.unlock();
	}
}
void monitoring_thread::on_force_stop(){
	if (bool(ProcManuallyStopped)){
		func_locker.lock();
		ProcManuallyStopped();
		func_locker.unlock();
	}
}

bool monitoring_thread::start_thread(){
	if (monitoring)
		return false;
	uintptr_t  result;
	result = _beginthreadex(nullptr, NULL, monitoring_function, reinterpret_cast<void*>(this), NULL, nullptr);
	if (result == NULL || result == -1L)
		return false;
	thread_handle = HANDLE(result);
	monitoring = true;
	return true;
}

bool monitoring_thread::terminate_thread_unsafe(){
	log.log_message("unsafe thread termination");
	bool result = false;
	if (thread_handle != nullptr){
		if (TerminateThread(thread_handle, 0) != 0)
			result = true;
		CloseHandle(thread_handle);
		thread_handle = nullptr;
	}
	monitoring = false;
	return result;
}

bool monitoring_thread::terminate_thread_safe(){
	DWORD result = 0;
	if (SetEvent(force_stop_event) != 0)
		return false;
	result = WaitForSingleObject(thread_handle, wait_time);
	if (result == WAIT_FAILED || result == WAIT_TIMEOUT)
		return false;
	else{
		CloseHandle(thread_handle);
		thread_handle = nullptr;
	}
	monitoring = false;
	return true;
}

bool monitoring_thread::terminate_thread(){
	if (!terminate_thread_safe())
		if (!terminate_thread_unsafe())
			throw(excepts("cannot terminate thread"));
	return true;
}

bool monitoring_thread::start_process(){
	on_start();

	STARTUPINFO sinfo;
	PROCESS_INFORMATION pinfo;

	ZeroMemory(&sinfo, sizeof(sinfo));
	ZeroMemory(&pinfo, sizeof(pinfo));
	sinfo.cb = sizeof(sinfo);

	if (!CreateProcess(path.c_str(), const_cast<LPWSTR>(arguments.c_str()), nullptr, nullptr, FALSE, CREATE_NEW_CONSOLE | CREATE_SUSPENDED, NULL, NULL, &sinfo, &pinfo)){
		log.log_current_time_message("failed to create process with error - " + to_string(GetLastError()) + '\n');
		return false;
	}

	proc_info = pinfo;

	log.locker.lock();
	log.log_current_time(true,true);
	log.log << "Created process with id - " << proc_info.dwProcessId << endl;
	log.log << "Hadle - " << proc_info.hProcess << endl;
	log.log << "Main thread id - " << proc_info.dwThreadId << endl;
	log.log << "Main thread handle - " << proc_info.hThread << endl;
	log.locker.unlock();

	while (ResumeThread(proc_info.hThread) > 0);

	process_up = true;
	return true;
}

bool monitoring_thread::process_forse_stop(bool reset){
	if (reset)
		ResetEvent(force_stop_event);

	if (proc_info.hProcess != nullptr){
		TerminateProcess(proc_info.hProcess, 0);
		CloseHandle(proc_info.hProcess);
		proc_info.hProcess = nullptr;
	}
	last_exit_code = 0;
	process_up = false;
	return true;
}


////////////////////////////////////////////////////////////////
//process_manager class (just a monitoring_thread class wrapper)
////////////////////////////////////////////////////////////////
process_manager::process_manager(){
	on_init();
}

process_manager::process_manager(const wstring& path, const wstring& arguments) {
	on_init();

	load_path_args(path, arguments);
	if (!start())
		throw(excepts("constructor - can't start monitoring thread"));
}

void process_manager::on_init(){
	global_locker.lock();
	handlers++;
	handler = handlers;
	global_locker.unlock();

	string f_name = "log_file_" + to_string(handlers) + ".txt";
	mon_thread.log.open(f_name);
	mon_thread.log.lock_for_continious_writing();
	mon_thread.log.log_current_time(true,true);
	mon_thread.get_log() << "initialised process_manager object " << handlers << " - " << reinterpret_cast<void*>(this) << endl;
	mon_thread.log.unlock();
}


process_manager::~process_manager(){
	if (mon_thread.log.is_open())
		mon_thread.log.close();
}

bool process_manager::load_path_args(const wstring& path_in, const wstring& arguments_in, bool lock){
	if (mon_thread.process_up || mon_thread.monitoring || path_in.empty())
		return false;
	if (lock)
		locker.lock();
	mon_thread.path = path_in;
	mon_thread.arguments = arguments_in;
	if (lock)
		locker.unlock();
	return true;
}

bool process_manager::start(bool lock){
	if (lock)
		locker.lock();
	bool result = mon_thread.start_thread();
	if (lock)
		locker.unlock();
	return result;
}

int process_manager::get_process_state(){
	if (mon_thread.process_up)
		return PM_ACTIVE;
	if (mon_thread.rebooting)
		return PM_REBOOTING;
	return PM_ENDED;
}

void process_manager::clear_process_info(bool lock){
	if (lock)
		locker.lock();
	mon_thread.process_forse_stop(true);
	mon_thread.terminate_thread();
	mon_thread.last_exit_code = 0;
	mon_thread.path.clear();
	mon_thread.arguments.clear();
	if(lock)
		locker.unlock();
}


void process_manager::full_clear(bool lock){
	if(lock)
		locker.lock();
	mon_thread.process_forse_stop(true);
	mon_thread.terminate_thread();
	mon_thread.ProcStart = move(function<void()>());
	mon_thread.ProcCrash = move(function<void()>());
	mon_thread.ProcEnded = move(function<void()>());
	mon_thread.ProcManuallyStopped = move(function<void()>());
	mon_thread.last_exit_code = 0;
	mon_thread.path.clear();
	mon_thread.arguments.clear();
	if (lock)
		locker.unlock();
}

bool process_manager::load_on_proc_start_function(function<void()>& func, bool wait){
	if (wait)
		mon_thread.func_locker.lock();
	else if (!mon_thread.func_locker.try_lock())
		return false;
	mon_thread.ProcStart = func;
	mon_thread.func_locker.unlock();
	return true;
}
bool process_manager::load_on_proc_crash_function(function<void()>& func, bool wait){
	if (wait)
		mon_thread.func_locker.lock();
	else if (!mon_thread.func_locker.try_lock())
		return false;
	mon_thread.ProcCrash = func;
	mon_thread.func_locker.unlock();
	return true;
}
bool process_manager::load_on_proc_end_function(function<void()>& func, bool wait){
	if (wait)
		mon_thread.func_locker.lock();
	else if (!mon_thread.func_locker.try_lock())
		return false;
	mon_thread.ProcEnded = func;
	mon_thread.func_locker.unlock();
	return true;
}
bool process_manager::load_on_proc_manual_stop_function(function<void()>& func, bool wait){
	if (wait)
		mon_thread.func_locker.lock();
	else if (!mon_thread.func_locker.try_lock())
		return false;
	mon_thread.ProcManuallyStopped = func;
	mon_thread.func_locker.unlock();
	return true;
}

bool process_manager::reset_on_proc_start_function(bool wait) {
	if (wait)
		mon_thread.func_locker.lock();
	else if (!mon_thread.func_locker.try_lock())
		return false;
	mon_thread.ProcStart = move(function<void()>());
	mon_thread.func_locker.unlock();
	return true;
}

bool process_manager::reset_on_proc_crash_function(bool wait) {
	if (wait)
		mon_thread.func_locker.lock();
	else if (!mon_thread.func_locker.try_lock())
		return false;
	mon_thread.ProcCrash = move(function<void()>());
	mon_thread.func_locker.unlock();
	return true;
}
bool process_manager::reset_on_proc_end_function(bool wait) {
	if (wait)
		mon_thread.func_locker.lock();
	else if (!mon_thread.func_locker.try_lock())
		return false;
	mon_thread.ProcEnded = move(function<void()>());
	mon_thread.func_locker.unlock();
	return true;
}
bool process_manager::reset_on_proc_manual_stop_function(bool wait) {
	if (wait)
		mon_thread.func_locker.lock();
	else if (!mon_thread.func_locker.try_lock())
		return false;
	mon_thread.ProcManuallyStopped = move(function<void()>());
	mon_thread.func_locker.unlock();
	return true;
}

bool process_manager::open_process(DWORD id){
	locker.lock();
	if (mon_thread.process_up){
		locker.unlock();
		throw(excepts("trying to open process when another process is being monitored"));
	}

	HANDLE process_handle = OpenProcess(SYNCHRONIZE | PROCESS_TERMINATE | PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_CREATE_THREAD, FALSE, id);
	if (process_handle == NULL){
		mon_thread.log.log_current_time_message("OpenProcess failed");
		locker.unlock();
		return false;
	}
	wchar_t str[MAX_PATH];
	DWORD size=MAX_PATH;

	if (QueryFullProcessImageName(process_handle, 0, str, &size) == 0){
		mon_thread.log.log_current_time_message("QueryFullProcessImageName failed");
		CloseHandle(process_handle);
		locker.unlock();
		return false;
	}
	
	wstring tmp;
	try{
		get_cmd_line(id, tmp);
	}
	catch (excepts ex){
		mon_thread.log.log_current_time_message("failed to get cmd arguments");
		clear_process_info(false);
		locker.unlock();
		return false;
	}
	
	ZeroMemory(&mon_thread.proc_info, sizeof(mon_thread.proc_info));
	mon_thread.proc_info.dwProcessId = id;
	mon_thread.proc_info.hProcess = process_handle;
	load_path_args(str, tmp, false);
	mon_thread.process_up = true;
	start(false);

	locker.unlock();
	return true;
}