#include "../chatlib.h"

char temp[256];
char logMessage[256];

char * logFile = "example_log.txt";

int server_fd, server_socket;

User * users[MAX_CLIENTS];
int number_of_users = 0;

Room rooms[MAX_NUMBER_OF_ROOMS];
int number_of_rooms = 0;

Queue * upload_queue;
Queue * waiting_queue;

pthread_mutex_t rooms_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;

sem_t log_sem;

void writeToLog() {

sem_wait( & log_sem);

  if (strstr(logMessage, "REJECTED") != NULL || strstr(logMessage, "ERROR") != NULL) {
    write(1, RED, strlen(RED));
    write(1, logMessage, strlen(logMessage));
    write(1, "\033[0m", 4);
  } else {
    write(1, GREEN, strlen(GREEN));
    write(1, logMessage, strlen(logMessage));
    write(1, "\033[0m", 4);
  }

  write(1, "\n", strlen("\n"));

  int fd, lock;
  char timestamp[25];

  lock = LOCK_EX;
  fd = open(logFile, WRITE_FLAGS, PERMS);

  if (fd == -1) {
    write(1, "Could not open log file: ", strlen("Could not open log file: "));
    write(1, logFile, strlen(logFile));
    write(1, "\n", strlen("\n"));
    
    sem_post( & log_sem);
    return;
  }

  if (flock(fd, lock) == -1) {
    if (errno == EWOULDBLOCK) {
      write(1, "Log file: ", strlen("Log file: "));
      write(1, logFile, strlen(logFile));
      write(1, " is already locked.\n", strlen(" is already locked.\n"));

      if (close(fd) == -1) {
        write(1, "Could not close log file: ", strlen("Could not close log file: "));
        write(1, logFile, strlen(logFile));
        write(1, "\n", strlen("\n"));
      }
    }
    write(1, "Log file couldn't be locked\n", strlen("Log file couldn't be locked\n"));
    sem_post( & log_sem);
    return;
  }

  strcpy(timestamp, strtok(get_timestamp(), "\n"));

  write(fd, "[", 1);
  write(fd, timestamp, strlen(timestamp));
  write(fd, "] ", 2);
  write(fd, logMessage, strlen(logMessage));
  write(fd, "\n", 1);

  if (flock(fd, LOCK_UN) == -1) {
    write(1, "Could not unlock log file: ", strlen("Could not unlock log file: "));
    write(1, logFile, strlen(logFile));
    write(1, "\n", strlen("\n"));
  }

  if (close(fd) == -1) {
    write(1, "Could not close log file: ", strlen("Could not close log file: "));
    write(1, logFile, strlen(logFile));
    write(1, "\n", strlen("\n"));
  }
  sem_post( & log_sem);
}

void handler(int signum) {
  if (signum == SIGINT) {
  
  Response response;
  
      memset(logMessage, '\0', sizeof(logMessage));
      strcat(logMessage, "[ERROR] Server received SIGINT\n");
      writeToLog();

      memset( & response, '\0', sizeof(Response));
      strncpy(response.message, logMessage, strlen(logMessage));
      
      for(int i=0; i<number_of_users; i++){
        if (send(users[i] -> socket_fd, & response, sizeof(response), 0) <= 0) {
          write(1, "Problem with sending response to socket\n", strlen("Problem with sending response to socket\n"));
        }
      }
      
      for(int i=0; i<number_of_users; i++){
    free(users[i]);
  }
    
    sem_destroy( & log_sem);
    
    destroy(upload_queue);
  destroy(waiting_queue);
  
    close(server_socket);
  close(server_fd);
  
    exit(EXIT_SUCCESS);
  }
}

void * send_file(void * arg) {
  int i, count, user_num, to_user_num, src, dst;
  ssize_t bytes_read = 0;
  ssize_t bytes_written = 0;
  ssize_t bw;
  char source[MAX_INPUT_LENGTH];
  char destination[MAX_INPUT_LENGTH];
  char dest[MAX_INPUT_LENGTH];
  char buffer[4096];
  char name_part[256];
  char extension[10];
  char *name;
  size_t length;
  Request req;
  Response response;

  while (1) {
    to_user_num = -1;
    if (!is_empty(upload_queue)) {
      pthread_mutex_lock( & queue_mutex);

      req = pop(upload_queue);

      for (i = 0; i < number_of_users; i++) {
        if (strcmp(users[i] -> username, req.username) == 0) {
          user_num = i;
          break;
        }
      }

      for (i = 0; i < number_of_users; i++) {
        if (strcmp(users[i] -> username, req.to_username) == 0) {
          to_user_num = i;
          break;
        }
      }

      memset(source, '\0', sizeof(source));
      strcat(source, ARCHIVE);
      strcat(source, req.username);
      strcat(source, "/");
      strcat(source, req.filename);

      memset(destination, '\0', sizeof(dest));
      strcat(destination, ARCHIVE);
      strcat(destination, req.to_username);
      strcat(destination, "/");
      strcat(destination, req.filename);
      
      strncpy(dest, destination, strlen(destination));
    dest[strlen(destination)] = '\0';

   count = 1;
    while (access(dest, F_OK) == 0) {
        name = strrchr(destination, '.');

        length = name - destination + 1;
        
        strncpy(name_part, destination, length);
        name_part[length] = '\0';        
        
strcpy(extension, "_");
itoa(count, temp);
        strcat(extension, temp);

        strncpy(dest, name_part, strlen(dest));
        dest[strlen(dest) - 1] = '\0';
        strncat(dest, extension, strlen(dest));
        strncat(dest, name, strlen(dest));

        count++;
    }

      src = open(source, O_RDONLY, PERMS);
      if (src == -1) {
        write(1, "Error opening source file.\n", 28);
        break;
      }

      dst = open(dest, O_WRONLY | O_CREAT | O_TRUNC, PERMS);
      if (dst == -1) {
        write(1, "Error opening destination file.\n", 32);
        close(src);
        break;
      }
      while ((bytes_read = read(src, buffer, sizeof(buffer))) > 0) {
        bytes_written = 0;
        while (bytes_written < bytes_read) {
          bw = write(dst, buffer + bytes_written, bytes_read - bytes_written);
          if (bw == -1) {
            write(1, "Error writing to destination file.\n", 36);
            close(src);
            close(dst);
            break;
          }
          bytes_written += bw;
        }
      }

      if (bytes_read == -1) {
        write(1, "Error reading from source file.\n", 33);
      }

      memset(logMessage, '\0', sizeof(logMessage));
      strcat(logMessage, "[SENDFILE] ");
      strcat(logMessage, req.username);
      strcat(logMessage, " sent ");
      strcat(logMessage, req.filename);
      strcat(logMessage, " to ");
      strcat(logMessage, req.to_username);
      strcat(logMessage, "\n");
      writeToLog();

      memset( & response, '\0', sizeof(Response));
      strncpy(response.message, logMessage, strlen(logMessage));

      if (send(users[user_num] -> socket_fd, & response, sizeof(response), 0) <= 0) {
        write(1, "Problem with sending response to socket\n", strlen("Problem with sending response to socket\n"));
      }

      if (send(users[to_user_num] -> socket_fd, & response, sizeof(response), 0) <= 0) {
        write(1, "Problem with sending response to socket\n", strlen("Problem with sending response to socket\n"));
      }

      close(src);
      close(dst);

      if (!is_empty(waiting_queue) && size(upload_queue) < MAX_UPLOAD_QUEUE) {
        Request next = pop(waiting_queue);
        push(upload_queue, next);
      }
      pthread_mutex_unlock( & queue_mutex);
      sleep(2);
    } else {
      sleep(1);
    }
  }
  return NULL;
}

