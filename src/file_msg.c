#include "file_msg.h"
#include <stdio.h>
#include <stdlib.h>
#include "log_daemon.h"

int create_dir(PEVENT_STRUCT pevent, const int8_t *dir)
{
  int8_t        host_dir[MAX_DNAM_LEN + 1];
  struct stat   file_stat;
  int8_t        user_str[15];
  int8_t        user_log[128];

  memset(user_str, 0, sizeof(user_str));
  memset(host_dir, 0, MAX_DNAM_LEN);

  if(dir == NULL)
    return -1;

  if(check_dir(dir) == -1)
    return -1;

  if(pevent->st_user == 0)    // 发生在注册时
      sprintf(host_dir, "%s/%s", ADMIN_HOME, dir);
  else                        // 发生在登录时
  {
    //  当前路径(转真实路径) / 请求创建路径
    if(get_host_dir(pevent->st_dir) == NULL)
      return -1;

    if((strlen(get_host_dir(pevent->st_dir)) + strlen(dir)) > MAX_DNAM_LEN)
      return -1;

    sprintf(host_dir, "%s/%s", get_host_dir(pevent->st_dir), dir);
  }

  if(stat(host_dir, &file_stat) == -1)
  {
    if(errno != ENOENT)
      return sys_err_log("stat error in make directore for test");
  }
  else
    return -1;

  if(mkdir(host_dir, S_IRWXU) == -1)
    return -1;

  memset(user_log, 0, sizeof(user_log));
  sprintf(user_log, "user %d create directory : %s", pevent->st_user, &host_dir[strlen(ADMIN_HOME)]);
  insert_mysql_log(pevent->st_user, user_log);

  return 0;
}
int change_dir(PEVENT_STRUCT pevent, const int8_t *dir, int flags)
{
  int8_t      user_dir[MAX_DNAM_LEN + 1];
  int8_t      user_log[128];
  struct stat file_stat;

  memset(user_dir, 0, sizeof(user_dir));

  // 进入目录事件
  if(flags == 1)
  {

    if(dir == NULL)
    {
      // write(STDOUT_FILENO, "1", 2);
      // printf("%d | %s\n", pevent->st_user, pevent->st_dir);
      if(pevent->st_user != 0 && strlen(pevent->st_dir) == 0)
        sprintf(user_dir, "/%d", pevent->st_user);
      else
        return -1;
    }
    else if(strlen(pevent->st_dir) == 0)
    {
      // write(STDOUT_FILENO, "2", 2);
      return -1;
    }
    else
    {
      if(check_dir(dir) == -1)
        return -1;

      // 检查目录的合法性
      if(!get_host_dir(pevent->st_dir) ||
        (strlen(get_host_dir(pevent->st_dir)) + strlen(dir)) > MAX_DNAM_LEN)
        return -1;

      sprintf(user_dir, "%s/%s", pevent->st_dir, dir);
    }


    // 测试目录的准确性
    if(test_dir(get_host_dir(user_dir)) == -1)
    {
      // printf("目录不存在 : %s\n", user_dir);
      return -1;
    }

    memset(pevent->st_dir, 0, sizeof(pevent->st_dir));
    strncpy(pevent->st_dir, user_dir, sizeof(pevent->st_dir));
  }
  // 用户退出目录事件
  else if(flags == 2)
  {
    int8_t   *p;
    int8_t    ch;
    char      user_main[20];

    memset(user_main, 0, sizeof(user_main));
    sprintf(user_main, "/%d", pevent->st_user);

    if(strlen(user_main) >= strlen(pevent->st_dir))
      return -1;

    p = &pevent->st_dir[strlen(pevent->st_dir)-1];

    do{
      ch = *p;
      *p = 0;
      p--;
    }while(ch != '/');
  }

  memset(user_log, 0, sizeof(user_log));
  sprintf(user_log, "user : %d change directory: %s", pevent->st_user, pevent->st_dir);
  insert_mysql_log(pevent->st_user, user_log);

  return 0;
}
int remove_file(PEVENT_STRUCT pevent, const int8_t *file)
{
  struct  stat    file_msg;
  uint8_t         user_file[MAX_DNAM_LEN+1];

  if(file == NULL)
    return -1;

  if(check_file(file) == -1)
    return -1;

  memset(user_file, 0, sizeof(user_file));

  if(!get_host_dir(pevent->st_dir) || (strlen(get_host_dir(pevent->st_dir)) + strlen(file)) > MAX_DNAM_LEN)
    return -1;

  sprintf(user_file, "%s/%s",get_host_dir(pevent->st_dir), file);

  if(stat(user_file, &file_msg) == -1)
  {
    if(errno == ENOENT)
      return -1;

    return sys_err_log("stat error when client remove file");
  }

  if(S_ISDIR(file_msg.st_mode))
    return -1;

  if(unlink(user_file) == -1)
    return sys_err_log("unlink error when client remove file");

  return 0;
}
int remove_directory(PEVENT_STRUCT pevent, const int8_t *dir)
{
  struct  stat    file_msg;
  uint8_t         user_file[MAX_DNAM_LEN+1];
  struct dirent   *cur_dir;
  DIR             *dir_fd;

  if(dir == NULL)
    return -1;

  if(check_dir(dir) == -1 )
    return -1;

  memset(user_file, 0, sizeof(user_file));

  if(!get_host_dir(pevent->st_dir) || (strlen(get_host_dir(pevent->st_dir)) + strlen(dir)) > MAX_DNAM_LEN)
    return -1;

  sprintf(user_file, "%s/%s",get_host_dir(pevent->st_dir), dir);

  if(is_empdir(user_file))
    return -1;

  if(rmdir(user_file) == -1)
    return sys_err_log("rmdir error when client remove directory");

  return 0;

}
int copy_file(PEVENT_STRUCT pevent, PCLI_TO_SERV client_msg)
{
  /*
  * 复制文件
  */
  int           read_len;
  int8_t        file_buff[1024];
  int           fin,fout;
  struct stat   file_stat;
  int8_t        dst_dir[MAX_DNAM_LEN+1];
  int8_t        src_dir[MAX_DNAM_LEN+1];
  int8_t        src_file[MAX_DNAM_LEN +1];
  int8_t        dst_file[MAX_DNAM_LEN +1];


  memset(src_dir, 0, MAX_DNAM_LEN+1);
  memset(dst_dir, 0, MAX_DNAM_LEN+1);
  memset(src_file, 0, MAX_DNAM_LEN+1);
  memset(dst_file, 0, MAX_DNAM_LEN+1);

  if(pevent->st_user == 0)
    return -1;

  // 求原路径跟目的路径
  if(pth_get_host_dir(pevent->st_dir, src_dir, MAX_NAME_LEN+1) == -1)
    return -1;
  // 获取目的文件路径
  if(get_reality_dir(pevent, client_msg->cts_main, dst_dir, MAX_DNAM_LEN+1) == -1)
    return -1;

  // 测试文件是否合法
  if(check_file(client_msg->cts_fname) == -1)
    return -1;
  // 测试用户当前目录中文件是否存在


  sprintf(src_file, "%s/%s", src_dir,client_msg->cts_fname);   // 获取源文件
  sprintf(dst_file, "%s/%s", dst_dir,client_msg->cts_fname);   // 获取目的路径

  if(strlen(dst_file) > MAX_DNAM_LEN)
    return -1;

  // 测试目的路径中是否有文件

  if(stat(src_file, &file_stat) == -1)
    return sys_err_log("stat src file error in copy file");

  if(!S_ISREG(file_stat.st_mode))
    return -1;

  if(stat(dst_file, &file_stat) != -1)
    return -1;
  // 执行复制操作
  // printf("%s\n", src_file);
  // printf("%s\n", dst_file);
  if((fin = open(src_file, O_RDWR)) == -1)
    return sys_err_log("open file to read error in copy file");

  if((fout = open(dst_file, O_RDWR|O_CREAT|O_EXCL, S_IRUSR|S_IWUSR)) == -1)
    return sys_err_log("open file to write error in copy file");

  //printf("%s\n", "copy start");
  while (read_len = read(fin, file_buff, sizeof(file_buff)))
  {
    if(read_len == -1)
    {
      unlink(dst_file);
      return sys_err_log("read file error when copy file");
    }
    //printf("%s", "-");
    write(fout, file_buff, read_len);
  }

  //printf("%s\n", "copy finish");
  close(fin);
  close(fout);

  return 0;
}
int move_file(PEVENT_STRUCT pevent, PCLI_TO_SERV client_msg)
{
  struct stat   file_stat;
  int8_t        dst_dir[MAX_DNAM_LEN+1];
  int8_t        src_dir[MAX_DNAM_LEN+1];
  int8_t        src_file[MAX_DNAM_LEN +1];
  int8_t        dst_file[MAX_DNAM_LEN +1];


  memset(src_dir, 0, MAX_DNAM_LEN+1);
  memset(dst_dir, 0, MAX_DNAM_LEN+1);
  memset(src_file, 0, MAX_DNAM_LEN+1);
  memset(dst_file, 0, MAX_DNAM_LEN+1);

  if(pevent->st_user == 0)
    return -1;

  // 求原路径跟目的路径
  if(pth_get_host_dir(pevent->st_dir, src_dir, MAX_NAME_LEN+1) == -1)
    return -1;

  // 获取目的文件路径
  if(get_reality_dir(pevent, client_msg->cts_main, dst_dir, MAX_DNAM_LEN+1) == -1)
    return -1;

  // 测试文件是否合法
  if(check_file(client_msg->cts_fname) == -1)
    return -1;
  // 测试用户当前目录中文件是否存在


  sprintf(src_file, "%s/%s", src_dir,client_msg->cts_fname);   // 获取源文件
  sprintf(dst_file, "%s/%s", dst_dir,client_msg->cts_fname);   // 获取目的路径

  if(strlen(dst_file) > MAX_DNAM_LEN)
    return -1;


  // 测试目的路径中是否有文件

  if(stat(src_file, &file_stat) == -1)
    return sys_err_log("stat src file error in copy file");

  if(!S_ISREG(file_stat.st_mode))
    return -1;

  if(stat(dst_file, &file_stat) != -1)
    return -1;

  if(rename(src_file, dst_file) == -1)
    return sys_err_log("rename error in remove file");
}
int show_dir(PEVENT_STRUCT pevent)
{
	struct dirent *cur_dir;
  DIR           *dir_fd;
  SERV_TO_CLI   server_msg;

	if(strlen(pevent->st_dir) == 0)
		return -1;

	if(test_dir(get_host_dir(pevent->st_dir)) == -1)
		return -1;

	if((dir_fd = opendir(get_host_dir(pevent->st_dir))) == NULL)
		return -1;

  while(cur_dir = readdir(dir_fd))
  {
    if(!strcmp(cur_dir->d_name, ".") || !strcmp(cur_dir->d_name, ".."))
      continue;

    //printf("user : %d\n", pevent->st_user);

    memset(&server_msg, 0, sizeof(server_msg));
    server_msg.stc_events = SE_DONOW;

    // 发送目录信息
    strcpy(server_msg.stc_fname, cur_dir->d_name);
    if(cur_dir->d_type == DT_DIR)
      server_msg.stc_ftype = FILE_DIR;
    else
    {
      server_msg.stc_ftype = FILE_NOR;
      server_msg.stc_fsize = cur_dir->d_reclen;
    }

    if(get_client_dir(pevent) == NULL)
      return -1;

    strcpy(server_msg.stc_main, get_client_dir(pevent));

    if(send(pevent->st_fd, &server_msg, sizeof(server_msg), 0) == -1)
    {
      sys_err_log("send to client directory error");
      return -2;
    }
  }

  if(closedir(dir_fd) == -1)
    return sys_err_log("closedir error");

  return 0;
}
int upload_file(PEVENT_STRUCT pevent, PCLI_TO_SERV client_msg)
{
  int           fout;
  struct stat   file_stat;
  int16_t       recv_len;
  TRANS_CONTEXT file_context;
  SERV_TO_CLI   serv_msg;
  int8_t        dst_dir[MAX_DNAM_LEN+1];
  int8_t        dst_file[MAX_DNAM_LEN +1];

  memset(dst_dir, 0, MAX_DNAM_LEN+1);
  memset(dst_file, 0, MAX_DNAM_LEN+1);

  if(pevent->st_user == 0)
    return -1;

  //printf("%s\n", "user pass");
  //if(strlen(client_msg->cts_main) == 0)
    //return -1;
  //printf("%s\n", " pass");
  // 获取目的文件路径

  if(pth_get_host_dir(pevent->st_dir, dst_dir, MAX_NAME_LEN+1) == -1)
    return -1;

  //printf("%s\n", "get_host_dir pass");

  // 测试文件是否合法
  if(check_file(client_msg->cts_fname) == -1)
    return -1;
  //printf("%s\n", "check_file pass");

  sprintf(dst_file, "%s/%s", dst_dir,client_msg->cts_fname);   // 获取目的路径
  //printf("%s\n", dst_file);
  if(strlen(dst_file) > MAX_DNAM_LEN)
    return -1;

  //printf("%s\n", "length pass");

  // 测试目的路径中是否有文件
  if(stat(dst_file, &file_stat) != -1)
    return -1;  // 多线程中不能靠访问 errno 全局变量来获取真实属性

  //printf("%s\n", "stat_file pass");

  if((fout = open(dst_file, O_RDWR|O_CREAT|O_EXCL, S_IRUSR|S_IWUSR)) == -1)
    return sys_err_log("open file to write error in copy file");

  //printf("%s\n", "open_file pass");

  memset(&serv_msg, 0, sizeof(serv_msg));
  serv_msg.stc_events = SE_OK;  // 允许传送数据
  strncpy(serv_msg.stc_main, get_client_dir(pevent), sizeof(serv_msg.stc_main));

  if(send(pevent->st_fd, &serv_msg, sizeof(serv_msg), 0) == -1)
  {
    close(fout);
    unlink(dst_file);
    sys_err_log("send error to client when client request pass in upload file");
    return -2;
  }

  while (1)
  {
    //printf("%s", "*");
    // 接受客户端的数据
    recv_len = recv(pevent->st_fd, &file_context, sizeof(file_context), 0);

    if(recv_len == -1)  // 客户端错误 -- 关闭客户端
    {
      serv_log_write("recv error from upload", 4);
      unlink(dst_file);
      return -2;
    }
    if(recv_len == 0)
    {
      unlink(dst_file);
      return -2;
    }
    if(ntohs(file_context.trans_len) > sizeof(file_context.trans_data))
    {
      //printf("%s\n", "length error");
      unlink(dst_file);
      return -2;
    }
    if(file_context.trans_flags != 0 && file_context.trans_flags != 1 ||
      file_context.trans_flags != 1 && file_context.trans_len == 0)
    {
      //printf("%s\n", "flage error");
      unlink(dst_file);
      return -2;
    }

    if(send(pevent->st_fd, "1", 1, 0) == -1)
    {
      serv_log_write("recv error from upload", 4);
      unlink(dst_file);
      return -2;
    }
    if(file_context.trans_flags)
      break;

    //printf("写文件大小 : %d\n", ntohs(file_context.trans_len));

    if(write(fout, file_context.trans_data, ntohs(file_context.trans_len)) == -1)
    {
      unlink(dst_file);
      return sys_err_log("write error when upload file with client");
    }
  }

  close(fout);
  return 0;
}
int download_file(PEVENT_STRUCT pevent, PCLI_TO_SERV client_msg)
{
  struct stat   file_stat;
  int           fin;
  uint8_t       recv_flags;
  TRANS_CONTEXT file_context;
  SERV_TO_CLI   serv_msg;
  int16_t       read_len;
  int8_t        src_dir[MAX_DNAM_LEN+1];
  int8_t        src_file[MAX_DNAM_LEN +1];


  memset(src_dir, 0, MAX_DNAM_LEN+1);
  memset(src_file, 0, MAX_DNAM_LEN+1);

  if(pevent->st_user == 0)
    return -1;

  // 求原路径跟目的路径
  if(pth_get_host_dir(pevent->st_dir, src_dir, MAX_NAME_LEN+1) == -1)
    return -1;

  // 测试文件是否合法
  if(check_file(client_msg->cts_fname) == -1)
    return -1;
  // 测试用户当前目录中文件是否存在

  sprintf(src_file, "%s/%s", src_dir,client_msg->cts_fname);   // 获取源文件

  // 测试目的路径中是否有文件
  //printf("下载文件路径:%s\n", src_file);

  if(stat(src_file, &file_stat) == -1)
    return sys_err_log("stat src file error in copy file");

  if(!S_ISREG(file_stat.st_mode))
    return -1;

  if((fin = open(src_file, O_RDWR)) == -1)
    return sys_err_log("open file error when download with client");

  memset(&serv_msg, 0, sizeof(serv_msg));

  serv_msg.stc_events = SE_OK;  // 允许传送数据
  strncpy(serv_msg.stc_main, get_client_dir(pevent), sizeof(serv_msg.stc_main));

  if(send(pevent->st_fd, &serv_msg, sizeof(serv_msg), 0) == -1)
  {
    close(fin);
    sys_err_log("send error to client when client request pass in download file");
    return -2;
  }

  while(1)
  {
    //printf("%s", "|");
    memset(&file_context, 0, sizeof(file_context));

    read_len = read(fin, file_context.trans_data, sizeof(file_context.trans_data));

    if(read_len  == -1)
    {
      close(fin);
      return sys_err_log("read file error when client download file");
    }
    else if(read_len == 0)
      file_context.trans_flags = 1;
    else
      file_context.trans_flags = 0;

    file_context.trans_len = htons(read_len);

    //printf("发送文件大小 : %d\n", read_len);

    if(send(pevent->st_fd, &file_context, sizeof(file_context), 0) == -1)
    {
      close(fin);
      serv_log_write("send error to client when download file", 4);
      return -2;
    }

    if(recv(pevent->st_fd, &recv_flags,1, 0) == -1)
    {
      close(fin);
      serv_log_write("send error to client when download file", 4);
      return -2;
    }

    if(read_len == 0)
      break;
  }

  close(fin);

  return 0;
}
int8_t * get_host_dir(const int8_t *dir)
{
  static int8_t host_dir[MAX_DNAM_LEN + 1];

  // write(STDOUT_FILENO, "start get_host_dir\n", strlen("start get_host_dir\n"));

  if(dir == NULL)
    return NULL;

  if(strlen(dir) == 0 ||
    (strlen(dir) + strlen(ADMIN_HOME) + 1) >= MAX_DNAM_LEN)
    {
        // write(STDOUT_FILENO, "get_host_dir error\n", strlen("get_host_dir error\n"));
        return NULL;
    }

  // write(STDOUT_FILENO, "test get_host_dir\n", strlen("test get_host_dir\n"));

  memset(host_dir, 0, MAX_DNAM_LEN);

  sprintf(host_dir, "%s%s", ADMIN_HOME, dir);
  // printf("%s\n", host_dir);

  return host_dir;
}
int pth_get_host_dir(const int8_t *dir, int8_t *host_dir, int n)
{
  if(dir == NULL || host_dir == NULL)
    return -1;

  if(strlen(dir) == 0 ||
    (strlen(dir) + strlen(ADMIN_HOME) + 1) >= MAX_DNAM_LEN)
    {
        // write(STDOUT_FILENO, "get_host_dir error\n", strlen("get_host_dir error\n"));
        return -1;
    }

  memset(host_dir, 0, n);
  sprintf(host_dir, "%s%s", ADMIN_HOME, dir);

  return 0;
}
int8_t *get_client_dir(PEVENT_STRUCT pevent)
{
  int8_t user_dir[MAX_NAME_LEN + 1];

  if(pevent == NULL)
  {
    //printf("%s\n", "NULL");
    return  NULL;
  }


  if(strlen(pevent->st_dir) == 0)
  {
    //printf("%s\n", "len=0");
    return  NULL;
  }

  if(pevent->st_user == 0)
  {
    //printf("%s\n", "len=0");
    return  NULL;
  }

  //printf("%d\n",  pevent->st_user);
  memset(user_dir, 0, MAX_NAME_LEN +1);
  sprintf(user_dir, "%d", pevent->st_user);

  if(strlen(&pevent->st_dir[strlen(user_dir) + 1]) == 0)
    return "/";

  return &pevent->st_dir[strlen(user_dir) + 1];
}
int get_reality_dir(PEVENT_STRUCT pevent, int8_t *request_dir, int8_t* reality_dir, int n)
{
  int8_t user_dir[MAX_DNAM_LEN +1];

  //printf("request_dir : %s\n", request_dir);

  if(request_dir == NULL || request_dir == NULL)
    return -1;

  if(pevent->st_user == 0)
    return -1;
  if(!strcmp(request_dir, "/"))
    sprintf(user_dir, "%s/%d", ADMIN_HOME,pevent->st_user);
  else
    sprintf(user_dir, "%s/%d%s", ADMIN_HOME,pevent->st_user,request_dir);

  if(test_dir(user_dir) == -1)
    return -1;

  memset(reality_dir, 0, n);

  strncpy(reality_dir, user_dir, n);

  return 0;
}
static int check_dir(const int8_t *dir)
{
  int i = 0;

  if(dir == NULL)
    return -1;

  if(strlen(dir) == 0 || strlen(dir) > MAX_NAME_LEN)
    return -1;

  for(i = 0; i < strlen(dir); i++)
  {
    if(dir[i] == '/' || dir[i] == '.' || dir[i] == '\\' || dir[i] == '*' || dir[i] == '?')
      return -1;
  }

  return 0;
}
static int check_file(const int8_t *dir)
{
  int i;

  if(dir == NULL || strlen(dir) == 0 || strlen(dir) > MAX_NAME_LEN)
    return -1;

  for(i = 0; i < strlen(dir); i++)
  {
    if(dir[i] == '/' || dir[i] == '\\' || dir[i] == '*' || dir[i] == '?')
      return -1;
  }

  if(dir[strlen(dir) - 1] == '.')
    return -1;

  return 0;
}
static int test_dir(const int8_t *dir)
{
  struct stat   file_stat;

  if(dir == NULL)
    return -1;

  if(strlen(dir) == 0)
    return -1;

  if(stat(dir, &file_stat) == -1)
    return -1;

  if(!S_ISDIR(file_stat.st_mode))
    return -1;

  if((strlen(dir) + strlen(ADMIN_HOME) + 1) >= MAX_DNAM_LEN)
    return -1;

  return 0;
}
static int is_empdir(const int8_t *dir)
{
  struct dirent   *cur_dir;
  DIR             *dir_fd;
  int             flags = 0;

  if(test_dir(dir) == -1)
    return -1;

  if((dir_fd = opendir(dir)) == NULL)
    return -1;

  while (cur_dir = readdir(dir_fd)) {
    if(!strcmp(cur_dir->d_name, ".") || !strcmp(cur_dir->d_name, ".."))
      continue;

    flags = 1;
    break;
  }

  return flags;
}
