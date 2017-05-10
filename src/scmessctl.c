#include "scmessctl.h"
#include "pthread_pool.h"
#include "file_msg.h"

extern SERVICE    service;

int user_login(PEVENT_STRUCT pevent, PCLI_TO_SERV client_msg)
{
  char         log_msg[128];
  char         ip_addr[INET_ADDRSTRLEN];
  SERV_TO_CLI  recv_msg;
  int i = 0;


  // printf("%s\n", "用户登录事件");
  // printf("username : %s\n", client_msg->cts_fname);
  // printf("userpass : %s\n", client_msg->cts_main);
  // 用户名与格式测试
  //printf("%s\n", "format test");
  memset(&recv_msg, 0, sizeof(recv_msg));

  if(strlen(client_msg->cts_fname) >= 10           ||
      strlen(client_msg->cts_main) >= MAX_PASS_LEN ||
      strlen(client_msg->cts_fname) == 0 ||
      strlen(client_msg->cts_main) == 0
  )
    {
      recv_msg.stc_events = SE_FAILE;
      if(send(pevent->st_fd, &recv_msg, sizeof(recv_msg), 0) == -1)
      {
        sys_err_log("send to client message error when login");
        return -2;
      }

      return -1;
    }

  for(i = 0; i < strlen(client_msg->cts_fname); i++)
    if(!isdigit(client_msg->cts_fname[i]))
    {
      recv_msg.stc_events = SE_FAILE;
      if(send(pevent->st_fd, &recv_msg, sizeof(recv_msg), 0) == -1)
      {
        sys_err_log("send to client message error when login");
        return -2;
      }

      return -1;
    }

  // 用户与密码准确性测试
  //printf("%s\n", "passwd test");
  memset(&recv_msg, 0, sizeof(recv_msg));
  if(user_login_record(client_msg->cts_fname, client_msg->cts_main, recv_msg.stc_fname))
  {
    recv_msg.stc_events = SE_FAILE;
    if(send(pevent->st_fd, &recv_msg, sizeof(recv_msg), 0) == -1)
    {
      sys_err_log("send to client message error when login");
      return -2;
    }

    return -1;
  }

  // 激活用户状态
  //printf("%s\n", "active test");
  pevent->st_time = time(NULL);
  pevent->st_user = atoi(client_msg->cts_fname);
  //strncpy(pevent->st_dir, client_msg->cts_fname, sizeof(pevent->st_dir));
  pevent->st_live = 20;

  // 更改用户当前目录
  if(change_dir(pevent, NULL, 1) == -1)
  {
    recv_msg.stc_events = SE_FAILE;
    if(send(pevent->st_fd, &recv_msg, sizeof(recv_msg), 0) == -1)
      return sys_err_log("send to client message error when login");
    return -1;
  }

  // 日志记录
  //printf("%s\n", "log record test");
  memset(log_msg, 0, sizeof(log_msg));
  sprintf(log_msg, "user login in %s", inet_ntop(AF_INET, &pevent->st_addr, ip_addr, INET_ADDRSTRLEN));
  insert_mysql_log(pevent->st_user, log_msg);

  // 响应客户端
  //printf("%s\n", "recv test");
  recv_msg.stc_events = SE_OK;
  strncpy(recv_msg.stc_main, get_client_dir(pevent), sizeof(recv_msg.stc_main));
  if(send(pevent->st_fd, &recv_msg, sizeof(recv_msg), 0) == -1)
  {
    sys_err_log("send to client error");
    return -2;
  }


  return user_show_dir(pevent);
}

