#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string.h>
#include <string>
#include <string_view>
#include <algorithm>
#include <sys/socket.h>
#include <unistd.h>

constexpr std::size_t BUFFER_SIZE = 4096;

class FileHelper {
public:
  // Return the file extension from a given filesystem path
  static std::filesystem::path file_ext(const std::filesystem::path &path) {
    return path.extension();
  }

  // Determine the MIME type based on file extension (case-insensitive)
  static std::string mime_type(const std::string &ext) {
    // Convert extension to lowercase for case-insensitive comparison
    std::string lower_ext = ext;
    std::transform(lower_ext.begin(), lower_ext.end(), lower_ext.begin(),
                   ::tolower);

    // Match known extensions to their MIME types
    if (lower_ext == EXT_HTML || lower_ext == EXT_HTM)
      return MIME_HTML;
    else if (lower_ext == EXT_TXT)
      return MIME_TXT;
    else if (lower_ext == EXT_JPG || lower_ext == EXT_JPEG)
      return MIME_JPEG;
    else if (lower_ext == EXT_PNG)
      return MIME_PNG;

    // Return default MIME type if extension is unknown
    else
      return MIME_DEFAULT;
  }

private:
  // Known file extensions
  static constexpr const char *EXT_HTML = "html";
  static constexpr const char *EXT_HTM = "htm";
  static constexpr const char *EXT_TXT = "txt";
  static constexpr const char *EXT_JPG = "jpg";
  static constexpr const char *EXT_JPEG = "jpeg";
  static constexpr const char *EXT_PNG = "png";

  // Corresponding MIME types
  static constexpr const char *MIME_HTML = "text/html";
  static constexpr const char *MIME_TXT = "text/plain";
  static constexpr const char *MIME_JPEG = "image/jpeg";
  static constexpr const char *MIME_PNG = "image/png";
  static constexpr const char *MIME_DEFAULT = "application/octet-stream"; // fallback for unknown types
};

class UrlDecoder {
public:
  // Decode URL-encoded string; sets 'correct' false if invalid sequences found
  std::string decode(std::string_view src, bool &correct) const {
    std::string output;
    output.reserve(src.size()); // Reserve space to avoid reallocations
    correct = true;

    for (size_t i = 0; i < src.size(); ++i) {
      char c = src[i];

      if (c == PERCENT) {
        // '%' signals start of hex-encoded byte; expect two hex digits after
        // If missing or invalid, mark as incorrect and copy '%' literally
        if (i + 2 >= src.size() || !is_hex(src[i + 1]) || !is_hex(src[i + 2])) {
          correct = false;
          output += PERCENT;
          continue;
        }

        // Convert hex pair to single char
        int high = hex_value(src[i + 1]);
        int low = hex_value(src[i + 2]);
        output += static_cast<char>((high << 4) | low);
        i += 2; // Skip processed hex digits
      }
      else if (c == PLUS) {
        output += SPACE; // '+' in URLs represents space
      }
      else {
        output += c; // Copy all other characters as-is
      }
    }

    return output;
  }

private:
  // URL special chars
  static constexpr char PERCENT = '%';
  static constexpr char PLUS = '+';
  static constexpr char SPACE = ' ';

  // Check if character is valid hex digit
  static bool is_hex(char ch) {
    return (ch >= ZERO && ch <= NINE) || (ch >= LOWER_A && ch <= LOWER_F) || (ch >= UPPER_A && ch <= UPPER_F);
  }

  // Convert hex digit to integer value
  static int hex_value(char ch) {
    if (ch >= ZERO && ch <= NINE) return ch - ZERO;
    if (ch >= LOWER_A && ch <= LOWER_F) return ch - LOWER_A + 10;
    return ch - UPPER_A + 10;
  }

  // Hex digits
  static constexpr char ZERO = '0';
  static constexpr char NINE = '9';
  static constexpr char LOWER_A = 'a';
  static constexpr char LOWER_F = 'f';
  static constexpr char UPPER_A = 'A';
  static constexpr char UPPER_F = 'F';
};

class HttpResponseBuilder {
public:
    static std::string build_http_response(const std::filesystem::path& file_path) {
        // First, check if the requested file exists (e.g., "index.html")
        if (!std::filesystem::exists(file_path)) {
            return response_404();
        }

        // Read the whole file into a string
        std::ifstream file(file_path, std::ios::binary);
        if (!file) {
            return response_404();
        }

        std::ostringstream file_content;
        file_content << file.rdbuf();

        const std::string& mime = FileHelper::mime_type(file_path.extension().string());

        // Build the HTTP header and response
        std::ostringstream response;
        response << STATUS_OK
                 << "Content-Type: " << mime << "\r\n"
                 << "Content-Length: " << file_content.str().size() << "\r\n"
                 << "\r\n"
                 << file_content.str();

        return response.str();
    }

private:
    static std::string response_404() {
        std::ostringstream response;
        response << STATUS_NOT_FOUND
                 << CONTENT_TYPE_TEXT
                 << "Content-Length: " << strlen(MSG_NOT_FOUND) << "\r\n"
                 << "\r\n"
                 << MSG_NOT_FOUND;
        return response.str();
    }

