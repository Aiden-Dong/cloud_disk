#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#define MAX_NAME_LEN 					50												// 用户名的长度
#define MAX_DNAM_LEN					175												// 相对路径的长度
#define MAX_PASS_LEN					25												// 密码长度

/*
 *	文件格式
 */
#define FILE_NOR 1		// 普通文件
#define FILE_DIR 2		// 目录文件

/*
 * 服务端标志
 */
#define SE_SUCESS 0		// 操作成功
#define SE_OK	    1		// 请求通过
#define SE_REFUSE 2		// 请求拒绝
#define SE_FAILE  3		// 操作失败
#define SE_ERROR  4		// 服务异常（服务端关闭）
#define SE_DONOW  5		// 正载执行服务操作
#define SE_FINISH 6		// 请求操作完成
/*
 * 客户端标志
*/
#define CL_LOGIN  1		// 用户登录
#define CL_ENROLL 2		// 用户注册
#define CL_EXIT	  3		// 用户退出
#define CL_CDDIR  4		// 进入目录
#define CL_EXDIR  5		// 退出目录
#define CL_CRDIR  6		// 创建目录
#define CL_RMFIL  7		// 删除文件
#define CL_RMDIR 	8		// 删除目录
#define CL_CPFIL  9		// 复制文件
#define	CL_MVFIL  10 	// 移动文件
#define CL_SNDFL  11	// 上传文件
#define CL_RCVFL  12	// 下载文件
#define CL_ERROR  0		// 意外错误

/*
* client send to server
*/
typedef struct{
uint8_t	    cts_events;			               // 用户请求事件
int8_t      cts_fname[MAX_NAME_LEN+1];		 // 文件名
uint32_t 	  cts_fsize;			               // 文件大小
int8_t	    cts_main[MAX_DNAM_LEN+1];		   // 主目录
}CLI_TO_SERV, *PCLI_TO_SERV;

/*
* server send to client
*/
typedef struct{
	uint8_t 	  stc_events;	   		        	// 服务器回应事件
	int8_t 	    stc_fname[MAX_NAME_LEN+1];	// 文件名
	uint8_t 	  stc_ftype;			          	// 文件格式
	uint32_t 	  stc_fsize;			          	// 文件大小
	int8_t			stc_main[MAX_DNAM_LEN+1];			// 用户的当前目录
}SERV_TO_CLI, *PSERV_TO_CLI;

typedef struct{
  uint8_t     trans_flags;    // 0 run 1 stop
  uint16_t    trans_len;
  int8_t      trans_data[1024];
}TRANS_CONTEXT, *PTRANS_CONTEXT;


void err_que(const char *msg)
{
	puts(msg);
	exit(0);
}

void err_sys(const char *msg)
{
	printf("%s:%s\n", msg, strerror(errno));
	exit(errno);
}

// 登录测试
int register_test(int sockfd, const char *username, const char *userpass);

int main(int argc, char *argv[])
{
	int											sockfd;
	struct sockaddr_in			serviceaddr;
	int											swit_case;
	char										dir_name[20];
	char										file_name[20];
	if(argc != 4)
		err_que("register Ipaddress username userpass");

	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		err_sys("socket error");

	memset(&serviceaddr, 0, sizeof(serviceaddr));
	serviceaddr.sin_family = AF_INET;
	serviceaddr.sin_port	= htons(8068);
	if(inet_pton(AF_INET, argv[1], (void *)&serviceaddr.sin_addr.s_addr) == -1)
		err_sys("inet_pton error");

	// 链接服务器
	if(connect(sockfd, (struct sockaddr *)&serviceaddr, sizeof(serviceaddr)) == -1)
		err_sys("connect error");

	if(register_test(sockfd, argv[2], argv[3]) == -1)			// 测试登录事件
	{
			close(sockfd);
			return 0;
	}

	close(sockfd);
	return 0;
}

int register_test(int sockfd, const char *username, const char *userpass)
{
	CLI_TO_SERV							client_msg;
	SERV_TO_CLI							server_msg;
	int 										recv_len;
	/*
	*		登录事件
	*/
		//printf("%s\n", "用户登录");
		memset(&client_msg, 0, sizeof(client_msg));
		client_msg.cts_events = CL_ENROLL;
		strncpy(client_msg.cts_fname,username, sizeof(client_msg.cts_fname));
		strncpy(client_msg.cts_main,userpass, sizeof(client_msg.cts_main));

		if(send(sockfd, &client_msg, sizeof(client_msg), 0) == -1)
			err_sys("send error");

		if(recv(sockfd, &server_msg, sizeof(server_msg), 0) == -1)
			err_sys("recv error");

    printf("%s\n", "=======================================");
    printf("event    : %d\n", server_msg.stc_events);			// 事件
    printf("fname    : %s\n", server_msg.stc_fname);			// 文件名

		if(server_msg.stc_events != SE_OK)
		{
				printf("%s\n", "注册处理");
				//close(sockfd);
				return -1;
		}

		printf("%s : %s\n", "注册处理", server_msg.stc_fname);

		return 0;
}
