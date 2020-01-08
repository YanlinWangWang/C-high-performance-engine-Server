#pragma once
#ifndef _EasyTcpServer_h_
#define _EasyTcpServer_h_

#ifdef _WIN32
	#define WIN32_LEAN_AND_MEAN
	#define _WINSOCK_DEPRECATED_NO_WARNINGS
	#include<windows.h>
	#include<WinSock2.h>
	#pragma comment(lib,"ws2_32.lib")
#else
	#include<unistd.h> //uni std
	#include<arpa/inet.h>
	#include<string.h>
	#include <sys/epoll.h>			/* epoll头文件 */
	#include <fcntl.h>	                /* nonblocking需要 */

	#define SOCKET int
	#define INVALID_SOCKET  (SOCKET)(~0)
	#define SOCKET_ERROR            (-1)
#endif

#include<stdio.h>
#include<vector>
#include"MessageHeader.h"

#define MAXEPOLL 100000

class EasyTcpServer
{
private:

	SOCKET _sock;//变量
	std::vector<SOCKET> g_clients;//变量组
	//处理网络消息
	int _nCount = 0;
	//缓冲区
	char szRecv[409600] = {};

	int epoll_fd = 0;
	struct 	epoll_event	ev;
	struct 	epoll_event	evs[MAXEPOLL];

public:
	EasyTcpServer()
	{
		_sock = INVALID_SOCKET;
	}
	virtual ~EasyTcpServer()
	{
		Close();
	}

 
//!> 设置非阻塞
//!> 
int setnonblocking( int fd )
{
	if( fcntl( fd, F_SETFL, fcntl( fd, F_GETFD, 0 )|O_NONBLOCK ) == -1 )
	{
		printf("Set blocking error ");
		return -1;
	}
	return 0;
}
	//初始化Socket
	SOCKET InitSocket()
	{
#ifdef _WIN32
		//启动Windows socket 2.x环境
		WORD ver = MAKEWORD(2, 2);
		WSADATA dat;
		WSAStartup(ver, &dat);
#endif
		if (INVALID_SOCKET != _sock)
		{
			printf("<socket=%d>关闭旧连接...\n", (int)_sock);
			Close();
		}
		_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

				//设置非阻塞
		setnonblocking(_sock);

		if (INVALID_SOCKET == _sock)
		{
			printf("错误，建立socket失败...\n");
		}
		else {
			printf("建立socket=<%d>成功...\n", (int)_sock);
		}

		return _sock;
	}

	//绑定IP和端口号
	int Bind(const char* ip, unsigned short port)
	{
		//if (INVALID_SOCKET == _sock)
		//{
		//	InitSocket();
		//}
		// 2 bind 绑定用于接受客户端连接的网络端口
		sockaddr_in _sin = {};
		_sin.sin_family = AF_INET;
		_sin.sin_port = htons(port);//host to net unsigned short

#ifdef _WIN32
		if (ip) {
			_sin.sin_addr.S_un.S_addr = inet_addr(ip);
		}
		else {
			_sin.sin_addr.S_un.S_addr = INADDR_ANY;
		}
#else
		if (ip) {
			_sin.sin_addr.s_addr = inet_addr(ip);
		}
		else {
			_sin.sin_addr.s_addr = INADDR_ANY;
		}
#endif
		int ret = bind(_sock, (sockaddr*)& _sin, sizeof(_sin));
		if (SOCKET_ERROR == ret)
		{
			printf("错误,绑定网络端口<%d>失败...\n", port);
		}
		else {
			printf("绑定网络端口<%d>成功...\n", port);
		}
		return ret;
	}

	//监听端口号
	int Listen(int n)
	{
		// 3 listen 监听网络端口
		int ret = listen(_sock, n);
		if (SOCKET_ERROR == ret)
		{
			printf("socket=<%d>错误,监听网络端口失败...\n", _sock);
		}
		else {
			printf("socket=<%d>监听网络端口成功...\n", _sock);
		}
		return ret;
	}

	//接受客户端连接
	SOCKET Accept()
	{
		// 4 accept 等待接受客户端连接
		sockaddr_in clientAddr = {};
		int nAddrLen = sizeof(sockaddr_in);
		SOCKET _cSock = INVALID_SOCKET;
#ifdef _WIN32
		//accept函数接收一个监听的socket作为输入 并返回一个新的派生链接socket
		_cSock = accept(_sock, (sockaddr*)& clientAddr, &nAddrLen);
#else
		_cSock = accept(_sock, (sockaddr*)& clientAddr, (socklen_t*)& nAddrLen);
#endif
		if (INVALID_SOCKET == _cSock)
		{
			printf("socket=<%d>错误,接受到无效客户端SOCKET...\n", (int)_sock);
		}
		else
		{
			NewUserJoin userJoin;
			SendDataToAll(&userJoin);//群发消息
			g_clients.push_back(_cSock);//将新加入的socket提取到队列里面去
			printf("socket=<%d>新客户端加入：socket = %d,IP = %s \n", (int)_sock, (int)_cSock, inet_ntoa(clientAddr.sin_addr));
		}
		return _cSock;
	}

