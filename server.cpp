#define WIN32_LEAN_AND_MEAN
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include<windows.h>
#include<WinSock2.h>
#include<stdio.h>

#include <vector>
#include <iostream>
using namespace std;


#pragma comment(lib,"ws2_32.lib")

enum CMD
{
	CMD_LOGIN,
	CMD_LOGIN_RESULT,
	CMD_LOGOUT,
	CMD_LOGOUT_RESULT,
	CMD_NEW_USER_JOIN,
	CMD_ERROR
};

struct DataHeader
{
	short dataLength;
	short cmd;
};

//DataPackage
struct Login: public DataHeader
{
	Login()
	{
		dataLength = sizeof(Login);
		cmd = CMD_LOGIN;
	}
	char userName[32];
	char PassWord[32];
};

struct LoginResult : public DataHeader
{
	LoginResult()
	{
		dataLength = sizeof(LoginResult);
		cmd = CMD_LOGIN_RESULT;
		result = 0;
	}
	int result;
};

struct Logout : public DataHeader
{
	Logout()
	{
		dataLength = sizeof(Logout);
		cmd = CMD_LOGOUT;
	}
	char userName[32];
};

struct LogoutResult : public DataHeader
{
	LogoutResult()
	{
		dataLength = sizeof(LogoutResult);
		cmd = CMD_LOGOUT_RESULT;
		result = 0;
	}
	int result;
};

struct NewUserJoin : public DataHeader
{
	NewUserJoin()
	{
		dataLength = sizeof(NewUserJoin);
		cmd = CMD_NEW_USER_JOIN;
		scok = 0;
	}
	int scok;
};

//socket 向量值
std::vector<SOCKET> g_clients;

int processor(SOCKET _cSock)
{
	//缓冲区
	char szRecv[4096] = {};
	// 5 接收客户端数据
	int nLen = recv(_cSock, szRecv, sizeof(DataHeader), 0);
	DataHeader* header = (DataHeader*)szRecv;
	if (nLen <= 0)
	{
		printf("客户端<Socket=%d>已退出，任务结束。\n", _cSock);
		return -1;
	}
	switch (header->cmd)
	{
	case CMD_LOGIN:
	{
		recv(_cSock, szRecv + sizeof(DataHeader), header->dataLength - sizeof(DataHeader), 0);
		Login* login = (Login*)szRecv;
		printf("收到客户端<Socket=%d>请求：CMD_LOGIN,数据长度：%d,userName=%s PassWord=%s\n", _cSock, login->dataLength, login->userName, login->PassWord);
		//忽略判断用户密码是否正确的过程
		LoginResult ret;
		send(_cSock, (char*)&ret, sizeof(LoginResult), 0);
	}
	break;
	case CMD_LOGOUT:
	{
		recv(_cSock, szRecv + sizeof(DataHeader), header->dataLength - sizeof(DataHeader), 0);
		Logout* logout = (Logout*)szRecv;
		printf("收到客户端<Socket=%d>请求：CMD_LOGOUT,数据长度：%d,userName=%s \n", _cSock, logout->dataLength, logout->userName);
		//忽略判断用户密码是否正确的过程
		LogoutResult ret;
		send(_cSock, (char*)&ret, sizeof(ret), 0);
	}
	break;
	default:
	{
		DataHeader header = { 0,CMD_ERROR };
		send(_cSock, (char*)&header, sizeof(header), 0);
	}
	break;
	}
}

