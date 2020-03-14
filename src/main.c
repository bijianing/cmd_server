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
#define __DEBUGLOG_DEBUG				0
#define __DEBUGLOG_CONF					0
#define __DEBUGLOG_FUNC					0
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

/* ==================================================================== */
/* ENUMS																*/
/* ==================================================================== */

/* ==================================================================== */
/* VARIABLES															*/
/* ==================================================================== */

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

struct conf_path
{
	char path[PATH_MAX_LEN];
	char name[CMD_MAX_LEN];
};

struct conf
{
	unsigned port;
	struct conf_cmd *cmds;
	int cmd_cnt;
	struct conf_path *paths;
	int path_cnt;
};

struct conf gconf;

int parse_conf(void)
{
	int ret = -1;
	int port;
	config_t cfg;
	config_setting_t *setting;

	memset(&gconf, 0, sizeof(gconf));
	config_init(&cfg);
	if(!config_read_file(&cfg, CONF_FILE_PATH))
	{
		ErrPrint("%s:%d - %s\n", config_error_file(&cfg),
				config_error_line(&cfg), config_error_text(&cfg));
		goto err;
	}

	/* Get port */
	if(!config_lookup_int(&cfg, "port", &port)) {
		ErrPrint("No 'name' setting in configuration file.\n");
		goto err;
	}
	ConfPrint("Port:%d\n", port);
	gconf.port = (unsigned short)port;
#if 1
	/* Get paths */
	setting = config_lookup(&cfg, "path");
	if(setting == NULL) {
		ErrPrint("No cmds in conf\n");
	} else {
		int count = config_setting_length(setting);
		int i;

		if ((gconf.paths = malloc(count * sizeof(struct conf_path))) == NULL) {
			ErrPrint("path alloc failed\n");
			goto err;
		}
		memset(gconf.paths, 0, count * sizeof(struct conf_path));
		ConfPrint("\n");
		ConfPrint("%-40s%-10s\n", "path", "name");
		ConfPrint("path count:%d--------------------------------\n", count);
		for(i = 0; i < count; ++i)
		{
			config_setting_t *e = config_setting_get_elem(setting, i);

			const char *path, *name;
			if (!e) {
				ErrPrint("get element failed, name:%s, i:%d, count:%d\n",
						"path", i, count);
			}
			if((path = config_setting_get_string(e)) == NULL) {
				ErrPrint("get path in index:%d failed\n", i);
				continue;
			}

			if((name = config_setting_name(e)) == NULL) {
				ErrPrint("get path name in index:%d failed\n", i);
				continue;
			}
			strncpy(gconf.paths[gconf.path_cnt].path, path, PATH_MAX_LEN);
			strncpy(gconf.paths[gconf.path_cnt].name, name, CMD_MAX_LEN);
			ConfPrint("%-40s%-10s\n", gconf.paths[gconf.path_cnt].path,
					gconf.paths[gconf.path_cnt].name);
			gconf.path_cnt++;
		}
	}
#endif

	/* Get commands */
	setting = config_lookup(&cfg, "cmd");
	if(setting == NULL) {
		ErrPrint("No cmds in conf\n");
	} else {
		int count = config_setting_length(setting);
		int i;

		if ((gconf.cmds = malloc(count * sizeof(struct conf_cmd))) == NULL) {
			ErrPrint("cmds alloc failed\n");
			goto err;
		}
		memset(gconf.cmds, 0, count * sizeof(struct conf_cmd));
		ConfPrint("%-40s%-10s\n", "command", "name");
		ConfPrint("--------------------------------\n");
		for(i = 0; i < count; ++i)
		{
			config_setting_t *e = config_setting_get_elem(setting, i);

			/* Only output the record if all of the expected fields are present. */
			const char *command, *name, *html;

			if(!(config_setting_lookup_string(e, "command", &command)
					&& config_setting_lookup_string(e, "name", &name))) {
				ErrPrint("get cmd in index:%d failed\n", i);
				continue;
			}
			ConfPrint("%-40s%-10s\n", command, name);
			strncpy(gconf.cmds[gconf.cmd_cnt].cmd, command, PATH_MAX_LEN);
			strncpy(gconf.cmds[gconf.cmd_cnt].name, name, CMD_MAX_LEN);
			gconf.cmd_cnt++;
		}
	}
	ConfPrint("parse done\n");

	ret = 0;

err:
	PosPrint
	config_destroy(&cfg);
	PosPrint
	return ret;
}

