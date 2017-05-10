#ifndef PTHREAD_POOL_H
#define PTHREAD_POOL_H

#include "service_base.h"
#include "socket_server.h"


#define POOL_COUNT 20       // 线程池的数量
#define TASK_OK     0       // 任务请求成功
#define TASK_FALTH  1       // 任务请求失败
#define TASK_LOGOUT 2       // 用户下线
#define TASK_ERROR  3       // 任务错误

// 线程间的传送消息的实体
typedef struct{
  PEVENT_STRUCT pevent;
  CLI_TO_SERV   climsg;
}THREADMSG, *PTHREADMSG;

// FIFO 信息
typedef struct{
  int   pipe_read;
  int   pipe_write;
  int   pipe_id;
}FIFOMSG, *PFIFOMSG;

// 任务处理线程信息
typedef struct{
  PEVENT_STRUCT cell_pevent;
  int8_t        cell_flags;   //0 free  1 run
  int           cell_pipe;
}CELLMSG, *PCELLMSG;

// 从任务处理线程获取状态消息
typedef struct{
  uint8_t recv_mark;      // 任务处理线程标示
  uint8_t recv_flags;     // 任务处理线程状态返回
}RECVFLAG, *PRECVFLAG;

/*
* 中转服务线程
*/
void* trans_server(void * args);
// 中转服务线程初始化
static int trans_server_init(int main_fd, int *transfd, PCELLMSG cellmsg, int *epoll_fd, int *serv_pipe);
// 开启线程
static int trans_server_pthread_create(int* trans_fd, int *cell_pipe_read, int i);

/*
* 任务处理线程
*/
void* task_cell(void * args);
// 任务处理线程初始化
static int task_cell_init(int readfd, int writefd);
// 发送消息中转线程
static int task_send_msg(int writefd,int index, int8_t flags);
#endif
