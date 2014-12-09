#pragma once
#include <string>

using namespace std;

class excepts
{
public:
	excepts();
	excepts(const string& str);
	excepts(const excepts& exc);
	~excepts();
	string& msg_view();
private:
	string message;
};