static char *find_path(const char *name)
{
	int i;

	for (i = 0; i < gconf.path_cnt; i++) {
		DbgPrint("name:%s, gconf.paths[%d].name:%s\n", name, i, gconf.paths[i].name);
		if (strcmp(name, gconf.paths[i].name) == 0) {
			return gconf.paths[i].path;
		}
	}

	return NULL;
}

static char *find_cmd(const char *name)
{
	int i;

	for (i = 0; i < gconf.cmd_cnt; i++) {
		if (strcmp(name, gconf.cmds[i].name) == 0) {
			return gconf.cmds[i].cmd;
		}
	}

	return NULL;
}

/*
 * same as strchr but return index */
static int istrchr(const char *str, char c)
{
	int i;

	for (i = 0; str[i] != '\0'; i++) {
		DbgPrint("str[%d]:%c\n", i, str[i]);
		if (str[i] == c)
			return i;
	}

	return -1;
}

static void replace_named_path(const char *cmd, char *out_path)
{
	int i, start, end;
	char path_name[CMD_MAX_LEN];
	char *path, *out = out_path;
	const char *p = cmd;

	DbgPrint("cmd:%s\n", cmd);
	while ((start = istrchr(p, '[')) >= 0) {
		end = istrchr(p + start, ']');
		if (end) {
			memcpy(out, p, start);
			out += start;
			memset(path_name, 0, CMD_MAX_LEN);
			memcpy(path_name, p + start + 1, end - start - 1);
			if ((path = find_path(path_name)) == NULL) {
				DbgPrint("not found path:%s\n", path_name);
				out += sprintf(out, "[%s]", path_name);
			} else {
				DbgPrint("found path name:%s, path:%s \n", path_name, path);
				out += sprintf(out, "%s", path);
			}
			p += (end - start + 1);
		} else {
			break;
		}
	}
	strcpy(out, p);
	DbgPrint("end, out:%s\n", out);
}

static int run_cmd(const char *cmd)
{
	int status;

	status = system(cmd);
	if (WIFEXITED(status)) {
		return WEXITSTATUS(status);
	}

	return -1;
}
#if 0
static void write_str_with_buffer(int sock, char *buf, int *pos, const char *str)
{
	int i;

	for (i = 0; str[i] != '\0'; i++) {
		if (*pos + i >= BUFSZ) {
			write(sock, buf, BUFSZ);
			*pos = 0;
		}
		buf[*pos + i] = str[i];
	}
}
#endif

static int write_str_to_buf(char *buf, const char *str)
{
	int i;

	for (i = 0; str[i] != '\0'; i++) {
		buf[i] = str[i];
	}

	return i;
}