int main()
{
	//启动Windows socket 2.x环境
	WORD ver = MAKEWORD(2, 2);
	WSADATA dat;
	WSAStartup(ver, &dat);
	//------------

	//-- 用Socket API建立简易TCP服务端
	// 1 建立一个socket 套接字
	SOCKET _sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	// 2 bind 绑定用于接受客户端连接的网络端口
	sockaddr_in _sin = {};
	_sin.sin_family = AF_INET;
	_sin.sin_port = htons(4567);//host to net unsigned short
	_sin.sin_addr.S_un.S_addr = INADDR_ANY;//inet_addr("127.0.0.1");
	if (SOCKET_ERROR == bind(_sock, (sockaddr*)&_sin, sizeof(_sin)))
	{
		printf("错误,绑定网络端口失败...\n");
	}
	else {
		printf("绑定网络端口成功...\n");
	}
	// 3 listen 监听网络端口
	if (SOCKET_ERROR == listen(_sock, 5))
	{
		printf("错误,监听网络端口失败...\n");
	}
	else {
		printf("监听网络端口成功...\n");
	}

	while (true)
	{
		//伯克利套接字 BSD socket
		fd_set fdRead;//描述符（socket） 集合  
		fd_set fdWrite;
		fd_set fdExp;
		//清理集合
		FD_ZERO(&fdRead);
		FD_ZERO(&fdWrite);
		FD_ZERO(&fdExp);
		//将描述符（socket）加入集合  
		FD_SET(_sock, &fdRead);
		FD_SET(_sock, &fdWrite);
		FD_SET(_sock, &fdExp);

		for (int n = (int)g_clients.size()-1; n >= 0 ; n--)
		{
			FD_SET(g_clients[n], &fdRead);
		}

		/*原型

				int   select(
				int   nfds ,
				fd_set*   readfds ,
				fd_set*   writefds ,
				fd_set*   exceptfds ,
				const struct timeval*   timeout
					);

				nfds：windows中本参数忽略，仅起到兼容作用。
				readfds：（可选）指针，指向一组等待可读性检查的套接口。
				writefds：（可选）指针，指向一组等待可写性检查的套接口。
				exceptfds：（可选）指针，指向一组等待错误检查的套接口。
				timeout：select()最多等待时间，对阻塞操作则为NULL。

				注释：
				本函数用于确定一个或多个套接口的状态。对每一个套接口，调用者可查询它的可读性、可写性及错误状态信息。用fd_set结构来表示一组等待检查的套接口。
				在调用返回时，这个结构存有满足一定条件的套接口组的子集，并且select()返回满足条件的套接口的数目。有一组宏可用于对fd_set的操作
				readfds参数标识等待可读性检查的套接口。如果该套接口正处于监听listen()状态，则若有连接请求到达，该套接口便被标识为可读，这样一个accept()调用保证可以无阻塞完成。
				writefds参数标识等待可写性检查的套接口。如果一个套接口正在connect()连接（非阻塞），可写性意味着连接顺利建立。如果套接口并未处于connect()调用中，可写性意味着send()和sendto()调用将无阻塞完成。
				exceptfds参数标识等待带外数据存在性或意味错误条件检查的套接口。
				timeout代表最大等待时间，设置为NULL等待时间为永久，select变成阻塞模型
				如果对readfds、writefds或exceptfds中任一个组类不感兴趣，可将它置为空NULL。
				*/
		timeval t = {1,0};//超时时间设置为1s一次
		int ret = select(_sock + 1, &fdRead, &fdWrite, &fdExp, &t);//1s之内没有监听到变化该函数就会返回
		if (ret < 0)
		{
			printf("select任务结束。\n");
			break;
		}
		//判断描述符（socket）是否在集合中  
		if (FD_ISSET(_sock, &fdRead))
		{
			FD_CLR(_sock, &fdRead);
			// 4 accept 等待接受客户端连接
			sockaddr_in clientAddr = {};
			int nAddrLen = sizeof(sockaddr_in);
			SOCKET _cSock = INVALID_SOCKET;
			_cSock = accept(_sock, (sockaddr*)&clientAddr, &nAddrLen);//收到请求
			if (INVALID_SOCKET == _cSock)
			{
				printf("错误,接受到无效客户端SOCKET...\n");
			}
			else 
			{
				//对于每一个客户端发送消息 接收到新的用户加入
				for (int n = (int)g_clients.size() - 1; n >= 0; n--)
				{
					NewUserJoin userJoin;
					send(g_clients[n], (const char*)&userJoin, sizeof(NewUserJoin), 0);//发消息
				}
				g_clients.push_back(_cSock);
				printf("新客户端加入：socket = %d,IP = %s \n", (int)_cSock, inet_ntoa(clientAddr.sin_addr));

			}
		}
		for (size_t n = 0; n < fdRead.fd_count; n++)
		{
			//处理进程
			if (-1 == processor(fdRead.fd_array[n]))
			{
				auto iter = find(g_clients.begin(), g_clients.end(), fdRead.fd_array[n]);
				if (iter != g_clients.end())
				{
					g_clients.erase(iter);
				}
			}
		}

		printf("空闲时间处理其它业务..\n");
	}

	for (size_t n = g_clients.size() - 1; n >= 0; n--)
	{
		closesocket(g_clients[n]);
	}
	// 8 关闭套节字closesocket
	closesocket(_sock);
	//------------
	//清除Windows socket环境
	WSACleanup();
	printf("已退出。\n");
	getchar();
	return 0;
}