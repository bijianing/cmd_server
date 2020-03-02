#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <signal.h>
#include <libconfig.h>
#include <cmd_server.h>

#define __DEBUGLOG_ERROR				1
#define __DEBUGLOG_INFO					1
#define __DEBUGLOG_DEBUG				1
#define __DEBUGLOG_CONF					1
#define __DEBUGLOG_FUNC					1
#define __DEBUGLOG_POS					0

#define MOD_NAME						"CmdSrv"
#if __DEBUGLOG_ERROR
#define	ErrPrint(f, x...)				printf("%-8s: ERR: %4d: %-20s: " f, MOD_NAME, __LINE__, __func__, ##x)
#else
#define	ErrPrint(f, x...)
#endif

#if __DEBUGLOG_INFO
#define	InfPrint(f, x...)				printf("%-8s: INF: %4d: %-20s: " f, MOD_NAME, __LINE__, __func__, ##x)
#else
#define	InfPrint(f, x...)
#endif

#if __DEBUGLOG_DEBUG
#define	DbgPrint(f, x...)				printf("%-8s: DBG: %4d: %-20s: " f, MOD_NAME, __LINE__, __func__, ##x)
#else
#define	DbgPrint(f, x...)
#endif

#if __DEBUGLOG_CONF
#define	ConfPrint(f, x...)				printf("%-8s: CONF: %4d: %-20s: " f, MOD_NAME, __LINE__, __func__, ##x)
#else
#define	ConfPrint(f, x...)
#endif

#if __DEBUGLOG_FUNC
#define	FunPrint(f, x...)				printf("%-8s: FUN: %4d: %s: " f, MOD_NAME, __LINE__, __func__, ##x)
#else
#define	FunPrint(f, x...)
#endif

#if __DEBUGLOG_POS
#define	PosPrint						printf("%-8s: POS: %4d: %-20s()\n", MOD_NAME, __LINE__, __func__);
#else
#define	PosPrint
#endif




/* ==================================================================== */
/* DEFINE																*/
/* ==================================================================== */
#define ARYSIZ(ary)					(sizeof(ary) / sizeof(ary[0]))
#define CMD_START					'.'
#define CMD_END						'.'
#define CMD_MAX_LEN					16
#define PATH_MAX_LEN				512
#define PARAM_MAX_SZ				8
#define CONN_TIMEOUT				(60 * 10)
#define WAIT_AFTER_READ				(500 * 1000)
#define CMD_RET_NOT_FOUND			-9999
#define CMD_RET_HTTP_OK				10
#define CMD_RET_STR_OK				"Execute succeeded!\n"
#define CMD_RET_STR_NOT_FOUND		"Not found!\n"
#define CMD_RET_STR_UNKNOW			"Unknow!\n"
#define CMD_END_STR					"End\n"

#define BUFSZ						4096

#define IMG_FILE_PATH			"screenshot.png"
#define IMG_FILE_WAIT_TIME		5000

/* ==================================================================== */
/* ENUMS																*/
/* ==================================================================== */
typedef enum {
	eCMD_RES_OK,
	eCMD_RES_HTTP_OK,
	eCMD_RES_NOT_FOUND,
} CmdRet_t;

/* ==================================================================== */
/* VARIABLES															*/
/* ==================================================================== */
static int g_http_mode = 1;
static int sock_data_sz = 0;

/* ==================================================================== */
/* STRUCTS															  */
/* ==================================================================== */

typedef struct cmd_param
{
	const char *params[PARAM_MAX_SZ];
	int cnt;
} CmdParam_t;

typedef struct cmd_info
{
	int (*fun)(struct cmd_info *info, int sock, CmdParam_t *param);
	const char *cmd;
} CmdInfo_t;

/* ==================================================================== */
/* FUNCTIONS															*/
/* ==================================================================== */
void sig_handler(int sig, siginfo_t *info, void *ctx)
{

	ErrPrint("got signal SIGPIPE\n");

}

struct conf_cmd
{
	char cmd[PATH_MAX_LEN];
	char name[CMD_MAX_LEN];
};
struct conf
{
	unsigned port;
	struct conf_cmd *cmds;
	int cmd_cnt;
};

struct conf gconf;

int parse_conf(void)
{
	int port;
	config_t cfg;
	config_setting_t *setting;

	config_init(&cfg);
	if(!config_read_file(&cfg, "server.conf"))
	{
		ErrPrint("%s:%d - %s\n", config_error_file(&cfg),
				config_error_line(&cfg), config_error_text(&cfg));
		config_destroy(&cfg);
		return -1;
	}

	/* Get the store name. */
	if(!config_lookup_int(&cfg, "port", &port)) {
		ErrPrint("No 'name' setting in configuration file.\n");
		config_destroy(&cfg);
		return -2;
	}
	ConfPrint("Port:%d\n", port);
	gconf.port = (unsigned short)port;

	setting = config_lookup(&cfg, "cmds");
	if(setting == NULL) {
		ErrPrint("No cmds in conf\n");
		config_destroy(&cfg);
		return -3;
	} else {
		int count = config_setting_length(setting);
		int i;

		ConfPrint("%-20s%-10s\n", "command", "name");
		for(i = 0; i < count; ++i)
		{
			config_setting_t *e = config_setting_get_elem(setting, i);

			/* Only output the record if all of the expected fields are present. */
			const char *command, *name, *html;

			/* command */
			if (config_setting_is_group(e)) {
				if(!(config_setting_lookup_string(e, "command", &command)
						&& config_setting_lookup_string(e, "name", &name)))
					continue;
				ConfPrint("%-20s%-10s\n", command, name);

			/* html */
			} else {
				if(!(html = config_setting_get_string(e)))
					continue;
				ConfPrint("html:%s\n", html);
			}
		}
	}
}

int main(int argc, const char* argv[])
{
	unsigned short port = 12345;

	if (argc > 2) {
		printf("Usage: %s [port]\n", argv[0]);
		exit(1);
	}

	if (argc == 2) {
		port = atoi(argv[1]);
		if (port <= 0 || port > 99999) {
			ErrPrint("port range: 1 - 9999\n");
			exit(2);
		}
	}

	struct sigaction sa_sigpipe;
	memset(&sa_sigpipe, 0, sizeof(sa_sigpipe));
	sa_sigpipe.sa_sigaction = sig_handler;
	sa_sigpipe.sa_flags = SA_SIGINFO;

	if ( sigaction(SIGPIPE, &sa_sigpipe, NULL) < 0 ) {
		ErrPrint("signal SIGPIPE handler register failed\n");
		exit(3);
	}

	parse_conf();

	return 0;
}
