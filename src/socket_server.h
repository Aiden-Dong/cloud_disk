#ifndef SOCKET_SERVER_H_
#define SOCKET_SERVER_H_

#include "service_base.h"

/*
 *	文件格式
 */
#define FILE_NOR 1		// 普通文件
#define FILE_DIR 2		// 目录文件

/*
 * 服务端标志
 */
#define SE_SUCESS 0		// 操作成功
#define SE_OK	    1		// 请求通过
#define SE_REFUSE 2		// 请求拒绝
#define SE_FAILE  3		// 操作失败
#define SE_ERROR  4		// 服务异常（服务端关闭）
#define SE_DONOW  5		// 正载执行服务操作
#define SE_FINISH 6		// 请求操作完成
/*
 * 客户端标志
*/
#define CL_LOGIN  1		// 用户登录
#define CL_ENROLL 2		// 用户注册
#define CL_EXIT	  3		// 用户退出
#define CL_CDDIR  4		// 进入目录
#define CL_EXDIR  5		// 退出目录
#define CL_CRDIR  6		// 创建目录
#define CL_RMFIL  7		// 删除文件
#define CL_RMDIR 	8		// 删除目录
#define CL_CPFIL  9		// 复制文件
#define	CL_MVFIL  10 	// 移动文件
#define CL_SNDFL  11	// 上传文件
#define CL_RCVFL  12	// 下载文件
#define CL_ERROR  0		// 意外错误

/*
* client send to server
*/
typedef struct{
uint8_t	    cts_events;			               // 用户请求事件
int8_t      cts_fname[MAX_NAME_LEN+1];		 // 文件名
uint32_t 	  cts_fsize;			               // 文件大小
int8_t	    cts_main[MAX_DNAM_LEN+1];		   // 主目录
}CLI_TO_SERV, *PCLI_TO_SERV;

/*
* server send to client
*/
typedef struct{
	uint8_t 	  stc_events;	   		        	// 服务器回应事件
	int8_t 	    stc_fname[MAX_NAME_LEN+1];	// 文件名
	uint8_t 	  stc_ftype;			          	// 文件格式
	uint32_t 	  stc_fsize;			          	// 文件大小
	int8_t			stc_main[MAX_DNAM_LEN+1];		// 用户的当前目录
}SERV_TO_CLI, *PSERV_TO_CLI;

//====================================================================

// 服务器初始化
int socket_init();
// 服务器启动监听事件
void server_listen();
// 接受客户端的链接请求
int server_accept();
// 分析客户端请求信息
int client_request(PEVENT_STRUCT client_event);
// 删除客户端节点
int delete_node(PEVENT_STRUCT pevent);

// 调试
void show_message_link();
#endif
