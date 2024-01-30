#include "debug.h"
#include "socket.h"
#include "fifo.h"

#include <stdlib.h>
#include <signal.h>
#include <pthread.h>

// SIGUSR1 - interrupting threads

pthread_t stdinThread;  // Communication from engine to client
pthread_t stdoutThread; // Communication from client to engine

int serverfd = -1;
int sockfd = -1;

int stdinFIFO = -1;
int stdoutFIFO = -1;

bool acceptNewClient = true; // The UCI node should continue running

// Communication from client to engine
void* stdout_routine(void* arg)
{
  info_print("Redirecting socket -> fifo");

  char buffer[1024];
  memset(buffer, '\0', sizeof(buffer));

  int status;

  while((status = socket_read(sockfd, buffer, sizeof(buffer))) > 0)
  {
    debug_print(stdout, "socket -> fifo", "%s", buffer);

    // If the client wants to quit
    if(!strncmp(buffer, "quit", 4)) break;

    if(buffer_write(stdinFIFO, buffer, sizeof(buffer)) == -1) break;

    memset(buffer, '\0', sizeof(buffer));
  }
  info_print("Stopped socket -> fifo");

  // If stdin thread has interrupted stdout thread
  if(status == -1 && errno == EINTR)
  {
    info_print("stdout routine interrupted"); 
  }

  // Interrupt stdin thread
  pthread_kill(stdinThread, SIGUSR1);

  return NULL;
}

// Communication from engine to client
void* stdin_routine(void* arg)
{
  info_print("Redirecting fifo -> socket");

  char buffer[1024];
  memset(buffer, '\0', sizeof(buffer));

  int status;

  while((status = buffer_read(stdoutFIFO, buffer, sizeof(buffer))) > 0)
  {
    debug_print(stdout, "fifo -> socket", "%s", buffer);

    if(socket_write(sockfd, buffer, sizeof(buffer)) == -1) break;

    memset(buffer, '\0', sizeof(buffer));
  }
  info_print("Stopped fifo -> socket");

  // If stdout thread has interrupted stdin thread
  if(status == -1 && errno == EINTR)
  {
    info_print("stdin routine interrupted"); 
  }
  // The FIFO interrupted this thread
  else acceptNewClient = false;

  // Interrupt stdout thread
  pthread_kill(stdoutThread, SIGUSR1);

  return NULL;
}

bool stdin_stdout_thread_create(pthread_t* stdinThread, pthread_t* stdoutThread)
{
  if(pthread_create(stdinThread, NULL, &stdin_routine, NULL) != 0)
  {
    error_print("Could not create stdin thread");

    return false;
  }
  if(pthread_create(stdoutThread, NULL, &stdout_routine, NULL) != 0)
  {
    error_print("Could not create stdout thread");

    // Interrupt stdin thread
    pthread_kill(*stdinThread, SIGUSR1);

    return false;
  }
  return true;
}

void stdin_stdout_thread_join(pthread_t stdinThread, pthread_t stdoutThread)
{
  if(pthread_join(stdinThread, NULL) != 0)
  {
    error_print("Could not join stdin thread");
  }
  if(pthread_join(stdoutThread, NULL) != 0)
  {
    error_print("Could not join stdout thread");
  }
}

// This is executed when the user interrupts the program
// - interrupt and stop the threads
// - close stdin and stdout FIFOs
// - close sock and server sockets
// - exit the program
void sigint_handler(int signum)
{
  error_print("Keyboard interrupt");

  stdin_stdout_fifo_close(&stdinFIFO, &stdoutFIFO);

  socket_close(&sockfd);
  socket_close(&serverfd);

  exit(1); // Exits the program with status 1
}

void sigint_handler_setup(void)
{
  struct sigaction sigAction;

  sigAction.sa_handler = sigint_handler;
  sigAction.sa_flags = 0;
  sigemptyset(&sigAction.sa_mask);

  sigaction(SIGINT, &sigAction, NULL);
}

void sigusr1_handler(int signum) {}

void sigusr1_handler_setup(void)
{
  struct sigaction sigAction;

  sigAction.sa_handler = sigusr1_handler;
  sigAction.sa_flags = 0;
  sigemptyset(&sigAction.sa_mask);

  sigaction(SIGUSR1, &sigAction, NULL);
}

void signals_handler_setup(void)
{
  signal(SIGPIPE, SIG_IGN); // Ignores SIGPIPE
  
  sigint_handler_setup();

  sigusr1_handler_setup();
}

void client_routine(void)
{
  if(stdin_stdout_thread_create(&stdinThread, &stdoutThread))
  {
    stdin_stdout_thread_join(stdinThread, stdoutThread);
  }
}

void server_routine(const char address[], int port)
{
  while(acceptNewClient)
  {
    if(!socket_accept(&sockfd, serverfd, address, port))
    {
      error_print("Failed to accept socket");

      return;
    }

    client_routine();

    socket_close(&sockfd);
  } 
}

int main(int argc, char* argv[])
{
  signals_handler_setup();

  char address[] = "";
  int port = 5555;

  char stdinFIFOname[] = "stdin";
  char stdoutFIFOname[] = "stdout";

  bool openOrder = true;
  
  if(!stdin_stdout_fifo_open(&stdinFIFO, stdinFIFOname, &stdoutFIFO, stdoutFIFOname, openOrder)) return 1;

  info_print("Creating socket server");

  if(server_socket_create(&serverfd, address, port, 1))
  {
    server_routine(address, port);

    socket_close(&serverfd);
  }

  if(!stdin_stdout_fifo_close(&stdinFIFO, &stdoutFIFO)) return 2;

  return 0;
}
