#include "pthread_pool.h"
#include "file_msg.h"
#include "scmessctl.h"

static pthread_mutex_t pipe_mutex = PTHREAD_MUTEX_INITIALIZER;
extern SERVICE    service;


void* trans_server(void * args)
{
  int                 epoll_fd;                     // 事件监听池
  int                 main_fd;                      // 中转服务线程与主线程的通信描述符
  int                 trans_fd;                     // 中转服务接受任务处理线程消息描述符
  int                 serv_pipe[2];                 // 任务处理线程与中转服务线程交互管道[任务处理线程->中转服务线程]

  CELLMSG             cell_info[POOL_COUNT];        // 任务处理线程信息表

  THREADMSG           main_recv;                    // 从主线程接受消息
  uint8_t             main_send;                    // 发送给主线程的状态码
  THREADMSG           cell_send;                    // 发送给任务处理线程事件
  RECVFLAG            cell_recv;                    // 从任务处理队列接受的状态码
  SERV_TO_CLI         client_send;                  // 发送给客户端事件

  struct epoll_event  listen_event[POOL_COUNT];     // 中转线程池监听事件
  struct epoll_event  main_event;                   // 主线程客户事件

  int                 event_count;
  int                 i, index, x;
  int8_t              test_msg[128];
  int                 recv_len;

  /*
  *    接受主线程消息测试准确性
  */
  // printf("%s\n", "transform thread create");

  main_fd = (int)args;

  // 中转线程初始化
  if(trans_server_init(main_fd, &trans_fd, cell_info, &epoll_fd, serv_pipe) == -1)
  {
    close(main_fd);

    if(send(main_fd, "error.", strlen("error."), 0) == -1)
      return sys_err_log("send error when transform pthread send to main pthread");

    return NULL;
  }

  if(send(main_fd, "imok.", strlen("imok."), 0) == -1)
  {
    sys_err_log("send error when transform pthread send to main pthread");

    for(x = 0; x < POOL_COUNT; x++)
      close(cell_info[x].cell_pipe);

    close(epoll_fd);
    close(main_fd);

    return NULL;
  }

  /*
  *   任务处理
  */
  // write(cell_fd[2], "hello world", strlen("hello world"));

  while(1)
  {
    if((event_count = epoll_wait(epoll_fd, listen_event, POOL_COUNT, 5000)) == -1)
    {
      sys_err_log("epoll_wait error in transform server");

      for(x = 0; x < POOL_COUNT; x++)
        close(cell_info[x].cell_pipe);

      close(epoll_fd);
      close(main_fd);

      return NULL;
    }


    for(i = 0; i < event_count; i++)
    {
      if(listen_event[i].data.fd == main_fd)        // 主线程事件
      {
        if(listen_event[i].events & EPOLLERR)
        {}
        else if(listen_event[i].events & EPOLLIN)
        {
          memset(&main_recv, 0, sizeof(main_recv));

          recv_len = recv(main_fd, &main_recv, sizeof(main_recv), 0);

          if(recv_len == -1)  //接受主线程信息错误
          {
            for(x = 0; x < POOL_COUNT; x++)
              close(cell_info[x].cell_pipe);
            close(epoll_fd);
            close(main_fd);
            return NULL;
          }

          // printf("%s\n", "主线程提交任务");

          // 服务器关闭套接字事件
          if(recv_len == 0)
          {
            // 服务器关闭引起
            for(x = 0; x < POOL_COUNT; x++)
              close(cell_info[x].cell_pipe);
            close(epoll_fd);
            close(main_fd);
            return NULL;
          }

          if(main_recv.pevent->st_fd == 0)
            continue;

          for(i = 0; i < POOL_COUNT; i++)
          {
            // 从任务线程池中查找空闲的线程
            if(cell_info[i].cell_flags == 0)
            {
              index = i;
              break;
            }
          }
          // 线程池忙
          if(i == POOL_COUNT)
          {
            memset(&client_send, 0, sizeof(client_send));
            client_send.stc_events = SE_FAILE;
            sprintf(client_send.stc_main, get_client_dir(main_recv.pevent));
            if(send(main_recv.pevent->st_fd, &client_send, sizeof(client_send), 0) == -1)
              delete_node(main_recv.pevent);
            continue;
          }

          // 处理用户在监听池中的摘除
          if(epoll_ctl(service.serv_epoll, EPOLL_CTL_DEL, main_recv.pevent->st_fd, NULL) == -1)
          {
            sys_err_log("epoll_ctl error in transform server when EPOLL_CTL_DEL");

            memset(&client_send, 0, sizeof(client_send));

            client_send.stc_events = SE_FAILE;

            sprintf(client_send.stc_main, get_client_dir(main_recv.pevent));
            if(send(main_recv.pevent->st_fd, &client_send, sizeof(client_send), 0) == -1)
            {
              delete_node(main_recv.pevent);
              continue;
            }

            continue;
          }

          // 重置生存期
          pthread_mutex_lock(&main_recv.pevent->st_mutex);
          if(main_recv.pevent->st_live == 0)
          {
            pthread_mutex_unlock(&main_recv.pevent->st_mutex);
            continue;
          }
          // printf("%s\n", "hello one");
          main_recv.pevent->st_live = -2;
          pthread_mutex_unlock(&main_recv.pevent->st_mutex);

          /*
          memset(&client_send, 0, sizeof(client_send));
          client_send.stc_events = SE_OK;
          strncpy(client_send.stc_main, get_client_dir(main_recv.pevent), sizeof(client_send.stc_main));
          if(send(main_recv.pevent->st_fd, &client_send, sizeof(client_send), 0) == -1)
            sys("send to client error when pool full");
          */

          // 中转线程处理工作
          cell_info[index].cell_flags = 1;                    // 设置其为工作状态
          cell_info[index].cell_pevent = main_recv.pevent;    // 信息保留

          // 向任务处理线程提交任务
          if(write(cell_info[index].cell_pipe, &main_recv, sizeof(main_recv)) == -1)
            err_sys("write erro to cell thread in epoll");
        }
      }
      else if(listen_event[i].data.fd == trans_fd)   // 任务处理线程事件
      {
        // 中转线程管道通信发生错误
        if(listen_event[i].events & EPOLLERR)
        {

        }
        else if(listen_event[i].events & EPOLLIN)
        {
          //printf("%s\n", "发生读事件");

          // 通信管道发生错误
          if((recv_len = read(trans_fd, &cell_recv, sizeof(cell_recv))) == -1)
          {
            for(x = 0; x < POOL_COUNT; x++)
              close(cell_info[x].cell_pipe);
            close(epoll_fd);
            close(main_fd);
            return NULL;
          }

          //管道被关闭 -- 无法获取是哪个线程的问题
          if(recv_len == 0)
          {
            for(x = 0; x < POOL_COUNT; x++)
              close(cell_info[x].cell_pipe);
            close(epoll_fd);
            close(main_fd);
            return NULL;
          }

          index = cell_recv.recv_mark;  // 获取线程号

          // printf("任务处理线程池中 %d 发生消息 : %d \n", cell_recv.recv_mark, cell_recv.recv_flags);
          /*
          * 任务处理线程成功返回消息
          */
          if(cell_recv.recv_flags == TASK_OK)
          {
            // printf("%s\n", "中转线程成功事件处理");
            // printf("%d 线程成功返回消息\n", cell_recv.recv_mark);
            // 将客户端重新加入监听
            if(cell_info[index].cell_flags != 1 || cell_info[index].cell_pevent == NULL)
            {
              cell_info[index].cell_pevent = NULL;
              cell_info[index].cell_flags = 0;
              continue;
            }

            memset(&main_event, 0, sizeof(main_event));
            main_event.events = EPOLLIN | EPOLLERR;
            main_event.data.ptr = cell_info[index].cell_pevent;

            if(epoll_ctl(service.serv_epoll, EPOLL_CTL_ADD, cell_info[index].cell_pevent->st_fd, &main_event) == -1)
            {
              sys_err_log("epoll_ctl error from add client listener when after task");
              close(cell_info[index].cell_pevent->st_fd);
              // 置回工作线程状态
              cell_info[index].cell_pevent = NULL;
              cell_info[index].cell_flags = 0;
              continue;
            }

            // 重置客户端的生存期
            pthread_mutex_lock(&cell_info[index].cell_pevent->st_mutex);
            cell_info[index].cell_pevent->st_live = 20;
            pthread_mutex_unlock(&cell_info[index].cell_pevent->st_mutex);

            // 向客户端发送成功消息
            /*
            memset(&client_send, 0, sizeof(client_send));
            client_send.stc_events = SE_OK;
            strncpy(client_send.stc_main, get_client_dir(cell_info[index].cell_pevent), MAX_DNAM_LEN);
            if(send(cell_info[index].cell_pevent->st_fd, &client_send, sizeof(client_send), 0) == -1)
              sys_err_log("send to client socket error in transform server");
            */

            // 置回工作线程状态
            cell_info[index].cell_pevent = NULL;
            cell_info[index].cell_flags = 0;
          }
          else if(cell_recv.recv_flags == TASK_FALTH )
          {

            // printf("%s\n", "中转线程错误事件处理");
            // printf("%d 线程成功返回消息\n", cell_recv.recv_mark);
            // 将客户端重新加入监听
            if(cell_info[index].cell_flags != 1 || cell_info[index].cell_pevent == NULL)
            {
              // printf("%s\n", "置回");
              cell_info[index].cell_pevent = NULL;
              cell_info[index].cell_flags = 0;
              continue;
            }

            memset(&main_event, 0, sizeof(main_event));
            main_event.events = EPOLLIN | EPOLLERR;
            main_event.data.ptr = cell_info[index].cell_pevent;

            if(epoll_ctl(service.serv_epoll, EPOLL_CTL_ADD, cell_info[index].cell_pevent->st_fd, &main_event) == -1)
            {
              sys_err_log("epoll_ctl error from add client listener when after task");
              close(cell_info[index].cell_pevent->st_fd);
              // 置回工作线程状态
              cell_info[index].cell_pevent = NULL;
              cell_info[index].cell_flags = 0;
              continue;
            }

            // 重置客户端的生存期
            pthread_mutex_lock(&cell_info[index].cell_pevent->st_mutex);
            cell_info[index].cell_pevent->st_live = 20;
            pthread_mutex_unlock(&cell_info[index].cell_pevent->st_mutex);

            /*
            // 向客户端发送失败消息
            memset(&client_send, 0, sizeof(client_send));
            client_send.stc_events = SE_FAILE;
            strncpy(client_send.stc_main, get_client_dir(cell_info[index].cell_pevent), MAX_DNAM_LEN);

            // if(send(cell_info[index].cell_pevent->st_fd, &client_send, sizeof(client_send), 0) == -1)
            //   sys_err_log("send to client socket error in transform server");
            */
            // 置回工作线程状态
            cell_info[index].cell_pevent = NULL;
            cell_info[index].cell_flags = 0;
          }
          else if(cell_recv.recv_flags == TASK_LOGOUT)
          {
            delete_node(cell_info[index].cell_pevent);

            cell_info[index].cell_pevent = NULL;
            cell_info[index].cell_flags = 0;
            // 将客户端下线
          }
          else if(cell_recv.recv_flags == TASK_ERROR)
          {
            /*
            * 任务处理线程错误
            */
            if(cell_info[index].cell_flags != 1 || cell_info[index].cell_pevent == NULL)
            {
              cell_info[index].cell_pevent = NULL;
              cell_info[index].cell_flags = 0;
              continue;
            }

            memset(&main_event, 0, sizeof(main_event));
            main_event.events = EPOLLIN | EPOLLERR;
            main_event.data.ptr = cell_info[index].cell_pevent;

            if(epoll_ctl(service.serv_epoll, EPOLL_CTL_ADD, cell_info[index].cell_pevent->st_fd, &main_event) == -1)
            {
              sys_err_log("epoll_ctl error from add client listener when after task");
              close(cell_info[index].cell_pevent->st_fd);
              // 置回工作线程状态
              cell_info[index].cell_pevent = NULL;
              cell_info[index].cell_flags = 0;
              continue;
            }

            // 重置客户端的生存期
            pthread_mutex_lock(&cell_info[index].cell_pevent->st_mutex);
            cell_info[index].cell_pevent->st_live = 20;
            pthread_mutex_lock(&cell_info[index].cell_pevent->st_mutex);

            // 向客户端发送失败消息
            memset(&client_send, 0, sizeof(client_send));
            client_send.stc_events = SE_FAILE;
            strncpy(client_send.stc_main, get_client_dir(cell_info[index].cell_pevent), MAX_DNAM_LEN);

            if(send(cell_info[index].cell_pevent->st_fd, &client_send, sizeof(client_send), 0) == -1)
            {
              sys_err_log("send to client message error when cell thread error");
              delete_node(cell_info[index].cell_pevent);
            }
            // 重启一个线程
            close(cell_info[index].cell_pipe);
            if(trans_server_pthread_create(serv_pipe, &cell_info[index].cell_pipe, index) == -1)
            {
              for(x = 0; x < POOL_COUNT; x++)
                close(cell_info[x].cell_pipe);
              close(epoll_fd);
              close(main_fd);
              return NULL;
            }
          }
        }
      }
    }
    // printf("%s\n", "中转线程处理完任务");
  }

  return NULL;
}