    static constexpr const char* STATUS_OK = "HTTP/1.1 200 OK\r\n";
    static constexpr const char* STATUS_NOT_FOUND = "HTTP/1.1 404 Not Found\r\n";
    static constexpr const char* CONTENT_TYPE_TEXT = "Content-Type: text/plain\r\n";
    static constexpr const char* MSG_NOT_FOUND = "404 Not Found";
};

class ClientHandler {
public:
    // Constructor: takes a client socket file descriptor and initializes RAII guard
    explicit ClientHandler(int client_fd) noexcept : client_fd_(client_fd), guard_(client_fd) {}

    // Main handler function to process the client request step-by-step
    void handle() noexcept {
        if (!receive_request()) return;    // Receive raw HTTP request from client
        if (!parse_request()) return;      // Parse HTTP GET request and extract URL
        if (!decode_url()) return;         // Decode URL (percent-encoding)

        // Construct full file path by appending decoded URL to root directory
        const std::filesystem::path file_path = std::filesystem::path(ROOT_DIR) / decoded_path_;

        // Build HTTP response (status line, headers, body) from the requested file
        const std::string response = HttpResponseBuilder::build_http_response(file_path);

        // Send the complete HTTP response back to the client socket
        ::send(client_fd_, response.data(), response.size(), 0);
    }

private:
    int client_fd_;  // Client socket file descriptor
    std::array<char, BUFFER_SIZE> buffer_{}; // Buffer to store raw bytes from client
    ssize_t bytes_received_ = 0;              // Number of bytes read from socket
    std::string_view request_;                // View into buffer holding the HTTP request
    std::string url_to_decode_;               // Raw URL extracted from the HTTP request
    std::string decoded_path_;                // Decoded URL path (percent-decoded)
    bool decode_success_ = false;             // Flag indicating successful URL decoding

    // Constants for server root directory and standard HTTP 400 response message
    static constexpr std::string_view ROOT_DIR = "www";
    static constexpr std::string_view BAD_REQUEST_RESPONSE = "HTTP/1.1 400 Bad Request\r\nContent-Length:0\r\n\r\n";

    // Regex pattern to parse simple HTTP GET request and capture requested path
    static constexpr std::string_view GET_REQUEST_REGEX = R"(^GET /([^ ]*) HTTP/1)";

    // RAII guard to automatically close the socket on object destruction
    struct FdGuard {
        int fd;
        explicit FdGuard(int fd_in) : fd(fd_in) {}
        ~FdGuard() { if (fd != -1) ::close(fd); }
        FdGuard(const FdGuard&) = delete;
        FdGuard& operator=(const FdGuard&) = delete;
    } guard_;

    // Receive raw HTTP request data from client socket
    bool receive_request() noexcept {
        bytes_received_ = ::recv(client_fd_, buffer_.data(), buffer_.size(), 0);
        if (bytes_received_ <= 0) {
            return false; // Connection closed or error
        }
        request_ = std::string_view(buffer_.data(), static_cast<std::size_t>(bytes_received_));
        return true;
    }

    // Parse the HTTP GET request and extract the requested URL path
    bool parse_request() noexcept {
        static const std::regex get_re(GET_REQUEST_REGEX.data(), std::regex::extended);
        std::cmatch match;

        // Use regex to find "GET /path HTTP/1" pattern
        if (!std::regex_search(request_.data(), request_.data() + request_.size(), match, get_re) || match.size() != 2) {
            send_response(BAD_REQUEST_RESPONSE); // Malformed request -> send 400 Bad Request
            return false;
        }
        url_to_decode_ = match[1].str(); // Extract the raw URL path
        return true;
    }

    // Decode percent-encoded URL (e.g., %20 -> space)
    bool decode_url() noexcept {
        UrlDecoder decoder;
        decoded_path_ = decoder.decode(url_to_decode_, decode_success_);
        if (!decode_success_) {
            send_response(BAD_REQUEST_RESPONSE); // Invalid encoding -> send 400 Bad Request
            return false;
        }
        return true;
    }

    // Helper function to send raw response bytes to client socket
    void send_response(std::string_view response) noexcept {
        // Use ::send to explicitly call global send() function from sockets API
        ::send(client_fd_, response.data(), response.size(), 0);
    }
};


/*
int main(int argc, char *argv[]) {
    int server_fd;
    struct sockaddr_in server_addr;

    // create server socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // config socket
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // bind socket to port
    if (bind(server_fd, 
            (struct sockaddr *)&server_addr, 
            sizeof(server_addr)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // listen for connections
    if (listen(server_fd, 10) < 0) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", PORT);
    while (1) {
        // client info
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int *client_fd = malloc(sizeof(int));

        // accept client connection
        if ((*client_fd = accept(server_fd, 
                                (struct sockaddr *)&client_addr, 
                                &client_addr_len)) < 0) {
            perror("accept failed");
            continue;
        }

        // create a new thread to handle client request
        pthread_t thread_id;
        pthread_create(&thread_id, NULL, handle_client, (void *)client_fd);
        pthread_detach(thread_id);
    }

    close(server_fd);
    return 0;
}*/