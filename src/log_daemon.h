#ifndef LOG_DAEMON_H_
#define LOG_DAEMON_H_

#include "service_base.h"

#define MYSQL_USER_NAME "saligia"
#define MYSQL_HOST_ADDR "127.0.0.1"
#define MYSQL_USER_PASS "360628836989"
#define MYSQL_DATABASE "saligia"


// 完成 mysql 的初始化
int serv_mysql_init();

// 完成 本地log 的初始化
int serv_log_init();
// 将服务器事件写入 日志
int serv_log_write(const char *msg, int level);

#endif
