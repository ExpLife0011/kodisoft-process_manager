#ifndef NOMINMAX
#define NOMINMAX
#endif

#define _CRT_SECURE_NO_WARNINGS

#include <thread>
#include <iostream>
#include <limits>
#include <vector>
#include "process_manager.h"

void flush(){
	cin.clear();
	cin.ignore(numeric_limits<streamsize>::max(), '\n');
}

void func(){
	cout << "lalka started" << endl;
}

using namespace std;


int main(){
//	process_manager proc1;
//	proc1.open_process(3936);
	get_cmd_line(3328);
	cin.get();
	return 0;
}

/*
int main(){
	process_manager proc1;
	function<void()> f(func);
	proc1.load_on_proc_start_function(f);
	proc1.load_path_args(TEXT("d:\\test.exe"), TEXT("1 2 3 4 5"));
	proc1.start();
	chrono::seconds t(1);
	this_thread::sleep_for(t);
	logger log("new.txt");
	proc1.change_log(log);
	flush();
	cin.get();
	return 0;
}
*/

/*
int main(){
	vector<thread> thread_pool;
	logger log("testing_logger.txt");
	logger log2("testing_logger2.txt");
	thread_pool.resize(100);
	for (int i = 0; i < 100; i++)
		thread_pool[i] = thread([&log,i](){
		thread::id id = this_thread::get_id();
		for (int j = 0; j < 1000; j ++)
			log.log_message(to_string(i) + string(" - ") + to_string(j));
	});
	for (int i = 0; i < 100; i++)
		thread_pool[i].detach();
	chrono::seconds wait1(1);
	chrono::seconds wait2(10);
	this_thread::sleep_for(wait1);
	log = move(log2);
	this_thread::sleep_for(wait2);
	return 0;
}
*/