static int trans_server_init(int main_fd, int *transfd, PCELLMSG cellmsg, int *epoll_fd, int *serv_pipe)
{
  int                 i;
  int                 cell_pipe[POOL_COUNT][2]; // 任务处理线程与中转服务线程交互管道[中转服务线程->任务处理线程]
  uint8_t             test_msg[20];             // 接受服务器的调试事件
  struct epoll_event  pool_event;

  /*
  * 接受服务器消息并测试
  */
  memset(test_msg, 0, sizeof(test_msg));
  if(recv(main_fd, test_msg, sizeof(test_msg), 0) == -1)
    return sys_err_log("recv error when transform pthread recv from main pthread");

  // printf("transform recv from main thread in init :%s\n", test_msg);
  if(strcmp(test_msg, "ruok?"))
    return -1;

  // printf("%s\n", "pipe init");
  /*
  *   初始化与任务处理线程的通信管道
  */
  if(pipe(serv_pipe) == -1)
    return sys_err_log("pipe error when service create transform pthread");

  *transfd = serv_pipe[0];

  // 初始化线程池
  for(i = 0; i < POOL_COUNT; i++)
  {
    if(trans_server_pthread_create(serv_pipe, &cellmsg[i].cell_pipe, i) == -1)
      return sys_err_log("epoll_create error in trans_server_init");
  }

  if((*epoll_fd = epoll_create(10)) == -1)
    return sys_err_log("epoll_create error in trans_server_init");

  /*
  * 添加主线程的事件
  */
  memset(&pool_event, 0, sizeof(pool_event));
  pool_event.events = EPOLLIN | EPOLLERR;
  pool_event.data.fd = main_fd;

  if(epoll_ctl(*epoll_fd, EPOLL_CTL_ADD, main_fd, &pool_event) == -1)
    return sys_err_log("epoll_ctl error in trans_server_init");

  /*
  * 添加任务处理线程的消息回收事件
  */
  memset(&pool_event, 0, sizeof(pool_event));
  pool_event.events = EPOLLIN | EPOLLERR;
  pool_event.data.fd = serv_pipe[0];

  if(epoll_ctl(*epoll_fd, EPOLL_CTL_ADD, serv_pipe[0], &pool_event) == -1)
    return sys_err_log("epoll_ctl error in trans_server_init");

  return 0;
}

