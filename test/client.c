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
int login_test(int sockfd, const char *username, const char *userpass);
// 进入目录
int mkin_test(int sockfd, const char *dir);
// 退出目录
int mkex_text(int sockfd);
// 删除文件
int remove_file(int sockfd, const char*file);
// 删除目录
int remove_dir(int sockfd, const char *dir);
// 上传文件
int upload_test(int sockfd, const char *path, const char *file);
// 下载文件
int download_test(int sockfd, const char *path, const char *file);
// 创建目录
int create_dir_test(int sockfd ,const char *dir);

int main(int argc, char *argv[])
{
	int											sockfd;
	struct sockaddr_in			serviceaddr;
	int											swit_case;
	char										dir_name[20];
	char										file_name[20];
	if(argc != 4)
		err_que("client Ipaddress username userpass");

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

	if(login_test(sockfd, argv[2], argv[3]) == -1)			// 测试登录事件
	{
			close(sockfd);
			return 0;
	}

	while(1)
	{
		printf("%s\n", "请选择操作");
		scanf("%d", &swit_case);
		getchar();
		printf("你的操作 : %d\n", swit_case);
		switch (swit_case) {
			case 1:		// 进入目录
				memset(dir_name, 0, sizeof(dir_name));
				printf("%s\n", "要进入的目录名");
				fgets(dir_name, 20, stdin);
				if(dir_name[strlen(dir_name)-1] == '\n')
					dir_name[strlen(dir_name)-1] = 0;
				mkin_test(sockfd, dir_name);
				break;
			case 2:	// 退出目录
				mkex_text(sockfd);
				break;
			case 3:	// 删除文件
				memset(file_name, 0, sizeof(file_name));
				printf("%s\n", "请输入要删除的文件名");
				fgets(file_name, 20, stdin);
				if(file_name[strlen(file_name)-1] == '\n')
					file_name[strlen(file_name)-1] = 0;
				remove_file(sockfd, file_name);
				break;
			case 4:	// 删除目录
				memset(dir_name, 0, sizeof(dir_name));
				printf("%s\n", "请输入要删除的目录名");
				fgets(dir_name, 20, stdin);
				if(dir_name[strlen(dir_name)-1] == '\n')
					dir_name[strlen(dir_name)-1] = 0;
				remove_dir(sockfd, dir_name);
				break;
			case 5: // 创建目录
				memset(dir_name, 0, sizeof(dir_name));
				printf("%s\n", "请输入要创建的目录名");
				fgets(dir_name, 20, stdin);
				if(dir_name[strlen(dir_name)-1] == '\n')
					dir_name[strlen(dir_name)-1] = 0;
				create_dir_test(sockfd, dir_name);
				break;
			case 6: // 上传文件
				memset(dir_name, 0, sizeof(dir_name));
				memset(file_name, 0, sizeof(file_name));

				printf("%s\n", "请输入要上传的路径名");
				fgets(dir_name, 20, stdin);
				if(dir_name[strlen(dir_name)-1] == '\n')
					dir_name[strlen(dir_name)-1] = 0;
				printf("%s\n", "请输入要上传的文件名");
				fgets(file_name, 20, stdin);
				if(file_name[strlen(file_name)-1] == '\n')
					file_name[strlen(file_name)-1] = 0;
				upload_test(sockfd, dir_name, file_name);
				break;
			case 7: // 下载文件
				memset(dir_name, 0, sizeof(dir_name));
				printf("%s\n", "请输入要保存的路径名");
				memset(file_name, 0, sizeof(file_name));
				fgets(dir_name, 20, stdin);
				if(dir_name[strlen(dir_name)-1] == '\n')
					dir_name[strlen(dir_name)-1] = 0;
				printf("%s\n", "请输入要下载的文件名");
				fgets(file_name, 20, stdin);
				if(file_name[strlen(file_name)-1] == '\n')
					file_name[strlen(file_name)-1] = 0;
				download_test(sockfd, dir_name, file_name);
				break;
			case 0:
				puts("1. 进入目录");
				puts("2. 退出目录");
				puts("3. 删除文件");
				puts("4. 删除目录");
				puts("5. 创建目录");
				puts("6. 上传文件");
				puts("7. 下载文件");
			default:
				;
		}
	}

	close(sockfd);
	return 0;
}

