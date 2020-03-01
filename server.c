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


#define __DEBUGLOG_ERROR				1
#define __DEBUGLOG_INFO					1
#define __DEBUGLOG_DEBUG				1
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

typedef struct cmd_info
{
	int (*fun)(struct cmd_info *info, int sock);
	const char *cmd;
	const char *param;
} CmdInfo_t;


/* ==================================================================== */
/* FUNCTIONS															*/
/* ==================================================================== */

int readc(int sock, char *c)
{
	static int p = 0;
	static char buf[BUFSZ + 1];

	if (p >= sock_data_sz) {
		p = 0;
		sock_data_sz = read(sock, buf, sizeof(buf));
		usleep(WAIT_AFTER_READ);
		if (sock_data_sz == 0 && errno == 0) {
			InfPrint("remote peer exited\n");
			return -1;
		} else if (sock_data_sz <= 0) {
			ErrPrint("read failed:%s\n", strerror(errno));
			return -2;
		}
		buf[sock_data_sz] = 0;
//		DbgPrint("read:%s\n", buf);
	}

	*c = buf[p++];
//	DbgPrint("readc internal:%c\n", *c);

	return 0;
}

static const char *httpd_get_header_date(void)
{
	int n;
	time_t tm;
	struct tm *date;
	static char buf[BUFSZ];
	const char *day_name[] = {
		"Sun",
		"Mon",
		"Tue",
		"Wed",
		"Thu",
		"Fri",
		"Sat"
	};
	const char *mon_name[] = {
		"Jan",
		"Feb",
		"Mar",
		"Apr",
		"May",
		"Jun",
		"Jul",
		"Aug",
		"Sep",
		 "Oct",
		 "Nov",
		 "Dec"
	};

	tm = time(NULL);
	date = localtime(&tm);

	if (!date) {
		ErrPrint("localtime() failed\n");
		return "";
	}
	DbgPrint("day:%d, mon:%d\n", date->tm_wday, date->tm_mon);
	n = sprintf(buf, "%s, %02d %s %04d %02d:%02d:%02d GMT",
			day_name[date->tm_wday],
			date->tm_mday,
			mon_name[date->tm_mon],
			date->tm_year, date->tm_hour, date->tm_min, date->tm_sec);

	return buf;;
}

int wait_cmd(int sock, char *cmd, int sz)
{
	int i;
	char key[4] = { 0 };
	char c;
	char *pcmd;

	FunPrint("Start\n");
#if 1
	strcpy(cmd, "http_");
#else
	memset(cmd, 0 sz);
#endif
	pcmd = cmd + strlen(cmd);

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
		pcmd[i] = c;
	}

	if (i == 0) {
		InfPrint("index.html\n");
		strcpy(pcmd, "index");
		return 0;
	} else if (i >= CMD_MAX_LEN) {
		ErrPrint("command len = %d too long\n", CMD_MAX_LEN);
		return -5;
	}
	pcmd[i] = 0;
	DbgPrint("cmd len:%d, cmd:%s, pcmd:%s\n", i, cmd, pcmd);

	return 0;
}

int write_cmd_result_tobuf(char *buf, const char *cmd, CmdRet_t ret)
{
	const char *str;

	switch (ret)
	{
		case eCMD_RES_OK:
			str = CMD_RET_STR_OK;
			break;
		case eCMD_RES_NOT_FOUND:
			str = CMD_RET_STR_NOT_FOUND;
			break;
		case eCMD_RES_HTTP_OK:
			/* HTTP response, dont write anything */
			return 0;
		default:
			str = CMD_RET_STR_UNKNOW;
			break;
	}

	return snprintf(buf, BUFSZ, "Command:%s : %s\n", cmd, str);
}

int write_cmd_result(int sock, const char *cmd, CmdRet_t ret)
{
	char buf[BUFSZ];
	int n;

	n = write_cmd_result_tobuf(buf, cmd, ret);
	ret = write(sock, buf, n);

	if (ret != n) {
		return -1;
	}

	return 0;
}

int cmd_notepad(CmdInfo_t *info, int sock)
{
	system("/mnt/c/Windows/System32/notepad.exe &");

	return write_cmd_result(sock, info->cmd, eCMD_RES_OK);
}

