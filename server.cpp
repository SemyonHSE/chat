#include <iostream>
#include <cstring>
#include <thread>
#include <mutex>
#include <WinSock2.h>
#include <unordered_map>
#include <string>
#include <fstream>

#define SERVER_PORT 5208 // Порт сервера
#define BUF_SIZE 1024  //буфер будет иметь размер 1024 байта
#define MAX_CLNT 256    // Максимальное количество подключений
#define HISTORY_FILE "chat_history.txt" // Файл для хранения истории сообщений

void handle_clnt(SOCKET clnt_sock);
void send_msg(const std::string& msg);
void save_msg_to_history(const std::string& msg);
std::string get_chat_history();
int output(const char* arg, ...);
int error_output(const char* arg, ...);
void error_handling(const std::string& message);

int clnt_cnt = 0;
std::mutex mtx;
std::unordered_map<std::string, SOCKET> clnt_socks;

int main(int argc, const char** argv) {
    //Запуск программных интерфейсов для работы с сокетами
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        error_handling("WSAStartup() failed!");
        return 1;
    }

    SOCKET serv_sock;
    serv_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serv_sock == INVALID_SOCKET) {
        error_handling("socket() failed!");
        return 1;
    }

    struct sockaddr_in serv_addr, clnt_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(SERVER_PORT);

    if (bind(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == SOCKET_ERROR) {
        error_handling("bind() failed!");
        return 1;
    }

    printf("The server is running on port %d\n", SERVER_PORT);

    if (listen(serv_sock, MAX_CLNT) == SOCKET_ERROR) {
        error_handling("listen() error!");
        return 1;
    }

    SOCKET clnt_sock;
    int clnt_addr_size;
    while (1) {
        clnt_addr_size = sizeof(clnt_addr);
        clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &clnt_addr_size);
        if (clnt_sock == INVALID_SOCKET) {
            error_handling("accept() failed!");
            continue;
        }

        mtx.lock();
        clnt_cnt++;
        mtx.unlock();

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
    bool quit_flag = false;

    char tell_name[13] = "#new client:";
    while (!quit_flag && recv(clnt_sock, msg, sizeof(msg), 0) > 0) {
        if (std::strlen(msg) > std::strlen(tell_name)) {
            char pre_name[13];
            strncpy_s(pre_name, msg, 12);
            pre_name[12] = '\0';
            if (std::strcmp(pre_name, tell_name) == 0) {
                char name[20];
                strcpy_s(name, msg + 12);
                if (clnt_socks.find(name) == clnt_socks.end()) {
                    output("The name of socket %d: %s\n", clnt_sock, name);
                    mtx.lock();
                    clnt_socks[name] = clnt_sock;

                    // Send chat history to the new client
                    std::string history = get_chat_history();
                    send(clnt_sock, history.c_str(), history.length() + 1, 0);

                    mtx.unlock();
                } else {
                    std::string error_msg = std::string(name) + " exists already. Please quit and enter with another name!";
                    send(clnt_sock, error_msg.c_str(), error_msg.length() + 1, 0);
                    mtx.lock();
                    clnt_cnt--;
                    mtx.unlock();
                    closesocket(clnt_sock);
                    return;
                }
            }
        }

        if (std::string(msg) == "quit" || std::string(msg) == "Quit") {
            quit_flag = true;
        } else {
            send_msg(std::string(msg));
            save_msg_to_history(std::string(msg)); // Save message to history
        }
    }

    if (quit_flag) {
        std::string leave_msg;
        std::string name;
        mtx.lock();
        for (auto it = clnt_socks.begin(); it != clnt_socks.end(); ++it) {
            if (it->second == clnt_sock) {
                name = it->first;
                clnt_socks.erase(it);
                break;
            }
        }
        clnt_cnt--;
        mtx.unlock();
        leave_msg = "Client " + name + " leaves the chat room";
        send_msg(leave_msg);
        save_msg_to_history(leave_msg); // Save leave message to history
        output("Client %s leaves the chat room\n", name.c_str());
    }

    closesocket(clnt_sock);
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

void save_msg_to_history(const std::string& msg) {
    std::ofstream file(HISTORY_FILE, std::ios::app);
    if (file.is_open()) {
        file << msg << std::endl;
        file.close();
    } else {
        error_output("Failed to open history file for writing.\n");
    }
}

std::string get_chat_history() {
    std::ifstream file(HISTORY_FILE);
    std::string history, line;
    if (file.is_open()) {
        while (getline(file, line)) {
            history += line + "\n";
        }
        file.close();
    } else {
        error_output("Failed to open history file for reading.\n");
    }
    return history;
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
