#include "base_daemon.h"
#include "pthread_pool.h"
#include "public_base.h"
#include "log_daemon.h"
#include "socket_server.h"

SERVICE service;

int server_init()
{
  int         my_fd;
  struct stat file_mesg;
  int         res;

  // 本地日志初始化
  if(serv_log_init() == -1)
  {
    // printf("%s\n", "service init log falth");
    return -1;
  }

  // 开启守护进程模式
  if(daemon_start() == -1)
    return -1;

  // Mysql 初始化
  if(serv_mysql_init() == -1)
    return -1;

  // 用户主目录测试
  if(stat(ADMIN_HOME, &file_mesg) == -1)
    if(mkdir(ADMIN_HOME, S_IRWXU) == -1)
      return sys_err_log("mkdir error");
  else if(!S_ISDIR(file_mesg.st_mode))
    return -1;

  serv_log_write("User home check finsh", 1);

  // 打开 socket 并进入监听模式
  if(socket_init() == -1)
    return -1;

  // 打开 epoll 池
  if(epoll_init() == -1)
    return -1;

  // 初始化事件链
  list_event();

  // 设置信号处理
  if(signal_init() == -1)
    return -1;

  // 开启线程
  if(pthread_contrl_init() == -1)
    return -1;

  serv_log_write("thread create sucess", 1);

  return 0;
}

