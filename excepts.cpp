#include "excepts.h"


excepts::excepts()
{
}

excepts::excepts(const string& str) : message(str)
{
}

excepts::excepts(const excepts& exc){
	message = exc.message;
}

excepts::~excepts()
{
}

string excepts::msg_view(){
	return message;
}