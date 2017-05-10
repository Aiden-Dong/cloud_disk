#ifndef BASE_DAEMON_H_
#define BASE_DAEMON_H_

#include "service_base.h"

// daemon lock file
#define FILE_FLAGE "./.version"

// server init operator
int server_init();
// 服务器初始化

static int daemon_start(void);
// 守护进程模式开启
static int epoll_init();
// epool 池初始化
static void list_event();
// 用户信息链初始化
static int signal_init();
// 信号处理初始化
static int pthread_contrl_init();
// 线程初始化
static void *user_status_clean(void *args);
// 无响应用户清理线程
#endif
