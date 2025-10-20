#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <arm_acle.h>
#include <sys/mman.h>

#define GRANULE 0x10

#define MAX_NAME_SIZE 0x18
#define MAX_PASSWORD_SIZE 0x10
#define MAX_SESSION 0x4

struct __attribute__((aligned(GRANULE))) Session {
  int admin; // 1: admin, 0: user
  char data[0x18];
};

struct __attribute__((aligned(GRANULE))) User {
  char name[MAX_NAME_SIZE];
  struct Session *session;
};

void get_input(char *buf, int size) {
  char c;
  int i = 0;
  while (i <= size) {
    ssize_t r = read(STDIN_FILENO, &c, 1);
    if (r <= 0 || c == '\n') {
      break;
    }
    buf[i++] = c;
  }
}

void init_admin(struct Session *session) {
  session->admin = 1;

  const char low = '!';
  const char high = '~';
  for (int i = 0; i < MAX_PASSWORD_SIZE; i++) {
    session->data[i] = (char)(rand() % (high - low + 1) + low);
  }
  session->data[MAX_PASSWORD_SIZE] = '\0';
}

int main(void) {
  setbuf(stdout, NULL);
  srand(time(NULL));

  size_t session_size = sizeof(struct Session) * MAX_SESSION;
  struct Session *sessions =
      mmap(NULL, session_size, PROT_READ | PROT_WRITE | PROT_MTE,
           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (sessions == MAP_FAILED) {
    puts("Failed to initialize sessions.");
    exit(EXIT_FAILURE);
  }
  sessions = __arm_mte_create_random_tag(sessions, 1); // drop tag 0
  for (size_t off = 0; off < session_size; off += GRANULE) {
    __arm_mte_set_tag((uint8_t *)sessions + off);
  }
  int occupied[MAX_SESSION] = {0};

  occupied[0] = 1;
  init_admin(&sessions[0]);

  char buf[0x20];
  while (1) {
    puts("=== MTE Computer ===");

    struct User *user = malloc(sizeof(*user));
    puts("Login:");

    get_input(user->name, MAX_NAME_SIZE);
    if (strcmp(user->name, "admin") == 0) {
      puts("Password:");

      get_input(buf, MAX_PASSWORD_SIZE);
      if (strcmp(buf, sessions[0].data)) {
        puts("Wrong username or password.");
        continue;
      }

      puts("Welcom Admin!");
      user->session = &sessions[0];

    } else {
      int idx = -1;
      for (int i = 1; i < MAX_SESSION; i++) {
        if (!occupied[i]) {
          occupied[i] = 1;
          idx = i;
          break;
        }
      }

      if (idx == -1) {
        puts("Temporary user has reached max limit!");
        continue;
      }

      user->session = &sessions[idx];
    }

    int logged_out = 0;
    while (!logged_out) {
      puts("\nMenu:");
      if (user->session->admin) {
        puts("  0) Admin Console");
      }
      puts("  1) Change username");
      puts("  9) Logout");
      puts("Your choice:");

      get_input(buf, 0x10);
      int opt = atoi(buf);
      switch (opt) {
      case 0: {
        if (!user->session->admin) {
          puts("You are not admin!");
          break;
        }

        puts("Welcome Admin!");
        system("sh");
        break;
      }
      case 1: {
        puts("New username:");
        get_input(user->name, MAX_NAME_SIZE);
        printf("Successfully modified username: %s\n", user->name);
        break;
      }
      case 9: {
        logged_out = 1;
        puts("Logged out!");
        break;
      }
      default:
        break;
      }
    }
  }

  return 0;
}
