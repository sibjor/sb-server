#include <arpa/inet.h>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <netinet/in.h>
#include <regex>
#include <string.h>
#include <string>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <thread>
#include <sstream>
#include <fstream>

#define PORT 80
#define BUFFER_SIZE 104857600

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

/*
void *handle_client(void *arg) {
    int client_fd = *((int *)arg);
    char *buffer = (char *)malloc(BUFFER_SIZE * sizeof(char));

    // receive request data from client and store into buffer
    ssize_t bytes_received = recv(client_fd, buffer, BUFFER_SIZE, 0);
    if (bytes_received > 0) {
        // check if request is GET
        regex_t regex;
        regcomp(&regex, "^GET /([^ ]*) HTTP/1", REG_EXTENDED);
        regmatch_t matches[2];

        if (regexec(&regex, buffer, 2, matches, 0) == 0) {
            // extract filename from request and decode URL
            buffer[matches[1].rm_eo] = '\0';
            const char *url_encoded_file_name = buffer + matches[1].rm_so;
            char *file_name = url_decode(url_encoded_file_name);

            // get file extension
            char file_ext[32];
            strcpy(file_ext, get_file_extension(file_name));

            // build HTTP response
            char *response = (char *)malloc(BUFFER_SIZE * 2 * sizeof(char));
            size_t response_len;
            build_http_response(file_name, file_ext, response, &response_len);

            // send HTTP response to client
            send(client_fd, response, response_len, 0);

            free(response);
            free(file_name);
        }
        regfree(&regex);
    }
    close(client_fd);
    free(arg);
    free(buffer);
    return NULL;
}
*/
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
}
*/