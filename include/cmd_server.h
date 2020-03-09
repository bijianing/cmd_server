#ifndef __CMD_SERVER_H__
#define __CMD_SERVER_H__

/* ==================================================================== */
/* DEFINE																*/
/* ==================================================================== */
#define ARYSIZ(ary)					(sizeof(ary) / sizeof(ary[0]))
#define CMD_START					'.'
#define CMD_END						'.'
#define CMD_MAX_LEN					64
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
#define CONF_FILE_PATH			"server.conf"
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
/* STRUCTS															  */
/* ==================================================================== */

void reset_sock_buf(void);
int readc(int sock, char *c);
const char *get_http_type(const char *path);
int write_cmd_result_tobuf(char *buf, const char *cmd, CmdRet_t ret);
int write_cmd_result(int sock, const char *cmd, CmdRet_t ret);
int http_write_header(int sock, const char *type, long content_length);
int cmd_internal_http_send_file(int sock, const char *path, int tmo);





#endif /* __CMD_SERVER_H__ */

