#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <thread>
#include <fstream>
#include <sstream>
#include <map>
#include <vector>
using namespace std;


// Define header map
using header_map = map<string, string>;

// Struct for request
struct request {
  string method;
  string path;
  string version;
  header_map headers;
  string body;
};

// Struct for response
struct response {
  string version = "HTTP/1.1";
  int status_code;
  string status_msg;
  header_map headers;
  string body;
};


string extract_info(const string& request, const string to_find, const string until) {
  // Find the position of the first space after GET
  size_t start_pos = request.find(to_find) + to_find.length();  // +4 to skip over "GET "
  
  // Find the position of the next space after the URL
  size_t end_pos = request.find(until, start_pos);
  
  // Extract the URL (substring between the two spaces)
  string url = request.substr(start_pos, end_pos - start_pos);
  
  return url;
}

// Function to parse headers from request
void parse_headers(const string& request, header_map& headers) {
  size_t header_start = request.find("\r\n") + 2;  // Skip first line (method, path, version)
  size_t header_end = request.find("\r\n\r\n");   // End of headers

  string headers_part = request.substr(header_start, header_end - header_start);

  stringstream ss(headers_part);
  string line;
  while (getline(ss, line)) {
      size_t colon_pos = line.find(": ");
      if (colon_pos != string::npos) {
          string key = line.substr(0, colon_pos);
          string value = line.substr(colon_pos + 2);
          headers[key] = value;
      }
  }
}

// Function to parse the request data
request parse_request(const string& request_str) {
  request req;
  
  // Parse the request line
  req.method = extract_info(request_str, "", " ");
  req.path = extract_info(request_str, req.method + " ", " ");
  req.version = extract_info(request_str, req.method + " " + req.path + " ", "\r\n");
  
  // Parse headers
  parse_headers(request_str, req.headers);
  
  // Parse the body (if any)
  size_t body_start = request_str.find("\r\n\r\n") + 4;  // Skip headers
  if (body_start != string::npos) {
      req.body.assign(request_str.begin() + body_start, request_str.end());
  }

  return req;
}


// Function to create a response
string create_response(const response& resp) {
  stringstream ss;
  ss << resp.version << " " << resp.status_code << " " << resp.status_msg << "\r\n";
  
  // Add headers
  for (const auto& header : resp.headers) {
      ss << header.first << ": " << header.second << "\r\n";
  }
  
  ss << "\r\n";  // End of headers
  
  // Add body
  ss << resp.body;
  return ss.str();
}

void handleClient (int client, int argc, char** argv) {

  char receivebuf[1024];
  int receivedbytes = recv(client, receivebuf, sizeof(receivebuf) - 1, 0);
  receivebuf[receivedbytes] = '\0';
  string request_str(receivebuf);

  // Parse req
  request req = parse_request(request_str);

  response resp;

  string reply = "";

  if (req.method == "GET"){
    if (req.path == "/") {
      resp.status_code = 200;
      resp.status_msg = "OK";
    }
    else if (req.path.starts_with("/user-agent")) {
      resp.status_code = 200;
      resp.status_msg = "OK";
      resp.headers["Content-Type"] = "text/plain";
      resp.headers["Content-Length"] = to_string(req.headers["User-Agent"].length());
      resp.body = req.headers["User-Agent"].c_str();
    }
    else if (req.path.starts_with("/echo")) {
      string echocontent = req.path.substr(6, req.path.length());
      resp.status_code = 200;
      resp.status_msg = "OK";
      resp.headers["Content-Type"] = "text/plain";
      resp.headers["Content-Length"] = to_string(echocontent.length());
      resp.body = echocontent;
    }
    else if (req.path.starts_with("/files")) {
      string file = req.path.substr(7, req.path.length());
      if (argc >= 3) {
        if (string(argv[1]) == "--directory") {
          file = argv[2] + file;
        }
      }
      ifstream myfile(file);
      if (myfile.is_open()) {
        ostringstream oss;
        oss << myfile.rdbuf();
        resp.body = oss.str();
        resp.status_code = 200;
        resp.status_msg = "OK";
        resp.headers["Content-Type"] = "application/octet-stream";
        resp.headers["Content-Length"] = to_string(resp.body.length());
      } else {
        resp.status_code = 404;
        resp.status_msg = "Not Found";
      }
    }
    else {
      resp.status_code = 404;
      resp.status_msg = "Not Found";
    }
  } else if (req.method == "POST") {
    if (req.path.starts_with("/files/")) {
      string file_name = req.path.substr(7, req.path.length());
      if (argc >= 3) {
        if (string(argv[1]) == "--directory") {
          file_name = argv[2] + file_name;
        }
      }
      ofstream wfile(file_name);
      wfile << req.body.c_str();
      wfile.close();
      resp.status_code = 201;
      resp.status_msg = "Created";
    }
  }

  string response_str = create_response(resp);
  send(client, response_str.c_str(), response_str.length(), 0);
  close(client);

}

int main(int argc, char **argv) {
  // Flush after every std::cout / std::cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;
  
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
   std::cerr << "Failed to create server socket\n";
   return 1;
  }
  
  // Since the tester restarts your program quite often, setting SO_REUSEADDR
  // ensures that we don't run into 'Address already in use' errors
  int reuse = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
    std::cerr << "setsockopt failed\n";
    return 1;
  }
  
  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(4221);
  
  if (bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) != 0) {
    std::cerr << "Failed to bind to port 4221\n";
    return 1;
  }
  
  int connection_backlog = 5;
  if (listen(server_fd, connection_backlog) != 0) {
    std::cerr << "listen failed\n";
    return 1;
  }
  
  while(1) {
    struct sockaddr_in client_addr;
    int client_addr_len = sizeof(client_addr);
    
    std::cout << "Waiting for a client to connect...\n";
    
    int client = accept(server_fd, (struct sockaddr *) &client_addr, (socklen_t *) &client_addr_len);
    std::cout << "Client connected\n";
    std::thread client_thread(handleClient, client, argc, argv);
    client_thread.detach();
  }

  close(server_fd);
  return 0;
}
