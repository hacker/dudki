#include <unistd.h>
#include <fstream>
#include <stdexcept>
using namespace std;
#include "util.h"

void pid_file::set(const string& f,bool u) {
    ofstream of(f.c_str(),ios::trunc);
    if(!of)
	throw runtime_error(string(__PRETTY_FUNCTION__)+": failed to open file for writing pid");
    of << getpid() << endl;
    of.close();
    file_name = f;
    unlink_pid = u;
}
void pid_file::unlink() {
    if(!unlink_pid)
	return;
    ::unlink(file_name.c_str());
}

