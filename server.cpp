#include <iostream>
#include <cstring>
#include <thread>
#include <mutex>
#include <cstdlib>
#include <WinSock2.h>
#include <unordered_map>
#include <string>
//#pragma comment(lib, 'ws2_32.lib');
#define SERVER_PORT 5208 // Порт сервера
#define BUF_SIZE 1024
#define MAX_CLNT 256    // Максимальное количество подключений

void handle_clnt(SOCKET clnt_sock);
void send_msg(const std::string& msg);
int output(const char* arg, ...);
int error_output(const char* arg, ...);
void error_handling(const std::string& message);

int clnt_cnt = 0;
std::mutex mtx;
std::unordered_map<std::string, SOCKET> clnt_socks;

int main(int argc, const char** argv) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        error_handling("WSAStartup() failed!");
        return 1;
    }

    SOCKET serv_sock, clnt_sock;
    struct sockaddr_in serv_addr, clnt_addr;
    int clnt_addr_size;

    // Создаем сокет для TCP-соединения
    serv_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serv_sock == INVALID_SOCKET) {
        error_handling("socket() failed!");
        return 1;
    }

    // Заполняем структуру адреса сервера
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(SERVER_PORT);

    // Привязываем сокет к адресу
    if (bind(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == SOCKET_ERROR) {
        error_handling("bind() failed!");
        return 1;
    }

    printf("The server is running on port %d\n", SERVER_PORT);

    // Слушаем входящие подключения
    if (listen(serv_sock, MAX_CLNT) == SOCKET_ERROR) {
        error_handling("listen() error!");
        return 1;
    }

    while (1) {
        clnt_addr_size = sizeof(clnt_addr);
        // Принимаем входящее подключение
        clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &clnt_addr_size);
        if (clnt_sock == INVALID_SOCKET) {
            error_handling("accept() failed!");
            continue;
        }

        mtx.lock();
        clnt_cnt++;
        mtx.unlock();

        // Создаем поток для обработки клиента
        std::thread th(handle_clnt, clnt_sock);
        th.detach();

        output("Connected client IP: %s\n", inet_ntoa(clnt_addr.sin_addr));
    }

    closesocket(serv_sock);
    WSACleanup();
    return 0;
}

void handle_clnt(SOCKET clnt_sock) {
    char msg[BUF_SIZE];
    int flag = 0;

    char tell_name[13] = "#new client:";
    while (recv(clnt_sock, msg, sizeof(msg), 0) != 0) {
        if (std::strlen(msg) > std::strlen(tell_name)) {
            char pre_name[13];
            strncpy_s(pre_name, msg, 12);

            pre_name[12] = '\0';
            if (std::strcmp(pre_name, tell_name) == 0) {
                char name[20];
                strcpy_s(name, msg + 12);
                if (clnt_socks.find(name) == clnt_socks.end()) {
                    output("The name of socket %d: %s\n", clnt_sock, name);
                    clnt_socks[name] = clnt_sock;
                }
                else {
                    std::string error_msg = std::string(name) + " exists already. Please quit and enter with another name!";
                    send(clnt_sock, error_msg.c_str(), error_msg.length() + 1, 0);
                    mtx.lock();
                    clnt_cnt--;
                    mtx.unlock();
                    flag = 1;
                }
            }
        }

        if (flag == 0)
            send_msg(std::string(msg));
    }
    if (flag == 0) {
        std::string leave_msg;
        std::string name;
        mtx.lock();
        for (auto it = clnt_socks.begin(); it != clnt_socks.end(); ++it) {
            if (it->second == clnt_sock) {
                name = it->first;
                clnt_socks.erase(it->first);
            }
        }
        clnt_cnt--;
        mtx.unlock();
        leave_msg = "Client " + name + " leaves the chat room";
        send_msg(leave_msg);
        output("Client %s leaves the chat room\n", name.c_str());
        closesocket(clnt_sock);
    }
    else {
        closesocket(clnt_sock);
    }
}

void send_msg(const std::string& msg) {
    mtx.lock();
    std::string pre = "@";
    int first_space = msg.find_first_of(" ");
    if (first_space != std::string::npos && msg.compare(first_space + 1, 1, pre) == 0) {
        int space = msg.find_first_of(" ", first_space + 1);
        if (space != std::string::npos) {
            std::string receive_name = msg.substr(first_space + 2, space - first_space - 2);
            std::string send_name = msg.substr(0, first_space);
            if (clnt_socks.find(receive_name) == clnt_socks.end()) {
                std::string error_msg = "[error] there is no client named " + receive_name;
                send(clnt_socks[send_name], error_msg.c_str(), error_msg.length() + 1, 0);
            }
            else {
                send(clnt_socks[receive_name], msg.c_str(), msg.length() + 1, 0);
                send(clnt_socks[send_name], msg.c_str(), msg.length() + 1, 0);
            }
        }
    }
    else {
        for (auto& client : clnt_socks) {
            send(client.second, msg.c_str(), msg.length() + 1, 0);
        }
    }
    mtx.unlock();
}


int output(const char* arg, ...) {
    int res;
    va_list ap;
    va_start(ap, arg);
    res = vfprintf(stdout, arg, ap);
    va_end(ap);
    return res;
}

int error_output(const char* arg, ...) {
    int res;
    va_list ap;
    va_start(ap, arg);
    res = vfprintf(stderr, arg, ap);
    va_end(ap);
    return res;
}

void error_handling(const std::string& message) {
    std::cerr << message << std::endl;
}