static int trans_server_pthread_create(int* trans_fd, int *cell_pipe_read, int i)
{
  int                 pth_err;
  int                 cell_pipe[2];
  uint8_t             test_msg[20];             // 接受服务器的调试事件
  FIFOMSG             fifo_msg;                 // 管道信息
  pthread_t           pth_pool_fd;
  pthread_attr_t      pth_pool_attr;

  /*
  *   建立通信管道
  */
  if(pipe(cell_pipe) == -1)
    return sys_err_log("pipe error when service create transform pthread");

  /*
  * 任务线程初始化
  */
  // printf("%s\n", "cell thread init");
  pthread_attr_init(&pth_pool_attr);
  pthread_attr_setdetachstate(&pth_pool_attr, PTHREAD_CREATE_DETACHED);

  //printf("%s\n", "hello world");
  memset(test_msg, 0, sizeof(test_msg));
  memset(&fifo_msg, 0, sizeof(fifo_msg));

  fifo_msg.pipe_read = cell_pipe[0];       // 任务处理线程的消息读取
  fifo_msg.pipe_write = trans_fd[1];       // 任务处理线程的消息写入
  fifo_msg.pipe_id  = i;                   // 任务处理线程的身份表示

  if(pth_err = pthread_create(&pth_pool_fd, &pth_pool_attr, task_cell, (void *)&fifo_msg))
    return pth_sys_err_log("pthread_create error when service create transform pthread", pth_err);

  // printf("%s:%d\n", "pthread create", i);

  if(write(cell_pipe[1], "ruok?", strlen("ruok?")) == -1)
    return sys_err_log("write error when service init send message to cell pthread pool");

  if(read(trans_fd[0], test_msg, sizeof(test_msg)) == -1)
    return sys_err_log("read error when service ipth_errnit recv message from cell pthread pool");

  // printf("%s\n", test_msg);

  if(strcmp(test_msg, "imok."))
    return -1;

  *cell_pipe_read = cell_pipe[1];

  pthread_attr_destroy(&pth_pool_attr);

  return 0;
}

