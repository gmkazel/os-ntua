#include <stdio.h>
#include <unistd.h>

void zing(void) {
  char* username;
  username = getlogin();

  if (username == NULL) {
    printf("Error getting username\n");
  } else {
    printf("Welcome, %s!\n", username);
  }

  return;
}
