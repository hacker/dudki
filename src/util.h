#ifndef __UTIL_H
#define __UTIL_H

#include <string>
using namespace std;

class pid_file {
    public:
	string file_name;
	bool unlink_pid;

	pid_file()
	    : unlink_pid(false) { }
	~pid_file() { unlink(); }

	void set(const string& f,bool u=true);
	void unlink();
};

#endif /* __UTIL_H */
