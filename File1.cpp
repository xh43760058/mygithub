#include <Winsock2.h>
#include <time.h>
#include <stdio.h>

#include <string.h>
#include <direct.h>     // 目录头文件
#pragma comment(lib,"Ws2_32.lib")

// http 默认端口是80，如果80端口被占用那么改个端口即可
#define DEFAULT_PORT 80
#define BUF_LENGTH 1024
#define MIN_BUF 128
#define USER_ERROR -1
#define SERVER "Server: csr_http1.1\r\n"

struct Unit{
	char sstr[50];
	char dstr[50];

};

int SaveToFile(char *filename ,Unit *unit_save,short counter);
int file_not_found(SOCKET sAccept);
char *GetIniKeyString(char *title,char *key,char *filename);
int file_ok(SOCKET sAccept, long flen);
int send_file(SOCKET sAccept, FILE *resource);
int send_not_found(SOCKET sAccept);
int openFileSend(char *path,SOCKET sAccept);
char *getKeyFromArg(char *key,char *url,char *value);
int ChangeFileToSend(char *filename,SOCKET sAccept,Unit *unit_sign,short counter);

DWORD WINAPI SimpleHTTPServer(LPVOID lparam)
{
    SOCKET sAccept = (SOCKET)(LPVOID)lparam;
    char recv_buf[BUF_LENGTH];
    char method[MIN_BUF];
	char url[MIN_BUF];
	char path[_MAX_PATH];
	char setpath[_MAX_PATH];
	char arg[MIN_BUF];
	char password[MIN_BUF];
	char id[MIN_BUF];

	int i, j, m;

    // 缓存清0，每次操作前都要记得清缓存，养成习惯；
	// 不清空可能出现的现象：输出乱码、换台机器乱码还各不相同
	// 原因：不清空会输出遇到 '\0'字符为止，所以前面的不是'\0' 也会一起输出
	memset(recv_buf,0,sizeof(recv_buf));
    if (recv(sAccept,recv_buf,sizeof(recv_buf),0) == SOCKET_ERROR)   //接收错误
    {
        printf("recv() Failed:%d\n",WSAGetLastError());
        return USER_ERROR;
    }
    else
        printf("recv data from client:%s\n",recv_buf); //接收成功，打印请求报文

    //处理接收数据
	i = 0; j = 0; m=0;
    // 取出第一个单词，一般为HEAD、GET、POST
    while (!(' ' == recv_buf[j]) && (i < sizeof(method) - 1))
    {
        method[i] = recv_buf[j];
        i++; j++;
    }
    method[i] = '\0';   // 结束符，这里也是初学者很容易忽视的地方

    // 如果不是GET或HEAD方法，则直接断开本次连接
    // 如果想做的规范些可以返回浏览器一个501未实现的报头和页面
	if (stricmp(method, "GET") && stricmp(method, "HEAD"))
    {
        closesocket(sAccept); //释放连接套接字，结束与该客户的通信
        printf("not get or head method.\nclose ok.\n");
        printf("***********************\n\n\n\n");
        return USER_ERROR;
    }
    printf("method: %s\n", method);

    // 提取出第二个单词(url文件路径，空格结束)，并把'/'改为windows下的路径分隔符'\'
    // 这里只考虑静态请求(比如url中出现'?'表示非静态，需要调用CGI脚本，'?'后面的字符串表示参数，多个参数用'+'隔开
    // 例如：www.csr.com/cgi_bin/cgi?arg1+arg2 该方法有时也叫查询，早期常用于搜索)
	i = 0;

	while ((' ' == recv_buf[j]) && (j < sizeof(recv_buf)))
        j++;
    while (!(' ' == recv_buf[j]) && (i < sizeof(recv_buf) - 1) && (j < sizeof(recv_buf)))
    {
		if (recv_buf[j] == '/')
			url[i] = '\\';
		else if(recv_buf[j] == ' ')
			break;
		else if(recv_buf[j] == '?')
		{
			j++;
			while (!(' ' == recv_buf[j]) && (j < sizeof(recv_buf)))
			{
				if(recv_buf[j] == ' ')
					break;
				else if(recv_buf[j] == '&')
					arg[m]=',';
				else
				   arg[m]=recv_buf[j];
				m++; j++;
			}
			break;
        }
        else
            url[i] = recv_buf[j];
		i++; j++;
	}

	url[i] = '\0';
	printf("url: %s\n",url);
	arg[m]='\0';
	char page[10];
	getKeyFromArg("page=",arg,page);

	if(strlen(page)==0)      //第一次登陆
	{
		_getcwd(path,_MAX_PATH);
		strcat(path,"\\index.html");
	   openFileSend(path,sAccept);
	   goto exit;
	}

	if(page[0]=='1')      //      用户认证
	{
		_getcwd(path,_MAX_PATH);
		_getcwd(setpath,_MAX_PATH);
		char user[20];
		char password[20];
		getKeyFromArg("user=",arg,user);
		getKeyFromArg("password=",arg,password);
		 if(stricmp(user, "admin")==0&&stricmp(password, "1")==0)
		 {
		   strcat(path,url);
		   strcat(setpath,"\\set.txt");
		   Unit *U=new Unit[3];
		   strcpy(U[0].sstr,"$ID$");
		   strcpy(U[0].dstr,GetIniKeyString("main","ID",setpath));
		   strcpy(U[1].sstr,"$IP$");
		   strcpy(U[1].dstr,GetIniKeyString("main","IP",setpath));
		   strcpy(U[2].sstr,"$PORT$");
		   strcpy(U[2].dstr,GetIniKeyString("main","PORT",setpath));
		   ChangeFileToSend(path,sAccept,U,3);
		   delete U;
		   goto exit;
		 }
		 else
		 {
			strcat(path,"\\index.html");
		   openFileSend(path,sAccept);
		   goto exit;

		 }
	}

	if(page[0]=='2')
	{
		char NewId[10];
		char NewIp[20];
		char NewPort[10];
		Unit *U=new Unit[3];
        _getcwd(setpath,_MAX_PATH);
		strcat(setpath,"\\set.txt");
		strcpy(U[0].sstr,"ID=");
		getKeyFromArg("id=",arg,U[0].dstr);
		strcpy(U[1].sstr,"IP=");
		getKeyFromArg("ip=",arg,U[1].dstr);
		strcpy(U[2].sstr,"PORT=");
		getKeyFromArg("port=",arg,U[2].dstr);
		SaveToFile(setpath,U,3);
        delete U;
        goto exit;
	}

	// 没有该文件则发送一个简单的404-file not found的html页面，并断开本次连接

	file_not_found(sAccept);
	// 如果method是GET，则发送自定义的file not found页面
	if(0 == stricmp(method, "GET"))
		send_not_found(sAccept);

	//closesocket(sAccept); //释放连接套接字，结束与该客户的通信
	printf("file not found.\nclose ok.\n");
	printf("***********************\n\n\n\n");


 exit:
	closesocket(sAccept); //释放连接套接字，结束与该客户的通信
	printf("close ok.\n");
	printf("***********************\n\n\n\n");

    return 0;

}

