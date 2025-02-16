#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
using namespace std;

string extract_url(const string& request) {
  // Find the position of the first space after GET
  size_t start_pos = request.find("GET ") + 4;  // +4 to skip over "GET "
  
  // Find the position of the next space after the URL
  size_t end_pos = request.find(" ", start_pos);
  
  // Extract the URL (substring between the two spaces)
  string url = request.substr(start_pos, end_pos - start_pos);
  
  return url;
}

int main(int argc, char **argv) {
  // Flush after every std::cout / std::cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;
  

  // Uncomment this block to pass the first stage
  //
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
  
  struct sockaddr_in client_addr;
  int client_addr_len = sizeof(client_addr);
  
  std::cout << "Waiting for a client to connect...\n";
  
  int client = accept(server_fd, (struct sockaddr *) &client_addr, (socklen_t *) &client_addr_len);

  char receivebuf[1024];
  int receivedbytes = recv(client, receivebuf, sizeof(receivebuf) - 1, 0);
  receivebuf[receivedbytes] = '\0';
  string request(receivebuf);

  string url = extract_url(receivebuf);
  string reply = "";

  if (url == "/") {
    reply = "HTTP/1.1 200 OK\r\n\r\n";
  }
  else if (url.starts_with("/echo")) {
    string echocontent = url.substr(6, url.length());
    reply = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: " 
                   + std::to_string(echocontent.length()) 
                   + "\r\n\r\n" 
                   + echocontent;
  }
  else {
    reply = "HTTP/1.1 404 Not Found\r\n\r\n";
  }
  send(client, reply.c_str(), reply.length(), 0);
  std::cout << "Client connected\n";

  close(server_fd);

  return 0;
}
