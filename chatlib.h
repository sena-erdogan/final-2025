#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/types.h>
#include <signal.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <complex.h>
#include <math.h>
#include <semaphore.h>
#include <time.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/file.h>
#include <errno.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/mman.h>
#include <semaphore.h>

#define READ_FLAGS O_RDONLY
#define WRITE_FLAGS (O_WRONLY | O_CREAT | O_APPEND)
#define PERMS (S_IRUSR | S_IWUSR | S_IWGRP)

#define ARCHIVE "archive/"

#define RED "\033[31m"
#define GREEN "\033[32m"
#define BLUE "\033[34m"

#define MAX_CLIENTS 300
#define FILE_SEND_TIME 1
#define MAX_NUMBER_OF_ROOMS 300
#define MAX_INPUT_LENGTH 100
#define MAX_MESSAGE_LENGTH 2048

#define MAX_USERNAME_LENGTH 16
#define MAX_FILE_SIZE_MB 3
#define MAX_ROOM_CAPACITY 15
#define MAX_ROOM_NAME_LENGTH 32
#define MAX_UPLOAD_QUEUE 5

const char * allowed_file_types[] = {
  "txt",
  "pdf",
  "jpg",
  "png"
};

typedef enum {
  CONNECT,
  JOIN,
  LEAVE,
  BROADCAST,
  WHISPER,
  SENDFILE,
  EXIT
}
Command;

typedef struct {
  Command command;
  char username[MAX_INPUT_LENGTH];
  char ip[MAX_INPUT_LENGTH];
  char room_name[MAX_INPUT_LENGTH];
  char message[MAX_MESSAGE_LENGTH];
  char to_username[MAX_INPUT_LENGTH];
  char filename[MAX_INPUT_LENGTH];
}
Request;

typedef struct {
  char message[MAX_MESSAGE_LENGTH];
}
Response;

typedef struct {
  char username[MAX_USERNAME_LENGTH];
  int socket_fd;
  char ip[MAX_INPUT_LENGTH];
  char room[MAX_ROOM_NAME_LENGTH];
}
User;

typedef struct {
  char name[MAX_ROOM_NAME_LENGTH];
  char users[MAX_ROOM_CAPACITY][MAX_USERNAME_LENGTH];
  int user_count;
}
Room;

typedef struct {
  char name[MAX_INPUT_LENGTH];
  int size;
}
ChatFile;

typedef struct Node {
  Request request;
  struct Node * next;
}
Node;

typedef struct {
  Node * front;
  Node * rear;
  int size;
}
Queue;

Queue * create() {
  Queue * queue = malloc(sizeof(Queue));
  queue -> front = NULL;
  queue -> rear = NULL;
  queue -> size = 0;
  return queue;
}

void destroy(Queue * queue) {
  Node * current = queue -> front;
  while (current != NULL) {
    Node * temp = current;
    current = current -> next;
    free(temp);
  }
  free(queue);
}

int is_empty(Queue * queue) {
  return queue -> size == 0;
}

void push(Queue * queue, Request request) {
  Node * new_node = malloc(sizeof(Node));
  new_node -> request = request;
  new_node -> next = NULL;

  if (is_empty(queue)) {
    queue -> front = new_node;
    queue -> rear = new_node;
  } else {
    queue -> rear -> next = new_node;
    queue -> rear = new_node;
  }

  queue -> size++;
}

Request pop(Queue * queue) {
  if (is_empty(queue)) {
    write(1, "Error: Queue is empty\n", strlen("Error: Queue is empty\n"));
    Request empty;
    strcpy(empty.username, "");
    return empty;
  } else {
    Request req = queue -> front -> request;
    Node * temp = queue -> front;
    queue -> front = queue -> front -> next;
    free(temp);
    queue -> size--;
    return req;
  }
}

int size(Queue * queue) {
  return queue -> size;
}

void itoa(int num, char * str) {
  int i = 0, sign = num;

  if (num == 0) {
    str[i++] = '0';
    str[i] = '\0';
    return;
  }

  if (num < 0) num = -num;

  while (num) {
    str[i++] = (num % 10) + '0';
    num /= 10;
  }

  if (sign < 0) str[i++] = '-';

  str[i] = '\0';

  for (int j = 0, k = i - 1; j < k; j++, k--) {
    char temp = str[j];
    str[j] = str[k];
    str[k] = temp;
  }
}

char * get_timestamp() {
  time_t now = time(NULL);
  return asctime(localtime( & now));
}

int is_integer(char * str) {
  if (strlen(str) < 1) {
    return 0;
  }

  for (int i = 0; i < strlen(str); i++) {
    if (str[i] < '0' || str[i] > '9') {
      return 0;
    }
  }
  return 1;
}

bool is_alphanumeric(char * str) {
  if (str == NULL) return false;

  for (int i = 0; str[i] != '\0'; i++) {
    char c = str[i];

    if (!((c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9'))) {
      return false;
    }
  }

  return true;
}