int user_register(PEVENT_STRUCT pevent, PCLI_TO_SERV client_msg)
{
  char         log_msg[128];
  char         ip_addr[INET_ADDRSTRLEN];
  SERV_TO_CLI  recv_msg;
  int i = 0;

  // printf("%s\n", "用户注册事件");
  memset(&recv_msg, 0, sizeof(recv_msg));
  if(strlen(client_msg->cts_fname) >= 30           ||
      strlen(client_msg->cts_main) >= MAX_PASS_LEN ||
      strlen(client_msg->cts_fname) == 0 ||
      strlen(client_msg->cts_main) == 0
  )
  {
    write(STDOUT_FILENO, "format error", strlen("format error"));
    recv_msg.stc_events = SE_FAILE;
    if(send(pevent->st_fd, &recv_msg, sizeof(recv_msg), 0) == -1)
    {
      sys_err_log("send to client message error when register");
      return -2;
    }

    return -1;
  }


   //write(STDOUT_FILENO, "登录信息\n", strlen("登录信息\n"));
  if(user_register_record(client_msg->cts_fname, client_msg->cts_main, recv_msg.stc_fname) == -1)
  {
    write(STDOUT_FILENO, "register error", strlen("register error"));
    recv_msg.stc_events = SE_FAILE;
    if(send(pevent->st_fd, &recv_msg, sizeof(recv_msg), 0) == -1)
    {
      sys_err_log("send to client message error when register");
      return -2;
    }

    return -1;
  }
   //write(STDOUT_FILENO, "登录信息完成\n", strlen("登录信息完成\n"));

  // 创建用户主目录
  //write(STDOUT_FILENO, "创建用户主目录\n", strlen("创建用户主目录\n"));
  if(create_dir(pevent, recv_msg.stc_fname) == -1)
  {
    write(STDOUT_FILENO, "create admin error", strlen("create admin error"));
    recv_msg.stc_events = SE_FAILE;
    if(send(pevent->st_fd, &recv_msg, sizeof(recv_msg), 0) == -1)
    {
      sys_err_log("send to client message error when register");
      return -2;
    }

    return -1;
  }
  //write(STDOUT_FILENO, "创建用户主目录完成\n", strlen("创建用户主目录完成\n"));

  recv_msg.stc_events = SE_OK;
  if(send(pevent->st_fd, &recv_msg, sizeof(recv_msg), 0) == -1)
  {
    sys_err_log("send to client message error when register");
    return -2;
  }
  
  memset(log_msg, 0, sizeof(log_msg));
  sprintf(log_msg, "user register in %s", inet_ntop(AF_INET, &pevent->st_addr, ip_addr, INET_ADDRSTRLEN));
  insert_mysql_log(atoi(client_msg->cts_fname), log_msg);

  return 0;
}

int user_change_dir(PEVENT_STRUCT pevent, PCLI_TO_SERV client_msg, uint8_t flags)
{
  uint8_t       log_msg[128];
  SERV_TO_CLI   server_msg;

  if(pevent->st_user == 0)
    return -1;

  memset(&server_msg, 0, sizeof(server_msg));
  // 进入目录
  if(flags == 1)
  {
    // printf("用户进入目录 : %s\n", client_msg->cts_fname);
    if(strlen(client_msg->cts_fname) == 0)
    {
      server_msg.stc_events = SE_FAILE;
      strcpy(server_msg.stc_main, get_client_dir(pevent));
      if(send(pevent->st_fd, &server_msg, sizeof(server_msg), 0) == -1)
      {
        sys_err_log("send to client error when client change directory falth");
        return -2;
      }
      return -1;
    }


    if(change_dir(pevent, client_msg->cts_fname, 1) == -1)
    {
      sprintf(log_msg, "entry directory error :%s", client_msg->cts_fname);
      insert_mysql_log(pevent->st_user, log_msg);

      server_msg.stc_events = SE_FAILE;
      strcpy(server_msg.stc_main, get_client_dir(pevent));
      if(send(pevent->st_fd, &server_msg, sizeof(server_msg), 0) == -1)
      {
        sys_err_log("send to client error when client change directory falth");
        return -2;
      }
      return -1;
    }

    sprintf(log_msg, "entry directory :%s", client_msg->cts_fname);
    insert_mysql_log(pevent->st_user, log_msg);


    server_msg.stc_events = SE_OK;
    strcpy(server_msg.stc_main, get_client_dir(pevent));
    if(send(pevent->st_fd, &server_msg, sizeof(server_msg), 0) == -1)
    {
      sys_err_log("send to client error when client change directory sucess");
      return -2;
    }


    return user_show_dir(pevent);
  }
  // 退出目录
  else if (flags == 2)
  {
    // printf("%s\n", "用户退出目录");
    if(change_dir(pevent, client_msg, 2) == -1)
    {
      insert_mysql_log(pevent->st_user, "exit directory error");
      server_msg.stc_events = SE_FAILE;
      strcpy(server_msg.stc_main, get_client_dir(pevent));
      if(send(pevent->st_fd, &server_msg, sizeof(server_msg), 0) == -1)
        return sys_err_log("send to client error when client change directory falth");
      return -1;
    }

    insert_mysql_log(pevent->st_user, "exit directory sucess");

    server_msg.stc_events = SE_OK;
    strcpy(server_msg.stc_main, get_client_dir(pevent));
    if(send(pevent->st_fd, &server_msg, sizeof(server_msg), 0) == -1)
      return sys_err_log("send to client error when client change directory sucess");



    return user_show_dir(pevent);
  }
  return -1;
}

