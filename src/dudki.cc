#include <unistd.h>
#include <signal.h>
#include <syslog.h>
#include <errno.h>
#include <iostream>
#include <fstream>
#include <stdexcept>
using namespace std;
#include "configuration.h"
#include "util.h"

#include "config.h"
#ifdef HAVE_GETOPT_H
# include <getopt.h>
#endif

#ifndef DEFAULT_CONF_FILE
# define DEFAULT_CONF_FILE "/etc/dudki.conf"
#endif

#define PHEADER PACKAGE " Version " VERSION
#define PCOPY   "Copyright (c) 2004 Klever Group"

bool finishing = false;
bool restarting = false;
static char **_argv = NULL;

static void lethal_signal_handler(int signum) {
    syslog(LOG_NOTICE,"Lethal signal received. Terminating.");
    finishing = true;
}
static void sighup_handler(int signum) {
    syslog(LOG_NOTICE,"SUGHUP received, reloading.");
    restarting = finishing = true;
}

void check_herd(configuration& config) {
    process::prepare_herd();
    for(processes_t::iterator i=config.processes.begin();i!=config.processes.end();++i)
	i->second.check(i->first,config);
    process::unprepare_herd();
}

void signal_self(const configuration& config,int signum) {
    ifstream pids(config.pidfile.c_str(),ios::in);
    if(!pids)
	throw runtime_error("Can't detect running instance");
    pid_t pid = 0;
    pids >> pid;
    if(!pid)
	throw runtime_error("Can't detect running instance");
    if(pid==getpid())
	throw 0;
    if(kill(pid,signum))
	throw runtime_error("Failed to signal running instance");
}