int cmd_update(CmdInfo_t *info, int sock)
{

	system("/mnt/d/bjn/update.exe &");

	return write_cmd_result(sock, info->cmd, eCMD_RES_OK);
}

void cmd_internal_restart(int n)
{
	char cmd_close[PATH_MAX_LEN];
	char cmd_max[PATH_MAX_LEN];

	snprintf(cmd_close, PATH_MAX_LEN, "/mnt/d/bjn/close%d.exe", n);
	snprintf(cmd_max, PATH_MAX_LEN, "/mnt/d/bjn/max%d.exe", n);

	system(cmd_close);
	sleep(10);
	system("/mnt/d/bjn/start_all.exe");
	sleep(50);
	system(cmd_max);
	sleep(1);
	system("/mnt/d/bjn/run.exe");
	sleep(1);
	system("/mnt/d/bjn/max_off.exe");
}

int cmd_restart1(CmdInfo_t *info, int sock)
{
	cmd_internal_restart(1);
	return write_cmd_result(sock, info->cmd, eCMD_RES_OK);
}

int cmd_restart2(CmdInfo_t *info, int sock)
{
	cmd_internal_restart(2);
	return write_cmd_result(sock, info->cmd, eCMD_RES_OK);
}

int cmd_restart3(CmdInfo_t *info, int sock)
{
	cmd_internal_restart(3);
	return write_cmd_result(sock, info->cmd, eCMD_RES_OK);
}

int cmd_restart_all(CmdInfo_t *info, int sock)
{
	cmd_internal_restart(1);
	cmd_internal_restart(2);
	cmd_internal_restart(3);
	return write_cmd_result(sock, info->cmd, eCMD_RES_OK);
}

int cmd_reboot(CmdInfo_t *info, int sock)
{

	system("/mnt/c/Windows/System32/shutdown.exe -f -r -t 0");
	//system("sudo reboot");

	return write_cmd_result(sock, info->cmd, eCMD_RES_OK);
}

int cmd_shutdown(CmdInfo_t *info, int sock)
{

	system("/mnt/c/Windows/System32/shutdown.exe -s -t 0");

	return write_cmd_result(sock, info->cmd, eCMD_RES_OK);
}

int cmd_move_mouse(CmdInfo_t *info, int sock)
{

	system("/mnt/d/bjn/move_mouse.exe &");

	return write_cmd_result(sock, info->cmd, eCMD_RES_OK);
}

int cmd_close1(CmdInfo_t *info, int sock)
{

	system("/mnt/d/bjn/close1.exe");

	return write_cmd_result(sock, info->cmd, eCMD_RES_OK);
}

int cmd_close2(CmdInfo_t *info, int sock)
{

	system("/mnt/d/bjn/close2.exe");

	return write_cmd_result(sock, info->cmd, eCMD_RES_OK);
}

int cmd_close3(CmdInfo_t *info, int sock)
{

	system("/mnt/d/bjn/close3.exe");

	return write_cmd_result(sock, info->cmd, eCMD_RES_OK);
}

int cmd_max1(CmdInfo_t *info, int sock)
{

	system("/mnt/d/bjn/max1.exe");

	return write_cmd_result(sock, info->cmd, eCMD_RES_OK);
}

int cmd_max2(CmdInfo_t *info, int sock)
{

	system("/mnt/d/bjn/max2.exe");

	return write_cmd_result(sock, info->cmd, eCMD_RES_OK);
}

int cmd_max3(CmdInfo_t *info, int sock)
{

	system("/mnt/d/bjn/max3.exe");

	return write_cmd_result(sock, info->cmd, eCMD_RES_OK);
}

int cmd_max_off(CmdInfo_t *info, int sock)
{

	system("/mnt/d/bjn/max_off.exe");

	return write_cmd_result(sock, info->cmd, eCMD_RES_OK);
}

int cmd_run(CmdInfo_t *info, int sock)
{

	system("/mnt/d/bjn/run.exe");

	return write_cmd_result(sock, info->cmd, eCMD_RES_OK);
}

int cmd_start_all(CmdInfo_t *info, int sock)
{

	system("/mnt/d/bjn/start_all.exe");

	return write_cmd_result(sock, info->cmd, eCMD_RES_OK);
}

int cmd_redraw(CmdInfo_t *info, int sock)
{

	system("./tools/nircmd/nircmd.exe monitor async_on");
	system("./tools/nircmd/nircmd.exe win hideshow ititle BlueStacks");

	return write_cmd_result(sock, info->cmd, eCMD_RES_OK);
}

