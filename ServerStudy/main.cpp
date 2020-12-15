#pragma comment(lib, "ws2_32")
#include<iostream>
#include<WinSock2.h>

using namespace std;

void errQuit(const wchar_t* msg) {
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, 
		WSAGetLastError(), 
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), 
		(LPTSTR)&lpMsgBuf, 
		0, 
		NULL);
	MessageBox(NULL, (LPCTSTR)lpMsgBuf, msg, MB_ICONERROR);
	LocalFree(lpMsgBuf);
	exit(1);
}

int main() {
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
		return 1;
	}
	MessageBox(NULL, L"윈도우 초기화 성공", L"알림", MB_OK);

	SOCKET tcpSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (tcpSocket == INVALID_SOCKET) {
		errQuit(L"socket()");
	}
	MessageBox(NULL, L"TCP Socket 성공", L"알림", MB_OK);

	u_short short1 = 0x1234;
	u_long long1 = 0x12345678;
	u_short short2;
	u_long long2;

	printf("[host byte -> network byte] \n");
	printf(" 0x%x -> 0x%x \n", short1, short2 = htons(short1));
	printf(" 0x%x -> 0x%x \n", long1, long2 = htonl(long1));

	printf("\n [network byte -> host byte] \n");
	printf("0x%x -> 0x%x \n", short2, ntohs(short2));
	printf("0x%x -> 0x%x \n", long2, ntohl(long2));

	printf("\n [wrong example] \n");
	printf("0x%x -> 0x%x \n", short1, htonl(short1));

	closesocket(tcpSocket);
	WSACleanup();
	return 0;
}