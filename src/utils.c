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

#include <cmd_server.h>


#define __DEBUGLOG_ERROR				1
#define __DEBUGLOG_INFO					1
#define __DEBUGLOG_DEBUG				1
#define __DEBUGLOG_FUNC					1
#define __DEBUGLOG_POS					0

#define MOD_NAME						"Util"
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
/* VARIABLES															*/
/* ==================================================================== */
static int sock_data_sz = 0;


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

void reset_sock_buf(void)
{
	sock_data_sz = 0;
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

const char *get_http_type(const char *path)
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

int cmd_internal_http_send_file(int sock, const char *path, int tmo)
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

