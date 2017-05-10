#include "public_base.h"
#include "base_daemon.h"

int main(int argc, char const *argv[])
{

  if(server_init() == -1)           // service init
    err_sys("server init error");

  serv_log_write("service open sucess", 1);

  server_listen();                // service liten from client

  return 0;
}