int user_create_dir(PEVENT_STRUCT pevent, PCLI_TO_SERV client_msg)
{
  uint8_t     log_msg[128];
  SERV_TO_CLI service_msg;

  //printf("%s\n", "用户创建目录");

  if(pevent->st_user == 0)
    return -1;

  memset(&service_msg, 0, sizeof(service_msg));

  strncpy(service_msg.stc_main, get_client_dir(pevent), strlen(service_msg.stc_main));

  if(create_dir(pevent, client_msg->cts_fname) == -1)
  {

    sprintf(log_msg, "create directory error: %s",client_msg->cts_fname);
    insert_mysql_log(pevent->st_user, log_msg);
    service_msg.stc_events = SE_FAILE;
    if(send(pevent->st_fd, &service_msg, sizeof(service_msg), 0) == -1)
    {
      sys_err_log("send to client error when client create directory falth");
      return -2;
    }
    return -1;
  }

  sprintf(log_msg, "create directory sucess: %s",client_msg->cts_fname);
  insert_mysql_log(pevent->st_user, log_msg);

  service_msg.stc_events = SE_OK;
  strcpy(service_msg.stc_main, get_client_dir(pevent));
  if(send(pevent->st_fd, &service_msg, sizeof(service_msg), 0) == -1)
  {
    sys_err_log("send to client error when client create directory sucess");
    return -2;
  }

  return 0;
}

int user_remove_file(PEVENT_STRUCT pevent, PCLI_TO_SERV client_msg)
{
  uint8_t       log_msg[128];
  SERV_TO_CLI   service_msg;

  // printf("%s\n", "用户删除文件");
  memset(&service_msg, 0, sizeof(service_msg));
  memset(log_msg, 0, sizeof(log_msg));

  if(pevent->st_user == 0)
    return -1;

  if(strlen(client_msg->cts_fname) == 0)
  {
    service_msg.stc_events = SE_FAILE;
    strncpy(service_msg.stc_main, get_client_dir(pevent), sizeof(service_msg.stc_main));
    if(send(pevent->st_fd, &service_msg, sizeof(service_msg), 0) == -1)
    {
      sys_err_log("send to client message error when client remove file failth");
      return -2;
    }
    return -1;
  }

  if(remove_file(pevent, client_msg->cts_fname) == -1)
  {
    sprintf(log_msg, "client remove file: %s falth in : %s", client_msg->cts_fname, pevent->st_dir);
    insert_mysql_log(pevent->st_user, log_msg);

    service_msg.stc_events = SE_FAILE;
    strncpy(service_msg.stc_main, get_client_dir(pevent), sizeof(service_msg.stc_main));
    if(send(pevent->st_fd, &service_msg, sizeof(service_msg), 0) == -1)
    {
      sys_err_log("send to client message error when client remove file failth");
      return -2;
    }

    return -1;
  }

  sprintf(log_msg, "client remove file : %s sucess in : %s", client_msg->cts_fname, pevent->st_dir);
  insert_mysql_log(pevent->st_user, log_msg);

  service_msg.stc_events = SE_OK;
  strncpy(service_msg.stc_main, get_client_dir(pevent), sizeof(service_msg.stc_main));
  if(send(pevent->st_fd, &service_msg, sizeof(service_msg), 0) == -1)
  {
    sys_err_log("send to client message error when client remove file sucess");
    return -2;
  }

  return 0;
}