void* task_cell(void * args)
{
  FIFOMSG     fifo_msg;           // 任务处理线程信息
  THREADMSG   cell_recv;
  int         recv_len;
  int         res_state;
  int8_t      log_msg[128];
  /*
  * 获取通信管道
  */

  fifo_msg.pipe_read = ((PFIFOMSG)args)->pipe_read;
  fifo_msg.pipe_write = ((PFIFOMSG)args)->pipe_write;
  fifo_msg.pipe_id = ((PFIFOMSG)args)->pipe_id;
  /*
  *   通信测试
  */

  // printf("pthread test %d\n", fifo_msg.pipe_id);
  if(task_cell_init(fifo_msg.pipe_read, fifo_msg.pipe_write) == -1)
    return NULL;
  // printf("cell thread create sucess : %d\n", fifo_msg.pipe_id);
  /*
  *   任务处理
  */
  while(1)
  {
    memset(log_msg, 0, sizeof(log_msg));

    if((recv_len = read(fifo_msg.pipe_read, &cell_recv, sizeof(cell_recv))) == -1)
        err_sys("read error");

    if(recv_len == -1)
    {
      close(fifo_msg.pipe_read);
      if(task_send_msg(fifo_msg.pipe_write, fifo_msg.pipe_id, TASK_FALTH) == -1)
        return NULL;
      return NULL;
    }
    else if(recv_len == 0)
    {
      // 服务器的关闭事件
      close(fifo_msg.pipe_read);
      return NULL;
    }

    // printf("线程池 %d 号线程获取任务\n", fifo_msg.pipe_id);
    // write(STDOUT_FILENO, log_msg, recv_len);

    switch (cell_recv.climsg.cts_events)
    {
      case CL_LOGIN:      // 登录
        if(cell_recv.pevent->st_user != 0)
          task_send_msg(fifo_msg.pipe_write, fifo_msg.pipe_id, TASK_LOGOUT);

        if(user_login(cell_recv.pevent, &cell_recv.climsg) != 0)
          task_send_msg(fifo_msg.pipe_write, fifo_msg.pipe_id, TASK_LOGOUT);
        else
          task_send_msg(fifo_msg.pipe_write, fifo_msg.pipe_id, TASK_OK);
        break;
      case CL_ENROLL:     // 注册
          user_register(cell_recv.pevent, &cell_recv.climsg);
          task_send_msg(fifo_msg.pipe_write, fifo_msg.pipe_id, TASK_LOGOUT);
          break;
      case CL_CDDIR:      // 进入目录
        res_state = user_change_dir(cell_recv.pevent, &cell_recv.climsg, 1);
        if(res_state == -1)
          task_send_msg(fifo_msg.pipe_write, fifo_msg.pipe_id, TASK_FALTH);
        else if(res_state == -2)
          task_send_msg(fifo_msg.pipe_write, fifo_msg.pipe_id, TASK_LOGOUT);
        else
          task_send_msg(fifo_msg.pipe_write, fifo_msg.pipe_id, TASK_OK);
          break;
      case CL_EXDIR:      // 退出目录
        res_state = user_change_dir(cell_recv.pevent, &cell_recv.climsg, 2);
        if(res_state == -1)
          task_send_msg(fifo_msg.pipe_write, fifo_msg.pipe_id, TASK_FALTH);
        else if(res_state == -2)
          task_send_msg(fifo_msg.pipe_write, fifo_msg.pipe_id, TASK_LOGOUT);
        else
          task_send_msg(fifo_msg.pipe_write, fifo_msg.pipe_id, TASK_OK);
          break;
      case CL_CRDIR:      // 创建目录
        res_state = user_create_dir(cell_recv.pevent, &cell_recv.climsg);
        if(res_state == -1)
          task_send_msg(fifo_msg.pipe_write, fifo_msg.pipe_id, TASK_FALTH);
        else if(res_state == -2)
          task_send_msg(fifo_msg.pipe_write, fifo_msg.pipe_id, TASK_LOGOUT);
        else
          task_send_msg(fifo_msg.pipe_write, fifo_msg.pipe_id, TASK_OK);
          break;
      case CL_RMFIL:      // 删除文件
        res_state = user_remove_file(cell_recv.pevent, &cell_recv.climsg);
        if(res_state == -1)
          task_send_msg(fifo_msg.pipe_write, fifo_msg.pipe_id, TASK_FALTH);
        else if(res_state == -2)
          task_send_msg(fifo_msg.pipe_write, fifo_msg.pipe_id, TASK_LOGOUT);
        else
          task_send_msg(fifo_msg.pipe_write, fifo_msg.pipe_id, TASK_OK);
          break;
      case CL_RMDIR:      // 删除目录
        res_state = user_remove_directoy(cell_recv.pevent, &cell_recv.climsg);
        if(res_state == -1)
          task_send_msg(fifo_msg.pipe_write, fifo_msg.pipe_id, TASK_FALTH);
        else if(res_state == -2)
          task_send_msg(fifo_msg.pipe_write, fifo_msg.pipe_id, TASK_LOGOUT);
        else
          task_send_msg(fifo_msg.pipe_write, fifo_msg.pipe_id, TASK_OK);
          break;
      case CL_CPFIL:      // 复制文件
        res_state = user_copy_file(cell_recv.pevent, &cell_recv.climsg);
        if(res_state == -1)
          task_send_msg(fifo_msg.pipe_write, fifo_msg.pipe_id, TASK_FALTH);
        else if(res_state == -2)
          task_send_msg(fifo_msg.pipe_write, fifo_msg.pipe_id, TASK_LOGOUT);
        else
          task_send_msg(fifo_msg.pipe_write, fifo_msg.pipe_id, TASK_OK);
        break;
      case CL_MVFIL:      // 移动文件
        res_state = user_move_file(cell_recv.pevent, &cell_recv.climsg);
        if(res_state == -1)
          task_send_msg(fifo_msg.pipe_write, fifo_msg.pipe_id, TASK_FALTH);
        else if(res_state == -2)
          task_send_msg(fifo_msg.pipe_write, fifo_msg.pipe_id, TASK_LOGOUT);
        else
          task_send_msg(fifo_msg.pipe_write, fifo_msg.pipe_id, TASK_OK);
        break;
      case CL_SNDFL:      // 上传文件
        res_state = user_trans_upload(cell_recv.pevent, &cell_recv.climsg);
        if(res_state == -1)
          task_send_msg(fifo_msg.pipe_write, fifo_msg.pipe_id, TASK_FALTH);
        else if(res_state == -2)
          task_send_msg(fifo_msg.pipe_write, fifo_msg.pipe_id, TASK_LOGOUT);
        else
          task_send_msg(fifo_msg.pipe_write, fifo_msg.pipe_id, TASK_OK);
        break;
      case CL_RCVFL:      // 下载文件
        res_state = user_trans_download(cell_recv.pevent, &cell_recv.climsg);
        if(res_state == -1)
          task_send_msg(fifo_msg.pipe_write, fifo_msg.pipe_id, TASK_FALTH);
        else if(res_state == -2)
          task_send_msg(fifo_msg.pipe_write, fifo_msg.pipe_id, TASK_LOGOUT);
        else
          task_send_msg(fifo_msg.pipe_write, fifo_msg.pipe_id, TASK_OK);
        break;
      default:            // 意外接受类型
        task_send_msg(fifo_msg.pipe_write, fifo_msg.pipe_id, TASK_FALTH);
          return NULL;
    }

  }

  return NULL;
}

