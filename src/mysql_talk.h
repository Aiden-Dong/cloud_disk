#ifndef MYSQL_TALK_H_
#define MYSQL_TALK_H_

#include "service_base.h"

// 向日志文件中插入一条日志信息
int insert_mysql_log(int id, const char *msg);
// 检查用户登录
int user_login_record(const char *user_id, const char *user_pass, char *user_name);
// 创建新用户
int user_register_record(const char *user_name, const char *user_pass, char *u_id);
// 用户退出
int user_logout_record(const int user_id);

#endif
