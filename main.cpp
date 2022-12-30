#include <iostream>
#include <windows.h>
#include <string>
#include <locale>
#include <codecvt>

using namespace std;
int sock;
DWORD g_dwErr;//用于取得错误信息
string ip_add = "127.0.0.1";
int port = 5700;
typedef struct {
    bool recv_flag; //0收到 1没收到
    bool recv_type;//0群组 1好友
    char qq[20];
    char id[20];
    char msg[801];
} MSG_TYPE;
MSG_TYPE msg_recv;

void msg_init() {
    msg_recv = {0};
}

int init_http() {
    /* 初始化 */
    WSADATA wsdata;
    WSAStartup(MAKEWORD(2, 2), &wsdata);
    struct hostent *host = gethostbyname(ip_add.c_str());
    /* 初始化一个连接服务器的结构体 */
    sockaddr_in serveraddr;
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(port);
    /* 此处也可以不用这么做，不需要用gethostbyname，把网址ping一下，得出IP也是可以的 */
    serveraddr.sin_addr.S_un.S_addr = *((int *) *host->h_addr_list);
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        cout << "socket error" << endl;
        return -1;
    }

    if (::connect(sock, (struct sockaddr *) &serveraddr, sizeof(sockaddr_in)) == -1) {
        g_dwErr = GetLastError();
        cout << "connect error" << endl;
        closesocket(sock);
        return -1;
    }
    cout << "connect succeed" << endl;

}
int send_msg(char *number, char *msg, bool type)//0 私聊，1群聊
{
    if (init_http() == -1)return -1;
    /* GET请求 */
    string send_http;
    if (!type) {


        send_http = "GET /send_private_msg?user_id=";
        send_http += number;
        send_http += "&message=";
        send_http += msg;
        send_http += " HTTP/1.1\r\nHost:";
        send_http += ip_add;
        send_http += ":" + to_string(port) + "\r\nConnection: close\r\n\r\n";
    } else {
        send_http = "GET /send_group_msg?group_id=";
        send_http += number;
        send_http += "&message=";
        send_http += msg;
        send_http += " HTTP/1.1\r\nHost:";
        send_http += ip_add;
        send_http += ":" + to_string(port) + "\r\nConnection: close\r\n\r\n";
    }
    if (send(sock, send_http.c_str(), send_http.length(), 0) > 0) {
        cout << "send succeed" << endl;
    } else {
        g_dwErr = GetLastError();
        cout << "send error, 错误编号： " << g_dwErr << endl;
        closesocket(sock);
        return -1;
    }
}

#include "Tlhelp32.h"

BOOL KillProcessByName(string processName) {
    BOOL bRet = FALSE;
    HANDLE SnapShot, ProcessHandle;
    SHFILEINFO shSmall;
    PROCESSENTRY32 ProcessInfo;

    string strSearchName;
    string strRunProcessName;
    //get the process list in the snapshot.
    SnapShot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (SnapShot != NULL) {
        //提升升级工具进程权限为SE_DEBUG_NAME，否则XP下无法杀掉进程
        HANDLE hToken;
        if (OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &hToken)) {
            LUID luid;
            if (LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &luid)) {
                TOKEN_PRIVILEGES TokenPrivileges;
                TokenPrivileges.PrivilegeCount = 1;
                TokenPrivileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
                TokenPrivileges.Privileges[0].Luid = luid;
                AdjustTokenPrivileges(hToken, FALSE, &TokenPrivileges, 0, NULL, NULL);
            }
            CloseHandle(hToken);
        }

        BOOL Status = Process32First(SnapShot, &ProcessInfo);
        while (Status) {
            // 获取进程文件信息
            SHGetFileInfo(ProcessInfo.szExeFile, 0, &shSmall, sizeof(shSmall), SHGFI_ICON | SHGFI_SMALLICON);

            // 检测进程是否需要关闭
            if (processName == ProcessInfo.szExeFile) {
                // 获取进程句柄，强行关闭
                ProcessHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, ProcessInfo.th32ProcessID);
                if (ProcessHandle != NULL) {
                    bRet = TerminateProcess(ProcessHandle, 1);
                    CloseHandle(ProcessHandle);
                }

            }
            // 获取下一个进程的信息
            Status = Process32Next(SnapShot, &ProcessInfo);
        }
    }

    return bRet;
}



