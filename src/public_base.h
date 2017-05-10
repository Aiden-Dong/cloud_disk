#ifndef PUBLIC_BASE_H_
#define PUBLIC_BASE_H_

#include "service_base.h"


/*
* 强制错误
*/
void err_que(const char *msg);
void err_sys(const char *msg);
void err_mysql(const char *msg);

/*
* 日志型错误
*/
int sys_err_log(const char *msg);
int mysql_err_log(const char *msg);
int pth_sys_err_log(const char *msg, int errn);


// 信号处理
void sig_porc_stop(int signo);

// 获取当前时间
int serv_get_nowtime(char *buff);

//信号发生时删除节点
static int sig_clean_node(PEVENT_STRUCT pevent);

#endif
