#ifndef __PROCESS_H
#define __PROCESS_H

#include <string>
#include <map>
using namespace std;

class configuration;

typedef map<string,string> headers_t;

class process {
    public:
	string pidfile;
	string restart_cmd;
	string notify;
	string user;
	string group;
	string chroot;
	headers_t mailto_headers;

	int patience;

	process()
	    : patience(0) { }

	void check(const string& id,configuration& config);
	void launch(const string& id,configuration& config);
	void do_notify(const string& id,const string& event,const string& description,configuration& config);
	void notify_mailto(const string& email,const string& id,const string& event,
		const string& description,configuration& config);

	void signal(int signum) const;
};

typedef map<string,process> processes_t;

#endif /* __PROCESS_H */
