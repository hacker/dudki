#ifndef __CONFIGURATION_H
#define __CONFIGURATION_H

#include <string>
using namespace std;
#include "process.h"

class configuration {
    public:
	processes_t processes;

	int check_interval;
	string pidfile;
	bool daemonize;
	headers_t mailto_headers;
	string notify;

	configuration();

	void parse(const string& cfile);
};

#endif /* __CONFIGURATION_H */