void * thread(void * arg) {
  int thread_socket = * (int * ) arg;

  Request request;
  Response response;
  
  struct stat st;

  int user_num = -1;
  int room_num = -1;
  int to_user_num = -1;
  int i = 0, j = 0;
  
  char file_name[MAX_INPUT_LENGTH];
  char extension[10];
  int file_size = 0.0;

  memset( & request, '\0', sizeof(Request));
  memset( & response, '\0', sizeof(Response));

  while (read(thread_socket, & request, sizeof(Request)) > 0) {

    pthread_mutex_lock( & rooms_mutex);

    user_num = -1;
    room_num = -1;

    for (i = 0; i < number_of_users; i++) {
      if (strcmp(users[i] -> username, request.username) == 0) {

        user_num = i;

        break;

      }

    }

    for (i = 0; i < number_of_rooms; i++) {
      if (strcmp(rooms[i].name, request.room_name) == 0 || strcmp(rooms[i].name, users[user_num] -> room) == 0) {

        room_num = i;

        break;

      }

    }
    
    if (strstr(request.message, "ERROR") != NULL){
           memset(logMessage, '\0', sizeof(logMessage));
      strcat(logMessage, "[ERROR] User ");
      strcat(logMessage, request.username);
      strcat(logMessage, " received SIGINT\n");
      writeToLog();
      
      if (room_num != -1) {
        if (rooms[room_num].user_count == 1) {

          users[user_num] -> room[0] = '\0';
          memset(rooms[room_num].name, 0, MAX_ROOM_NAME_LENGTH);
          rooms[room_num].user_count = 0;
          memset(rooms[room_num].users[0], 0, MAX_USERNAME_LENGTH);

          number_of_rooms -= 1;

          memset(logMessage, '\0', sizeof(logMessage));
          strcat(logMessage, "[LEAVE] ");
          strcat(logMessage, request.username);
          strcat(logMessage, " left the room ");
          strcat(logMessage, request.room_name);
          strcat(logMessage, "\n");
          writeToLog();

          memset( & response, '\0', sizeof(Response));
          strncpy(response.message, logMessage, strlen(logMessage));

          if (send(thread_socket, & response, sizeof(response), 0) <= 0) {
            write(1, "Problem with sending response to socket\n", strlen("Problem with sending response to socket\n"));
          }

          pthread_mutex_unlock( & rooms_mutex);
          continue;
        }

        users[user_num] -> room[0] = '\0';
        memset(rooms[room_num].name, 0, MAX_ROOM_NAME_LENGTH);
        rooms[room_num].user_count -= 1;
        memset(rooms[room_num].users[0], 0, MAX_USERNAME_LENGTH);

        for (i = room_num; i < number_of_rooms - 1; i++) {
          rooms[i] = rooms[i + 1];
        }

        number_of_rooms -= 1;
      }

      users[user_num] -> username[0] = '\0';
      users[user_num] -> ip[0] = '\0';

      for (i = user_num; i < number_of_users - 1; i++) {
        users[i] = users[i + 1];
      }

      number_of_users -= 1;
          
          pthread_mutex_unlock( & rooms_mutex);
      close(thread_socket);
      pthread_exit(NULL);
        }

    if (request.command == JOIN) {

      if (strlen(request.room_name) > MAX_ROOM_NAME_LENGTH) {

        memset(logMessage, '\0', sizeof(logMessage));
        strcat(logMessage, "[REJECTED] ");
        strcat(logMessage, request.username);
        strcat(logMessage, " room name ");
        strcat(logMessage, request.room_name);
        strcat(logMessage, " cannot be longer than the max length\n");
        writeToLog();

        memset( & response, '\0', sizeof(Response));
        strncpy(response.message, logMessage, strlen(logMessage));

        if (send(thread_socket, & response, sizeof(response), 0) <= 0) {
          write(1, "Problem with sending response to socket\n", strlen("Problem with sending response to socket\n"));
        }

        pthread_mutex_unlock( & rooms_mutex);
        continue;

      } else if (!is_alphanumeric(request.room_name)) {

        memset(logMessage, '\0', sizeof(logMessage));
        strcat(logMessage, "[REJECTED] ");
        strcat(logMessage, request.username);
        strcat(logMessage, " room  name ");
        strcat(logMessage, request.room_name);
        strcat(logMessage, " should be alphanumeric\n");
        writeToLog();

        memset( & response, '\0', sizeof(Response));
        strncpy(response.message, logMessage, strlen(logMessage));

        if (send(thread_socket, & response, sizeof(response), 0) <= 0) {
          write(1, "Problem with sending response to socket\n", strlen("Problem with sending response to socket\n"));
        }

        pthread_mutex_unlock( & rooms_mutex);
        continue;

      } else if (strlen(users[user_num] -> room) != 0) {

        memset(logMessage, '\0', sizeof(logMessage));
        strcat(logMessage, "[REJECTED] ");
        strcat(logMessage, users[user_num] -> username);
        strcat(logMessage, " is already in room ");
        strcat(logMessage, users[user_num] -> room);
        strcat(logMessage, "\n");
        writeToLog();

        memset( & response, '\0', sizeof(Response));
        strncpy(response.message, logMessage, strlen(logMessage));

        if (send(thread_socket, & response, sizeof(response), 0) <= 0) {
          write(1, "Problem with sending response to socket\n", strlen("Problem with sending response to socket\n"));
        }

        pthread_mutex_unlock( & rooms_mutex);
        continue;

      }

      if (rooms[room_num].user_count >= MAX_ROOM_CAPACITY) {

        memset(logMessage, '\0', sizeof(logMessage));
        strcat(logMessage, "[REJECTED] ");
        strcat(logMessage, request.username);
        strcat(logMessage, " room ");
        strcat(logMessage, request.room_name);
        strcat(logMessage, " is full\n");
        writeToLog();

        memset( & response, '\0', sizeof(Response));
        strncpy(response.message, logMessage, strlen(logMessage));

        if (send(thread_socket, & response, sizeof(response), 0) <= 0) {
          write(1, "Problem with sending response to socket\n", strlen("Problem with sending response to socket\n"));
        }

        pthread_mutex_unlock( & rooms_mutex);
        continue;
      }

      if (number_of_rooms >= MAX_NUMBER_OF_ROOMS && room_num == -1) {

        memset(logMessage, '\0', sizeof(logMessage));
        strcat(logMessage, "[REJECTED] ");
        strcat(logMessage, request.username);
        strcat(logMessage, "Max number of rooms is achieved\n");
        writeToLog();

        memset( & response, '\0', sizeof(Response));
        strncpy(response.message, logMessage, strlen(logMessage));

        if (send(thread_socket, & response, sizeof(response), 0) <= 0) {
          write(1, "Problem with sending response to socket\n", strlen("Problem with sending response to socket\n"));
        }

        pthread_mutex_unlock( & rooms_mutex);
        continue;
      }

      if (number_of_rooms < MAX_NUMBER_OF_ROOMS && room_num == -1) {

        Room * room = & rooms[number_of_rooms];
        memset(room -> name, 0, sizeof(room -> name));
        strncpy(room -> name, request.room_name, MAX_ROOM_NAME_LENGTH - 1);
        room -> name[strlen(request.room_name)] = '\0';
        room -> user_count = 0;
        memset(room -> users, 0, sizeof(room -> users));
        strncpy(room -> users[room -> user_count], request.username, MAX_USERNAME_LENGTH - 1);
        room -> users[room -> user_count][strlen(request.username)] = '\0';
        strncpy(users[user_num] -> room, request.room_name, MAX_ROOM_NAME_LENGTH - 1);
        users[user_num] -> room[strlen(request.room_name)] = '\0';
        room -> user_count += 1;
        number_of_rooms += 1;

        memset(logMessage, '\0', sizeof(logMessage));
        strcat(logMessage, "[JOIN] ");
        strcat(logMessage, request.username);
        strcat(logMessage, " joined the room ");
        strcat(logMessage, request.room_name);
        strcat(logMessage, "\n");
        writeToLog();

        memset( & response, '\0', sizeof(Response));
        strncpy(response.message, logMessage, strlen(logMessage));

        if (send(thread_socket, & response, sizeof(response), 0) <= 0) {
          write(1, "Problem with sending response to socket\n", strlen("Problem with sending response to socket\n"));
        }
        pthread_mutex_unlock( & rooms_mutex);
        continue;
      }

      strncpy(rooms[room_num].users[rooms[room_num].user_count], request.username, MAX_USERNAME_LENGTH - 1);
      rooms[room_num].user_count++;

      strncpy(users[user_num] -> room, request.room_name, MAX_ROOM_NAME_LENGTH - 1);

      memset(logMessage, '\0', sizeof(logMessage));
      strcat(logMessage, "[JOIN] ");
      strcat(logMessage, request.username);
      strcat(logMessage, " joined the room ");
      strcat(logMessage, request.room_name);
      strcat(logMessage, "\n");
      writeToLog();

      memset( & response, '\0', sizeof(Response));
      strncpy(response.message, logMessage, strlen(logMessage));

      if (send(thread_socket, & response, sizeof(response), 0) <= 0) {
        write(1, "Problem with sending response to socket\n", strlen("Problem with sending response to socket\n"));
      }
    } else if (request.command == LEAVE) {

      if (strcmp(users[user_num] -> room, "") == 0) {

        memset(logMessage, '\0', sizeof(logMessage));
        strcat(logMessage, "[REJECTED] ");
        strcat(logMessage, users[user_num] -> username);
        strcat(logMessage, " is not in any room\n");
        writeToLog();

        memset( & response, '\0', sizeof(Response));
        strncpy(response.message, logMessage, strlen(logMessage));

        if (send(thread_socket, & response, sizeof(response), 0) <= 0) {
          write(1, "Problem with sending response to socket\n", strlen("Problem with sending response to socket\n"));
        }

        pthread_mutex_unlock( & rooms_mutex);
        continue;
      }

      if (rooms[room_num].user_count == 1) {
        users[user_num] -> room[0] = '\0';
        memset(rooms[room_num].name, 0, MAX_ROOM_NAME_LENGTH);
        rooms[room_num].user_count = 0;
        memset(rooms[room_num].users[0], 0, MAX_USERNAME_LENGTH);

        number_of_rooms -= 1;

        memset(logMessage, '\0', sizeof(logMessage));
        strcat(logMessage, "[LEAVE] ");
        strcat(logMessage, request.username);
        strcat(logMessage, " left the room ");
        strcat(logMessage, request.room_name);
        strcat(logMessage, "\n");
        writeToLog();

        memset( & response, '\0', sizeof(Response));
        strncpy(response.message, logMessage, strlen(logMessage));

        if (send(thread_socket, & response, sizeof(response), 0) <= 0) {
          write(1, "Problem with sending response to socket\n", strlen("Problem with sending response to socket\n"));
        }

        pthread_mutex_unlock( & rooms_mutex);
        continue;
      }

      users[user_num] -> room[0] = '\0';
      memset(rooms[room_num].name, 0, MAX_ROOM_NAME_LENGTH);
      rooms[room_num].user_count -= 1;
      memset(rooms[room_num].users[0], 0, MAX_USERNAME_LENGTH);

      for (i = room_num; i < number_of_rooms - 1; i++) {
        rooms[i] = rooms[i + 1];
      }

      number_of_rooms -= 1;

      memset(logMessage, '\0', sizeof(logMessage));
      strcat(logMessage, "[LEAVE] ");
      strcat(logMessage, request.username);
      strcat(logMessage, " left the room ");
      strcat(logMessage, request.room_name);
      strcat(logMessage, "\n");
      writeToLog();

      memset( & response, '\0', sizeof(Response));
      strncpy(response.message, logMessage, strlen(logMessage));

      if (send(thread_socket, & response, sizeof(response), 0) <= 0) {
        write(1, "Problem with sending response to socket\n", strlen("Problem with sending response to socket\n"));
      }

    } else if (request.command == BROADCAST) {

      if (strlen(users[user_num] -> room) == 0) {

        memset(logMessage, '\0', sizeof(logMessage));
        strcat(logMessage, "[REJECTED] ");
        strcat(logMessage, users[user_num] -> username);
        strcat(logMessage, " Users cannot broadcast without joining a room first\n");
        writeToLog();

        memset( & response, '\0', sizeof(Response));
        strncpy(response.message, logMessage, strlen(logMessage));

        if (send(thread_socket, & response, sizeof(response), 0) <= 0) {
          write(1, "Problem with sending response to socket\n", strlen("Problem with sending response to socket\n"));
        }
        pthread_mutex_unlock( & rooms_mutex);
        continue;
      }

      if (rooms[room_num].user_count == 1) {

        memset(logMessage, '\0', sizeof(logMessage));
        strcat(logMessage, "[REJECT] ");
        strcat(logMessage, users[user_num] -> username);
        strcat(logMessage, " To broadcast, there must be other people in the room ");
        strcat(logMessage, rooms[room_num].name);
        strcat(logMessage, "\n");

        memset( & response, '\0', sizeof(Response));
        strncpy(response.message, logMessage, strlen(logMessage));

        writeToLog();

        if (send(thread_socket, & response, sizeof(response), 0) <= 0) {
          write(1, "Problem with sending response to socket\n", strlen("Problem with sending response to socket\n"));
        }
        pthread_mutex_unlock( & rooms_mutex);
        continue;
      }

      memset(logMessage, '\0', sizeof(logMessage));
      strcat(logMessage, "[BROADCAST] ");
      strcat(logMessage, users[user_num] -> username);
      strcat(logMessage, " broadcasted ");
      strcat(logMessage, request.message);
      strcat(logMessage, " to room ");
      strcat(logMessage, rooms[room_num].name);
      strcat(logMessage, "\n");

      memset( & response, '\0', sizeof(Response));
      strncpy(response.message, logMessage, strlen(logMessage));

      for (i = 0; i < rooms[room_num].user_count; i++) {
        if (strcmp(rooms -> users[i], request.username) != 0) {

          for (j = 0; j < number_of_users; j++) {
            if (strcmp(users[j] -> username, rooms -> users[i]) == 0) {
              to_user_num = j;

              break;

            }
          }

          if (to_user_num == -1) {

            memset(logMessage, '\0', sizeof(logMessage));
            strcat(logMessage, "[REJECTED] ");
            strcat(logMessage, users[user_num] -> username);
            strcat(logMessage, " A user disconnected before receiving broadcast message\n");
            writeToLog();

            memset( & response, '\0', sizeof(Response));
            strncpy(response.message, logMessage, strlen(logMessage));

            if (send(thread_socket, & response, sizeof(response), 0) <= 0) {
              write(1, "Problem with sending response to socket\n", strlen("Problem with sending response to socket\n"));

              pthread_mutex_unlock( & rooms_mutex);

              continue;
            }

          }

          if (send(users[to_user_num] -> socket_fd, & response, sizeof(response), 0) <= 0) {
            memset(logMessage, '\0', sizeof(logMessage));
            strcat(logMessage, "[REJECTED] ");
            strcat(logMessage, users[user_num] -> username);
            strcat(logMessage, " User ");
            strcat(logMessage, request.to_username);
            strcat(logMessage, " is disconnected\n");
            writeToLog();

            memset( & response, '\0', sizeof(Response));
            strncpy(response.message, logMessage, strlen(logMessage));

            if (send(users[user_num] -> socket_fd, & response, sizeof(response), 0) <= 0) {
              write(1, "Problem with sending response to socket\n", strlen("Problem with sending response to socket\n"));
            }
            pthread_mutex_unlock( & rooms_mutex);
            continue;
          }
        }
      }

      writeToLog();

      if (send(users[user_num] -> socket_fd, & response, sizeof(response), 0) <= 0) {
        write(1, "Problem with sending response to socket\n", strlen("Problem with sending response to socket\n"));
      }

    } else if (request.command == WHISPER) {

      if (strcmp(request.username, request.to_username) == 0) {

        memset(logMessage, '\0', sizeof(logMessage));
        strcat(logMessage, "[REJECTED] ");
        strcat(logMessage, users[user_num] -> username);
        strcat(logMessage, " Users cannot whisper to themselves\n");
        writeToLog();

        memset( & response, '\0', sizeof(Response));
        strncpy(response.message, logMessage, strlen(logMessage));

        if (send(thread_socket, & response, sizeof(response), 0) <= 0) {
          write(1, "Problem with sending response to socket\n", strlen("Problem with sending response to socket\n"));
        }

        pthread_mutex_unlock( & rooms_mutex);
        continue;

      }

      for (i = 0; i < number_of_users; i++) {
        if (strcmp(users[i] -> username, request.to_username) == 0) {
          to_user_num = i;

          break;

        }
      }

      if (to_user_num == -1) {

        memset(logMessage, '\0', sizeof(logMessage));
        strcat(logMessage, "[REJECTED] ");
        strcat(logMessage, users[user_num] -> username);
        strcat(logMessage, " User ");
        strcat(logMessage, request.to_username);
        strcat(logMessage, " does not exist\n");
        writeToLog();

        memset( & response, '\0', sizeof(Response));
        strncpy(response.message, logMessage, strlen(logMessage));

        if (send(thread_socket, & response, sizeof(response), 0) <= 0) {
          write(1, "Problem with sending response to socket\n", strlen("Problem with sending response to socket\n"));
        }

        pthread_mutex_unlock( & rooms_mutex);
        continue;

      }

      memset(logMessage, '\0', sizeof(logMessage));
      strcat(logMessage, "[WHISPER] ");
      strcat(logMessage, users[user_num] -> username);
      strcat(logMessage, " sent ");
      strcat(logMessage, request.message);
      strcat(logMessage, " to  ");
      strcat(logMessage, request.to_username);
      strcat(logMessage, "\n");
      writeToLog();

      memset( & response, '\0', sizeof(Response));
      strncpy(response.message, logMessage, strlen(logMessage));

      if (send(users[to_user_num] -> socket_fd, & response, sizeof(response), 0) <= 0) {
        memset(logMessage, '\0', sizeof(logMessage));
        strcat(logMessage, "[REJECTED] ");
        strcat(logMessage, users[user_num] -> username);
        strcat(logMessage, " User ");
        strcat(logMessage, request.to_username);
        strcat(logMessage, " is disconnected\n");
        writeToLog();

        memset( & response, '\0', sizeof(Response));
        strncpy(response.message, logMessage, strlen(logMessage));

        if (send(users[user_num] -> socket_fd, & response, sizeof(response), 0) <= 0) {
          write(1, "Problem with sending response to socket\n", strlen("Problem with sending response to socket\n"));
        }

        pthread_mutex_unlock( & rooms_mutex);
        continue;
      }

      memset( & response, '\0', sizeof(Response));
      strncpy(response.message, logMessage, strlen(logMessage));
      if (send(users[user_num] -> socket_fd, & response, sizeof(response), 0) <= 0) {
        write(1, "Problem with sending response to socket\n", strlen("Problem with sending response to socket\n"));
      }

    } else if (request.command == SENDFILE) {
    
      if (strcmp(request.username, request.to_username) == 0) {

        memset(logMessage, '\0', sizeof(logMessage));
        strcat(logMessage, "[REJECTED] ");
        strcat(logMessage, users[user_num] -> username);
        strcat(logMessage, " Users cannot send files to themselves\n");
        writeToLog();

        memset( & response, '\0', sizeof(Response));
        strncpy(response.message, logMessage, strlen(logMessage));

        if (send(thread_socket, & response, sizeof(response), 0) <= 0) {
          write(1, "Problem with sending response to socket\n", strlen("Problem with sending response to socket\n"));
        }

        pthread_mutex_unlock( & rooms_mutex);
        continue;

      }
      
      memset(file_name, '\0', sizeof(file_name));
      strcat(file_name, ARCHIVE);
      strcat(file_name, request.username);
      strcat(file_name, "/");
      strcat(file_name, request.filename);
      
      if (access(file_name, F_OK) != 0) {
        memset(logMessage, '\0', sizeof(logMessage));
        strcat(logMessage, "[REJECTED] ");
        strcat(logMessage, users[user_num] -> username);
        strcat(logMessage, " File should exist in your archive folder to be able to send it\n ");
        writeToLog();

        memset( & response, '\0', sizeof(Response));
        strncpy(response.message, logMessage, strlen(logMessage));

        if (send(users[user_num] -> socket_fd, & response, sizeof(response), 0) <= 0) {
          write(1, "Problem with sending response to socket\n", strlen("Problem with sending response to socket\n"));
        }
        pthread_mutex_unlock( & rooms_mutex);
        continue;
    }
    
    for(i=0;i<strlen(file_name);i++)
    {
        if(file_name[i]=='.')
        {
            break;
        }
    }

    if(i==strlen(file_name))
    {
        memset(logMessage, '\0', sizeof(logMessage));
        strcat(logMessage, "[REJECTED] ");
        strcat(logMessage, users[user_num] -> username);
        strcat(logMessage, " Filename is invalid\n ");
        writeToLog();

        memset( & response, '\0', sizeof(Response));
        strncpy(response.message, logMessage, strlen(logMessage));

        if (send(users[user_num] -> socket_fd, & response, sizeof(response), 0) <= 0) {
          write(1, "Problem with sending response to socket\n", strlen("Problem with sending response to socket\n"));
        }
        pthread_mutex_unlock( & rooms_mutex);
        continue;
    }
    
    j=0;
    memset( & extension, '\0', sizeof(extension));

    while(j<strlen(file_name)-i)
    {
    extension[j]=file_name[i+j+1];
    j++;
    }
    extension[j] ='\0';
    
    j=0;
    
    for (i = 0; i < sizeof(allowed_file_types) / sizeof(allowed_file_types[0]); i++) {
        if (strcmp(allowed_file_types[i], extension) == 0) {
          j=1;
          break;

        }
      }
      
      if(j == 0){
        memset(logMessage, '\0', sizeof(logMessage));
        strcat(logMessage, "[REJECTED] ");
        strcat(logMessage, users[user_num] -> username);
        strcat(logMessage, " File type is not allowed\n ");
        writeToLog();

        memset( & response, '\0', sizeof(Response));
        strncpy(response.message, logMessage, strlen(logMessage));

        if (send(users[user_num] -> socket_fd, & response, sizeof(response), 0) <= 0) {
          write(1, "Problem with sending response to socket\n", strlen("Problem with sending response to socket\n"));
        }
        
        pthread_mutex_unlock( & rooms_mutex);
        
        continue;
      }
      
      for (i = 0; i < number_of_users; i++) {
        if (strcmp(users[i] -> username, request.to_username) == 0) {
          to_user_num = i;

          break;
        }
      }

      if (to_user_num == -1) {

        memset(logMessage, '\0', sizeof(logMessage));
        strcat(logMessage, "[REJECTED] ");
        strcat(logMessage, users[user_num] -> username);
        strcat(logMessage, " User ");
        strcat(logMessage, request.to_username);
        strcat(logMessage, " does not exist\n");
        writeToLog();

        memset( & response, '\0', sizeof(Response));
        strncpy(response.message, logMessage, strlen(logMessage));

        if (send(thread_socket, & response, sizeof(response), 0) <= 0) {
          write(1, "Problem with sending response to socket\n", strlen("Problem with sending response to socket\n"));
        }

        pthread_mutex_unlock( & rooms_mutex);
        continue;

      }
          
      if (stat(file_name, &st) == 0) {
        file_size = st.st_size / (1024 * 1024);
    }else{
        write(1, "Problem with getting the size of file\n", strlen("Problem with getting the size of file\n"));
        pthread_mutex_unlock( & rooms_mutex);
        continue;
    }
    
    if(file_size >= MAX_FILE_SIZE_MB){
      memset(logMessage, '\0', sizeof(logMessage));
        strcat(logMessage, "[REJECTED] ");
        strcat(logMessage, users[user_num] -> username);
        strcat(logMessage, " File size cannot be larger than ");
        memset(temp, '\0', sizeof(temp));
        itoa(MAX_FILE_SIZE_MB, temp);
        strcat(logMessage, temp);
        strcat(logMessage, " MB\n ");

        writeToLog();

        memset( & response, '\0', sizeof(Response));
        strncpy(response.message, logMessage, strlen(logMessage));

        if (send(users[user_num] -> socket_fd, & response, sizeof(response), 0) <= 0) {
          write(1, "Problem with sending response to socket\n", strlen("Problem with sending response to socket\n"));
        }
        pthread_mutex_unlock( & rooms_mutex);
        continue;
    }

      pthread_mutex_lock( & queue_mutex);

      if (size(upload_queue) == MAX_UPLOAD_QUEUE) {
        push(waiting_queue, request);

        memset(logMessage, '\0', sizeof(logMessage));
        strcat(logMessage, "[SENDFILE] ");
        strcat(logMessage, users[user_num] -> username);
        strcat(logMessage, " initiated file transfer ");
        strcat(logMessage, request.filename);
        strcat(logMessage, " to ");
        strcat(logMessage, request.to_username);
        strcat(logMessage, ". Upload queue is full. Waiting queue size: ");
        memset(temp, '\0', sizeof(temp));
        itoa(size(waiting_queue), temp);
        strcat(logMessage, temp);
        strcat(logMessage, ". Estimated wait time: ");
        memset(temp, '\0', sizeof(temp));
        itoa((size(upload_queue) + size(waiting_queue)) * FILE_SEND_TIME, temp);
        strcat(logMessage, temp);
        strcat(logMessage, "seconds.\n");

        writeToLog();

        memset( & response, '\0', sizeof(Response));
        strncpy(response.message, logMessage, strlen(logMessage));

        if (send(users[user_num] -> socket_fd, & response, sizeof(response), 0) <= 0) {
          write(1, "Problem with sending response to socket\n", strlen("Problem with sending response to socket\n"));
        }

      } else {
        push(upload_queue, request);

        memset(logMessage, '\0', sizeof(logMessage));
        strcat(logMessage, "[SENDFILE] ");
        strcat(logMessage, users[user_num] -> username);
        strcat(logMessage, " initiated file transfer ");
        strcat(logMessage, request.filename);
        strcat(logMessage, " to ");
        strcat(logMessage, request.to_username);
        strcat(logMessage, ". Upload queue size: ");
        memset(temp, '\0', sizeof(temp));
        itoa(size(upload_queue), temp);
        strcat(logMessage, temp);
        strcat(logMessage, "\n");

        writeToLog();

        memset( & response, '\0', sizeof(Response));
        strncpy(response.message, logMessage, strlen(logMessage));

        if (send(users[user_num] -> socket_fd, & response, sizeof(response), 0) <= 0) {
          write(1, "Problem with sending response to socket\n", strlen("Problem with sending response to socket\n"));
        }
      }

      pthread_mutex_unlock( & queue_mutex);

    } else if (request.command == EXIT) {

      if (room_num != -1) {
        if (rooms[room_num].user_count == 1) {

          users[user_num] -> room[0] = '\0';
          memset(rooms[room_num].name, 0, MAX_ROOM_NAME_LENGTH);
          rooms[room_num].user_count = 0;
          memset(rooms[room_num].users[0], 0, MAX_USERNAME_LENGTH);

          number_of_rooms -= 1;

          memset(logMessage, '\0', sizeof(logMessage));
          strcat(logMessage, "[LEAVE] ");
          strcat(logMessage, request.username);
          strcat(logMessage, " left the room ");
          strcat(logMessage, request.room_name);
          strcat(logMessage, "\n");
          writeToLog();

          memset( & response, '\0', sizeof(Response));
          strncpy(response.message, logMessage, strlen(logMessage));

          if (send(thread_socket, & response, sizeof(response), 0) <= 0) {
            write(1, "Problem with sending response to socket\n", strlen("Problem with sending response to socket\n"));
          }

          pthread_mutex_unlock( & rooms_mutex);
          continue;
        }

        users[user_num] -> room[0] = '\0';
        memset(rooms[room_num].name, 0, MAX_ROOM_NAME_LENGTH);
        rooms[room_num].user_count -= 1;
        memset(rooms[room_num].users[0], 0, MAX_USERNAME_LENGTH);

        for (i = room_num; i < number_of_rooms - 1; i++) {
          rooms[i] = rooms[i + 1];
        }

        number_of_rooms -= 1;
      }

      users[user_num] -> username[0] = '\0';
      users[user_num] -> ip[0] = '\0';

      for (i = user_num; i < number_of_users - 1; i++) {
        users[i] = users[i + 1];
      }

      number_of_users -= 1;

      memset(logMessage, '\0', sizeof(logMessage));
      strcat(logMessage, "[EXIT] ");
      strcat(logMessage, request.username);
      strcat(logMessage, " disconnected. Goodbye!\n");
      writeToLog();

      memset( & response, '\0', sizeof(Response));
      strncpy(response.message, logMessage, strlen(logMessage));

      if (send(thread_socket, & response, sizeof(response), 0) <= 0) {
        write(1, "Problem with sending response to socket\n", strlen("Problem with sending response to socket\n"));
      }

      pthread_mutex_unlock( & rooms_mutex);
      close(thread_socket);
      pthread_exit(NULL);

    } else {
      write(1, "Something wrong with the user sending a command\n", strlen("Something wrong with the user sending a command\n"));

      continue;

    }

    memset( & request, '\0', sizeof(Request));
    pthread_mutex_unlock( & rooms_mutex);
  }

  close(thread_socket);
  pthread_exit(NULL);
}

