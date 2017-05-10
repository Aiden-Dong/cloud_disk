#ifndef SCMESSCTL_H_
#define SCMESSCTL_H_

#include "service_base.h"
#include "socket_server.h"


// 用户登录
int user_login(PEVENT_STRUCT pevent, PCLI_TO_SERV client_msg);
// 用户注册
int user_register(PEVENT_STRUCT pevent, PCLI_TO_SERV client_msg);
// 用户改变目录
int user_change_dir(PEVENT_STRUCT pevent, PCLI_TO_SERV client_msg, uint8_t flags);
// 用户创建目录
int user_create_dir(PEVENT_STRUCT pevent, PCLI_TO_SERV client_msg);
// 用户删除文件
int user_remove_file(PEVENT_STRUCT pevent, PCLI_TO_SERV client_msg);
// 用户删除目录
int user_remove_directoy(PEVENT_STRUCT pevent, PCLI_TO_SERV client_msg);
// 复制文件
int user_copy_file(PEVENT_STRUCT pevent, PCLI_TO_SERV client_msg);
// 移动文件
int user_move_file(PEVENT_STRUCT pevent, PCLI_TO_SERV client_msg);
// 上传文件
int user_trans_upload(PEVENT_STRUCT pevent, PCLI_TO_SERV client_msg);
// 下载文件
int user_trans_download(PEVENT_STRUCT pevent, PCLI_TO_SERV client_msg);


// 耗时任务处理
int user_havey_task(PEVENT_STRUCT pevent, PCLI_TO_SERV client_msg);


static int user_show_dir(PEVENT_STRUCT pevent);

#endif
