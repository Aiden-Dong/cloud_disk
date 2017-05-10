#ifndef SERVICE_BASE_H_
#define SERVICE_BASE_H_

#include <mysql/mysql.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <stdint.h>
#include <dirent.h>
#include <unistd.h>
#include <netdb.h>
#include <ctype.h>
#include <errno.h>

/*
*   服务器相关信息配置
*/
#define MAX_NAME_LEN 					50												// 用户名的长度
#define MAX_DNAM_LEN					175												// 相对路径的长度
#define MAX_PASS_LEN					25												// 密码长度
#define LOG_FILE        			"../log/service.log"      // 日志文件名
#define LOG_FILE_BAK					"../log/service_bak.log"	// 日志备份文件
#define ADMIN_HOME						"../admin"								// 用户操作主目录
#define SOCK_PORT	      			"eva-service"							// socket 端口

/*
 *	epoll 监听的结构
 */
typedef struct EPOLL_EVENT{
	int32_t 						st_fd;				// 关联的 socket 文件描述符
	uint32_t            st_user;  		// 用户标志符
	time_t							st_time;			// 关联的时间
	uint32_t				   	st_addr;			// 客户端的IP地址
	uint32_t 				    st_live;			// 会话生存期
	pthread_mutex_t			st_mutex;			// 会话锁
	int8_t							st_dir[215];	// 用户当前目录
	struct EPOLL_EVENT	*st_next;			// 下一个链表信息结构
}EVENT_STRUCT, *PEVENT_STRUCT;

typedef struct{
	PEVENT_STRUCT 					e_list;				// 用户事件链
	uint8_t									e_flags;			// 用户标志
	pthread_mutex_t					e_lock;				// 用户锁
}EVENT_LIST, *PEVENT_LIST;

/*
* 记录服务器核心管理的接口
*/
typedef struct{
	int 					serv_sock;	      // socket 文件描述符
	int 					serv_trans;				// unix 	文件描述符
	int 					serv_epoll;	      // epoll  文件描述符
	MYSQL     		serv_mysql;       // mysql  文件描述符
	int   				serv_log;         // log    文件描述符
	EVENT_LIST		serv_event;		  	// event  链表信息
}SERVICE, *PSERVICE;

#endif