int main(int argc, char * argv[]) {

  if (argc != 2 || !is_integer(argv[1])) {
    write(1, "Usage: ./chatserver <port>\n", strlen("Usage: ./chatserver <port>\n"));
    return 1;
  }

  char * port_number = argv[1];

  struct sigaction sa;
  memset( & sa, 0, sizeof(sa));
  sa.sa_handler = handler;
  sigaction(SIGINT, & sa, NULL);
  
  if (sem_init( & log_sem, 0, 1) != 0) {
                write(1, "Semaphore initialization failed\n", strlen("Semaphore initialization failed\n"));
                return 1;
        }

  struct sockaddr_in address;
  int opt = 1;
  socklen_t addrlen = sizeof(address);

  int port = atoi(port_number);

  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    write(1, "socket failed", strlen("socket failed"));
    
    sem_destroy( & log_sem);
    
    exit(EXIT_FAILURE);
  }

  if (setsockopt(server_fd, SOL_SOCKET,
      SO_REUSEADDR | SO_REUSEPORT, & opt,
      sizeof(opt))) {
    write(1, "setsockopt failed", strlen("setsockopt failed"));
    
    sem_destroy( & log_sem);
  close(server_fd);
  
  exit(EXIT_FAILURE);
  }

  memset(logMessage, '\0', sizeof(logMessage));
  strcat(logMessage, "[INFO] Server listening on port ");
  strcat(logMessage, port_number);
  strcat(logMessage, "...\n");
  writeToLog();

  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(port);

  if (bind(server_fd, (struct sockaddr * ) & address,
      sizeof(address)) <
    0) {
    write(1, "bind failed", strlen("bind failed"));
    
    sem_destroy( & log_sem);
  close(server_fd);
  
   exit(EXIT_FAILURE);
  }
  if (listen(server_fd, 3) < 0) {
    write(1, "listen error", strlen("listen error"));
    
    sem_destroy( & log_sem);
  close(server_fd);
  
  exit(EXIT_FAILURE);
  }

  if (sem_init( & log_sem, 0, 1) != 0) {
    write(1, "Log file semaphore initialization failed", strlen("Log file semaphore initialization failed"));
    
    sem_destroy( & log_sem);
  close(server_fd);
    return 1;
  }

  Request request;
  Response response;
  bool error_occurred = false;
  char folder_name[MAX_INPUT_LENGTH];

  upload_queue = create();
  waiting_queue = create();

  pthread_t sendfile_thread;
  if (pthread_create( & sendfile_thread, NULL, send_file, NULL) != 0) {
    write(1, "pthread create error\n", strlen("pthread create error\n"));
    
    sem_destroy( & log_sem);
  close(server_fd);
  
  destroy(upload_queue);
  destroy(waiting_queue);
  
  return 1;
  }

  pthread_detach(sendfile_thread);

  while (1) {

    memset(folder_name, '\0', sizeof(folder_name));
    strcat(folder_name, ARCHIVE);

    DIR * directory = opendir(folder_name);
    if (directory) {
      closedir(directory);
    } else {
      if (mkdir(folder_name, 0777) == -1) {
        write(1, "Error creating directory", strlen("Error creating directory"));
        
    sem_destroy( & log_sem);
  close(server_fd);
  
  destroy(upload_queue);
  destroy(waiting_queue);
        
        exit(EXIT_FAILURE);
      }
    }

    if ((server_socket = accept(server_fd, (struct sockaddr * ) & address, & addrlen)) < 0) {
      write(1, "accept error", strlen("accept error"));
    
    sem_destroy( & log_sem);
  close(server_fd);
  
  return 1;
    }

    error_occurred = false;

    memset( & request, '\0', sizeof(Request));

    if (read(server_socket, & request, sizeof(Request)) <= 0) {
      write(1, "Problem with reading from socket\n", strlen("Problem with reading from socket\n"));
      continue;
    }
    if (request.command == CONNECT) {
      if (strlen(request.username) > MAX_USERNAME_LENGTH) {
        memset(logMessage, '\0', sizeof(logMessage));
        strcat(logMessage, "[REJECTED] Username cannot be longer than ");
        memset(temp, '\0', sizeof(temp));
        itoa(MAX_USERNAME_LENGTH, temp);
        strcat(logMessage, temp);
        strcat(logMessage, " characters: ");
        strcat(logMessage, request.username);
        strcat(logMessage, "\n");
        writeToLog();

        memset( & response, '\0', sizeof(Response));

        strncpy(response.message, logMessage, strlen(logMessage));

        if (send(server_socket, & response, sizeof(response), 0) <= 0) {
          write(1, "Problem with sending response to socket\n", strlen("Problem with sending response to socket\n"));
    
    sem_destroy( & log_sem);
    close(server_socket);
  close(server_fd);
  
  destroy(upload_queue);
  destroy(waiting_queue);
        }

        error_occurred = true;

        continue;

      }
      if (!is_alphanumeric(request.username)) {
        memset(logMessage, '\0', sizeof(logMessage));
        strcat(logMessage, "[REJECTED] Username must be alphanumeric: ");
        strcat(logMessage, request.username);
        strcat(logMessage, "\n");
        writeToLog();

        memset( & response, '\0', sizeof(Response));

        strncpy(response.message, logMessage, strlen(logMessage));

        if (send(server_socket, & response, sizeof(response), 0) <= 0) {
          write(1, "Problem with sending response to socket\n", strlen("Problem with sending response to socket\n"));
        }

        error_occurred = true;
        continue;

      }

      for (int i = 0; i < number_of_users; i++) {
        if (strcmp(users[i] -> username, request.username) == 0) {
          memset(logMessage, '\0', sizeof(logMessage));
          strcat(logMessage, "[REJECTED] Duplicate username attempted: ");
          strcat(logMessage, request.username);
          strcat(logMessage, "\n");
          writeToLog();

          memset( & response, '\0', sizeof(Response));

          strncpy(response.message, logMessage, strlen(logMessage));

          if (send(server_socket, & response, sizeof(response), 0) <= 0) {
            write(1, "Problem with sending response to socket\n", strlen("Problem with sending response to socket\n"));
          }

          error_occurred = true;
          break;

        }
      }

      if (error_occurred) {
        continue;
      }

      users[number_of_users] = malloc(sizeof(User));

      strncpy(users[number_of_users] -> username, request.username, strlen(request.username));

      users[number_of_users] -> username[strlen(request.username)] = '\0';

      users[number_of_users] -> room[0] = '\0';

      users[number_of_users] -> socket_fd = server_socket;

      strncpy(users[number_of_users] -> ip, request.ip, strlen(request.ip));

      number_of_users += 1;

      memset(logMessage, '\0', sizeof(logMessage));
      strcat(logMessage, "[CONNECT] New client connected: ");
      strcat(logMessage, request.username);
      strcat(logMessage, " from ");
      strcat(logMessage, request.ip);
      strcat(logMessage, "\n");
      writeToLog();

      memset( & response, '\0', sizeof(Response));

      strncpy(response.message, logMessage, strlen(logMessage));

      if (send(server_socket, & response, sizeof(response), 0) <= 0) {
        write(1, "Problem with sending response to socket\n", strlen("Problem with sending response to socket\n"));
      }

      memset(folder_name, '\0', sizeof(folder_name));
      strcat(folder_name, ARCHIVE);
      strcat(folder_name, request.username);
      strcat(folder_name, "/");

      DIR * directory = opendir(folder_name);
      if (directory) {
        closedir(directory);
      } else {
        if (mkdir(folder_name, 0777) == -1) {
          write(1, "Error creating directory", strlen("Error creating directory"));
          
          for(int i=0; i<number_of_users; i++){
    free(users[i]);
  }
    
    sem_destroy( & log_sem);
    close(server_socket);
  close(server_fd);
  
  destroy(upload_queue);
  destroy(waiting_queue);
  
          exit(EXIT_FAILURE);
        }
      }

      pthread_t thread_id;
      if (pthread_create( & thread_id, NULL, thread, & server_socket) != 0) {
        write(1, "pthread create error\n", strlen("pthread create error\n"));
        continue;
      }

      pthread_detach(thread_id);

    } else {
      write(1, "Something wrong with a user connecting to the server\n", strlen("Something wrong with a user connecting to the server\n"));

      continue;

    }
  }
  
  for(int i=0; i<number_of_users; i++){
    free(users[i]);
  }

  sem_destroy( & log_sem);
  
  destroy(upload_queue);
  destroy(waiting_queue);
  
  close(server_socket);
  close(server_fd);
  return 0;
}
