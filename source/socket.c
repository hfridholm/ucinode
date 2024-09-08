/*
 * Written by Hampus Fridholm
 *
 * Last updated: 2024-09-06
 */

#include "socket.h"

/*
 * Create sockaddr from address and port
 *
 * PARAMS
 * - bool debug | Print debug messages
 *
 * RETURN (struct sockaddr_in addr)
 */
static struct sockaddr_in sockaddr_create(int sockfd, const char* address, int port, bool debug)
{
  struct sockaddr_in addr;

  if(strlen(address) == 0)
  {
    socklen_t addrlen = sizeof(addr);

    if(getsockname(sockfd, (struct sockaddr*) &addr, &addrlen) == -1)
    {
      if(debug) error_print("Failed to get sock name: %s", strerror(errno));
    }
  }
  else addr.sin_addr.s_addr = inet_addr(address);

  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);

  return addr;
}

/*
 * bind, with debug messages
 *
 * RETURN (int status)
 * -  0 | Success
 * - -1 | Failed to bind socket
 */
static int socket_bind(int sockfd, const char* address, int port, bool debug)
{
  struct sockaddr_in addr = sockaddr_create(sockfd, address, port, debug);

  if(debug) info_print("Binding socket (%s:%d)", address, port);

  if(bind(sockfd, (struct sockaddr*) &addr, sizeof(addr)) == -1)
  {
    if(debug) error_print("Failed to bind socket (%s:%d): %s", address, port, strerror(errno));

    return -1;
  }
  
  if(debug) info_print("Binded socket (%s:%d)", address, port);

  return 0;
}

/*
 * listen, with debug messages
 *
 * RETURN (int status)
 * -  0 | Success
 * - -1 | Failed to listen to socket
 */
static int socket_listen(int sockfd, int backlog, bool debug)
{
  if(debug) info_print("Start listen to socket");

  if(listen(sockfd, backlog) == -1)
  {
    if(debug) error_print("Failed to listen to socket: %s", strerror(errno));

    return -1;
  }

  if(debug) info_print("Listening to socket");

  return 0;
}

/*
 * socket, with debug messages
 *
 * RETURN (int sockfd)
 * - >=0 | Success
 * -  -1 | Failed to create socket
 */
static int socket_create(bool debug)
{
  if(debug) info_print("Creating socket");

  int sockfd = socket(AF_INET, SOCK_STREAM, 0);

  if(sockfd == -1)
  {
    if(debug) error_print("Failed to create socket: %s", strerror(errno));

    return -1;
  }

  if(debug) info_print("Created socket (%d)", sockfd);

  return sockfd;
}

/*
 * Create a server socket, bind it and start listening for clients
 *
 * RETURN (int servfd)
 * - >=0 | Success
 * -  -1 | Failed to create server socket
 */
int server_socket_create(const char* address, int port, bool debug)
{
  int servfd = socket_create(debug);

  if(servfd == -1) return -1;

  if(socket_bind(servfd, address, port, debug) == -1 || socket_listen(servfd, 1, debug) == -1)
  {
    socket_close(&servfd, debug);

    return -1;
  }

  return servfd;
}

/*
 * accept, but with address and port, and with debug messages
 *
 * RETURN (int sockfd)
 * - >=0 | Success
 * -  -1 | Failed to accept socket
 */
int socket_accept(int servfd, const char* address, int port, bool debug)
{
  struct sockaddr_in sockaddr = sockaddr_create(servfd, address, port, debug);

  int addrlen = sizeof(sockaddr);

  if(debug) info_print("Accepting socket");

  int sockfd = accept(servfd, (struct sockaddr*) &sockaddr, (socklen_t*) &addrlen);

  if(sockfd == -1)
  {
    if(debug) error_print("Failed to accept socket: %s", strerror(errno));

    return -1;
  }

  if(debug) info_print("Accepted socket (%d)", sockfd);

  return sockfd;
}

/*
 * close, but with pointer to file descriptor, and with debug messages
 *
 * Note: If no open socket is supplied, nothing is done
 *
 * RETURN (int status)
 * - 0 | Success
 * - 1 | Failed to close socket
 */
int socket_close(int* sockfd, bool debug)
{
  if(!sockfd || *sockfd == -1) return 0;

  if(debug) info_print("Closing socket (%d)", *sockfd);

  if(close(*sockfd) == -1)
  {
    if(debug) error_print("Failed to close socket: %s", strerror(errno));

    return -1;
  }

  if(debug) info_print("Closed socket");

  *sockfd = -1;

  return 0;
}

/*
 * Read a single line to a buffer from a socket connection
 *
 * RETURN (ssize_t size)
 * - >0 | The number of read characters
 * -  0 | Nothing to read, end of file
 * - -1 | Failed to read from socket
 */
ssize_t socket_read(int sockfd, char* buffer, size_t size)
{
  if(errno != 0) return -1;

  if(!buffer) return 0;

  char symbol = '\0';
  ssize_t index;

  for(index = 0; index < size && symbol != '\n'; index++)
  {
    ssize_t status = recv(sockfd, &symbol, 1, 0);

    if(status == -1 || errno != 0) return -1; // ERROR

    buffer[index] = symbol;

    if(status == 0) return 0; // End Of File
  }

  return index;
}

/*
 * Write a single line from a buffer to a socket connection
 *
 * RETURN (ssize_t size)
 * - >0 | The number of written characters
 * -  0 | Nothing to write to, end of file
 * - -1 | Failed to write to socket
 */
ssize_t socket_write(int sockfd, const char* buffer, size_t size)
{
  if(errno != 0) return -1;

  if(!buffer) return 0;

  ssize_t index;
  char symbol = '\0';

  for(index = 0; index < size && symbol != '\n'; index++)
  {
    symbol = buffer[index];

    ssize_t status = send(sockfd, &symbol, 1, 0);

    if(status == -1 || errno != 0) return -1; // ERROR
    
    if(status == 0) return 0; // End Of File

    if(symbol == '\0') break;
  }

  return index;
}
