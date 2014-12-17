#include "logger.h"

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