	//是否工作中 只要sock还活着就行
	bool isRun()
	{
		return _sock != INVALID_SOCKET;
	}

//接收数据 将收到的数据返回回去 处理粘包 拆分包
	int RecvData(SOCKET _cSock)
	{
		// 5 接收客户端数据
		int nLen = (int)recv(_cSock, szRecv, 409600, 0);
		//printf("nLen=%d\n", nLen);
		if (nLen <= 0)
		{
			printf("客户端<Socket=%d>已退出，任务结束。\n", _cSock);
			return -1;
		}
		LoginResult ret;
		SendData(_cSock, &ret);
		/*
		DataHeader* header = (DataHeader*)szRecv;
		recv(_cSock, szRecv + sizeof(DataHeader), header->dataLength - sizeof(DataHeader), 0);
		OnNetMsg(_cSock, header);
		*/
		return 0;
	}

	//启动进程 判断
	bool OnRun()
	{

		if (isRun())
		{
				//!> 创建epoll
		//!> 
		epoll_fd = epoll_create( MAXEPOLL );	//!> create
		ev.events = EPOLLIN|EPOLLET;		//!> accept Read!
		ev.data.fd = _sock;					//!> 将listen_fd 加入

		if( epoll_ctl( epoll_fd, EPOLL_CTL_ADD, _sock, &ev ) < 0 )
		{
			printf("Epoll Error n");
		}

		int	cur_fds = 1; //最大监听事件数量
		int wait_fds = 0;//等待处理的fd
		if( ( wait_fds = epoll_wait( epoll_fd, evs, cur_fds, -1 ) ) == -1 )
		{
			printf( "Epoll Wait Error ");
		}
 

		for(int i = 0; i < wait_fds; i++ )
		{
			if( evs[i].data.fd == _sock && cur_fds < MAXEPOLL )	
			//!> if是监听端口有事
			{	
				int _cSock = Accept();//接受链接 将新加入的加入到socket队列中去
				printf( "Server get from client !\n"/*,  inet_ntoa(cliaddr.sin_addr), cliaddr.sin_port */);
				
				ev.events = EPOLLIN|EPOLLET;		//!> ET模式
				ev.data.fd = _cSock;					//!> 将sock 加入
				int res = epoll_ctl( epoll_fd, EPOLL_CTL_ADD, _cSock, &ev );
				if(  res < 0 )
				{
					printf("Epoll Error 接受错误\n");
				}
				++cur_fds; 
				continue;		
			}
			
			//!> 下面处理数据
			//!> 
			if (-1 == RecvData(g_clients[i]))//将受到的数据返回出去
					{
						//删除对应的socket链接
						auto iter = g_clients.begin() + i;//std::vector<SOCKET>::iterator
						if (iter != g_clients.end())
						{
							#ifdef WIN32
							closesocket(*iter);//关闭对应的处理socket
							#endif
							close(*iter);
							g_clients.erase(iter);
							epoll_ctl( epoll_fd, EPOLL_CTL_DEL, evs[i].data.fd, &ev );	//!> 删除计入的fd
							--cur_fds;					//!> 减少一个呗！
						}
					}
			
				}
		}
		return false;

	}
	
	//响应网络消息
	virtual void OnNetMsg(SOCKET _cSock, DataHeader* header)
	{
		switch (header->cmd)
		{
		case CMD_LOGIN:
		{

			Login* login = (Login*)header;
			printf("收到客户端<Socket=%d>请求：CMD_LOGIN,数据长度：%d,userName=%s PassWord=%s\n", _cSock, login->dataLength, login->userName, login->PassWord);
			//忽略判断用户密码是否正确的过程
			LoginResult ret;
			SendData(_cSock, &ret);
		}
		break;
		case CMD_LOGOUT:
		{
			Logout* logout = (Logout*)header;
			printf("收到客户端<Socket=%d>请求：CMD_LOGOUT,数据长度：%d,userName=%s \n", _cSock, logout->dataLength, logout->userName);
			//忽略判断用户密码是否正确的过程
			LogoutResult ret;
			SendData(_cSock, &ret);
		}
		break;
		default:
		{
			DataHeader header = { 0,CMD_ERROR };
			send(_cSock, (char*)& header, sizeof(header), 0);
		}
		break;
		}
	}

	//发送指定Socket数据
	int SendData(SOCKET _cSock, DataHeader* header)
	{
		if (isRun() && header)
		{
			return send(_cSock, (const char*)header, header->dataLength, 0);
		}
		return SOCKET_ERROR;
	}

	void SendDataToAll(DataHeader* header)
	{
		for (int n = (int)g_clients.size() - 1; n >= 0; n--)
		{
			SendData(g_clients[n], header);
		}
	}

	//关闭Socket
	void Close()
	{
		if (_sock != INVALID_SOCKET)
		{
#ifdef _WIN32
			//立即关闭所有派生socket
			for (int n = (int)g_clients.size() - 1; n >= 0; n--)
			{
				closesocket(g_clients[n]);
			}
			// 8 关闭套节字closesocket 关闭服务进程socket
			closesocket(_sock);
			//------------
			//清除Windows socket环境
			WSACleanup();
#else
			for (int n = (int)g_clients.size() - 1; n >= 0; n--)
			{
				close(g_clients[n]);
			}
			// 8 关闭套节字closesocket
			close(_sock);
#endif
		}
	}
};
#endif