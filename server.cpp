#include <arpa/inet.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <iostream>
#include <string>

class Utils {
public:
    static const char* get_file_extension(const char* file_name) {
        const char* dot = strrchr(file_name, '.');
        if (!dot || dot == file_name) return "";
        return dot + 1;
    }

    static const char* get_mime_type(const char* file_ext) {
        if (strcasecmp(file_ext, "html") == 0 || strcasecmp(file_ext, "htm") == 0) {
            return "text/html";
        } else if (strcasecmp(file_ext, "txt") == 0) {
            return "text/plain";
        } else if (strcasecmp(file_ext, "jpg") == 0 || strcasecmp(file_ext, "jpeg") == 0) {
            return "image/jpeg";
        } else if (strcasecmp(file_ext, "png") == 0) {
            return "image/png";
        }
        return "application/octet-stream";
    }

    static bool case_insensitive_compare(const char* s1, const char* s2) {
        while (*s1 && *s2) {
            if (tolower((unsigned char)*s1) != tolower((unsigned char)*s2)) {
                return false;
            }
            ++s1;
            ++s2;
        }
        return *s1 == *s2;
    }

    static char* url_decode(const char* src) {
        size_t src_len = strlen(src);
        char* decoded = new char[src_len + 1];
        size_t decoded_len = 0;
        for (size_t i = 0; i < src_len; ++i) {
            if (src[i] == '%' && i + 2 < src_len) {
                int hex_val;
                sscanf(src + i + 1, "%2x", &hex_val);
                decoded[decoded_len++] = static_cast<char>(hex_val);
                i += 2;
            } else {
                decoded[decoded_len++] = src[i];
            }
        }
        decoded[decoded_len] = '\0';
        return decoded;
    }
};

class HttpClientHandler {
public:
    explicit HttpClientHandler(int client_fd)
        : client_fd(client_fd) {}

    ~HttpClientHandler() {
        close(client_fd);
    }

    void handle() {
        char* buffer = new char[BUFFER_SIZE];
        ssize_t bytes_received = recv(client_fd, buffer, BUFFER_SIZE, 0);

        if (bytes_received > 0) {
            regex_t regex;
            regcomp(&regex, "^GET /([^ ]*) HTTP/1", REG_EXTENDED);
            regmatch_t matches[2];

            if (regexec(&regex, buffer, 2, matches, 0) == 0) {
                buffer[matches[1].rm_eo] = '\0';
                const char* url_encoded_file_name = buffer + matches[1].rm_so;
                char* file_name = Utils::url_decode(url_encoded_file_name);

                char file_ext[32];
                strcpy(file_ext, Utils::get_file_extension(file_name));

                char* response = new char[BUFFER_SIZE * 2];
                size_t response_len;
                build_http_response(file_name, file_ext, response, &response_len);

                send(client_fd, response, response_len, 0);

                delete[] response;
                delete[] file_name;
            }
            regfree(&regex);
        }

        delete[] buffer;
    }

private:
    int client_fd;
    static constexpr size_t BUFFER_SIZE = 104857600;

    void build_http_response(const char* file_name,
                             const char* file_ext,
                             char* response,
                             size_t* response_len) {
        const char* mime_type = Utils::get_mime_type(file_ext);
        char* header = new char[BUFFER_SIZE];

        snprintf(header, BUFFER_SIZE,
                 "HTTP/1.1 200 OK\r\n"
                 "Content-Type: %s\r\n"
                 "\r\n",
                 mime_type);

        int file_fd = open(file_name, O_RDONLY);
        if (file_fd == -1) {
            snprintf(response, BUFFER_SIZE,
                     "HTTP/1.1 404 Not Found\r\n"
                     "Content-Type: text/plain\r\n"
                     "\r\n"
                     "404 Not Found");
            *response_len = strlen(response);
            delete[] header;
            return;
        }

        struct stat file_stat;
        fstat(file_fd, &file_stat);

        *response_len = 0;
        memcpy(response, header, strlen(header));
        *response_len += strlen(header);

        ssize_t bytes_read;
        while ((bytes_read = read(file_fd,
                                 response + *response_len,
                                 BUFFER_SIZE - *response_len)) > 0) {
            *response_len += bytes_read;
        }

        delete[] header;
        close(file_fd);
    }
};

class HttpServer {
public:
    static constexpr int PORT = 8080;
    static constexpr size_t BUFFER_SIZE = 104857600;

    HttpServer() {
        setup_socket();
    }

    ~HttpServer() {
        close(server_fd);
    }

    void run() {
        std::cout << "Server listening on port " << PORT << std::endl;
        while (true) {
            struct sockaddr_in client_addr{};
            socklen_t client_addr_len = sizeof(client_addr);
            int* client_fd = new int;

            *client_fd = accept(server_fd,
                                reinterpret_cast<struct sockaddr*>(&client_addr),
                                &client_addr_len);
            if (*client_fd < 0) {
                perror("accept failed");
                delete client_fd;
                continue;
            }

            pthread_t thread_id;
            pthread_create(&thread_id, nullptr, thread_entry, client_fd);
            pthread_detach(thread_id);
        }
    }

private:
    int server_fd;

    void setup_socket() {
        server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd < 0) {
            perror("socket failed");
            exit(EXIT_FAILURE);
        }

        struct sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(PORT);

        if (bind(server_fd,
                 reinterpret_cast<struct sockaddr*>(&server_addr),
                 sizeof(server_addr)) < 0) {
            perror("bind failed");
            exit(EXIT_FAILURE);
        }

        if (listen(server_fd, 10) < 0) {
            perror("listen failed");
            exit(EXIT_FAILURE);
        }
    }

    static void* thread_entry(void* arg) {
        int client_fd = *static_cast<int*>(arg);
        delete static_cast<int*>(arg);

        HttpClientHandler handler(client_fd);
        handler.handle();

        return nullptr;
    }
};

int main() {
    HttpServer server;
    server.run();
    return 0;
}