int find_str(char *a, int num, char *b, int start_num) {
    int len_b = strlen(b);
    for (int i = start_num; i < num - len_b; i++) {
        if (a[i] == b[0])//第一个字符对上
        {
            bool ok = 1;
            for (int j = 1; j < len_b; j++) {

                if (a[i + j] != b[j]) {
                    ok = 0;
                    break;
                }
            }
            if (ok == 1)return i + len_b;

        }
    }
    return -1;
}

void solve_msg(char *msg, int num) {
    int find_flag = find_str(msg, num, "收到", 0);
    int find_type = find_str(msg, num, "好友", 0);
    if (find_flag >= 0) msg_recv.recv_flag = 1;
    if (find_type >= 0) msg_recv.recv_type = 1;
    if (msg_recv.recv_flag) {
        cout << "收到";
        if (msg_recv.recv_type) {
            cout << "好友";
            int msg_start = find_str(msg, num, ") 的消息: ", find_type);
            int id_start = find_str(msg, num, "(", msg_start - strlen(") 的消息: ") - 12);
            int msg_end = num - 1;
            while (msg[msg_end] != '(')msg_end--;
            for (int i = msg_end + 1; msg[i] != ')'; i++) {
                msg_recv.id[i - (msg_end + 1)] = msg[i];
                msg_recv.id[i - (msg_end + 1) + 1] = '\0';
            }

            for (int i = id_start; msg[i] != ')'; i++) {
                msg_recv.qq[i - id_start] = msg[i];
                msg_recv.qq[i - id_start + 1] = '\0';
            }
            for (int i = msg_start; i < msg_end - 1; i++) {
                msg_recv.msg[i - msg_start] = msg[i];
                msg_recv.msg[i - msg_start + 1] = '\0';

            }
        send_msg(msg_recv.qq, msg_recv.msg, 0);

            cout << msg_recv.qq << "的消息:" << msg_recv.msg << "id:" << msg_recv.id;
            cout << endl;
        } else cout << "群组";
    }
    cout << endl;
}

int main() {
    bool init_http_flag=1;
    msg_init();
    cout << KillProcessByName("go-cqhttp.exe");
    STARTUPINFO si = {0};
    PROCESS_INFORMATION pi = {0};
    char ReadBuff[3000] = {0};
    DWORD ReadNum = 0;
    HANDLE hRead = NULL;
    HANDLE hWrite = NULL;
    SECURITY_ATTRIBUTES sa = {0};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = 0;
    BOOL bRet = CreatePipe(&hRead, &hWrite, &sa, 0);
    if (bRet == TRUE) {
        cout << "管道创建成功" << endl;
    } else {
        cout << "创建管道失败，错误代码为：" << GetLastError() << endl;
    }
    HANDLE hTemp = GetStdHandle(STD_OUTPUT_HANDLE);
    SetStdHandle(STD_OUTPUT_HANDLE, hWrite);
    GetStartupInfo(&si);//获取本进程当前的STARTUPINFO结构信息
    si.cb = sizeof(STARTUPINFO);
    si.wShowWindow = SW_HIDE;
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.hStdError = hWrite;
    si.hStdOutput = hWrite;

    bRet = CreateProcess(NULL, "./go-cqhttp.exe", NULL, NULL, TRUE, NULL, NULL, NULL, &si, &pi);
    SetStdHandle(STD_OUTPUT_HANDLE, hTemp);
    if (bRet == TRUE) cout << "成功创建子进程" << endl;
     else cout << "创建子进程失败，错误代码为 ：" << GetLastError() << endl;

    CloseHandle(hWrite);
    while (1) {
        msg_init();

        ReadFile(hRead, ReadBuff, 100, &ReadNum, NULL);
        ReadBuff[ReadNum] = '\0';

        if (ReadNum != 0) {
            solve_msg(ReadBuff, ReadNum);
            cout << ReadBuff << endl;
            if (find_str(ReadBuff, ReadNum, "网络诊断完成. 未发现问题", 0) >= 0&&init_http_flag) {

            }
        }
    }
    return 0;
}

