#include <stdexcept>
using namespace std;
#include <dotconf.h>
#include "configuration.h"

#ifndef DEFAULT_PID_FILE
# define DEFAULT_PID_FILE "/var/run/dudki.pid"
#endif

configuration::configuration()
    : check_interval(60), pidfile(DEFAULT_PID_FILE),
    daemonize(true) {
    }

enum dc_ctx {
    DCC_ROOT = 1,
    DCC_PROCESS = 2
};
struct dc_context {
    dc_ctx ctx;
    configuration* cf;
    process* ps;

    dc_context()
	: ctx(DCC_ROOT), cf(NULL), ps(NULL) { }
};

static DOTCONF_CB(dco_check_interval) { dc_context *dcc = (dc_context*)ctx;
    dcc->cf->check_interval = cmd->data.value;
    return NULL;
}
static DOTCONF_CB(dco_daemonize) { dc_context *dcc = (dc_context*)ctx;
    dcc->cf->daemonize = cmd->data.value;
    return NULL;
}

static DOTCONF_CB(dco_pid_file) { dc_context *dcc = (dc_context*)ctx;
    switch(dcc->ctx) {
	case DCC_ROOT:
	    dcc->cf->pidfile = cmd->data.str;
	    break;
	case DCC_PROCESS:
	    dcc->ps->pidfile = cmd->data.str;
	    break;
	default:
	    return "Unexpected PidFile";
    }
    return NULL;
}
static DOTCONF_CB(dco_mailto_header) { dc_context *dcc = (dc_context*)ctx;
    if(cmd->arg_count!=2)
	return "Invalid number of arguments";
    string h = cmd->data.list[0];
    string v = cmd->data.list[1];
    switch(dcc->ctx) {
	case DCC_ROOT:
	    dcc->cf->mailto_headers[h] = v;
	    break;
	case DCC_PROCESS:
	    dcc->ps->mailto_headers[h] = v;
	    break;
	default:
	    return "Unexpected MailtoHeader";
    }
    return NULL;
}
static DOTCONF_CB(dco_notify) { dc_context *dcc = (dc_context*)ctx;
    switch(dcc->ctx) {
	case DCC_ROOT:
	    dcc->cf->notify = cmd->data.str;
	    break;
	case DCC_PROCESS:
	    dcc->ps->notify = cmd->data.str;
	    break;
	default:
	    return "Unexpected Notify";
    }
    return NULL;
}

static DOTCONF_CB(dco_process) { dc_context *dcc = (dc_context*)ctx;
    string id = cmd->data.str;
    if(id[id.length()-1]=='>')
	id.erase(id.length()-1);
    dcc->ps = &(dcc->cf->processes[id]);
    dcc->ctx = DCC_PROCESS;
    return NULL;
}
static DOTCONF_CB(dco__process) { dc_context *dcc = (dc_context*)ctx;
    dcc->ps = NULL;
    dcc->ctx = DCC_ROOT;
    return NULL;
}

static DOTCONF_CB(dco_restart_command) { dc_context *dcc = (dc_context*)ctx;
    dcc->ps->restart_cmd = cmd->data.str;
    return NULL;
}
static DOTCONF_CB(dco_user) { dc_context *dcc = (dc_context*)ctx;
    dcc->ps->user = cmd->data.str;
    return NULL;
}
static DOTCONF_CB(dco_group) { dc_context *dcc = (dc_context*)ctx;
    dcc->ps->group = cmd->data.str;
    return NULL;
}
static DOTCONF_CB(dco_chroot) { dc_context *dcc = (dc_context*)ctx;
    dcc->ps->chroot = cmd->data.str;
    return NULL;
}

static const configoption_t dc_options[] = {
    { "CheckInterval", ARG_INT, dco_check_interval, NULL, DCC_ROOT },
    { "Daemonize", ARG_TOGGLE, dco_daemonize, NULL, DCC_ROOT },
    { "PidFile", ARG_STR, dco_pid_file, NULL, DCC_ROOT|DCC_PROCESS },
    { "MailtoHeader", ARG_STR, dco_mailto_header, NULL, DCC_ROOT|DCC_PROCESS },
    { "Notify", ARG_STR, dco_notify, NULL, DCC_ROOT|DCC_PROCESS },
    { "<Process", ARG_STR, dco_process, NULL, DCC_ROOT },
    {  "RestartCommand", ARG_STR, dco_restart_command, NULL, DCC_PROCESS },
    {  "User", ARG_STR, dco_user, NULL, DCC_PROCESS },
    {  "Group", ARG_STR, dco_group, NULL, DCC_PROCESS },
    {  "Chroot", ARG_STR, dco_chroot, NULL, DCC_PROCESS },
    { "</Process>", ARG_NONE, dco__process, NULL, DCC_PROCESS },
    LAST_OPTION
};

static const char *dc_context_checker(command_t *cmd,unsigned long mask) {
    dc_context *dcc = (dc_context*)cmd->context;
    if( (mask==CTX_ALL) || ((mask&dcc->ctx)==dcc->ctx) )
	return NULL;
    return "misplaced option";
}
static FUNC_ERRORHANDLER(dc_error_handler) {
    throw runtime_error(string("error parsing config file: ")+msg);
}
    
void configuration::parse(const string& cfile) {
    struct dc_context dcc;
    dcc.cf = this;
    dcc.ctx = DCC_ROOT;
    configfile_t *cf = dotconf_create((char*)cfile.c_str(),dc_options,(context_t*)&dcc,CASE_INSENSITIVE);
    if(!cf)
	throw runtime_error(string(__PRETTY_FUNCTION__)+": failed to dotconf_create()");
    cf->errorhandler = (dotconf_errorhandler_t) dc_error_handler;
    cf->contextchecker = (dotconf_contextchecker_t) dc_context_checker;
    if(!dotconf_command_loop(cf))
	throw runtime_error(string(__PRETTY_FUNCTION__)+": failed to dotconf_command_loop()");
    dotconf_cleanup(cf);
}