int cmd_monitor_on(CmdInfo_t *info, int sock)
{

	system("./tools/nircmd/nircmd.exe monitor async_on");

	return write_cmd_result(sock, info->cmd, eCMD_RES_OK);
}

int cmd_monitor_off(CmdInfo_t *info, int sock)
{

	system("./tools/nircmd/nircmd.exe monitor async_off");

	return write_cmd_result(sock, info->cmd, eCMD_RES_OK);
}


int cmd_test(CmdInfo_t *info, int sock)
{

	system("/mnt/d/bjn/mouse_test.exe &");

	return write_cmd_result(sock, info->cmd, eCMD_RES_OK);
}

int cmd_enableHttp(CmdInfo_t *info, int sock)
{

	g_http_mode = 1;

	return write_cmd_result(sock, info->cmd, eCMD_RES_OK);
}

int http_write_header(int sock, const char *type, long content_length)
{
	int n, ret;
	char buf[BUFSZ];

	n = snprintf(buf, BUFSZ, "HTTP/1.1 200 OK\r\n"
			"Date: %s\r\n"
			"Content-Type: %s\r\n"
			"Content-Length: %ld\r\n"
			"Connection: close\r\n\r\n",
			httpd_get_header_date(),
			type,
			content_length);
	DbgPrint("n:%d, buf:%s\n", n, buf);
	ret = write(sock, buf, n);
	if (ret != n) {
		ErrPrint("write failed, n:%d ret:%d\n", n, ret);
		return -1;
	}

	return n;
}

static int str_end_with(const char *str, const char *suffix)
{
	if (!str || !suffix)
		return 0;
	size_t lenstr = strlen(str);
	size_t lensuffix = strlen(suffix);
	if (lensuffix >  lenstr)
		return 0;
	return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}

static const char *get_http_type(const char *path)
{
	int i;
	struct ext_type {
		const char *ext;
		const char *type;
	} tbl[] = {
		{ ".png", "image/png" },
		{ ".html", "text/html" },
		{ ".htm", "text/html" },
		{ ".txt", "text/plain" },
	};

	for (i = 0; i < ARYSIZ(tbl); i++) {
		if (str_end_with(path, tbl[i].ext))
			return tbl[i].type;
	}

	return "text/html";
}

static int cmd_internal_http_send_file(int sock, const char *path, int tmo)
{
	struct stat st;
	int n, ret, fd, wrote;
	char buf[BUFSZ];

	while (tmo > 0) {
		if (stat(path, &st) == 0) {
			break;
		}

		/* sleep 1ms */
		tmo--;
		usleep(1000);
	}
	if (tmo <= 0) {
		ErrPrint("File %s Not found\n", path);
		return -1;
	}
	//usleep(1000000);

	if ((st.st_mode & S_IFMT) != S_IFREG) {
		ErrPrint("File %s is not a file\n", path);
		return -2;
	}

	ret = http_write_header(sock, get_http_type(path), st.st_size);
	if (ret <= 0) {
		ErrPrint("write header failed, ret:%d\n", ret);
		return -3;
	}

	fd = open(path, O_RDONLY);
	if (fd < 0) {
		ErrPrint("open() failed: %s\n", strerror(errno));
		return -4;
	}

	wrote = 0;
	while ((n = read(fd, buf, BUFSZ)) > 0) {
		DbgPrint("write :%d bytes to fd:%d\n", n, fd);
		ret = write(sock, buf, n);
		wrote += n;
		DbgPrint("total:%ld, wrote:%d read %d bytes from fd:%d, write to fd:%d\n", st.st_size, wrote, n, fd, sock);
		if (ret != n) {
			ErrPrint("write data failed, n:%d, ret:%d\n", n, ret);
			return -5;
		}
	}

	return 0;
}

int cmd_http_close1(CmdInfo_t *info, int sock)
{

	system("/mnt/d/bjn/close1.exe");

	return write_cmd_result(sock, info->cmd, eCMD_RES_OK);
}

int cmd_http_close2(CmdInfo_t *info, int sock)
{

	system("/mnt/d/bjn/close2.exe");

	return write_cmd_result(sock, info->cmd, eCMD_RES_OK);
}

