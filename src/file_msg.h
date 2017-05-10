#ifndef FILE_MSG_H_
#define FILE_MSG_H_

#include "service_base.h"
#include "socket_server.h"

// 文件传输协议
typedef struct{
  uint8_t     trans_flags;    // 0 run 1 stop
  uint16_t    trans_len;
  int8_t      trans_data[1024];
}TRANS_CONTEXT, *PTRANS_CONTEXT;

/*
*   /dir1/dir2
*/
// 将用户路径转真实路径
int8_t * get_host_dir(const int8_t *dir);
// 创建目录
int create_dir(PEVENT_STRUCT pevent, const int8_t *dir);
// 进入目录
/*
  1 -- 进入目录
  2 -- 退出目录
*/
int change_dir(PEVENT_STRUCT pevent, const int8_t *dir, int flags);
// 删除文件
int remove_file(PEVENT_STRUCT pevent, const int8_t *dir);
// 删除目录
int remove_directory(PEVENT_STRUCT pevent, const int8_t *dir);
// 显示目录中的内容
int show_dir(PEVENT_STRUCT pevent);
// 将用户目录转客户端路径
int8_t* get_client_dir(PEVENT_STRUCT pevent);
// 复制文件
int copy_file(PEVENT_STRUCT pevent, PCLI_TO_SERV client_msg);
// 移动文件
int move_file(PEVENT_STRUCT pevent, PCLI_TO_SERV client_msg);
// 上传文件
int upload_file(PEVENT_STRUCT pevent, PCLI_TO_SERV client_msg);
// 下载文件
int download_file(PEVENT_STRUCT pevent, PCLI_TO_SERV client_msg);

/*
* 线程级事件
*/
// 将用户请求目录转真实目录
int get_reality_dir(PEVENT_STRUCT pevent, int8_t *request_dir, int8_t* reality_dir, int n);
// 获取用户的所在目录
int pth_get_host_dir(const int8_t *dir, int8_t *host_dir, int n);



// 测试目录
static int test_dir(const int8_t *dir);
// 测试目录是否为空
static int is_empdir(const int8_t *dir);
//测试目录合法性
static int check_dir(const int8_t *dir);
//测试文件合法性
static int check_file(const int8_t *dir);

#endif
