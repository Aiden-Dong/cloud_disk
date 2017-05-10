#include "log_daemon.h"


extern SERVICE service;
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

int serv_mysql_init()
{
  int res;

  if(mysql_init(&service.serv_mysql) == NULL)
    return -1;
  else if(mysql_real_connect(&service.serv_mysql, MYSQL_HOST_ADDR, MYSQL_USER_NAME, MYSQL_USER_PASS, MYSQL_DATABASE, 3306, NULL, 0) == NULL)
  {
    res = mysql_err_log("mysql connect error");

    mysql_close(&service.serv_mysql);

    return -1;
  }


  serv_log_write("mysql init sucess", 2);
  return 0;
}

int serv_log_init()
{
  struct stat file_msg;
  int fd;

  unlink(LOG_FILE_BAK);
  if (stat(LOG_FILE, &file_msg) != -1) {
    rename(LOG_FILE, LOG_FILE_BAK);
  }
  
  if((fd = open(LOG_FILE, O_RDWR|O_CREAT|O_APPEND, S_IRUSR|S_IWUSR)) == -1)
   return -1;

   service.serv_log = fd;
   return 0;
}

int serv_log_write(const char *msg, int level)
{
  char time_buff[20];
  char even_context[100];

  memset(time_buff, 0, sizeof(time_buff));
  memset(even_context, 0, sizeof(even_context));

  if(serv_get_nowtime(time_buff) == -1)
    return -1;

  if(level == 1)          // service level
    sprintf(even_context, "%s  %-7s\t%s\n", time_buff, "server", msg);
  else if(level == 2)     // user level
    sprintf(even_context, "%s  %-7s\t%s\n", time_buff, "mysql", msg);
  else if(level == 3)
      sprintf(even_context, "%s  %-7s\t%s\n", time_buff, "socket", msg);
  else if(level == 4)
    sprintf(even_context, "%s  %-7s\t%s\n", time_buff, "client", msg);
  else
    return -1;

  pthread_mutex_lock(&log_mutex);

  if(write(service.serv_log, even_context, strlen(even_context)) == -1)
  {
    pthread_mutex_unlock(&log_mutex);
    err_sys("write error");
  }

  pthread_mutex_unlock(&log_mutex);

  return 0;
}