// 发送404 file_not_found报头
int file_not_found(SOCKET sAccept)
{
    char send_buf[MIN_BUF];
	sprintf(send_buf, "HTTP/1.1 404 NOT FOUND\r\n");
    send(sAccept, send_buf, strlen(send_buf), 0);
	sprintf(send_buf, "Connection: keep-alive\r\n");
    send(sAccept, send_buf, strlen(send_buf), 0);
    sprintf(send_buf, SERVER);
    send(sAccept, send_buf, strlen(send_buf), 0);
    sprintf(send_buf, "Content-Type: text/html\r\n");
    send(sAccept, send_buf, strlen(send_buf), 0);
    sprintf(send_buf, "\r\n");
    send(sAccept, send_buf, strlen(send_buf), 0);
    return 0;
}

// 发送200 ok报头
int file_ok(SOCKET sAccept, long flen)
{
    char send_buf[MIN_BUF];
	sprintf(send_buf, "HTTP/1.1 200 OK\r\n");
    send(sAccept, send_buf, strlen(send_buf), 0);
    sprintf(send_buf, "Connection: keep-alive\r\n");
    send(sAccept, send_buf, strlen(send_buf), 0);
	sprintf(send_buf, SERVER);
    send(sAccept, send_buf, strlen(send_buf), 0);
    sprintf(send_buf, "Content-Length: %ld\r\n", flen);
    send(sAccept, send_buf, strlen(send_buf), 0);
    sprintf(send_buf, "Content-Type: text/html\r\n");
    send(sAccept, send_buf, strlen(send_buf), 0);
    sprintf(send_buf, "\r\n");
    send(sAccept, send_buf, strlen(send_buf), 0);
    return 0;
}

