#pragma once

#include <string>
#include <ctime>
#include <mutex>
#include <fstream>

using namespace std;

class logger{
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

	bool		log_current_time(bool lock = true, bool owned = true);
	bool		log_message(const string& message, bool lock = true);
	bool		log_current_time_message(const string& message, bool lock = true);

	//unstable
	void		lock_for_continious_writing(){ locker.lock(); };
	//unstable
	bool		try_lock_for_continious_writing(){ return locker.try_lock(); };
	//unstable
	void		unlock(){ locker.unlock(); };
	ofstream& get_log(){ return log; };

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
	logger(const logger& l);
	ofstream	log;

	mutex		locker;
	mutex		swap_locker;
};