#include "socket_server.h"
#include "log_daemon.h"
#include "mysql_talk.h"
#include "scmessctl.h"

extern SERVICE    service;

int socket_init()
{
  int                 sockfd;
  int                 errinfo;
  int                 sock_on;
  char                msgbuff[100];
  struct addrinfo     iniaddr, *resaddr, *headaddr;

  memset(&iniaddr, 0, sizeof(iniaddr));
  iniaddr.ai_flags = AI_PASSIVE;
  iniaddr.ai_family = AF_INET;
  iniaddr.ai_protocol=IPPROTO_TCP;
  iniaddr.ai_socktype = SOCK_STREAM;

  memset(msgbuff, 0, sizeof(msgbuff));

  if(errinfo = getaddrinfo(NULL, SOCK_PORT, &iniaddr, &resaddr))
  {
    sprintf(msgbuff, "getaddrinfo error : %s", gai_strerror(errinfo));
    serv_log_write(msgbuff, 3);
    return -1;
  }

  headaddr = resaddr;

  for(; resaddr != NULL; resaddr=resaddr->ai_next)
  {
    if((sockfd = socket(resaddr->ai_family, resaddr->ai_socktype, resaddr->ai_protocol)) == -1)
      continue;

    sock_on = 1;
    if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &sock_on, sizeof(sock_on)) == -1)
      continue;
    if(bind(sockfd, resaddr->ai_addr, resaddr->ai_addrlen) == -1)
      continue;
    break;
  }

  if(resaddr == NULL)
  {
    freeaddrinfo(headaddr);
    return sys_err_log("socket bind error");
  }

  if(listen(sockfd, 5) == -1)
    return sys_err_log("listen error");

  service.serv_sock = sockfd;

  serv_log_write("socket bind sucess", 1);

  return 0;
}
void server_listen()
{
  struct epoll_event    event_listen, res_listen[100];
  PEVENT_STRUCT         levent_content;
  int                   res_count;
  int                   i;

  if(listen(service.serv_sock, 10) == -1)
    err_sys("listen error");

  levent_content = (PEVENT_STRUCT)malloc(sizeof(EVENT_STRUCT));

  memset(levent_content, 0, sizeof(EVENT_STRUCT));
  levent_content->st_fd   = service.serv_sock;
  levent_content->st_user = 0;
  levent_content->st_time = time(NULL);
  pthread_mutex_init(&levent_content->st_mutex, NULL);

  event_listen.events = EPOLLIN | EPOLLERR;
  event_listen.data.ptr = levent_content;

  pthread_mutex_lock(&service.serv_event.e_lock);
  levent_content->st_next = service.serv_event.e_list;
  service.serv_event.e_list = levent_content;
  pthread_mutex_unlock(&service.serv_event.e_lock);

  if(epoll_ctl(service.serv_epoll,EPOLL_CTL_ADD, service.serv_sock, &event_listen) == -1)
    err_sys("service socket ctl error");

  serv_log_write("service epoll add server socket sucess", 3);

  while(1)
  {
      res_count = epoll_wait(service.serv_epoll, res_listen, 100, 5000);

      for(i = 0; i < res_count; i++)
      {
        // printf("%s\n", "消息产生");
        PEVENT_STRUCT pevent = (PEVENT_STRUCT)res_listen[i].data.ptr;

        if(pevent->st_fd == service.serv_sock) // 新的连接请求到达
        {
          // printf("%s\n", "服务器消息");
          if(res_listen[i].events & EPOLLERR)
          {
            sys_err_log("server socket have a error event has found");
            sig_porc_stop();
          }

          if(res_listen[i].events & EPOLLIN)
            server_accept();
        }
        else if(pevent->st_fd == service.serv_trans)  // 中转服务器事件
        {
          // 中转线程到达的事件都是错误事件
          sig_porc_stop();
        }
        else                                            // 客户端连接事件
        {
          if(res_listen[i].events & EPOLLERR)
          {
            // printf("%s\n", "客户端错误产生");
            insert_mysql_log(pevent->st_user, "client error from a error message");
            delete_node(pevent);
          }
          if(res_listen[i].events & EPOLLIN)
          {
            // printf("%s\n", "客户请求消息到达");
            client_request(pevent);
          }
        }
      }

      if(service.serv_event.e_flags == 1)
        service.serv_event.e_flags = 2;
      // printf("%s\n", "hello world");
      // show_message_link();
  }
}
int server_accept()
{
  int                 clsockfd, claddr_len;
  struct sockaddr_in  client_sock;
  struct epoll_event  client_event;
  PEVENT_STRUCT       client_msg;
  char                msgbuff[128];
  char                client_ip[INET_ADDRSTRLEN];
  PEVENT_STRUCT       test_p;

  claddr_len = sizeof(struct sockaddr_in);

  // accept client request from connect
  if((clsockfd = accept(service.serv_sock, (struct sockaddr *)&client_sock, &claddr_len)) == -1)
  {
    close(clsockfd);
    return sys_err_log("accept error from server");
  }

  //serv_log_write("accept client connect for server");

  client_msg = (PEVENT_STRUCT)malloc(sizeof(EVENT_STRUCT));

  memset(client_msg, 0, sizeof(EVENT_STRUCT));
  client_msg->st_fd   = clsockfd;
  client_msg->st_addr = client_sock.sin_addr.s_addr;
  pthread_mutex_init(&client_msg->st_mutex, NULL);
  client_msg->st_live = 1;

  client_event.events   = EPOLLIN | EPOLLERR;   // listen event from client;
  client_event.data.ptr = client_msg;

  if(epoll_ctl(service.serv_epoll, EPOLL_CTL_ADD, clsockfd, &client_event) == -1)
  {
    close(clsockfd);
    free(client_msg);
    return sys_err_log("epoll_ctl error from add client socket");
  }

  // add event list
  pthread_mutex_lock(&service.serv_event.e_lock);
  client_msg->st_next = service.serv_event.e_list;
  service.serv_event.e_list  = client_msg;
  pthread_mutex_unlock(&service.serv_event.e_lock);

  // write message for log
  memset(msgbuff, 0, sizeof(msgbuff));
  sprintf(msgbuff, "client add event listen: %s",
                  inet_ntop(AF_INET, &client_msg->st_addr, client_ip, INET_ADDRSTRLEN));
  serv_log_write(msgbuff, 3);

  return 0;
}
int delete_node(PEVENT_STRUCT pevent)
{
  PEVENT_STRUCT pfont;
  char          ip_addr[INET_ADDRSTRLEN];
  char          logout_msg[128];

  if(pevent == NULL)
    return -1;

   // printf("%s\n", "delete_node");
  // printf("%s\n", "close epoll");
  // remove it from event_link;
  // printf("user logout in : %s", inet_ntop(AF_INET,&pevent->st_addr, ip_addr, INET_ADDRSTRLEN));

  //
  if(pevent->st_fd == service.serv_sock || pevent->st_fd == service.serv_trans)
    return -1;

  if(pevent->st_live != -2)
  {
    if(epoll_ctl(service.serv_epoll, EPOLL_CTL_DEL, pevent->st_fd, NULL) == -1)
      return sys_err_log("cilent socket remove on the event was failth");

    // printf("%s\n", "close fd");
    // close it
  }

  close(pevent->st_fd);
  // 在线用户的退出动作
  if(pevent->st_user != 0)
  {
    // printf("%s\n", "用户退出");
    user_logout_record(pevent->st_user);
    sprintf(logout_msg,"user logout in : %s", inet_ntop(AF_INET,&pevent->st_addr, ip_addr, INET_ADDRSTRLEN));
    insert_mysql_log(pevent->st_user, logout_msg);

    memset(logout_msg, 0, sizeof(logout_msg));
    sprintf(logout_msg, "client leave event listen : %s", inet_ntop(AF_INET,&pevent->st_addr, ip_addr, INET_ADDRSTRLEN));
    serv_log_write(logout_msg, 3);
  }

  // printf("%s\n", "free malloc");
  // free memory

  pthread_mutex_lock(&service.serv_event.e_lock);

  if(pevent == service.serv_event.e_list)
  {
    // printf("%s\n", "1");
    service.serv_event.e_list = pevent->st_next;
    free(pevent);
  }
  else
  {
    // printf("%s\n", "2");
    pfont = service.serv_event.e_list;

    while(pfont != NULL && pfont->st_next != pevent)
      pfont = pfont->st_next;

    if(pfont->st_next == pevent)
    {
      pfont->st_next = pevent->st_next;
      free(pevent);
    }
  }
  pthread_mutex_unlock(&service.serv_event.e_lock);
  // printf("%s\n", "commit");
  return 0;
}
int client_request(PEVENT_STRUCT client_event)
{
  int recv_len;
  CLI_TO_SERV client_msg;

  //printf("%s\n", "客户端请求消息到达");

  memset(&client_msg, 0, sizeof(client_msg));

  if((recv_len = recv(client_event->st_fd, &client_msg, sizeof(client_msg), 0)) == -1)
    return sys_err_log("recv client mesg error");

  // 客户端的非协议的关闭事件
  if(recv_len == 0)
  {
    delete_node(client_event);
    return;
  }


  // 测试存活期
  pthread_mutex_lock(&client_event->st_mutex);
  if(client_event->st_live > 0)
    client_event->st_live = 20;
  else
    return 0;
  pthread_mutex_unlock(&client_event->st_mutex);

  switch (client_msg.cts_events) {
    case CL_LOGIN:      // 登录
      /*
      if(client_event->st_user != 0)
        return 0;
      if(user_login(client_event, &client_msg) == -1)
        delete_node(client_event);
      break;
      */
    case CL_ENROLL:     // 注册
      // user_register(client_event, &client_msg);
      // delete_node(client_event);
      // break;
    case CL_CDDIR:      // 进入目录
      // user_change_dir(client_event, &client_msg, 1);
      // break;
    case CL_EXDIR:      // 退出目录
      // user_change_dir(client_event, &client_msg, 2);
      // break;
    case CL_CRDIR:      // 创建目录
      // user_create_dir(client_event, &client_msg);
      // break;
    case CL_RMFIL:      // 删除文件
      // user_remove_file(client_event, &client_msg);
      // break;
    case CL_RMDIR:      // 删除目录
      // user_remove_directoy(client_event, &client_msg);
      // break;
    case CL_CPFIL:      // 复制文件
      // user_havey_task(client_event, &client_msg);
      // break;
    case CL_MVFIL:      // 移动文件
      // user_havey_task(client_event, &client_msg);
      // break;
    case CL_SNDFL:      // 上传文件
      // user_havey_task(client_event, &client_msg);
      // break;
    case CL_RCVFL:      // 下载文件
      user_havey_task(client_event, &client_msg);
      break;
    default:            // 意外接受类型
      delete_node(client_event);
  }

  return 0;
}
void show_message_link()
{
  PEVENT_STRUCT         test_p;
  char                  ip_addr[INET_ADDRSTRLEN];
  // ----------------测试---------------------

  test_p = service.serv_event.e_list;

  while(test_p != NULL)
  {
    puts("----------------------------------------------------------");
    printf("ip     : %s\n", inet_ntop(AF_INET, &test_p->st_addr, ip_addr, INET_ADDRSTRLEN));
    printf("dir    : %s\n", test_p->st_dir);
    printf("live   : %d\n", test_p->st_live);
    printf("user   : %d\n", test_p->st_user);
    puts("----------------------------------------------------------");
    test_p = test_p->st_next;
  }
}