int user_remove_directoy(PEVENT_STRUCT pevent, PCLI_TO_SERV client_msg)
{
  uint8_t       log_msg[128];
  SERV_TO_CLI   service_msg;

  // printf("%s\n", "用户删除目录");
  memset(&service_msg, 0, sizeof(service_msg));
  memset(log_msg, 0, sizeof(log_msg));

  if(pevent->st_user == 0)
    return -1;

  if(strlen(client_msg->cts_fname) == 0)
  {
    service_msg.stc_events = SE_FAILE;
    strncpy(service_msg.stc_main, get_client_dir(pevent), sizeof(service_msg.stc_main));
    if(send(pevent->st_fd, &service_msg, sizeof(service_msg), 0) == -1)
    {
      sys_err_log("send to client message error when client remove directory failth");
      return -2;
    }
    return -1;
  }

  if(remove_directory(pevent, client_msg->cts_fname) == -1)
  {
    sprintf(log_msg, "client remove directory: %s falth in : %s", client_msg->cts_fname, pevent->st_dir);
    insert_mysql_log(pevent->st_user, log_msg);

    service_msg.stc_events = SE_FAILE;
    strncpy(service_msg.stc_main, get_client_dir(pevent), sizeof(service_msg.stc_main));
    if(send(pevent->st_fd, &service_msg, sizeof(service_msg), 0) == -1)
    {
      sys_err_log("send to client message error when client remove directory failth");
      return -2;
    }

    return -1;
  }

  sprintf(log_msg, "client remove directory : %s sucess in : %s", client_msg->cts_fname, pevent->st_dir);
  insert_mysql_log(pevent->st_user, log_msg);

  service_msg.stc_events = SE_OK;
  strncpy(service_msg.stc_main, get_client_dir(pevent), sizeof(service_msg.stc_main));
  if(send(pevent->st_fd, &service_msg, sizeof(service_msg), 0) == -1)
  {
    sys_err_log("send to client message error when client remove directory sucess");
    return -2;
  }

  return 0;
}
int user_copy_file(PEVENT_STRUCT pevent, PCLI_TO_SERV client_msg)
{
  SERV_TO_CLI serv_msg;
  int8_t      mysql_log[128];

  // printf("复制文件正在进行 : %s : %s\n", client_msg->cts_fname, client_msg->cts_main);

  memset(&serv_msg, 0, sizeof(serv_msg));
  memset(&mysql_log, 0, sizeof(mysql_log));

  if(copy_file(pevent,client_msg) == -1)
  {
    // printf("%s\n", "复制文件失败");
    sprintf(mysql_log, "copy file : %s failth to %s", client_msg->cts_fname, client_msg->cts_main);
    insert_mysql_log(pevent->st_user, mysql_log);
    serv_msg.stc_events = SE_FAILE;
    strncpy(serv_msg.stc_main, get_client_dir(pevent), sizeof(serv_msg.stc_main));
    if(send(pevent->st_fd, &serv_msg, sizeof(serv_msg), 0) == -1)
    {
      sys_err_log("send error to client when client request falith in copy file");
      return -2;
    }

    return -1;
  }

  // printf("%s\n", "复制文件成功");
  sprintf(mysql_log, "copy file : %s failth to %s", client_msg->cts_fname, client_msg->cts_main);
  insert_mysql_log(pevent->st_user, mysql_log);

  serv_msg.stc_events = SE_OK;
  strncpy(serv_msg.stc_main,get_client_dir(pevent), sizeof(serv_msg.stc_main));
  if(send(pevent->st_fd, &serv_msg, sizeof(serv_msg), 0) == -1)
  {
    sys_err_log("send error to client when client request sucess in copy file");
    return -2;
  }

  return 0;
}

int user_move_file(PEVENT_STRUCT pevent, PCLI_TO_SERV client_msg)
{
  SERV_TO_CLI serv_msg;
  int8_t      mysql_log[128];

  // printf("移动文件正在进行 : %s : %s\n", client_msg->cts_fname, client_msg->cts_main);

  memset(&serv_msg, 0, sizeof(serv_msg));
  memset(&mysql_log, 0, sizeof(mysql_log));

  if(move_file(pevent,client_msg) == -1)
  {
    // printf("%s\n", "移动文件失败");
    sprintf(mysql_log, "move file : %s failth to %s", client_msg->cts_fname, client_msg->cts_main);
    insert_mysql_log(pevent->st_user, mysql_log);
    serv_msg.stc_events = SE_FAILE;
    strncpy(serv_msg.stc_main, get_client_dir(pevent), sizeof(serv_msg.stc_main));
    if(send(pevent->st_fd, &serv_msg, sizeof(serv_msg), 0) == -1)
    {
      sys_err_log("send error to client when client request falith in copy file");
      return -2;
    }

    return -1;
  }

  // printf("%s\n", "移动文件成功");
  sprintf(mysql_log, "move file : %s failth to %s", client_msg->cts_fname, client_msg->cts_main);
  insert_mysql_log(pevent->st_user, mysql_log);

  serv_msg.stc_events = SE_OK;
  strncpy(serv_msg.stc_main,get_client_dir(pevent), sizeof(serv_msg.stc_main));
  if(send(pevent->st_fd, &serv_msg, sizeof(serv_msg), 0) == -1)
  {
    sys_err_log("send error to client when client request sucess in move file");
    return -2;
  }

  return 0;
}