#define HTTP_BUFSZ			(1024 * 1024)
int response_index(int sock)
{
	char buf[HTTP_BUFSZ];
	int pos = 0;
	char line[BUFSZ];
	char conv_cmd[BUFSZ];
	char cmd_name[CMD_MAX_LEN];
	const char *cmd;
	char *pline;
	int start, end, ret;
	FILE *fp = fopen("index.html", "r");

	if (!fp) {
		ErrPrint("index.html not found\n");
		return CMD_RET_NOT_FOUND;
	}

	while (fgets(line, BUFSZ, fp)) {
		pline = line;
		while (1) {
			start = istrchr(pline, '[');
			end = istrchr(pline, ']');
			/* No cmd found: write all line */
			if (start < 0 || end < 0) {
				pos += write_str_to_buf(buf + pos, pline);
				break;
			}

			/* replace command */
			memset(cmd_name, 0, CMD_MAX_LEN);
			memcpy(cmd_name, pline + start + 1, end - start - 1);
			if ((cmd = find_cmd(cmd_name)) == NULL) {
				sprintf(conv_cmd,
						"<font size=\"3\" color=\"red\">"
						"command %s not found </font>",
						cmd_name);
			} else {
				sprintf(conv_cmd,
						"<a href=\"%s%s\" target=\"result\">%s</a>",
						CMD_PREFIX, cmd, cmd_name);
			}

			pline[start] = '\0';
			pos += write_str_to_buf(buf + pos, pline);
			pos += write_str_to_buf(buf + pos, conv_cmd);
			pline = pline + end + 1;
		}
	}

	fclose(fp);

	ret = http_write_header(sock, get_http_type(".html"), pos);
	if (ret <= 0) {
		ErrPrint("write header failed, ret:%d, err:%s\n", ret, strerror(errno));
		return -3;
	}

	ret = write(sock, buf, pos);
	if (ret != pos) {
		ErrPrint("write data failed, n:%d, ret:%d, err:%s\n",
				pos, ret, strerror(errno));
		return -5;
	}
}

int execute_cmd(const char *cmd, int sock)
{
	int i, ret, n;
	char path[PATH_MAX_LEN];
	char buf[BUFSZ];

	if (strcmp(cmd, "index") == 0) {
		return response_index(sock);
	}
#if 0
	for (i = 0; i < gconf.cmd_cnt; i++) {
		DbgPrint("cmd:%s, cmd name[%d]:%s\n", cmd, i, gconf.cmds[i].name);
		if (0 == strncmp(cmd, gconf.cmds[i].name, CMD_MAX_LEN)) {
			break;
		}
	}

	if (i >= gconf.cmd_cnt) {
		ErrPrint("Command not found, name:%s\n", gconf.cmds[i].name);
		return CMD_RET_NOT_FOUND;
	}
	DbgPrint("cmd name:%s, path:%s\n", gconf.cmds[i].name, path);
#endif

	replace_named_path(cmd, path);
	DbgPrint("cmd:%s, path:%s\n", cmd, path);
	if (access(path, X_OK) < 0) {
		ErrPrint("File access failed, name:%s, path:%s\n", gconf.cmds[i].name, path);
		n = sprintf(buf, "Command not found:\npath:%s", path);
	} else {
		ret = run_cmd(path);
		n = sprintf(buf, "Command return:%d\npath:%s", ret, path);
	}

	ret = http_write_header(sock, get_http_type(".txt"), n);
	if (ret <= 0) {
		ErrPrint("write header failed, ret:%d, err:%s\n", ret, strerror(errno));
		return -3;
	}
	write(sock, buf, n);

	return 0;
}

int wait_connect(int fd)
{
	int len, ret = 0;
	fd_set	rfds;			// 接続待ち、受信待ちをするディスクリプタの集合
	struct timeval	tv;		// タイムアウト時間
	struct sockaddr_in client;
	char ip_str[INET_ADDRSTRLEN];

	FD_ZERO( &rfds );
	FD_SET( fd, &rfds );
	tv.tv_sec = CONN_TIMEOUT;
	tv.tv_usec = 0;

	if ((ret = select(fd + 1, &rfds, NULL, NULL, &tv)) <= 0) {
		DbgPrint("select timeout, ret:%d\n", ret);
		return -1;
	}

	InfPrint("Connected!\n");
	len = sizeof(client);
	ret = accept(fd, (struct sockaddr *)&client, &len);
	InfPrint("Accepted!\n");
	if (inet_ntop(AF_INET, &client.sin_addr, ip_str, INET_ADDRSTRLEN) == NULL) {
		ErrPrint("Unknow remote IP address\n");
		return -2;
	}
	InfPrint("remote IP:%s Port:%d\n", ip_str, ntohs(client.sin_port));

	return ret;
}