// 发送自定义的file_not_found页面
int send_not_found(SOCKET sAccept)
{
    char send_buf[MIN_BUF];
    sprintf(send_buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
    send(sAccept, send_buf, strlen(send_buf), 0);
    sprintf(send_buf, "<BODY><h1 align='center'>404</h1><br/><h1 align='center'>file not found.</h1>\r\n");
    send(sAccept, send_buf, strlen(send_buf), 0);
    sprintf(send_buf, "</BODY></HTML>\r\n");
    send(sAccept, send_buf, strlen(send_buf), 0);
    return 0;
}

// 发送请求的资源
int send_file(SOCKET sAccept, FILE *resource)
{
    char send_buf[BUF_LENGTH];
    while (1)
    {
        memset(send_buf,0,sizeof(send_buf));       //缓存清0
        fgets(send_buf, sizeof(send_buf), resource);
    //  printf("send_buf: %s\n",send_buf);
		if (SOCKET_ERROR == send(sAccept, send_buf, strlen(send_buf), 0))
		{
			printf("send() Failed:%d\n",WSAGetLastError());
			return USER_ERROR;
		}
        if(feof(resource))
            return 0;
    }
}

int main()
{
    WSADATA wsaData;
	SOCKET sListen,sAccept;        //服务器监听套接字，连接套接字
    int serverport=DEFAULT_PORT;   //服务器端口号
    struct sockaddr_in ser,cli;   //服务器地址，客户端地址
	int iLen;


    printf("-----------------------\n");
    printf("Server waiting\n");
    printf("-----------------------\n");

    //第一步：加载协议栈
    if (WSAStartup(MAKEWORD(2,2),&wsaData) !=0)
    {
        printf("Failed to load Winsock.\n");
        return USER_ERROR;
    }

    //第二步：创建监听套接字，用于监听客户请求
    sListen =socket(AF_INET,SOCK_STREAM,0);
    if (sListen == INVALID_SOCKET)
    {
        printf("socket() Failed:%d\n",WSAGetLastError());
        return USER_ERROR;
    }

    //创建服务器地址：IP+端口号
    ser.sin_family=AF_INET;
    ser.sin_port=htons(serverport);               //服务器端口号
    ser.sin_addr.s_addr=htonl(INADDR_ANY);   //服务器IP地址，默认使用本机IP

    //第三步：绑定监听套接字和服务器地址
    if (bind(sListen,(LPSOCKADDR)&ser,sizeof(ser))==SOCKET_ERROR)
    {
        printf("blind() Failed:%d\n",WSAGetLastError());
        return USER_ERROR;
    }

    //第五步：通过监听套接字进行监听
    if (listen(sListen,5)==SOCKET_ERROR)
    {
        printf("listen() Failed:%d\n",WSAGetLastError());
        return USER_ERROR;
    }
    while (1)  //循环等待客户的请求
    {
        //第六步：接受客户端的连接请求，返回与该客户建立的连接套接字
        iLen=sizeof(cli);
        sAccept=accept(sListen,(struct sockaddr*)&cli,&iLen);
        if (sAccept==INVALID_SOCKET)
        {
            printf("accept() Failed:%d\n",WSAGetLastError());
            break;
        }
        //第七步，创建线程接受浏览器请求
        DWORD ThreadID;
        CreateThread(NULL,0,SimpleHTTPServer,(LPVOID)sAccept,0,&ThreadID);
    }
    closesocket(sListen);
    WSACleanup();
    return 0;
}


char *getKeyFromArg(char *key,char *url,char *value)
{
	value[0]=0;
	char *pos1=strstr(url,key);
	if(pos1==NULL)return NULL;
	char *pos2=pos1;
	while(!((*pos2==',')||(*pos2==0)))
	{
		pos2++;
	}
	pos1+=strlen(key);
	int len=pos2-pos1;
	memcpy(value,pos1,len);
	value[len]=0;
	return value;

}

int openFileSend(char *path, SOCKET sAccept)
{
	FILE *resource = fopen(path,"rb");

		// 没有该文件则发送一个简单的404-file not found的html页面，并断开本次连接
		/*if(resource==NULL)
		{
			file_not_found(sAccept);
			// 如果method是GET，则发送自定义的file not found页面
			if(0 == stricmp(method, "GET"))
				send_not_found(sAccept);

			closesocket(sAccept); //释放连接套接字，结束与该客户的通信
			printf("file not found.\nclose ok.\n");
			printf("***********************\n\n\n\n");
			return USER_ERROR;
		} */

		// 求出文件长度，记得重置文件指针到文件头
		fseek(resource,0,SEEK_SET);
		fseek(resource,0,SEEK_END);
		long flen=ftell(resource);
		printf("file length: %ld\n", flen);
		fseek(resource,0,SEEK_SET);

		// 发送200 OK HEAD
		file_ok(sAccept, flen);

		// 如果是GET方法则发送请求的资源
		//if(0 == stricmp(method, "GET"))
		//{
			if(0 == send_file(sAccept, resource))
				printf("file send ok.\n");
			else
				printf("file send fail.\n");
		//}
		fclose(resource);
		return 0;
}

int ChangeFileToSend(char *filename,SOCKET sAccept,Unit *unit_sign,short counter)
{
	char line[1024];
	FILE *pfile=fopen(filename,"r+");
	while(!feof(pfile))
	{
		  char *index=NULL;
		  fgets(line,1024,pfile);
		   for(int i=0;i<counter;i++)
		   {
			   char * str1= unit_sign[i].sstr;
			   char * str2= unit_sign[i].dstr;

			   index=strstr(line,str1);
				  if(index)
				  {
					 int d1=strlen(str1);
					 int d2=strlen(str2);
					 short len=strlen(index+d1)+1;
					 if(d1!=d2)
					 {
					   memmove(
							index+d1+d2-d1,
						   index+d1,
					   len);
					 }
					 memcpy(index,str2,d2);
				  }

		   }
		   if (SOCKET_ERROR == send(sAccept, line, strlen(line), 0))
		   {
			   printf("send() Failed:%d\n",WSAGetLastError());
			   return USER_ERROR;
		   }

	}

   fclose(pfile);

 return 0;
}


char *GetIniKeyString(char *title,char *key,char *filename)
{
	FILE *fp;
	char szLine[1024];
	static char tmpstr[1024];
	int rtnval;
	int i = 0;
	int flag = 0;
	char *tmp;

	if((fp = fopen(filename, "r")) == NULL)
	{
		printf("have   no   such   file \n");
		return NULL;
	}
	while(!feof(fp))
	{
		rtnval = fgetc(fp);
		if(rtnval == EOF)
		{
			break;
		}
		else
		{
			szLine[i++] = rtnval;
		}
		if(rtnval == '\n')
		{
#ifdef LINUX
			i--;
#endif
			szLine[--i] = '\0';
			i = 0;
			tmp = strchr(szLine, '=');

			if(( tmp != NULL )&&(flag == 1))
			{
				if(strstr(szLine,key)!=NULL)
				{
					//ע����
					if ('#' == szLine[0])
					{
					}
					else if ( '/' == szLine[0] && '/' == szLine[1] )
					{

					}
					else
					{
					   //�Ҵ�key��Ӧ����
					   strcpy(tmpstr,tmp+1);
						fclose(fp);
						return tmpstr;
					}
				}
			}
			else
			{
				strcpy(tmpstr,"[");
				strcat(tmpstr,title);
				strcat(tmpstr,"]");
				if( strncmp(tmpstr,szLine,strlen(tmpstr)) == 0 )
				{
					//�ҵ�title
					flag = 1;
				}
			}
		}
	}
	fclose(fp);
	return NULL;
}


int SaveToFile(char *filename ,Unit *unit_save,short counter)
{

	char line[1024];

	FILE *pfile=NULL;
	pfile=fopen(filename,"r+");
	while(!feof(pfile))
	{
		  char *index=NULL;
		  fgets(line,1024,pfile);
		  for(int i=0;i<counter;i++)
		  {
			  char * str1= unit_save[i].sstr;
			  char * str2= unit_save[i].dstr;

			  index=strstr(line,str1);
			  if(index)
			  {

				 int d2=strlen(str2);
				 int d1=strlen(str1);
				 short len=strlen(index+d1)-1;
				 fseek(pfile,-len,SEEK_CUR);
				 fputs(str2,pfile);
				 fseek(pfile,1,SEEK_CUR);
				 fflush(pfile);
			  }
		  }
          

	}

   fclose(pfile);

 return 0;
}


