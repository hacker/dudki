#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <pwd.h>
#include <grp.h>
#include <dirent.h>
#include <sys/wait.h>
#include <syslog.h>
#include <errno.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace std;
#include "process.h"
#include "configuration.h"

static multimap<string,pid_t> procpids;

void process::check() const {
    if(!pidfile.empty()) {
	signal(0);
    }else if(!process_name.empty()) {
	if(procpids.empty())
	    gather_proc_info();
	if(procpids.find(process_name)==procpids.end())
	    throw runtime_error("no such process");
    } // XXX: or else?
}
void process::check(const string& id,configuration& config) {
    try {
	check();
	patience = 0;
    }catch(exception& e) {
	if(patience>60) { // TODO: configurable
	    patience = 0;
	}else{
	    if(patience<10) { // TODO: configurable
		syslog(LOG_NOTICE,"The process '%s' is down, trying to launch.",id.c_str());
		do_notify(id,"Starting up",
			"The named process seems to be down. Dudki will try\n"
			"to revive it by running the specified command.\n",
			config);
		try {
		    launch(id,config);
		}catch(exception& e) {
		    syslog(LOG_ERR,"Error trying to launch process '%s': %s",id.c_str(),e.what());
		}
	    }else if(patience==10){ // TODO: configurable like the above
		syslog(LOG_NOTICE,"Giving up on process '%s' for a while",id.c_str());
		do_notify(id,"Giving up",
			"After a number of attempts to relaunch the named process\n"
			"It still seems to be down. Dudki is giving up attempts\n"
			"to revive the process for a while.\n",
			config);
	    }
	    patience++;
	}
    }
}

void process::launch(const string& id,configuration& config) {
    uid_t uid = (uid_t)-1;
    gid_t gid = (gid_t)-1;
    if(!user.empty()) {
	struct passwd *ptmp = getpwnam(user.c_str());
	if(ptmp) {
	    uid = ptmp->pw_uid;
	    gid = ptmp->pw_gid;
	}else{
	    errno=0;
	    uid = strtol(user.c_str(),NULL,0);
	    if(errno)
		throw runtime_error("Failed to resolve User value to uid");
	}
    }
    if(!group.empty()) {
	struct group *gtmp = getgrnam(group.c_str());
	if(gtmp) {
	    gid = gtmp->gr_gid;
	}else{
	    errno = 0;
	    gid = strtol(group.c_str(),NULL,0);
	    if(errno)
		throw runtime_error("Failed to reslove Group value to gid");
	}
    }
    pid_t p = fork();
    if(p<0)
	throw runtime_error(string(__PRETTY_FUNCTION__)+": failed to fork()");
    if(!p) {
	// child
	try {
	    setsid();
	    if(user.empty()) {
		if((getgid()!=gid) && setgid(gid))
		    throw runtime_error(string(__PRETTY_FUNCTION__)+": failed to setgid()");
	    }else{
		if(initgroups(user.c_str(),gid))
		    throw runtime_error(string(__PRETTY_FUNCTION__)+": failed to initgroups()");
	    }
	    if(!chroot.empty()) {
		if(::chroot(chroot.c_str()))
		    throw runtime_error(string(__PRETTY_FUNCTION__)+": failed to chroot()");
	    }
	    if(!user.empty()) {
		if((getuid()!=uid) && setuid(uid))
		    throw runtime_error(string(__PRETTY_FUNCTION__)+": failed to setuid()");
	    }
	    char *argv[] = { "/bin/sh", "-c", (char*)restart_cmd.c_str(), NULL };
	    close(0); close(1); close(2);
	    execv("/bin/sh",argv);
	}catch(exception& e) {
	    syslog(LOG_ERR,"Error trying to launch process '%s': %s",id.c_str(),e.what());
	}
	_exit(-1);
    }
    // parent
    int rv;
    if(waitpid(p,&rv,0)<0)
	throw runtime_error(string(__PRETTY_FUNCTION__)+": failed to waitpid()");
}

void process::do_notify(const string& id,const string& event,const string& description,configuration& config) {
    string the_notify;
    if(!notify.empty())
	the_notify=notify;
    else if(!config.notify.empty())
	the_notify=config.notify;
    else
	return;
    try {
	string::size_type colon = the_notify.find(':');
	if(colon==string::npos)
	    throw runtime_error("invalid notify action specification");
	string nschema = the_notify.substr(0,colon);
	string ntarget = the_notify.substr(colon+1);
	if(nschema=="mailto") {
	    notify_mailto(ntarget,id,event,description,config);
	}else
	    throw runtime_error("unrecognized notification schema");
    }catch(exception& e) {
	syslog(LOG_ERR,"Notification error: %s",e.what());
    }
}