int cmd_http_close3(CmdInfo_t *info, int sock)
{

	system("/mnt/d/bjn/close3.exe");

	return write_cmd_result(sock, info->cmd, eCMD_RES_OK);
}

int cmd_http_max1(CmdInfo_t *info, int sock)
{
	char buf[BUFSZ];
	int n;

	n = write_cmd_result_tobuf(buf, info->cmd, eCMD_RES_OK);
	http_write_header(sock, "text/plain", n);

	return cmd_max1(info, sock);
}

int cmd_http_max2(CmdInfo_t *info, int sock)
{

	system("/mnt/d/bjn/max2.exe");

	return write_cmd_result(sock, info->cmd, eCMD_RES_OK);
}

int cmd_http_max3(CmdInfo_t *info, int sock)
{

	system("/mnt/d/bjn/max3.exe");

	return write_cmd_result(sock, info->cmd, eCMD_RES_OK);
}

int cmd_http_max_off(CmdInfo_t *info, int sock)
{

	system("/mnt/d/bjn/max_off.exe");

	return write_cmd_result(sock, info->cmd, eCMD_RES_OK);
}

int cmd_http_run(CmdInfo_t *info, int sock)
{

	system("/mnt/d/bjn/run.exe");

	return write_cmd_result(sock, info->cmd, eCMD_RES_OK);
}

int cmd_http_start_all(CmdInfo_t *info, int sock)
{

	system("/mnt/d/bjn/start_all.exe");

	return write_cmd_result(sock, info->cmd, eCMD_RES_OK);
}

int cmd_http_restart1(CmdInfo_t *info, int sock)
{
	cmd_internal_restart(1);
	return write_cmd_result(sock, info->cmd, eCMD_RES_OK);
}

int cmd_http_restart2(CmdInfo_t *info, int sock)
{
	cmd_internal_restart(2);
	return write_cmd_result(sock, info->cmd, eCMD_RES_OK);
}

int cmd_http_restart3(CmdInfo_t *info, int sock)
{
	cmd_internal_restart(3);
	return write_cmd_result(sock, info->cmd, eCMD_RES_OK);
}

int cmd_http_restart_all(CmdInfo_t *info, int sock)
{
	cmd_internal_restart(1);
	cmd_internal_restart(2);
	cmd_internal_restart(3);
	return write_cmd_result(sock, info->cmd, eCMD_RES_OK);
}

int cmd_http_reboot(CmdInfo_t *info, int sock)
{

	system("/mnt/c/Windows/System32/shutdown.exe -f -r -t 0");
	//system("sudo reboot");

	return write_cmd_result(sock, info->cmd, eCMD_RES_OK);
}

int cmd_http_shutdown(CmdInfo_t *info, int sock)
{

	system("/mnt/c/Windows/System32/shutdown.exe -s -t 0");

	return write_cmd_result(sock, info->cmd, eCMD_RES_OK);
}

int cmd_http_screenshot(CmdInfo_t *info, int sock)
{
	remove(IMG_FILE_PATH);
	system("./tools/nircmd/nircmd.exe monitor async_on");
	system("./tools/nircmd/nircmd.exe win hideshow ititle BlueStacks");
	system("./tools/nircmd/nircmd.exe savescreenshot " IMG_FILE_PATH);

	return cmd_internal_http_send_file(sock, IMG_FILE_PATH, IMG_FILE_WAIT_TIME);
}

int cmd_http_disable(CmdInfo_t *info, int sock)
{
	struct stat st;
	int n, ret, fd;
	char buf[BUFSZ];

	n = snprintf(buf, BUFSZ, "HTTP disabled");
	ret = http_write_header(sock, "text/plain", n);
	if (ret < 0) {
		ErrPrint("write header failed, n:%d, ret:%d\n", n, ret);
		return -1;
	}

	ret = write(sock, buf, n);
	if (ret != n) {
		ErrPrint("write data failed, n:%d, ret:%d\n", n, ret);
		return -2;
	}

	g_http_mode = 0;
	return 0;
}

int cmd_http_index(CmdInfo_t *info, int sock)
{
	
	return cmd_internal_http_send_file(sock, "index.html", 100);
}

int cmd_help(CmdInfo_t *info, int sock);

