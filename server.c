#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define CMD_START	','
#define CMD_END		','
#define CMD_MAX_LEN	16


int readc(int sock, char *c)
{
	static int p = 0;
	static int sz = 0;
	static char buf[1024];

	if (p >= sz) {
		p = 0;
		sz = read(sock, buf, sizeof(buf));
		if (sz <= 0) {
			printf("read failed:%s\n", strerror(errno));
			return -1;
		}
		buf[sz] = 0;
		printf("read:%s\n", buf);
	}

	*c = buf[p++];
	printf("readc internal:%c\n", *c);

	return 0;
}

int wait_cmd(int sock, char *cmd, int sz)
{
	char c;
	int i;

	if (sz <= CMD_MAX_LEN) {
		printf("command buffer too small\n");
		return -5;
	}

	/* find start char */
	while (1) {
		if (readc(sock, &c) < 0) {
			return -1;
		}

		printf("readc1:%c\n", c);
		if (c == CMD_START) break;
	}

	/* wait end char */
	for (i = 0; i < CMD_MAX_LEN; i++) {
		if (readc(sock, &c) < 0) {
			return -2;
		}

		printf("readc2:%c\n", c);
		if (c == CMD_END) break;

		printf("after break\n");
		cmd[i] = c;
	}

	if (i == 0) {
		printf("command len = 0\n");
		return -3;
	} else if (i >= CMD_MAX_LEN) {
		printf("command len = %d too long\n", CMD_MAX_LEN);
		return -4;
	}
	printf("cmd len:%d\n", i);
	cmd[i] = 0;

	return 0;
}

int main()
{
	int sock0;
	struct sockaddr_in addr;
	struct sockaddr_in client;
	int len;
	int sock;
	char cmd[1024];

	/* ソケットの作成 */
	sock0 = socket(AF_INET, SOCK_STREAM, 0);

	/* ソケットの設定 */
	addr.sin_family = AF_INET;
	addr.sin_port = htons(12345);
	addr.sin_addr.s_addr = INADDR_ANY;
	bind(sock0, (struct sockaddr *)&addr, sizeof(addr));

	/* TCPクライアントからの接続要求を待てる状態にする */
	listen(sock0, 5);

	while (1)
	{
		int end_flg = 0;
		/* TCPクライアントからの接続要求を受け付ける */
		len = sizeof(client);
		sock = accept(sock0, (struct sockaddr *)&client, &len);

		while (wait_cmd(sock, cmd, sizeof(cmd)) == 0)
		{
			printf("got cmd:%s\n", cmd);
			write(sock, cmd, strlen(cmd));
		}

		/* TCPセッションの終了 */
		close(sock);
	}

	/* listen するsocketの終了 */
	close(sock0);

	return 0;
}
