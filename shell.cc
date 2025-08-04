extern int yydebug;


#include "shell.hh"
#include <cstdio>
#include <unistd.h>
#include "shell.hh"
#include <signal.h>
#include <sys/wait.h>
#include<stdlib.h>






int yyparse(void);
void yyrestart(FILE *file);

int yylex_destroy(void);

extern "C" void reset_buffer(void);



// Function to handle ctr + c interrupt
void ctrl_c(int signal) {
  Shell::_currentCommand.clear();
  reset_buffer();
  printf("\n"); // Print a new line
  Shell::prompt();
}

void Shell::prompt() {
  if (isatty(0)) {
    printf("myshell>");
  }
  fflush(stdout);
}


void zombie(int signal) {
  int pid;
  while ((pid = waitpid(-1, NULL, WNOHANG)) > 0) {};
}



int main(int, char **argv) {

  // Set up Ctrl handler
  struct sigaction sa;
  sa.sa_handler = ctrl_c; // When SIGINT is recieved ctrl_c() will be called 
  sigemptyset(&sa.sa_mask); // makes sure no additional siganls are blocked with the handleer
  sa.sa_flags = SA_RESTART; // Makes system calls restard instead of failing 

  // Set up SIGINT handler
  // the struct sigaction sa will only handle SIGINT signals and none else
  if (sigaction(SIGINT, &sa, NULL)) {
    perror("sigaction");
    exit(2);
  }

  // Set up Zombie handler
  struct sigaction Zombie;
  Zombie.sa_handler = zombie; // When SIGCHILD is received zombie() is called
  sigemptyset(&Zombie.sa_mask); // Same as abovve
  Zombie.sa_flags = SA_RESTART; // Syscalls restart instead of failing

  // Set up handler to handle SIGCHILD signal
  if (sigaction(SIGCHLD, &Zombie, NULL)) {
    perror("sigaction");
    exit(2);
  }




  // Variable expanions: ${SHELL} --> prints path of shell executable
  // Rest of this handled in shell.l
  char *path = argv[0]; // gets the first arg (program itself)
  char *gustavo = realpath(path, NULL);
  setenv("SHELL", gustavo, 1);
  free(gustavo);


  /* Extra credit 2.7 */
  // Attempt to open the .shellsrc file
  FILE *file = fopen(".shellrc", "r");
  if (file) {
    yyrestart(file); // tell lex to read from 'file' and not stdin
    yyparse(); // parse the file
    yyrestart(stdin); // Go back to reading from terminal
    fclose(file);
    file = NULL;
  }
  else {
    Shell::prompt(); // prompt user, 'file' no existo
  }


  //yydebug = 1;


  yyparse();

  yylex_destroy();
}

Command Shell::_currentCommand;

std::string Shell::last_arg = "";
