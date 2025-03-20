#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h> 

int main(int argc, char* argv[]) {
  if (argc != 2) return 1;
  // Create socket and configure address 
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  struct sockaddr_in address = {
    .sin_family = AF_INET,
    .sin_addr.s_addr = INADDR_ANY,
    .sin_port = htons(8080)
  };

  // Bind socket to address 
  int status = bind(fd, (struct sockaddr*)&address, sizeof(address));

  // Listen for connections to web server 
  listen(fd, 5);

  // Run continuously 
  int client_socket; 

  // Simulate hardcoded respose 
  const char *front_response = 
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/plain\r\n\r\n";

  FILE* fptr = fopen(argv[1], "r");
  if (fptr == NULL) return 1;
  // Create buffer 
  int count = 0; 
  float v = 0.0;
  while (fscanf(fptr, "%f", &v) != EOF) count++;
  float* values = (float*)malloc(count * sizeof(float));
  fseek(fptr, SEEK_SET, 0);
  for (int i = 0; i < count; i++) fscanf(fptr, "%f", &values[i]);
  fclose(fptr);

  char* response_buffer = (char*)malloc(16 * count * sizeof(char));
  printf("Count: %d, size of buffer: %lu\n", count, 16 * count * sizeof(char));

  int offset = 0;
  // Create buffer from read file 
  for (int i = 0; i < count; i++) {
    offset += snprintf(response_buffer + offset, 16 * count - offset, "%.3f\n", values[i]);
  }
  // Null terminate buffer 
  response_buffer[offset] = '\0';
  
  printf("Response Buffer:\n%s\n", response_buffer);

  while (1) {
  
    client_socket = accept(fd, NULL, NULL);

    send(client_socket, front_response, strlen(front_response), 0);
    send(client_socket, response_buffer, strlen(response_buffer), 0);

    close(client_socket);
  }

  return 0;
}