void process::notify_mailto(const string& email,const string& id,const string& event,const string& description,configuration& config) {
    int files[2];
    if(pipe(files))
	throw runtime_error("Failed to pipe()");
    pid_t pid = vfork();
    if(pid==-1) {
	close(files[0]);
	close(files[1]);
	throw runtime_error("Failed to vfork()");
    }
    if(!pid) {
	// child
	if(dup2(files[0],0)!=0)
	    _exit(-1);
	close(1);
	close(files[0]);
	close(files[1]);
	execl("/usr/sbin/sendmail","usr/sbin/sendmail","-i",email.c_str(),NULL);
	_exit(-1);
    }
    // parent
    close(files[0]);
    FILE *mta = fdopen(files[1],"w");
    for(headers_t::const_iterator i=mailto_headers.begin();i!=mailto_headers.end();++i) {
	fprintf(mta,"%s: %s\n",i->first.c_str(),i->second.c_str());
    }
    for(headers_t::const_iterator i=config.mailto_headers.begin();i!=config.mailto_headers.end();++i) {
	if(mailto_headers.find(i->first)!=mailto_headers.end())
	    continue;
	fprintf(mta,"%s: %s\n",i->first.c_str(),i->second.c_str());
    }
    fprintf(mta,
	    "Subject: [%s] %s\n\n"
	    "%s\n"
	    "---\n"
	    "This message was sent automatically by the 'dudki' daemon\n",
	    id.c_str(), event.c_str(),
	    description.c_str() );
    fclose(mta);
    int status;
    waitpid(pid,&status,0);
    // TODO: check the return code
}

void process::signal(int signum) const {
    if(!pidfile.empty()) {
	ifstream pids(pidfile.c_str(),ios::in);
	if(!pids)
	    throw runtime_error("no pidfile found");
	pid_t pid = 0;
	pids >> pid;
	pids.close();
	if(!pid)
	    throw runtime_error("no pid in pidfile");
	if(kill(pid,signum))
	    throw runtime_error("failed to signal process");
    }else if(!process_name.empty()) {
	if(procpids.empty())
	    gather_proc_info();
	pair<multimap<string,pid_t>::const_iterator,multimap<string,pid_t>::const_iterator> range = procpids.equal_range(process_name);
	int count = 0;
	for(multimap<string,pid_t>::const_iterator i=range.first;i!=range.second;++i) {
	    pid_t pid = i->second;
	    if(kill(i->second,signum))
		throw runtime_error("failed to signal process");
	    count++;
	}
	if(!count)
	    throw runtime_error("no running instance detected");
    }else
	throw runtime_error("nothing is known about the process");
}

void process::prepare_herd() {
    procpids.clear();
}
void process::unprepare_herd() {
    procpids.clear();
}
void process::gather_proc_info() {
    vector<pid_t> allpids;
    DIR *pd = opendir("/proc");
    if(!pd)
	throw runtime_error("failed to open /proc");
    struct dirent *pde;
    pid_t selfpid = getpid();
    while(pde=readdir(pd)) {
	errno=0;
	pid_t pid = atoi(pde->d_name);
	if((!pid) || pid==selfpid)
	    continue;
	allpids.push_back(pid);
    }
    closedir(pd);
    char s[256];
    procpids.clear();
    for(vector<pid_t>::const_iterator i=allpids.begin();i!=allpids.end();++i) {
	int r = snprintf(s,sizeof(s),"/proc/%d/stat",*i);
	if(r>=sizeof(s) || r<1)
	    continue;
	string cmd;
	ifstream ss(s,ios::in);
	if(ss) {
	    getline(ss,cmd);
	    string::size_type op = cmd.find('(');
	    if(op==string::npos)
		continue;
	    cmd.erase(0,op+1);
	    string::size_type cp = cmd.find(')');
	    if(cp==string::npos)
		continue;
	    cmd.erase(cp);
	}else{
	    r = snprintf(s,sizeof(s),"/proc/%d/status",*i);
	    if(r>=sizeof(s) || r<1)
		continue;
	    ifstream ss(s,ios::in);
	    if(!ss)
		continue;
	    ss >> cmd;
	    if(cmd.empty())
		continue;
	}
	r = snprintf(s,sizeof(s),"/proc/%d/cmdline",*i);
	if(r>=sizeof(s) || r<1)
	    continue;
	ifstream cs(s,ios::binary);
	if(!cs)
	    continue;
	string command;
	while(cs) {
	    string cl;
	    getline(cs,cl,(char)0);
	    string::size_type lsl = cl.rfind('/');
	    if(lsl!=string::npos)
		cl.erase(0,lsl+1);
	    if(cl.substr(0,cmd.length())==cmd) {
		command = cl;
		break;
	    }
	}
	procpids.insert(pair<string,pid_t>(cmd,*i));
	if((!command.empty()) && cmd!=command)
	    procpids.insert(pair<string,pid_t>(command,*i));
    }
}