int user_trans_upload(PEVENT_STRUCT pevent, PCLI_TO_SERV client_msg)
{
  SERV_TO_CLI   serv_msg;
  int8_t        mysql_log[128];
  int           res_state;

  // printf("上传文件正在进行 : %s : %s\n", client_msg->cts_fname, client_msg->cts_main);
  res_state = upload_file(pevent,client_msg);

  memset(&serv_msg, 0, sizeof(serv_msg));
  memset(&mysql_log, 0, sizeof(mysql_log));

  if( res_state == -1)
  {
    // printf("%s\n", "上传文件失败");
    sprintf(mysql_log, "upload file : %s failth to : %s", client_msg->cts_fname, client_msg->cts_main);
    insert_mysql_log(pevent->st_user, mysql_log);
    serv_msg.stc_events = SE_FAILE;
    strncpy(serv_msg.stc_main, get_client_dir(pevent), sizeof(serv_msg.stc_main));
    if(send(pevent->st_fd, &serv_msg, sizeof(serv_msg), 0) == -1)
    {
      sys_err_log("send error to client when client request falith in upload file");
      return -2;
    }

    return -1;
  }
  else if(res_state == -2)
    return -2;

  // printf("%s\n", "上传文件成功");
  sprintf(mysql_log, "upload file : %s sucess to : %s", client_msg->cts_fname, client_msg->cts_main);
  insert_mysql_log(pevent->st_user, mysql_log);

  serv_msg.stc_events = SE_OK;
  strncpy(serv_msg.stc_main,get_client_dir(pevent), sizeof(serv_msg.stc_main));

  if(send(pevent->st_fd, &serv_msg, sizeof(serv_msg), 0) == -1)
  {
    sys_err_log("send error to client when client request sucess in upload file");
    return -2;
  }

  return 0;
}

int user_trans_download(PEVENT_STRUCT pevent, PCLI_TO_SERV client_msg)
{
  SERV_TO_CLI   serv_msg;
  int8_t        mysql_log[128];
  int           res_state;

  // printf("下载文件正在进行 : %s : %s\n", client_msg->cts_fname);

  res_state = download_file(pevent,client_msg);

  memset(&serv_msg, 0, sizeof(serv_msg));
  memset(&mysql_log, 0, sizeof(mysql_log));

  if( res_state == -1)
  {
    // printf("%s\n", "下载文件失败");
    sprintf(mysql_log, "download file : %s failth", client_msg->cts_fname);
    insert_mysql_log(pevent->st_user, mysql_log);
    serv_msg.stc_events = SE_FAILE;
    strncpy(serv_msg.stc_main, get_client_dir(pevent), sizeof(serv_msg.stc_main));
    if(send(pevent->st_fd, &serv_msg, sizeof(serv_msg), 0) == -1)
    {
      sys_err_log("send error to client when client request falith in download file");
      return -2;
    }
    return -1;
  }
  else if(res_state == -2)
    return -2;

  // printf("%s\n", "下载文件成功");
  sprintf(mysql_log, "download file : %s sucess", client_msg->cts_fname);
  insert_mysql_log(pevent->st_user, mysql_log);

  serv_msg.stc_events = SE_OK;
  strncpy(serv_msg.stc_main,get_client_dir(pevent), sizeof(serv_msg.stc_main));

  if(send(pevent->st_fd, &serv_msg, sizeof(serv_msg), 0) == -1)
  {
    sys_err_log("send error to client when client request sucess in download file");
    return -2;
  }

  return 0;
}

int user_havey_task(PEVENT_STRUCT pevent, PCLI_TO_SERV client_msg)
{
  THREADMSG     task_msg;

  //printf("%s\n", "user_havey_task job start");

  memset(&task_msg, 0, sizeof(task_msg));
  memcpy(&task_msg.climsg, client_msg, sizeof(CLI_TO_SERV));
  task_msg.pevent = pevent;

  if(send(service.serv_trans, &task_msg, sizeof(task_msg), 0) == -1)
    sys_err_log("send to transform server error ");

  return 0;
}

static int user_show_dir(PEVENT_STRUCT pevent)
{
  int res_state = 0;
  SERV_TO_CLI  recv_msg;

  res_state = show_dir(pevent);

  if(res_state == -1)
    recv_msg.stc_events = SE_FAILE;
  else if(res_state == 0)
    recv_msg.stc_events = SE_OK;
  else
    return -2;

  strncpy(recv_msg.stc_main, get_client_dir(pevent), sizeof(recv_msg.stc_main));

  if(send(pevent->st_fd, &recv_msg, sizeof(recv_msg), 0) == -1)
  {
      sys_err_log("send to client for show dir error");
      return -2;
  }

  return res_state;
}