static int task_cell_init(int readfd, int writefd)
{
  int8_t      test_msg[20];

  memset(test_msg, 0, sizeof(test_msg));

  if(read(readfd, test_msg, sizeof(test_msg)) == -1)
  {
    pthread_mutex_lock(&pipe_mutex);
    if(write(writefd,"error.",strlen("error.")) == -1)
      return sys_err_log("read error when cell pthread pool init recv msg from transform server");
    pthread_mutex_unlock(&pipe_mutex);

    return sys_err_log("read error when cell pthread pool init recv msg from transform server");
  }

  if(strcmp(test_msg, "ruok?"))
  {
    pthread_mutex_lock(&pipe_mutex);
    if(write(writefd,"error.",strlen("error.")) == -1)
      return sys_err_log("read error when cell pthread pool init recv msg from transform server");
    pthread_mutex_unlock(&pipe_mutex);

    return -1;
  }
  else
  {
    pthread_mutex_lock(&pipe_mutex);
    if(write(writefd,"imok.",strlen("imok.")) == -1)
      return sys_err_log("read error when cell pthread pool init recv msg from transform server");
    pthread_mutex_unlock(&pipe_mutex);
  }

  return 0;
}

static int task_send_msg(int writefd,int index, int8_t flags)
{
  RECVFLAG    cell_send;

  memset(&cell_send, 0, sizeof(cell_send));

  cell_send.recv_mark = index;
  cell_send.recv_flags = flags;

  pthread_mutex_lock(&pipe_mutex);
  if(write(writefd, &cell_send, sizeof(cell_send)) == -1)
      return sys_err_log("write error to transform server in cell task");
  pthread_mutex_unlock(&pipe_mutex);

  return 0;
}