static void *user_status_clean(void *args)
{
  struct timeval  time_out;
  PEVENT_STRUCT   list_p;
  int             flags = 0;
  char            logout_msg[128];
  char            ip_addr[INET_ADDRSTRLEN];
  PEVENT_STRUCT   *fpenvent ,pevent, lpevent;


  while(1)
  {
    // printf("%s\n", "==============================");
    // show_message_link();
    if(service.serv_event.e_list == NULL)
      return NULL;

    pthread_mutex_lock(&service.serv_event.e_lock);

    if(service.serv_event.e_flags == 2)
    {
      flags = 1;
      service.serv_event.e_flags = 0;
    }

    fpenvent = &service.serv_event.e_list;

    for(list_p = service.serv_event.e_list; list_p != NULL;)
    {
      pevent = list_p;
      list_p = list_p->st_next;

      if(pevent->st_fd == service.serv_sock
        || pevent->st_fd == service.serv_trans)
        {
            fpenvent = &pevent->st_next;
            continue;
        }

      // 生存期结束， 初步操作
      pthread_mutex_lock(&pevent->st_mutex);
      if(pevent->st_live == 0)              // 预处理
      {
        int st_fd = pevent->st_fd;
        pevent->st_fd   = 0;
        pevent->st_live = -1;

        if(epoll_ctl(service.serv_epoll, EPOLL_CTL_DEL, st_fd, NULL) == -1)
        {
          pthread_mutex_unlock(&pevent->st_mutex);
          err_sys("epoll_ctl error in clean user msg of del");
        }

        close(st_fd);
        // 设置其待处理
        service.serv_event.e_flags = 1;
      }
      else if(pevent->st_live == -1)    // 移除节点
      {
        if(flags == 1)
        {
          // 用户退出事件 mysql 处理
          user_logout_record(pevent->st_user);
          sprintf(logout_msg,"user logout in : %s", inet_ntop(AF_INET,&pevent->st_addr, ip_addr, INET_ADDRSTRLEN));
          insert_mysql_log(pevent->st_user, logout_msg);

          // 事件处理
          memset(logout_msg, 0, sizeof(logout_msg));
          sprintf(logout_msg, "client leave event listen : %s", inet_ntop(AF_INET,&pevent->st_addr, ip_addr, INET_ADDRSTRLEN));
          serv_log_write(logout_msg, 3);

          // 链表处理
          *fpenvent = list_p;

          free(pevent);
          pthread_mutex_unlock(&pevent->st_mutex);
          continue;
        }
      }
      else if(pevent->st_live == -2);    // 暂被移交节点
        //continue;
      else
        pevent->st_live --;

      fpenvent = &pevent->st_next;
      pthread_mutex_unlock(&pevent->st_mutex);
    }

    flags = 0;
    pthread_mutex_unlock(&service.serv_event.e_lock);
    time_out.tv_sec = 2;
    time_out.tv_usec = 0;
    select(0, NULL, NULL, NULL, &time_out);
  }
}
static int daemon_start(void)
{

  int          i,fd0, fd1, fd2;
  int          fd;
	pid_t        pid;
  sigset_t     mask_sig;
	struct       rlimit rl;
	struct       sigaction sa;
  int          daemon_fd;
  struct flock daemon_lock;

	umask(0);

  /*
  *   设置信号屏蔽
  */
  sigemptyset(&mask_sig);
  sigaddset(&mask_sig, SIGINT);
  sigprocmask(SIG_BLOCK, &mask_sig, NULL);

	if(getrlimit(RLIMIT_NOFILE, &rl) < 0)
		return sys_err_log("can not get file limit");

  for(i = 0; i < rl.rlim_max; i++)
  {
    if(i == service.serv_log)
      continue;
    else
      close(i);
  }

	if((pid = fork()) == -1)
		return sys_err_log("fork error");

	else if(pid != 0)
		exit(0);

	setsid();

	sa.sa_handler = SIG_IGN;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;

	if(sigaction(SIGHUP, &sa, NULL) < 0)
		return sys_err_log("sigaction error");


	if((pid = fork()) == -1)
		return sys_err_log("fork error");
	else if(pid != 0)
		exit(0);

  fd = open(FILE_FLAGE, O_WRONLY);

  memset(&daemon_lock, 0, sizeof(daemon_lock));
  daemon_lock.l_type = F_WRLCK;
  daemon_lock.l_whence = SEEK_SET;
  daemon_lock.l_start = 0;
  daemon_lock.l_len = 0;

  if(fcntl(fd, F_SETLK, &daemon_lock) == -1)
  {
    if(errno == EAGAIN)
      return 0;

    return sys_err_log("fcntl F_SETLK error");
  }


	fd0 = open("/dev/null", O_RDWR);
	fd1 = dup(0);
	fd2 = dup(0);

  serv_log_write("service daemon start sussess", 1);

  return 1;
}
static int epoll_init()
{
  int epoll_fd;

  if((epoll_fd = epoll_create(5)) == -1)
    return sys_err_log("epoll_create error");

  service.serv_epoll = epoll_fd;

  serv_log_write("epoll create sucess", 1);
  return 0;
}
static int signal_init()
{
  struct sigaction  act;
  sigset_t          unmask;

  memset(&act, 0, sizeof(struct sigaction));
  act.sa_handler = sig_porc_stop;
  sigemptyset(&act.sa_mask);
  sigaddset(&act.sa_mask, SIGINT);
  act.sa_flags |= SA_RESTART;

  if(sigaction(SIGINT, &act, NULL) == -1)
    return sys_err_log("sigaction error in signal_init");

  sigemptyset(&unmask);
  sigaddset(&unmask, SIGINT);

  if(sigprocmask(SIG_UNBLOCK, &unmask, NULL) == -1)
    return sys_err_log("sigprocmask error in signal_init");

  return 0;
}
static void list_event()
{
  service.serv_event.e_flags = 0;
  pthread_mutex_init(&service.serv_event.e_lock, NULL);
  service.serv_event.e_list = NULL;
}
static int pthread_contrl_init()
{
  int                   pth_status;
  struct epoll_event    event_listen;
  PEVENT_STRUCT         levent_content;
  int                   pth_unixfd[2];
  int8_t                test_msg[20];
  int8_t                log_msg[128];
  pthread_t             pth_clear;
  pthread_attr_t        pth_clear_attr;

  pthread_attr_init(&pth_clear_attr);
  pthread_attr_setdetachstate(&pth_clear_attr, PTHREAD_CREATE_DETACHED);

  // 初始化任务中转线程池
  if(socketpair(AF_UNIX, SOCK_STREAM, IPPROTO_IP, pth_unixfd) == -1)
    return sys_err_log("socketpair error in pthread_contrl_init");

  service.serv_trans = pth_unixfd[0];

  if((pth_status = pthread_create(&pth_clear, &pth_clear_attr, trans_server, (void *)pth_unixfd[1])) != 0)
    return pth_sys_err_log("pthread_create error for create transform thread", pth_status);

  pthread_attr_destroy(&pth_clear_attr);

  if(send(service.serv_trans, "ruok?", strlen("ruok?"), 0) == -1)
    return sys_err_log("send error in pthread_contrl_init when send msg to transform thread");

  memset(test_msg, 0, sizeof(test_msg));

  if(recv(service.serv_trans, test_msg, sizeof(test_msg), 0) == -1)
    return sys_err_log("recv error in pthread_contrl_init when recv msg from transform thread");

  //printf("recv event :%s\n", test_msg);

  if(strcmp(test_msg,"imok."))
  {
    sprintf(log_msg, "transform thread init error : %s", test_msg);
    serv_log_write(log_msg, 1);
    return -1;
  }

  /*
  * 将中转线程加入事件监听
  */

  levent_content = (PEVENT_STRUCT)malloc(sizeof(EVENT_STRUCT));

  memset(levent_content, 0, sizeof(EVENT_STRUCT));
  levent_content->st_fd   = service.serv_trans;
  levent_content->st_user = 0;
  levent_content->st_time = time(NULL);
  pthread_mutex_init(&levent_content->st_mutex, NULL);
  event_listen.events = EPOLLIN | EPOLLERR;
  event_listen.data.ptr = levent_content;

  pthread_mutex_lock(&service.serv_event.e_lock);
  levent_content->st_next = service.serv_event.e_list;
  service.serv_event.e_list = levent_content;
  pthread_mutex_unlock(&service.serv_event.e_lock);

  if(epoll_ctl(service.serv_epoll,EPOLL_CTL_ADD, service.serv_trans, &event_listen) == -1)
  {
    sys_err_log("service socket ctl error");
    return -1;
  }
  // 初始化用户清理线程
  if((pth_status = pthread_create(&pth_clear, &pth_clear_attr, user_status_clean, NULL)) != 0)
    return pth_sys_err_log("pthread_create error for create clear thread", pth_status);

  return 0;
}
