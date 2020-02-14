#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>

#define __DEBUGLOG_ERROR				1
#define __DEBUGLOG_INFO					1
#define __DEBUGLOG_DEBUG				0
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




#define ARYSIZ(ary)					(sizeof(ary) / sizeof(ary[0]))
#define CMD_START					'('
#define CMD_END						')'
#define CMD_MAX_LEN					16
#define WAIT_AFTER_READ				(500 * 1000)
#define CMD_NOT_FOUND_RET			-9999

#define BUFSZ						1024

/* ==================================================================== */
/* STRUCTS															  */
/* ==================================================================== */

struct cmd_info
{
	int (*fun)(char *result, int size);
	const char *cmd;
};


/* ==================================================================== */
/* FUNCTIONS															*/
/* ==================================================================== */

int readc(int sock, char *c)
{
	static int p = 0;
	static int sz = 0;
	static char buf[BUFSZ];

	if (p >= sz) {
		p = 0;
		sz = read(sock, buf, sizeof(buf));
		usleep(WAIT_AFTER_READ);
		if (sz == 0 && errno == 0) {
			InfPrint("remote peer exited\n");
			return -1;
		} else if (sz <= 0) {
			ErrPrint("read failed:%s\n", strerror(errno));
			return -2;
		}
		buf[sz] = 0;
		DbgPrint("read:%s\n", buf);
	}

	*c = buf[p++];
	DbgPrint("readc internal:%c\n", *c);

	return 0;
}

int wait_cmd(int sock, char *cmd, int sz)
{
	char c;
	int i;

	if (sz <= CMD_MAX_LEN) {
		ErrPrint("command buffer too small\n");
		return -5;
	}

start_read:
	/* find start char */
	while (1) {
		if (readc(sock, &c) < 0) {
			return -1;
		}

		DbgPrint("readc1:%c\n", c);
		if (c == CMD_START) break;
	}

	/* wait end char */
	for (i = 0; i < CMD_MAX_LEN; i++) {
		if (readc(sock, &c) < 0) {
			return -2;
		}

		DbgPrint("readc2:%c\n", c);
		if (c == CMD_END) break;
		if (c == '\n' || c == '\r') goto start_read;

		DbgPrint("after break\n");
		cmd[i] = c;
	}

	if (i == 0) {
		ErrPrint("command len = 0\n");
		return -3;
	} else if (i >= CMD_MAX_LEN) {
		ErrPrint("command len = %d too long\n", CMD_MAX_LEN);
		return -4;
	}
	DbgPrint("cmd len:%d\n", i);
	cmd[i] = 0;

	return 0;
}

int cmd_notepad(char *result, int size)
{
	system("/mnt/c/Windows/System32/notepad.exe &");

	sprintf(result, "execute succed");
	return 0;
}

int cmd_update(char *result, int size)
{

	system("/mnt/d/bjn/update.exe &");

	sprintf(result, "execute succed");
	return 0;
}

int cmd_restart(char *result, int size)
{

	system("/mnt/d/bjn/restart_all_vm.exe &");

	sprintf(result, "execute succed");
	return 0;
}

int cmd_reboot(char *result, int size)
{

	system("/mnt/c/Windows/System32/shutdown.exe -f -r -t 0");
	//system("sudo reboot");

	sprintf(result, "execute succed");
	return 0;
}

int cmd_shutdown(char *result, int size)
{

	system("/mnt/c/Windows/System32/shutdown.exe -s -t 0");

	sprintf(result, "execute succed");
	return 0;
}


int cmd_test(char *result, int size)
{

	system("/mnt/d/bjn/mouse_test.exe &");

	sprintf(result, "execute succed");
	return 0;
}


struct cmd_info cmd_table[] = 
{
	{ cmd_notepad,	"notepad" },
	{ cmd_test,	"test" },
	{ cmd_update,	"update" },
	{ cmd_restart,	"restart" },
	{ cmd_reboot,	"reboot" },
	{ cmd_shutdown,	"shutdown" },
};

int execute_cmd(const char *cmd, char *result, int size)
{
	int i;
	int ret = CMD_NOT_FOUND_RET;

	for (i = 0; i < ARYSIZ(cmd_table); i++) {
		if (0 == strncmp(cmd, cmd_table[i].cmd, strlen(cmd_table[i].cmd))) {
			ret = cmd_table[i].fun(result, size);
			break;
		}
	}

	return ret;
}

int wait_connect(int fd)
{
	int ret = 0;
	fd_set	rfds;			// 接続待ち、受信待ちをするディスクリプタの集合
	struct timeval	tv;		// タイムアウト時間

	FD_ZERO( &rfds );
	FD_SET( fd, &rfds );
	tv.tv_sec = 10;
	tv.tv_usec = 0;

	if (select(fd + 1, &rfds, NULL, NULL, &tv) <= 0) {
		ret = -1;
	}

	return ret;
}

int start_server(unsigned short port)
{
	int fd, ret, len, sock;
	struct sockaddr_in addr;
	struct sockaddr_in client;
	char cmd[BUFSZ];
	char result[BUFSZ];

	InfPrint("Using port %d\n", port);
	/* ソケットの作成 */
	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		ErrPrint("socket failed:%s, ret:%d\n", strerror(errno), fd);
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
		if (wait_connect(fd) < 0) {
			DbgPrint("wait connect timeout\n");
			goto end;
		}
		len = sizeof(client);
		sock = accept(fd, (struct sockaddr *)&client, &len);

		while (wait_cmd(sock, cmd, sizeof(cmd)) == 0)
		{
			memset(result, 0, sizeof(result));
			ret = execute_cmd(cmd, result, sizeof(result));
			if (ret == CMD_NOT_FOUND_RET) {
				sprintf(result, "Command \"%s\" Not Found", cmd);
			}
			InfPrint("execute cmd:%s, return:%d\n", cmd, ret);
			write(sock, result, strlen(result));
		}

		/* TCPセッションの終了 */
		close(sock);
	}

end:
	/* listen するsocketの終了 */
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

	while (1)
	{
		start_server(port);
	}

	return 0;
}
