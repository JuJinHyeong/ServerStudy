/*
## 소켓 서버 : 1 v n - IOCP
1. socket()            : 소켓생성
2. bind()            : 소켓설정
3. listen()            : 수신대기열생성
4. accept()            : 연결대기
5. read()&write()
    WIN recv()&send    : 데이터 읽고쓰기
6. close()
    WIN closesocket    : 소켓종료
*/
#include<iostream>
#include<tchar.h>
#include<winsock2.h>
#include<thread>
#include<vector>
using namespace std;

#pragma comment(lib, "Ws2_32.lib")

#define MAX_BUFFER        1024
#define SERVER_PORT        3500

struct SOCKETINFO
{
    WSAOVERLAPPED overlapped;
    WSABUF dataBuffer;
    SOCKET socket;
    char messageBuffer[MAX_BUFFER];
    int receiveBytes;
    int sendBytes;
};

void errQuit(const wchar_t* msg) {
    LPVOID lpMsgBuf;
    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
        NULL, WSAGetLastError(),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR)&lpMsgBuf, 0, NULL);
    MessageBox(NULL, (LPCTSTR)lpMsgBuf, msg, MB_ICONERROR);
    LocalFree(lpMsgBuf);
    exit(1);
}

DWORD WINAPI AcceptThread(SOCKET* listenSocket, HANDLE* hIOCP);
DWORD WINAPI ReceiveThread(HANDLE* hIOCP);

int _tmain(int argc, _TCHAR* argv[])
{
    // Winsock Start
    WSADATA WSAData;
    if (WSAStartup(MAKEWORD(2, 2), &WSAData) != 0)
    {
        cerr << "ERROR - Can not load 'winsock.dll' file" << endl;
        return 1;
    }

    // Overlapped io 를 사용하는 소켓생성
    SOCKET listenSocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
    if (listenSocket == INVALID_SOCKET)
    {
        errQuit(L"ERROR - Invalid socket");
        return 1;
    }

    // 서버정보 객체설정
    SOCKADDR_IN serverAddr;
    memset(&serverAddr, 0, sizeof(SOCKADDR_IN));
    serverAddr.sin_family = PF_INET;
    serverAddr.sin_port = htons(SERVER_PORT);
    serverAddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);

    // 소켓 Bind
    if (bind(listenSocket, (SOCKADDR*)&serverAddr, sizeof(SOCKADDR_IN)) == SOCKET_ERROR)
    {
        errQuit(L"ERROR - Fail bind");
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }
    else {
        cout << "Bind Success" << endl;
    }

    // 3. 수신대기열생성
    if (listen(listenSocket, 5) == SOCKET_ERROR)
    {
        errQuit(L"ERROR - Fail listen");
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }
    else {
        cout << "Listen Port:" << serverAddr.sin_port << endl;
    }

    // 완료결과를 처리하는 객체(CP : Completion Port) 생성
    HANDLE hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

    const int recvThreadCount = 1;
    vector<thread> hRecvThread;
    for (int i = 0; i < recvThreadCount; i++) {
        hRecvThread.push_back(thread(ReceiveThread, &hIOCP));
    }

    const int acceptThreadCount = 1;
    vector<thread> hAcceptThread;
    for (int i = 0; i < acceptThreadCount; i++) {
        hAcceptThread.push_back(thread(AcceptThread, &listenSocket, &hIOCP));
    }

    cout << "q를 눌러 종료합니다." << endl;
    while (true) {
        string command;
        cin >> command;
        if (command == "q" || command == "Q") {
            break;
        }
    }

    for (int i = 0; i < recvThreadCount; i++) {
        hRecvThread[i].join();
    }
    for (int i = 0; i < acceptThreadCount; i++) {
        hAcceptThread[i].join();
    }

    // 6-2. 리슨 소켓종료
    closesocket(listenSocket);

    // Winsock End
    WSACleanup();

    return 0;
}

DWORD WINAPI AcceptThread(SOCKET* listenSocket, HANDLE* hIOCP) {
    SOCKADDR_IN clientAddr;
    int addrLen = sizeof(SOCKADDR_IN);
    ZeroMemory(&clientAddr, addrLen);
    SOCKET clientSocket;
    SOCKETINFO* socketInfo;
    DWORD receiveBytes;
    DWORD flags;

    while (1)
    {
        clientSocket = accept(*listenSocket, (SOCKADDR*)&clientAddr, &addrLen);
        if (clientSocket == INVALID_SOCKET)
        {
            errQuit(L"ERROR - Accept Failure");
            return 1;
        }
        else {
            cout << "Client Connected" << endl;
        }

        socketInfo = (SOCKETINFO*)malloc(sizeof(SOCKETINFO));
        ZeroMemory(socketInfo, sizeof(SOCKETINFO));
        socketInfo->socket = clientSocket;
        socketInfo->receiveBytes = 0;
        socketInfo->sendBytes = 0;
        socketInfo->dataBuffer.len = MAX_BUFFER;
        socketInfo->dataBuffer.buf = socketInfo->messageBuffer;
        flags = 0;

        CreateIoCompletionPort((HANDLE)clientSocket, *hIOCP, (DWORD)socketInfo, 0);

        if (WSARecv(socketInfo->socket, &socketInfo->dataBuffer, 1, &receiveBytes, &flags, &(socketInfo->overlapped), NULL))
        {
            if (WSAGetLastError() != WSA_IO_PENDING)
            {
                errQuit(L"ERROR - IO pending Failure");
                return 1;
            }
        }
    }
}

DWORD WINAPI ReceiveThread(HANDLE* hIOCP)
{
    DWORD receiveBytes = 0;
    DWORD sendBytes = 0;
    DWORD completionKey = 0;
    DWORD flags = 0;
    SOCKETINFO* socketInfo;
    while (1)
    {
        // 입출력 완료 대기
        if (GetQueuedCompletionStatus(*hIOCP, &receiveBytes, (PULONG_PTR)&completionKey, (LPOVERLAPPED*)&socketInfo, INFINITE) == 0)
        {
            errQuit(L"ERROR - GetQueuedCompletionStatus Failure");
            closesocket(socketInfo->socket);
            free(socketInfo);
            return 1;
        }

        socketInfo->dataBuffer.len = receiveBytes;
        socketInfo->receiveBytes = receiveBytes;

        if (receiveBytes == 0)
        {
            closesocket(socketInfo->socket);
            free(socketInfo);
            continue;
        }
        else
        {
            printf("TRACE - Receive message : %s (%d bytes)\n", socketInfo->dataBuffer.buf, socketInfo->dataBuffer.len);

            if (WSASend(socketInfo->socket, &(socketInfo->dataBuffer), 1, &sendBytes, 0, NULL, NULL) == SOCKET_ERROR)
            {
                if (WSAGetLastError() != WSA_IO_PENDING)
                {
                    errQuit(L"ERROR - Fail WSASend");
                }
            }

            printf("TRACE - Send message : %s (%d bytes)\n", socketInfo->dataBuffer.buf, socketInfo->dataBuffer.len);

            if (WSARecv(socketInfo->socket, &(socketInfo->dataBuffer), 1, &receiveBytes, &flags, &socketInfo->overlapped, NULL) == SOCKET_ERROR)
            {
                if (WSAGetLastError() != WSA_IO_PENDING)
                {
                    errQuit(L"ERROR - Fail WSARecv");
                }
            }
        }
    }
}