/*
 * Written by Hampus Fridholm
 *
 * Last updated: 2024-09-06
 */

#define DEFAULT_ADDRESS "127.0.0.1"
#define DEFAULT_PORT    5555

#include "debug.h"
#include "socket.h"
#include "fifo.h"
#include "thread.h"

#include <stdlib.h>
#include <signal.h>
#include <argp.h>

pthread_t stdin_thread;
bool      stdin_running = false;

pthread_t stdout_thread;
bool      stdout_running = false;

int sockfd = -1;
int servfd = -1;

int stdin_fifo  = -1;
int stdout_fifo = -1;

bool fifo_reverse = false;

bool node_running = true;


static char doc[] = "ucinode - network server hosting UCI chess engine";

static char args_doc[] = "";

static struct argp_option options[] =
{
  { "stdin",   'i', "FIFO",    0, "Stdin FIFO" },
  { "stdout",  'o', "FIFO",    0, "Stdout FIFO" },
  { "address", 'a', "ADDRESS", 0, "Network address" },
  { "port",    'p', "PORT",    0, "Network port" },
  { "debug",   'd', 0,         0, "Print debug messages" },
  { 0 }
};

struct args
{
  char*  stdin_path;
  char*  stdout_path;
  char*  address;
  int    port;
  bool   debug;
};

struct args args =
{
  .stdin_path  = NULL,
  .stdout_path = NULL,
  .address     = NULL,
  .port        = -1,
  .debug       = false
};

/*
 * This is the option parsing function used by argp
 */
static error_t opt_parse(int key, char* arg, struct argp_state* state)
{
  struct args* args = state->input;

  switch(key)
  {
    case 'i':
      // If the output fifo has already been inputted,
      // open the output fifo before the input fifo
      if(args->stdout_path) fifo_reverse = true;

      args->stdin_path = arg;
      break;

    case 'o':
      args->stdout_path = arg;
      break;

    case 'a':
      args->address = arg;
      break;

    case 'p':
      int port = atoi(arg);

      if(port != 0) args->port = port;
      break;

    case 'd':
      args->debug = true;
      break;

    case ARGP_KEY_ARG:
      break;

    case ARGP_KEY_END:
      break;

    default:
      return ARGP_ERR_UNKNOWN;
  }

  return 0;
}

/*
 * Communication from client to engine
 */
void* stdout_routine(void* arg)
{
  stdout_running = true;

  if(args.debug) info_print("Start of stdout routine");

  char buffer[1024];
  memset(buffer, '\0', sizeof(buffer));

  int read_status  = -1;
  int write_status = -1;

  while((read_status = socket_read(sockfd, buffer, sizeof(buffer))) > 0)
  {
    debug_print(stdout, "client -> engine", "%s", buffer);

    if(!strncmp(buffer, "quit", 4)) break;

    if((write_status = buffer_write(stdout_fifo, buffer, sizeof(buffer))) <= 0) break;

    memset(buffer, '\0', sizeof(buffer));
  }

  if(errno != 0)
  {
    if(args.debug) error_print("%s", strerror(errno));
  }

  // This code block is not needed, but catches the stopped engine earlier
  // If the stdout pipe is broken (32), the node should not be running
  if(write_status == -1 && errno == 32)
  {
    if(args.debug) info_print("Shutting node down");

    node_running = false;
  }

  if(args.debug) info_print("Interrupting stdin routine");

  if(stdin_running) pthread_kill(stdin_thread, SIGUSR1);

  stdout_running = false;

  if(args.debug) info_print("End of stdout routine");

  return NULL;
}

/*
 * Communication from engine to client
 */
void* stdin_routine(void* arg)
{
  stdin_running = true;

  if(args.debug) info_print("Start of stdin routine");

  char buffer[1024];
  memset(buffer, '\0', sizeof(buffer));

  int read_status  = -1;
  int write_status = -1;

  while((read_status = buffer_read(stdin_fifo, buffer, sizeof(buffer))) > 0)
  {
    debug_print(stdout, "engine -> client", "%s", buffer);

    if((write_status = socket_write(sockfd, buffer, sizeof(buffer))) <= 0) break;

    memset(buffer, '\0', sizeof(buffer));
  }

  if(errno != 0)
  {
    if(args.debug) error_print("%s", strerror(errno));
  }

  // This code block is not needed, but catches the stopped engine earlier
  // If the stdin fifo is broken (End Of File), the node should not be running
  if(read_status == 0)
  {
    if(args.debug) info_print("Shutting node down");

    node_running = false;
  }

  if(args.debug) info_print("Interrupting stdout routine");

  if(stdout_running) pthread_kill(stdout_thread, SIGUSR1);

  stdin_running = false;

  if(args.debug) info_print("End of stdin routine");

  return NULL;
}

/*
 * Keyboard interrupt - close the program (the threads)
 */
