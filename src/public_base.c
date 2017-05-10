#include "public_base.h"
#include "log_daemon.h"
#include "socket_server.h"

extern SERVICE service;

void err_que(const char *msg)
{
  puts(msg);
  exit(0);
}

void err_sys(const char *msg)
{
  char event_buff[128];

  memset(event_buff, 0, sizeof(event_buff));
  sprintf(event_buff, "%s:%s", msg, strerror(errno));

  serv_log_write(event_buff, 1);

  exit(errno);
}

void err_mysql(const char *msg)
{
  int res;
  char event_buff[128];

  memset(event_buff, 0, sizeof(event_buff));
  sprintf(event_buff, "%s:%s", msg, mysql_error(&service.serv_mysql));

  serv_log_write(event_buff, 2);
  res = mysql_errno(&service.serv_mysql);

  exit(res);
}

int sys_err_log(const char *msg)
{
  char event_buff[128];

  memset(event_buff, 0, sizeof(event_buff));
  sprintf(event_buff, "%s:%s", msg, strerror(errno));

  serv_log_write(event_buff, 1);

  return -1;
}
int pth_sys_err_log(const char *msg, int errn)
{
  char event_buff[128];

  memset(event_buff, 0, sizeof(event_buff));
  sprintf(event_buff, "%s:%s", msg, strerror(errn));

  serv_log_write(event_buff, 1);

  return -1;
}
int mysql_err_log(const char *msg)
{
  char event_buff[128];

  memset(event_buff, 0, sizeof(event_buff));
  sprintf(event_buff, "%s:%s", msg, mysql_error(&service.serv_mysql));

  serv_log_write(event_buff, 2);

  return -1;
}
void sig_porc_stop(int signo)
{
    PEVENT_STRUCT *plist, cur_event;
    char ip_addr[INET_ADDRSTRLEN];

    /*
    *first close all socket from client
    */
    // close socket in epoll from server
    if(epoll_ctl(service.serv_epoll, EPOLL_CTL_DEL, service.serv_sock, NULL) == -1)
      err_sys("epoll_ctl delete error");

    close(service.serv_sock);

    // close all socket in epoll from client
    serv_log_write("server socket listen stop", 3);

    pthread_mutex_lock(&service.serv_event.e_lock);
    plist = &service.serv_event.e_list;
    // free malloc memory
    while(*plist != NULL)
    {
      cur_event = *plist;
      *plist = (*plist)->st_next;

      sig_clean_node(cur_event);
    }
    pthread_mutex_unlock(&service.serv_event.e_lock);

    // 关闭中转线程
    if(epoll_ctl(service.serv_epoll, EPOLL_CTL_DEL, service.serv_trans, NULL) == -1)
      err_sys("epoll_ctl delete error");
    close(service.serv_trans);

    // 关闭线程池
    close(service.serv_epoll);

    serv_log_write("all socket was close", 3);

    // clear event list
    plist = service.serv_event.e_list;
    pthread_mutex_lock(&service.serv_event.e_lock);
    if(service.serv_event.e_list != NULL)
      free(service.serv_event.e_list);
    pthread_mutex_unlock(&service.serv_event.e_lock);
    serv_log_write("event link clear all", 1);

    /*
    * close mysql event
    */
    mysql_close(&service.serv_mysql);
    serv_log_write("mysql close connect sucess", 2);

    /*
    * close log file
    */
    close(service.serv_log);
    exit(0);
}

int serv_get_nowtime(char *buff)
{
  struct timespec time_now;
  struct tm       *local_time;

  if(buff == NULL)
    return -1;

  if(clock_gettime( CLOCK_REALTIME, &time_now) == -1)
      return -1;

  local_time =localtime(&time_now.tv_sec);

  sprintf(buff, "%04d-%02d-%02d %02d:%02d:%02d",local_time->tm_year+1900,local_time->tm_mon + 1, local_time->tm_mday,
                                                local_time->tm_hour, local_time->tm_min, local_time->tm_sec);
}

static int sig_clean_node(PEVENT_STRUCT pevent)
{
  PEVENT_STRUCT pfont;
  char          ip_addr[INET_ADDRSTRLEN];
  char          logout_msg[128];

  if(pevent == NULL)
    return -1;

  if(pevent->st_live != -2 && pevent->st_fd != service.serv_trans && pevent->st_fd != service.serv_sock)
  {
    if(epoll_ctl(service.serv_epoll, EPOLL_CTL_DEL, pevent->st_fd, NULL) == -1)
      return sys_err_log("cilent socket remove on the event was failth");
  }

  if(pevent->st_fd != service.serv_trans && pevent->st_fd != service.serv_sock)
    close(pevent->st_fd);

  // 在线用户的退出动作
  if(pevent->st_user != 0)
  {
    user_logout_record(pevent->st_user);
    sprintf(logout_msg,"user logout in : %s", inet_ntop(AF_INET,&pevent->st_addr, ip_addr, INET_ADDRSTRLEN));
    insert_mysql_log(pevent->st_user, logout_msg);

    memset(logout_msg, 0, sizeof(logout_msg));
    sprintf(logout_msg, "client leave event listen : %s", inet_ntop(AF_INET,&pevent->st_addr, ip_addr, INET_ADDRSTRLEN));
    serv_log_write(logout_msg, 3);
  }

  // free memory
  free(pevent);

  return 0;
}