int main(int argc,char **argv) {
    try {
	_argv = new char*[argc+1];
	if(!_argv)
	    throw runtime_error("memory allocation problem at the very start");
	memmove(_argv,argv,sizeof(*_argv)*(argc+1));
	string config_file = DEFAULT_CONF_FILE;
	enum {
	    op_default,
	    op_work,
	    op_signal,
	    op_ensure,
	    op_test
	} op = op_default;
	int op_signum = 0;
	while(true) {
#define	SHORTOPTSTRING "f:hVLrkcets:"
#ifdef HAVE_GETOPT_LONG
	    static struct option opts[] = {
		{ "help", no_argument, 0, 'h' },
		{ "usage", no_argument, 0, 'h' },
		{ "version", no_argument, 0, 'V' },
		{ "license", no_argument, 0, 'L' },
		{ "config", required_argument, 0, 'f' },
		{ "kill", no_argument, 0, 'k' },
		{ "reload", no_argument, 0, 'r' },
		{ "signal", required_argument, 0, 's' },
		{ "check", no_argument, 0, 'c' },
		{ "ensure", no_argument, 0, 'e' },
		{ "test", no_argument, 0, 't' },
		{ NULL, 0, 0, 0 }
	    };
	    int c = getopt_long(argc,argv,SHORTOPTSTRING,opts,NULL);
#else /* !HAVE_GETOPT_LONG */
	    int c = getopt(argc,argv,SHORTOPTSTRING);
#endif /* /HAVE_GETOPT_LONG */
	    if(c==-1)
		break;
	    switch(c) {
		case 'h':
		    cerr << PHEADER << endl
			<< PCOPY << endl << endl
			<< " " << argv[0] << " [options] [processes]" << endl << endl <<
#ifdef HAVE_GETOPT_LONG
			" -h, --help\n"
			" --usage         display this text\n"
			" -V, --version   display version number\n"
			" -L, --license   show license\n"
			" -f filename, --config=filename\n"
			"                 specify the configuration file to use\n"
			"\n"
			" -k, --kill      stop running instance (send SIGTERM)\n"
			" -r, --reload    reload running instance (send SIGHUP)\n"
			" -s signum, --signal=signum\n"
			"                 send the specified signal to the running process\n"
			" -c, --check     check if the process is running\n"
			"     (the above commands operate on dudki itself if no\n"
			"      process name has been specified)\n"
			" -e, --ensure    ensure that dudki is running\n"
			" -t, --test      test configuration file and exit"
#else /* !HAVE_GETOPT_LONG */
			" -h              display this text\n"
			" -V              display version number\n"
			" -L              show license\n"
			" -f filename     specify the configuration file to use\n"
			"\n"
			" -k              stop running instance (send SIGTERM)\n"
			" -r              reload running instance (send SIGHUP)\n"
			" -s signum       send the specified signal to the running process\n"
			" -c              check if the process is running\n"
			"     (the above commands operate on dudki itself if no\n"
			"      process name has been specified)\n"
			" -e              ensure that dudki is running\n"
			" -t              test configuration file and exit"
#endif /* /HAVE_GETOPT_LONG */
			<< endl;
		    exit(0);
		    break;
		case 'V':
		    cerr << VERSION << endl;
		    exit(0);
		    break;
		case 'L':
		    extern const char *COPYING;
		    cerr << COPYING << endl;
		    exit(0);
		    break;
		case 'f':
		    config_file = optarg;
		    break;
		case 'k':
		    if(op!=op_default) {
			cerr << "Can't obey two or more orders at once" << endl;
			exit(1);
		    }
		    op = op_signal; op_signum = SIGTERM;
		    break;
		case 'r':
		    if(op!=op_default) {
			cerr << "Can't obey two or more orders at once" << endl;
			exit(1);
		    }
		    op = op_signal; op_signum = SIGHUP;
		    break;
		case 'c':
		    if(op!=op_default) {
			cerr << "Can't obey two or more orders at once" << endl;
			exit(1);
		    }
		    op = op_signal; op_signum = 0;
		    break;
		case 'e':
		    if(op!=op_default) {
			cerr << "Can't obey two or more orders at once" << endl;
			exit(1);
		    }
		    op = op_ensure;
		    break;
		case 't':
		    if(op!=op_default) {
			cerr << "Can't obey two or more orders at once" << endl;
			exit(1);
		    }
		    op = op_test;
		    break;
		case 's':
		    if(op!=op_default) {
			cerr << "Can't obey two or more orders at once" << endl;
			exit(1);
		    }
		    op = op_signal;
		    errno = 0;
		    op_signum = strtol(optarg,NULL,0);
		    if(errno) {
			cerr << "Can't obtain the signal value" << endl;
			exit(1);
		    }
		    break;
		default:
		    cerr << "Huh??" << endl;
		    exit(1);
		    break;
	    }
	}
	const char *sid = *argv;
	const char *t;
	while(t = index(sid,'/')) {
	    sid = t; sid++;
	}
	openlog(sid,LOG_CONS|LOG_PERROR|LOG_PID,LOG_DAEMON);
	configuration config;
	config.parse(config_file);
	switch(op) {
	    case op_test:
		cerr << "Configuration OK" << endl;
		break;
	    case op_signal:
		try {
		    if(optind>=argc) {
			signal_self(config,op_signum);
		    }else{
			int failures = 0;
			for(int narg=optind;narg<argc;narg++) {
			    try {
				processes_t::const_iterator i = config.processes.find(argv[narg]);
				if(i==config.processes.end())
				    throw runtime_error("no such process configured");
				if(op_signum)
				    i->second.signal(op_signum);
				else
				    i->second.check();
			    }catch(exception& e) {
				cerr << "dudki(" << argv[narg] << "): " << e.what() << endl;
				failures++;
			    }
			}
			if(failures)
			    throw runtime_error("not all processes have been successfully signaled");
		    }
		    exit(0);
		}catch(exception& e) {
		    exit(1);
		}
		break;
	    case op_ensure:
		try {
		    signal_self(config,0);
		    break;
		}catch(exception& e) {
		    syslog(LOG_NOTICE,"The dudki process is down, taking its place");
		    config.daemonize = true;
		}catch(int zero) {
		    // we throw zero in case we're ensuring that this very process is running.
		    // we don't have to daemonize if we're daemonic.
		    config.daemonize = false;
		}
	    case op_default:
	    case op_work:
		{
		    if(config.daemonize) {
			pid_t pf = fork();
			if(pf<0)
			    throw runtime_error(string(__PRETTY_FUNCTION__)+": failed to fork()");
			if(pf) {
			    _exit(0);
			}
		    }
		    pid_file pidfile;
		    pidfile.set(config.pidfile);
		    signal(SIGINT,lethal_signal_handler);
		    signal(SIGABRT,lethal_signal_handler);
		    signal(SIGTERM,lethal_signal_handler);
		    signal(SIGHUP,sighup_handler);
		    sigset_t sset;
		    sigemptyset(&sset);
		    sigaddset(&sset,SIGINT); sigaddset(&sset,SIGABRT);
		    sigaddset(&sset,SIGTERM); sigaddset(&sset,SIGHUP);
		    sigprocmask(SIG_UNBLOCK,&sset,NULL);
		    while(!finishing) {
			check_herd(config);
			sleep(config.check_interval);
		    }
		    if(restarting)
			execvp(_argv[0],_argv);
		}
		break;
	    default:
		throw runtime_error(string(__PRETTY_FUNCTION__)+": internal error");
	}
    }catch(exception& e) {
	cerr << "Oops: " << e.what() << endl;
	return 1;
    }
    exit(0);
}
