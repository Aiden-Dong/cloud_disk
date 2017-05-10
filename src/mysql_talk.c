#include "mysql_talk.h"
#include "public_base.h"
#include "service_base.h"

extern SERVICE service;

pthread_mutex_t mysql_log_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mysql_user_mutex = PTHREAD_MUTEX_INITIALIZER;

int insert_mysql_log(int id, const char *msg)
{
  char local_time[20];
  char mysql_log[256];

  memset(local_time, 0, sizeof(local_time));
  memset(mysql_log, 0, sizeof(mysql_log));

  serv_get_nowtime(local_time);
  sprintf(mysql_log, "insert into userlog values ('%s',%d,'%s')", local_time, id, msg);
  //printf("%s\n", mysql_log);

  pthread_mutex_lock(&mysql_log_mutex);
  if(mysql_query(&service.serv_mysql, mysql_log) != 0)
  {
    pthread_mutex_unlock(&mysql_log_mutex);
    return mysql_err_log("mysql_query 'INSERT' error");
  }
  pthread_mutex_unlock(&mysql_log_mutex);
  return 0;
}

int user_login_record(const char *user_id, const char *user_pass, char *user_name)
{
  char          query_msg[128];
  MYSQL_RES     *query_res;
  MYSQL_ROW     query_row;

  if(user_name == NULL)
    return -1;

  memset(query_msg, 0, sizeof(query_msg));

  sprintf(query_msg, "select u_name from usermessage where u_id=%s and u_passwd='%s' and u_flag=0",
          user_id, user_pass);

  pthread_mutex_lock(&mysql_user_mutex);

  if(mysql_query(&service.serv_mysql, query_msg) != 0)
  {
    pthread_mutex_unlock(&mysql_user_mutex);
    return mysql_err_log("mysql_query for 'SELECT' error");
  }


  if((query_res = mysql_use_result(&service.serv_mysql)) == NULL)
  {
    pthread_mutex_unlock(&mysql_user_mutex);
    return mysql_err_log("mysql_use_result error");
  }


  if((query_row = mysql_fetch_row(query_res)) == NULL)
  {
    pthread_mutex_unlock(&mysql_user_mutex);
    return -1;
  }


  strncpy(user_name, query_row[0], strlen(query_row[0]));
  mysql_free_result(query_res);

  memset(query_msg, 0, sizeof(query_msg));
  sprintf(query_msg, "update usermessage set u_flag=1 where u_id=%s", user_id);

  if(mysql_query(&service.serv_mysql, query_msg) != 0)
  {
    pthread_mutex_unlock(&mysql_user_mutex);
    return mysql_err_log("mysql_query for 'UPDATE' error");
  }

  pthread_mutex_unlock(&mysql_user_mutex);

  return 0;
}

int user_register_record(const char *user_name, const char *user_pass, char *user_id)
{
  MYSQL_RES   *query_res;
  MYSQL_ROW   query_row;
  char        local_time[20];
  char        insert_msg[256];
  char        select_msg[256];

  if(user_id == NULL)
    return -1;

  memset(local_time, 0, sizeof(local_time));
  memset(insert_msg, 0, sizeof(insert_msg));
  memset(select_msg, 0, sizeof(select_msg));

  serv_get_nowtime(local_time);
  sprintf(insert_msg, "insert into usermessage values (NULL, '%s', '%s', '%s', 0)", user_name, user_pass, local_time);

  pthread_mutex_lock(&mysql_user_mutex);

  if(mysql_query(&service.serv_mysql, insert_msg) != 0)
  {
    pthread_mutex_unlock(&mysql_user_mutex);
    return mysql_err_log("mysql_query error in register new user");
  }


  sprintf(select_msg, "select u_id from usermessage where u_name='%s' order by u_id desc limit 1", user_name);
  // write(STDOUT_FILENO, select_msg, strlen(select_msg));
  if(mysql_query(&service.serv_mysql, select_msg) != 0)
  {
    pthread_mutex_unlock(&mysql_user_mutex);
    return mysql_err_log("mysql query error in 'SELECT' from register");
  }
  // write(STDOUT_FILENO, "mysql_use_result\n", strlen("mysql_use_result\n"));
  if((query_res = mysql_use_result(&service.serv_mysql)) == NULL)
  {
    pthread_mutex_unlock(&mysql_user_mutex);
    return -1;
  }

  // write(STDOUT_FILENO, "mysql_fetch_row\n", strlen("mysql_fetch_row\n"));
  if((query_row = mysql_fetch_row(query_res)) == NULL)
  {
    pthread_mutex_unlock(&mysql_user_mutex);
    return -1;
  }
  // write(STDOUT_FILENO, query_row[0], strlen(query_row[0]));
  strncpy(user_id, query_row[0], 10);
  mysql_free_result(query_res);
  pthread_mutex_unlock(&mysql_user_mutex);
  return 0;
}

int user_logout_record(const int user_id)
{
  MYSQL_RES     *query_res;
  MYSQL_ROW     query_row;
  char          update_msg[128];
  char          query_msg[128];
  char          res_msg[52];

  sprintf(query_msg, "select u_flag from usermessage where u_id=%d", user_id);
  // printf("%s\n", query_msg);

  pthread_mutex_lock(&mysql_user_mutex);
  if(mysql_query(&service.serv_mysql, query_msg) != 0)
  {
    pthread_mutex_unlock(&mysql_user_mutex);
    return mysql_err_log("mysql_query for 'SELECT' error");
  }

  if((query_res = mysql_use_result(&service.serv_mysql)) == NULL)
  {
    pthread_mutex_unlock(&mysql_user_mutex);
    return mysql_err_log("mysql_use_result error");
  }

  if((query_row = mysql_fetch_row(query_res)) == NULL)
  {
    pthread_mutex_unlock(&mysql_user_mutex);
    return -1;
  }

  memset(res_msg, 0, sizeof(res_msg));
  strncpy(res_msg, query_row[0], sizeof(res_msg));
  mysql_free_result(query_res);

  if(strcmp(res_msg, "1"))
    return -1;

  sprintf(update_msg,  "update usermessage set u_flag=0 where u_id=%d", user_id);

  if(mysql_query(&service.serv_mysql, update_msg) != 0)
  {
    pthread_mutex_unlock(&mysql_user_mutex);
    return mysql_err_log("mysql_query for 'UPDATE' error");
  }

  pthread_mutex_unlock(&mysql_user_mutex);

  return 0;

}