struct cmd_info cmd_table[] = 
{
	{ cmd_screenshot,		"screenshot" },
	{ cmd_exec_rec,			"exec_rec" },
	{ cmd_exec_nir,			"exec_nir" },
#if 0
	{ cmd_reboot,			"reboot" },
	{ cmd_shutdown,			"shutdown" },
	{ cmd_enableHttp,		"enhttp" },

	{ cmd_close1,			"close1" },
	{ cmd_close2,			"close2" },
	{ cmd_close3,			"close3" },
	{ cmd_max1,				"max1" },
	{ cmd_max2,				"max2" },
	{ cmd_max3,				"max3" },
	{ cmd_max_off,			"max off" },
	{ cmd_run,				"run" },
	{ cmd_start_all,		"start all" },

	{ cmd_restart1,			"restart1" },
	{ cmd_restart2,			"restart2" },
	{ cmd_restart3,			"restart3" },
	{ cmd_restart_all,		"restart all" },

	{ cmd_http_screenshot,	"http_screenshot" },
	{ cmd_http_disable,		"http_disable" },
	{ cmd_http_index,		"http_index" },

	{ cmd_http_reboot,		"http_reboot" },
	{ cmd_http_shutdown,	"http_shutdown" },

	{ cmd_http_close1,		"http_close1" },
	{ cmd_http_close2,		"http_close2" },
	{ cmd_http_close3,		"http_close3" },
	{ cmd_http_max1,		"http_max1" },
	{ cmd_http_max2,		"http_max2" },
	{ cmd_http_max3,		"http_max3" },
	{ cmd_http_max_off,		"http_max_off" },
	{ cmd_http_run,			"http_run" },
	{ cmd_http_start_all,	"http_start_all" },

	{ cmd_http_restart1,			"http_restart1" },
	{ cmd_http_restart2,			"http_restart2" },
	{ cmd_http_restart3,			"http_restart3" },
	{ cmd_http_restart_all,		"http_restart_all" },

	{ cmd_help,				"http_help" },

	{ cmd_help,				"help" },

	{ cmd_notepad,			"notepad" },
	{ cmd_test,				"test" },
	{ cmd_update,			"update" },
	{ cmd_move_mouse,		"move_mouse" },
#endif
};

int cmd_help(CmdInfo_t *info, int sock)
{
	int i;

	for (i = 0; i < ARYSIZ(cmd_table); i++) {
		InfPrint("Cmd %d : %s\n", i, cmd_table[i].cmd);
	}

	return write_cmd_result(sock, info->cmd, eCMD_RES_OK);
}

int execute_cmd(const char *arg_cmd, int sock)
{
	int i;
	int ret = CMD_RET_NOT_FOUND;
	char cmd[CMD_MAX_LEN];
	const char *param;

	if ((param = strchr(arg_cmd, '/')) == NULL) {
		strcpy(cmd, arg_cmd);
	} else {
		for (i = 0; arg_cmd + i < param; i++) {
			cmd[i] = arg_cmd[i];
		}
		cmd[i] = '\0';
		param++;
	}

	for (i = 0; i < ARYSIZ(cmd_table); i++) {
		if (0 == strncmp(cmd, cmd_table[i].cmd, strlen(cmd_table[i].cmd))) {
			ret = cmd_table[i].fun(&cmd_table[i], sock);
			break;
		}
	}

	return ret;
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

		while (wait_cmd(sock, cmd, sizeof(cmd)) == 0)
		{
			ret = execute_cmd(cmd, sock);
			if (ret == CMD_RET_NOT_FOUND) {
				sprintf(result, "Command \"%s\" Not Found", cmd);
				write(sock, result, strlen(result));
			} else if (ret == CMD_RET_HTTP_OK) {
				DbgPrint("result:%s\n", result);
			}
			InfPrint("execute cmd:%s, return:%d\n", cmd, ret);
			break;
		}

		write(sock, CMD_END_STR, strlen(CMD_END_STR));
		/* TCPセッションの終了 */
		DbgPrint("close socket\n");
		close(sock);
		sock_data_sz = 0;
	}

end:
	/* listen するsocketの終了 */
	DbgPrint("close fd\n");
	close(fd);

	return 0;
}

void sig_handler(int sig, siginfo_t *info, void *ctx)
{

	ErrPrint("got signal SIGPIPE\n");

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

	while (1)
	{
		start_server(port);
	}

	return 0;
}