static void sigint_handler(int signum)
{
  if(args.debug) info_print("Keyboard interrupt");

  node_running = false;

  if(stdin_running)  pthread_kill(stdin_thread, SIGUSR1);

  if(stdout_running) pthread_kill(stdout_thread, SIGUSR1);
}

/*
 * Broken pipe - close the program (the threads)
 */
static void sigpipe_handler(int signum)
{
  if(args.debug) error_print("Pipe has been broken");

  node_running = false;

  if(stdin_running)  pthread_kill(stdin_thread, SIGUSR1);

  if(stdout_running) pthread_kill(stdout_thread, SIGUSR1);
}

/*
 * SIGUSR1 is sent when either stdin_thread or stdout_thread should be closed
 */
static void sigusr1_handler(int signum) { }

/*
 *
 */
static void sigint_handler_setup(void)
{
  struct sigaction sig_action;

  sig_action.sa_handler = sigint_handler;
  sig_action.sa_flags = 0;
  sigemptyset(&sig_action.sa_mask);

  sigaction(SIGINT, &sig_action, NULL);
}

/*
 *
 */
static void sigpipe_handler_setup(void)
{
  struct sigaction sig_action;

  sig_action.sa_handler = sigpipe_handler;
  sig_action.sa_flags = 0;
  sigemptyset(&sig_action.sa_mask);

  sigaction(SIGPIPE, &sig_action, NULL);
}

/*
 *
 */
static void sigusr1_handler_setup(void)
{
  struct sigaction sig_action;

  sig_action.sa_handler = sigusr1_handler;
  sig_action.sa_flags = 0;
  sigemptyset(&sig_action.sa_mask);

  sigaction(SIGUSR1, &sig_action, NULL);
}

/*
 *
 */
static void signals_handler_setup(void)
{
  sigpipe_handler_setup();
  
  sigint_handler_setup();

  sigusr1_handler_setup();
}

/*
 * Tell the chess engine to quit, by sending it a quit message
 */
static void engine_quit(void)
{
  if(args.debug) info_print("Quitting engine");

  if(stdout_fifo) message_write(stdout_fifo, "quit\n");
}

/*
 * Reset the chess engine for the next client, by 
 * - starting a new UCI game and
 * - setting the position to the start position 
 */
static int engine_reset(void)
{
  if(args.debug) info_print("Establishing engine UCI communication");

  message_write(stdout_fifo, "uci\n");

  char buffer[1024];
  memset(buffer, '\0', sizeof(buffer));

  // The "status" variable can be used to extract diagnostics
  int status;

  while((status = buffer_read(stdin_fifo, buffer, sizeof(buffer))) > 0)
  {
    if(strncmp(buffer, "uciok", 5) == 0) break;

    memset(buffer, '\0', sizeof(buffer));
  }

  if(errno != 0)
  {
    if(args.debug) error_print("%s", strerror(errno));

    return 1;
  }

  if(args.debug) info_print("Setting up new engine game");

  message_write(stdout_fifo, "ucinewgame\n");

  message_write(stdout_fifo, "position startpos\n");

  return 0;
}

/*
 * Run as long as the server is still running
 */
static void node_routine(void)
{
  while(node_running && servfd != -1)
  {
    if(engine_reset() != 0) break;

    sockfd = socket_accept(servfd, args.address, args.port, args.debug);

    // If the server socket fails, stop node
    if(sockfd == -1) break;

    stdin_stdout_thread_start(&stdin_thread, &stdin_routine, &stdout_thread, &stdout_routine, args.debug);

    socket_close(&sockfd, args.debug);
  }

  engine_quit();
}

/*
 * Create server socket using address and port arguments
 *
 * If either address or port is missing, use default value
 *
 * RETURN (int status)
 * - 0 | Success!
 * - 1 | Failed to create server socket
 */
static int args_server_socket_create(void)
{
  if(!args.address)   args.address = DEFAULT_ADDRESS;

  if(args.port == -1) args.port    = DEFAULT_PORT;

  servfd = server_socket_create(args.address, args.port, args.debug);

  return (servfd == -1) ? 1 : 0;
}

static struct argp argp = { options, opt_parse, args_doc, doc };

/*
 *
 */
int main(int argc, char* argv[])
{
  argp_parse(&argp, argc, argv, 0, 0, &args);

  signals_handler_setup();

  if(stdin_stdout_fifo_open(&stdin_fifo, args.stdin_path, &stdout_fifo, args.stdout_path, fifo_reverse, args.debug) == 0)
  {
    if(args_server_socket_create() == 0)
    {
      node_routine();
    }
  }

  stdin_stdout_fifo_close(&stdin_fifo, &stdout_fifo, args.debug);

  socket_close(&sockfd, args.debug);

  socket_close(&servfd, args.debug);

  if(args.debug) info_print("End of main");

  return 0;
}