int wait_cmd(int sock, char *cmd, int sz)
{
	int i;
	char key[4] = { 0 };
	char c;
	char cmdbuf[PATH_MAX_LEN];

	FunPrint("Start\n");

start_read:
	/* find GET */
	while (1) {
		if (readc(sock, &key[2]) < 0) {
			return -1;
		}

		if (strncmp(key, "GET", 3) == 0) break;
		key[0] = key[1];
		key[1] = key[2];
	}

	DbgPrint("########################################## Got GET\n");
	/* find / */
	while (1) {
		if (readc(sock, &c) < 0) {
			return -2;
		}

		if (c == '/') break;
		if (c != ' ') goto start_read;
	}

	/* get command */
	for (i = 0; i < CMD_MAX_LEN; i++) {
		if (readc(sock, &c) < 0) {
			return -3;
		}

		DbgPrint("get cmd c:%c\n", c);
		if (c == ' ') break;
		if (c == '\n' || c == '\r') goto start_read;
		cmdbuf[i] = c;
	}

	if (i == 0) {
		InfPrint("index.html\n");
		strcpy(cmd, "index");
		return 0;
	} else if (i >= CMD_MAX_LEN) {
		ErrPrint("command len = %d too long\n", CMD_MAX_LEN);
		return -5;
	}
	cmdbuf[i] = 0;
	if (memcmp(cmdbuf, CMD_PREFIX, strlen(CMD_PREFIX)) == 0) {
		strcpy(cmd, cmdbuf + strlen(CMD_PREFIX));
	} else {
		ErrPrint("command prefix not match, cmd:%s\n", cmdbuf);
		return -5;
	}
	DbgPrint("cmd len:%d, cmd:%s, cmdbuf:%s\n", i, cmd, cmdbuf);

	return 0;
}

int start_server(unsigned short port)
{
	int fd, ret, sock;
	struct sockaddr_in addr;
	char cmd[BUFSZ];
	char result[BUFSZ];
	int yes = 1;

	InfPrint("Using port %d\n", port);
	/* ソケットの作成 */
	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		ErrPrint("socket failed:%s, ret:%d\n", strerror(errno), fd);
		goto end;
	}

	ret = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof(yes));
	if (ret < 0) {
		ErrPrint("setsockopt failed:%s, ret:%d\n", strerror(errno), ret);
		goto end;
	}

	/* ソケットの設定 */
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;
	ret = bind(fd, (struct sockaddr *)&addr, sizeof(addr));
	if (ret < 0) {
		ErrPrint("bind failed:%s, ret:%d\n", strerror(errno), ret);
		goto end;
	}

	/* TCPクライアントからの接続要求を待てる状態にする */
	ret = listen(fd, 15);
	if (ret < 0) {
		ErrPrint("listen failed:%s, ret:%d\n", strerror(errno), ret);
		goto end;
	}

	while (1)
	{
		/* TCPクライアントからの接続要求を受け付ける */
		if ((sock = wait_connect(fd)) < 0) {
			DbgPrint("wait connect timeout\n");
			goto end;
		}

		if (wait_cmd(sock, cmd, sizeof(cmd)) == 0) {
			ret = execute_cmd(cmd, sock);
		}

		DbgPrint("close socket\n");
		close(sock);
		reset_sock_buf();
	}

end:
	/* listen するsocketの終了 */
	DbgPrint("close fd\n");
	close(fd);

	return 0;
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

	if ( parse_conf() < 0 ) {
		ErrPrint("parse config file failed\n");
		exit(4);
	}

	while (1)
	{
		start_server(port);
	}

	return 0;
}