int login_test(int sockfd, const char *username, const char *userpass)
{
	CLI_TO_SERV							client_msg;
	SERV_TO_CLI							server_msg;
	int 										recv_len;
	/*
	*		登录事件
	*/
		//printf("%s\n", "用户登录");
		memset(&client_msg, 0, sizeof(client_msg));
		client_msg.cts_events = CL_LOGIN;
		strncpy(client_msg.cts_fname,username, sizeof(client_msg.cts_fname));
		strncpy(client_msg.cts_main,userpass, sizeof(client_msg.cts_main));

		if(send(sockfd, &client_msg, sizeof(client_msg), 0) == -1)
			err_sys("send error");

		if(recv(sockfd, &server_msg, sizeof(server_msg), 0) == -1)
			err_sys("recv error");


		if(server_msg.stc_events != SE_OK)
		{
				printf("%s\n", "登录失败");
				//close(sockfd);
				return -1;
		}

		printf("%s : %s\n", "登录成功", server_msg.stc_fname);

		while (recv_len = recv(sockfd, &server_msg, sizeof(server_msg), 0))
		{
			if(recv_len == -1)
				err_sys("recv error");

			if(server_msg.stc_events != SE_DONOW)
					break;

			printf("%s\n", "=======================================");
			printf("event    : %d\n", server_msg.stc_events);			// 事件
			printf("fname    : %s\n", server_msg.stc_fname);			// 文件名
			printf("ftype    : %d\n", server_msg.stc_ftype);			// 文件格式
			printf("fsize    : %d\n", server_msg.stc_fsize);			// 文件大小
			printf("stc_main : %s\n", server_msg.stc_main);				// 当前目录
		}

		return 0;
}
int mkin_test(int sockfd, const char *dir)
{
	CLI_TO_SERV							client_msg;
	SERV_TO_CLI							server_msg;
	int 										recv_len;
	/*
	*	进入目录事件
	*/
	printf("%s\n", "进入目录");
	memset(&client_msg, 0, sizeof(client_msg));
	client_msg.cts_events = CL_CDDIR;
	strncpy(client_msg.cts_fname,dir, sizeof(client_msg.cts_fname));
	if(send(sockfd, &client_msg, sizeof(client_msg), 0) == -1)
		err_sys("send error");

	if(recv(sockfd, &server_msg, sizeof(server_msg), 0) == -1)
		err_sys("recv error");
	/*
	printf("%s\n", "=======================================");
	printf("event    : %d\n", server_msg.stc_events);			// 事件
	printf("fname    : %s\n", server_msg.stc_fname);			// 文件名
	printf("ftype    : %d\n", server_msg.stc_ftype);			// 文件格式
	printf("fsize    : %d\n", server_msg.stc_fsize);			// 文件大小
	printf("stc_main : %s\n", server_msg.stc_main);				// 当前目录
	*/
	if(server_msg.stc_events != SE_OK)
	{
			printf("%s\n", "操作失败");
			//close(sockfd);
			return -1;
	}

	while (recv_len = recv(sockfd, &server_msg, sizeof(server_msg), 0))
	{
		if(recv_len == -1)
			err_sys("recv error");

		if(server_msg.stc_events != SE_DONOW)
				break;

		printf("%s\n", "=======================================");
		printf("event    : %d\n", server_msg.stc_events);			// 事件
		printf("fname    : %s\n", server_msg.stc_fname);			// 文件名
		printf("ftype    : %d\n", server_msg.stc_ftype);			// 文件格式
		printf("fsize    : %d\n", server_msg.stc_fsize);			// 文件大小
		printf("stc_main : %s\n", server_msg.stc_main);				// 当前目录

	}

	printf("%s\n", "任务执行成功");

	return 0;
}
int mkex_text(int sockfd)
{
	CLI_TO_SERV							client_msg;
	SERV_TO_CLI							server_msg;
	int 										recv_len;
	/*
	* 退出目录事件
	*/

	printf("%s\n", "退出目录");
	memset(&client_msg, 0, sizeof(client_msg));
	client_msg.cts_events = CL_EXDIR;
	if(send(sockfd, &client_msg, sizeof(client_msg), 0) == -1)
		err_sys("send error");

	if(recv(sockfd, &server_msg, sizeof(server_msg), 0) == -1)
		err_sys("recv error");
	/*
	printf("%s\n", "=======================================");
	printf("event    : %d\n", server_msg.stc_events);			// 事件
	printf("fname    : %s\n", server_msg.stc_fname);			// 文件名
	printf("ftype    : %d\n", server_msg.stc_ftype);			// 文件格式
	printf("fsize    : %d\n", server_msg.stc_fsize);			// 文件大小
	printf("stc_main : %s\n", server_msg.stc_main);				// 当前目录
	*/
	if(server_msg.stc_events != SE_OK)
	{
			printf("%s\n", "操作失败");
			//close(sockfd);
			return -1;
	}

	while (recv_len = recv(sockfd, &server_msg, sizeof(server_msg), 0))
	{
		if(recv_len == -1)
			err_sys("recv error");

		if(server_msg.stc_events != SE_DONOW)
				break;

		printf("%s\n", "=======================================");
		printf("event    : %d\n", server_msg.stc_events);			// 事件
		printf("fname    : %s\n", server_msg.stc_fname);			// 文件名
		printf("ftype    : %d\n", server_msg.stc_ftype);			// 文件格式
		printf("fsize    : %d\n", server_msg.stc_fsize);			// 文件大小
		printf("stc_main : %s\n", server_msg.stc_main);				// 当前目录
	}

	printf("%s\n", "任务执行成功");

	return 0;
}
int remove_file(int sockfd, const char*file)
{
	CLI_TO_SERV							client_msg;
	SERV_TO_CLI							server_msg;
	int 										recv_len;

	/*
	*		删除目录事件
	*/

	printf("%s\n", "删除文件");
	memset(&client_msg, 0, sizeof(client_msg));
	client_msg.cts_events = CL_RMFIL;
	strncpy(client_msg.cts_fname,file, sizeof(client_msg.cts_fname));
	if(send(sockfd, &client_msg, sizeof(client_msg), 0) == -1)
		err_sys("send error");

	if(recv(sockfd, &server_msg, sizeof(server_msg), 0) == -1)
		err_sys("recv error");

	/*
	printf("%s\n", "=======================================");
	printf("event    : %d\n", server_msg.stc_events);			// 事件
	printf("fname    : %s\n", server_msg.stc_fname);			// 文件名
	printf("ftype    : %d\n", server_msg.stc_ftype);			// 文件格式
	printf("fsize    : %d\n", server_msg.stc_fsize);			// 文件大小
	printf("stc_main : %s\n", server_msg.stc_main);				// 当前目录
	*/

	if(server_msg.stc_events != SE_OK)
	{
			printf("%s\n", "操作失败");
			//close(sockfd);
			return -1;
	}

	printf("%s\n", "任务执行成功");

	return 0;
}
int remove_dir(int sockfd, const char *dir)
{
	CLI_TO_SERV							client_msg;
	SERV_TO_CLI							server_msg;
	int 										recv_len;
	/*
	*		删除目录事件
	*/

	printf("%s\n", "删除目录");
	memset(&client_msg, 0, sizeof(client_msg));
	client_msg.cts_events = CL_RMDIR;
	strncpy(client_msg.cts_fname,dir, sizeof(client_msg.cts_fname));
	if(send(sockfd, &client_msg, sizeof(client_msg), 0) == -1)
		err_sys("send error");

	if(recv(sockfd, &server_msg, sizeof(server_msg), 0) == -1)
		err_sys("recv error");

	/*
	printf("%s\n", "=======================================");
	printf("event    : %d\n", server_msg.stc_events);			// 事件
	printf("fname    : %s\n", server_msg.stc_fname);			// 文件名
	printf("ftype    : %d\n", server_msg.stc_ftype);			// 文件格式
	printf("fsize    : %d\n", server_msg.stc_fsize);			// 文件大小
	printf("stc_main : %s\n", server_msg.stc_main);				// 当前目录
	*/

	if(server_msg.stc_events != SE_OK)
	{
			printf("%s\n", "操作失败");
			//close(sockfd);
			return -1;
	}
	printf("%s\n", "任务执行成功");
	return 0;
}
int upload_test(int sockfd, const char *path, const char *file)
{
	CLI_TO_SERV							client_msg;
	SERV_TO_CLI							server_msg;
	int 										recv_len;
	TRANS_CONTEXT 					file_context;
	int 										fin;
	uint8_t									recv_flags;
	int16_t									read_len;

	/*
	*	 上传文件
	*/

	printf("%s\n", "上传文件");
	memset(&client_msg, 0, sizeof(client_msg));
	client_msg.cts_events = CL_SNDFL;
	strncpy(client_msg.cts_fname,file, sizeof(client_msg.cts_fname));
	//strncpy(client_msg.cts_main,"/one/thir", sizeof(client_msg.cts_main));

	if(send(sockfd, &client_msg, sizeof(client_msg), 0) == -1)
		err_sys("send error");

	/*
	* 接受问题响应
	*/

	if(recv(sockfd, &server_msg, sizeof(server_msg), 0) == -1)
		err_sys("recv error");

	/*
	printf("%s\n", "=======================================");
	printf("event    : %d\n", server_msg.stc_events);			// 事件
	printf("fname    : %s\n", server_msg.stc_fname);			// 文件名
	printf("ftype    : %d\n", server_msg.stc_ftype);			// 文件格式
	printf("fsize    : %d\n", server_msg.stc_fsize);			// 文件大小
	printf("stc_main : %s\n", server_msg.stc_main);				// 当前目录
	*/

	if(server_msg.stc_events != SE_OK)
	{
			printf("%s\n", "请求失败");
			//close(sockfd);
			return -1;
	}


	printf("%s\n", "请求成功正在上传");

	if((fin = open (path, O_RDONLY)) == -1)
		err_sys("文件打开失败");

	while(1)
	{
		memset(&file_context, 0, sizeof(file_context));
		printf("%s\n", "*");
		read_len = read(fin, &file_context.trans_data, sizeof(file_context.trans_data));

		printf("-");

		if(read_len == -1)
		{
			close(sockfd);
			close(fin);
			err_sys("read error");
		}

		else if(read_len == 0)
			file_context.trans_flags = 1;
		else
			file_context.trans_flags = 0;

		file_context.trans_len = htons(read_len);

		if(send(sockfd, &file_context, sizeof(file_context), 0) == -1)
		{
			close(sockfd);
			close(fin);
			err_sys("read error");
		}

		if(recv(sockfd, &recv_flags,1, 0) == -1)
    {
			close(sockfd);
			close(fin);
			err_sys("read error");
    }

		if(read_len == 0)
			break;
	}

	printf("%s\n", "文件发送完成");

	if(recv(sockfd, &server_msg, sizeof(server_msg), 0) == -1)
		err_sys("recv error");
	/*
	printf("%s\n", "=======================================");
	printf("event    : %d\n", server_msg.stc_events);			// 事件
	printf("fname    : %s\n", server_msg.stc_fname);			// 文件名
	printf("ftype    : %d\n", server_msg.stc_ftype);			// 文件格式
	printf("fsize    : %d\n", server_msg.stc_fsize);			// 文件大小
	printf("stc_main : %s\n", server_msg.stc_main);				// 当前目录
	*/

	if(server_msg.stc_events != SE_OK)
	{
		printf("%s\n", "上传任务失败");
		return -1;
	}

	printf("%s\n", "上传任务成功");

	return 0;
}
int download_test(int sockfd, const char *path, const char *file)
{
	CLI_TO_SERV							client_msg;
	SERV_TO_CLI							server_msg;
	TRANS_CONTEXT file_context;
	int 					fout;
	int16_t				read_len;
	int 					recv_len;

	/*
	*	 下载文件
	*/

	//printf("%s\n", "下载文件");
	memset(&client_msg, 0, sizeof(client_msg));
	client_msg.cts_events = CL_RCVFL;
	strncpy(client_msg.cts_fname,file, sizeof(client_msg.cts_fname));
	//strncpy(client_msg.cts_main,"/one/thir", sizeof(client_msg.cts_main));

	if(send(sockfd, &client_msg, sizeof(client_msg), 0) == -1)
		err_sys("send error");

	/*
	* 接受问题响应
	*/

	if(recv(sockfd, &server_msg, sizeof(server_msg), 0) == -1)
		err_sys("recv error");
	/*
	printf("%s\n", "=======================================");
	printf("event    : %d\n", server_msg.stc_events);			// 事件
	printf("fname    : %s\n", server_msg.stc_fname);			// 文件名
	printf("ftype    : %d\n", server_msg.stc_ftype);			// 文件格式
	printf("fsize    : %d\n", server_msg.stc_fsize);			// 文件大小
	printf("stc_main : %s\n", server_msg.stc_main);				// 当前目录
	*/
	if(server_msg.stc_events != SE_OK)
	{
			printf("%s\n", "下载请求失败");
			//close(sockfd);
			return -1;
	}

	printf("%s\n", "请求成功正在下载");


	if((fout = open (path, O_RDWR|O_CREAT|O_EXCL, S_IRUSR|S_IWUSR)) == -1)
		err_sys("文件打开失败");

	while(1)
	{
		memset(&file_context, 0,sizeof(file_context));
		if((recv_len = recv(sockfd, &file_context, sizeof(file_context), 0)) == -1)
			err_sys("recv file error");

		if(recv_len == -1)
		{
			close(sockfd);
			close(fout);
			unlink(path);
			err_sys("recv error");
		}

		if(recv_len == 0)
		{
			close(sockfd);
			close(fout);
			unlink(path);
			break;
		}

		if(write(fout, file_context.trans_data, ntohs(file_context.trans_len)) == -1)
		{
			close(sockfd);
			close(fout);
			unlink(path);
			err_sys("write error");
		}

		if(send(sockfd, "1", 1, 0) == -1)
			err_sys("recv error");

		if(ntohs(file_context.trans_len) == 0)
			break;

		printf("-");
	}

	if(recv(sockfd, &server_msg, sizeof(server_msg), 0) == -1)
		err_sys("recv error");
	/*
	printf("%s\n", "=======================================");
	printf("event    : %d\n", server_msg.stc_events);			// 事件
	printf("fname    : %s\n", server_msg.stc_fname);			// 文件名
	printf("ftype    : %d\n", server_msg.stc_ftype);			// 文件格式
	printf("fsize    : %d\n", server_msg.stc_fsize);			// 文件大小
	printf("stc_main : %s\n", server_msg.stc_main);				// 当前目录
	*/

	if(server_msg.stc_events != SE_OK)
	{
			printf("%s\n", "下载任务失败");
			//close(sockfd);
			return -1;
	}
	printf("%s\n", "下载任务执行成功");

	return 0;
}
int create_dir_test(int sockfd ,const char *dir)
{
	CLI_TO_SERV							client_msg;
	SERV_TO_CLI							server_msg;
	int 										recv_len;
	/*
	*		创建目录事件
	*/

	printf("%s\n", "创建目录");
	memset(&client_msg, 0, sizeof(client_msg));
	client_msg.cts_events = CL_CRDIR;
	strncpy(client_msg.cts_fname,dir, sizeof(client_msg.cts_fname));
	if(send(sockfd, &client_msg, sizeof(client_msg), 0) == -1)
		err_sys("send error");

	if(recv(sockfd, &server_msg, sizeof(server_msg), 0) == -1)
		err_sys("recv error");

	/*
	printf("%s\n", "=======================================");
	printf("event    : %d\n", server_msg.stc_events);			// 事件
	printf("fname    : %s\n", server_msg.stc_fname);			// 文件名
	printf("ftype    : %d\n", server_msg.stc_ftype);			// 文件格式
	printf("fsize    : %d\n", server_msg.stc_fsize);			// 文件大小
	printf("stc_main : %s\n", server_msg.stc_main);				// 当前目录
	*/

	if(server_msg.stc_events != SE_OK)
	{
			printf("%s\n", "操作失败");
			//close(sockfd);
			return -1;
	}
	printf("%s\n", "任务执行成功");
	return 0;
}
