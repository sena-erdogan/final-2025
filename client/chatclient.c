#include "../chatlib.h"

char * username;
char * input;
char temp[1024];
int client_socket;

void handler(int signum) {
  if (signum == SIGINT) {

    Request request;

    memset(temp, '\0', sizeof(temp));
    strcat(temp, "[ERROR] User ");
    strcat(temp, username);
    strcat(temp, " received SIGINT\n");

    write(1, RED, strlen(RED));
    write(1, temp, strlen(temp));
    write(1, "\033[0m", 4);

    memset( & request, '\0', sizeof(Request));
    strncpy(request.message, temp, strlen(temp));
    strncpy(request.username, username, strlen(username));

    send(client_socket, & request, sizeof(request), 0);

    free(username);
    free(input);
    close(client_socket);

    exit(EXIT_SUCCESS);
  }
}

int main(int argc, char * argv[]) {

  if (argc != 3) {
    write(1, "Usage: ./chatclient <server_ip> <port>\n", strlen("Usage: ./chatclient <server_ip> <port>\n"));
    return 1;
  }

  struct sigaction sa;
  memset( & sa, 0, sizeof(sa));
  sa.sa_handler = handler;
  sigaction(SIGINT, & sa, NULL);

  char * server_ip = argv[1];
  char * port_number = argv[2];

  int status;
  struct sockaddr_in serv_addr;

  int port = atoi(port_number);

  bool error_occurred = true;
  size_t len;

  Request request;
  Response response;

  username = malloc(MAX_INPUT_LENGTH * sizeof(char));

  while (error_occurred == true) {

    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
      write(1, "\n Socket creation error \n", strlen("\n Socket creation error \n"));
      return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, server_ip, & serv_addr.sin_addr) <=
      0) {
      write(1,
        "\nInvalid address/ Address not supported \n", strlen("\nInvalid address/ Address not supported \n"));
      return -1;
    }

    if ((status = connect(client_socket, (struct sockaddr * ) & serv_addr,
        sizeof(serv_addr))) <
      0) {
      write(1, "\nConnection Failed \n", strlen("\nConnection Failed \n"));
      return -1;
    }

    write(1, "Enter your username: ", strlen("Enter your username: "));

    memset(username, '\0', MAX_INPUT_LENGTH);

    read(0, username, MAX_INPUT_LENGTH);

    len = strlen(username);
    if (len > 0 && username[len - 1] == '\n') {
      username[len - 1] = '\0';
    }

    memset( & request, '\0', sizeof(Request));

    request.command = CONNECT;
    strncpy(request.username, username, strlen(username));
    strncpy(request.ip, server_ip, strlen(server_ip));

    send(client_socket, & request, sizeof(request), 0);

    memset( & response, '\0', sizeof(Response));

    if (read(client_socket, & response, sizeof(Response)) <= 0) {
      write(1, "Problem with reading response from socket\n", strlen("Problem with reading response from socket\n"));
    }
    if (strstr(response.message, "REJECTED") != NULL) {
      write(1, RED, strlen(RED));
      write(1, response.message, strlen(response.message));
      write(1, "\033[0m", 4);
      continue;
    } else if (strstr(response.message, "CONNECT") != NULL) {
      error_occurred = false;
    } else {
      write(1, "Problem with response from socket\n", strlen("Problem with response from socket\n"));
      close(client_socket);

      break;
    }

  }

  write(1, "\n/join <room_name> -> Join or create a room\n/leave → Leave the current room\n/broadcast <message> -> Send message to everyone in the room\n/whisper <username> <message> -> Send private message\n/sendfile <filename> <username> -> Send file to user (within file size limit)\n/exit -> Disconnect from the server\n\n", strlen("\n/join <room_name> -> Join or create a room\n/leave → Leave the current room\n/broadcast <message> -> Send message to everyone in the room\n/whisper <username> <message> -> Send private message\n/sendfile <filename> <username> -> Send file to user (within file size limit)\n/exit -> Disconnect from the server\n\n"));

  write(1, "> ", strlen("> "));

  input = malloc(MAX_INPUT_LENGTH * sizeof(char));

  char * command;

  char * arg;

  int max_fd;

  memset(input, '\0', MAX_INPUT_LENGTH);

  while (1) {

    fd_set read_fds;
    FD_ZERO( & read_fds);
    FD_SET(0, & read_fds);
    FD_SET(client_socket, & read_fds);

    max_fd = client_socket;

    if (select(max_fd + 1, & read_fds, NULL, NULL, NULL) < 0) {
      write(1, "A problem occured while initializing select()\n", strlen("A problem occured while initializing select()\n"));
      free(username);
      close(client_socket);
      return -1;
    }

    if (FD_ISSET(0, & read_fds)) {
      if (read(0, input, MAX_INPUT_LENGTH) > 0) {

        len = strlen(input);
        if (len > 0 && input[len - 1] == '\n') {
          input[len - 1] = '\0';
        }

        command = strtok(input, " \t");

        memset( & request, '\0', sizeof(Request));

        strncpy(request.username, username, strlen(username));
        strncpy(request.ip, server_ip, strlen(server_ip));

        if (command && strcmp(command, "/join") == 0) {

          request.command = JOIN;

          arg = strtok(NULL, " \t");

          if (arg) {
            if (strchr(arg, ' ')) {
              write(1, "Invalid input\n", strlen("Invalid input\n"));
              continue;
            }
            strncpy(request.room_name, arg, strlen(arg));
          } else {
            write(1, "Invalid input\n", strlen("Invalid input\n"));
            continue;
          }
        } else if (command && strcmp(command, "/leave") == 0) {

          arg = strtok(NULL, " \t");

          if (arg) {
            write(1, "Invalid input\n", strlen("Invalid input\n"));
            continue;
          }

          request.command = LEAVE;

        } else if (command && strcmp(command, "/broadcast") == 0) {

          request.command = BROADCAST;

          arg = strtok(NULL, "\n");

          if (arg) {
            strncpy(request.message, arg, strlen(arg));
          } else {
            write(1, "Invalid input\n", strlen("Invalid input\n"));
            continue;
          }
        } else if (command && strcmp(command, "/whisper") == 0) {

          request.command = WHISPER;

          arg = strtok(NULL, " \t");

          if (arg) {
            strncpy(request.to_username, arg, strlen(arg));

            arg = strtok(NULL, "\n");

            if (arg) {
              strncpy(request.message, arg, strlen(arg));
            } else {
              write(1, "Invalid input\n", strlen("Invalid input\n"));
              continue;
            }

          } else {
            write(1, "Invalid input\n", strlen("Invalid input\n"));
            continue;
          }
        } else if (command && strcmp(command, "/sendfile") == 0) {

          request.command = SENDFILE;

          arg = strtok(NULL, " \t");

          if (arg) {
            strncpy(request.filename, arg, strlen(arg));

            arg = strtok(NULL, "\n");

            if (arg) {
              strncpy(request.to_username, arg, strlen(arg));
            } else {
              write(1, "Invalid input\n", strlen("Invalid input\n"));
              continue;
            }
          } else {
            write(1, "Invalid input\n", strlen("Invalid input\n"));
            continue;
          }
        } else if (command && strcmp(command, "/exit") == 0) {

          arg = strtok(NULL, " \t");

          if (arg) {
            write(1, "Invalid input\n", strlen("Invalid input\n"));
            continue;
          }

          request.command = EXIT;

        } else {
          write(1, "Unknown command.\n> ", strlen("Unknown command.\n> "));
          memset(input, '\0', MAX_INPUT_LENGTH);
          continue;
        }

        send(client_socket, & request, sizeof(request), 0);

        memset(input, '\0', MAX_INPUT_LENGTH);

        write(1, "> ", strlen("> "));

      }
    }
    if (FD_ISSET(client_socket, & read_fds)) {
      memset( & response, '\0', sizeof(Response));

      if (read(client_socket, & response, sizeof(Response)) <= 0) {
        write(1, "Problem with reading response from socket\n", strlen("Problem with reading response from socket\n"));
      }
      if (strstr(response.message, "REJECTED") != NULL) {
        write(1, RED, strlen(RED));
        write(1, response.message, strlen(response.message));
        write(1, "\033[0m", 4);
      } else if (strstr(response.message, "ERROR") != NULL) {
        write(1, RED, strlen(RED));
        write(1, response.message, strlen(response.message));
        write(1, "\033[0m", 4);

        free(username);
        free(input);
        close(client_socket);
        return 0;
      } else {
        write(1, GREEN, strlen(GREEN));
        write(1, response.message, strlen(response.message));
        write(1, "\033[0m", 4);
      }

      if (strstr(response.message, "EXIT") != NULL) {
        free(username);
        free(input);
        close(client_socket);
        return 0;
      }

      write(1, "> ", strlen("> "));
    }
  }

  free(username);
  free(input);
  close(client_socket);
  return 0;
}
