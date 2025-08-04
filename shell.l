%{

#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include "y.tab.hh"
#include <sys/wait.h>
#include "shell.hh"

#include <string.h>

//////////// Start added code ////////////

extern "C" char * read_line();

int mygetc(FILE * f) {
  static char *p;
  char ch;

     if (!isatty(0)) {
  // stdin is not a tty. Call real getc
    return getc(f);
  }

  // stdin is a tty. Call our read_line.
  if (p==NULL || *p == 0) {
    char * s = read_line();
    p = s;
  }

  ch = *p;
  p++;

  return ch;
}

#undef getc
#define getc(f) mygetc(f)

//////////// End addec code //////////////


static void yyunput (int c,char *buf_ptr  );


extern int code;
extern int last_pid;



void myunputc(int c) {
  unput(c);
}


// Source builtIn function
void source(const char *file) {

  // Open the file passed
  FILE *fp = fopen(file, "r");

  // Create a buffer to hold contentes of the file
  // YY_BUF_SIZE defines size of buffer
  YY_BUFFER_STATE buff = yy_create_buffer(fp, YY_BUF_SIZE);

  // Pushed new buffer state onto lexer's buffer stack
  yypush_buffer_state(buff);

  // Start to parse the file
  yyparse();

  // Pop the buffer state from the buffer stack
  yypop_buffer_state();
}



%}

%option noyywrap

%%

\n {
  return NEWLINE;
}

[ \t] {
  /* Discard spaces and tabs */
}

">" {
  return GREAT;
}


"<" {
  return LESS;
}

"2>" {
  return TWOGREAT;
}

"|" {
  return PIPE;
}

">>" {
  return GREATGREAT;
}

"&" {
  return AMPERSAND;
}

">&" {
  return GREATAMPERSAND;
}

">>&" {
  return GREATGREATAMPERSAND;
}




"<("[^)]*")" {

  // Process substitution pattern: <(command)
  // EXTRA CREDIT

  std::string command = yytext;

  // Remove <( and )
  command = command.substr(2, command.length() - 3);

  // Create temporary directory
  char temp_dir[] = "/tmp/shell-XXXXXX";
  char* dir_name = mkdtemp(temp_dir);
  if (!dir_name) {
    perror("mkdtemp failed");
    return WORD;
  }

  // Create named pipe in temp directory
  std::string fifo_path = std::string(dir_name) + "/fifo";
  if (mkfifo(fifo_path.c_str(), 0666) < 0) {
    perror("mkfifo failed");
    rmdir(dir_name);
    return WORD;
  }

  // Fork to run the command
  pid_t pid = fork();
  if (pid == 0) {

      // Child process
      int fd = open(fifo_path.c_str(), O_WRONLY);
      if (fd < 0) {
          perror("open fifo");
          _exit(1);
      }

      // Redirect stdout to the named pipe
      dup2(fd, 1);
      close(fd);

      // Execute the command
      execl("/bin/sh", "sh", "-c", command.c_str(), NULL);
      perror("execl failed");
      _exit(1);
  }

  // Parent process continues
  // Return the fifo path as a WORD token
  yylval.cpp_string = new std::string(fifo_path);

  // Store cleanup information 
  Command::addCleanupPath(fifo_path);
  Command::addCleanupPath(dir_name);

  return WORD;
}



~[^ \n\t]* {
  //TILDE

  // Get the text parsed
  std::string text = std::string(yytext);

  // Check if only the ~ is given
  // If so expand to home directory with getenv("HOME")
  if (text.size() == 1) {
    yylval.cpp_string = new std::string(getenv("HOME"));
  }
  else { // Word after ~

    // If additional words after '~' it is a username i think
    // make homedirectory path in format "/homes/username"
    text = std::string("/homes/") + text.substr(1);

    // Store expaned path
    yylval.cpp_string = new std::string(text);
  }
  return WORD;
}





\$\{[^\t\n ]*\}([^ \t\n])* {

  // ENV. VAR. EXPANSION (3.1)
  // Regex matches strings like:
  // ${example}aaa

  // get the matched string
  std::string s = std::string(yytext);

  // Will be result string after processsing
  std::string end_s = "";

  // temp string to get var. name
  std::string temp = "";

  // flag to track if inside a variable
  bool brace = false;

  // Loop through characters in the string
  for (std::size_t i = 0; i < s.length(); ++i) {

    // Check if encounter '${'
    if (s[i] == '$' && i + 1 < s.length() && s[i+1] == '{') {

      // in new env variable
      brace = true;
      i++; // Skip the '{' character
      temp = "";

      // Collect variable name
      while (i + 1 < s.length() && s[i + 1] != '}') {
        temp += s[i + 1];
        i++;
      }

      // Check for end of variable ex[ansion '}'
      if (i + 1 < s.length() && s[i + 1] == '}') {
        i++; // Skip '}'

        // Lookup the env variable or handle the special cases
        // SPECIAl case of ${SHELL} is handled in shell.cc
        if (getenv(temp.c_str()) != NULL) {
          end_s += getenv(temp.c_str());
        }
        else if (temp == "$") { // Pringt pid of shell process
          end_s += std::to_string(getpid());
        }
        else if (temp == "!") { // print last PID, tracked in command.cc
          end_s += std::to_string(last_pid);
        }
        else if (temp == "?") { // Print return code of last command. tracked in command.cc
          end_s += std::to_string(code);
        }
        else if (temp == "_") { // Doesnt work booo
          end_s += Shell::last_arg;
        }
      }
    } else {
      // Copy character to result
      end_s += s[i];
    }
  }

  yylval.cpp_string = new std::string(end_s);
  return WORD;
}





["][^\n"]*["] {
  // Quotes
  yylval.cpp_string = new std::string(yytext);

  // Remove first and last character (the quotes)
  *yylval.cpp_string = yylval.cpp_string->substr(1, yylval.cpp_string->length()-2);

  return WORD;
}


[^ \t\n|><&]*\\[^\t\n]+ {
  // Escape characters

  std::string temp_string;

  // Loop through each character in yytext
  for (int index = 0; yytext[index] != '\0'; index++) {

    // If current char is a backslash and next char is not a space, skip adding this backslash.
    if (yytext[index] == '\\' && yytext[index + 1] != ' ') {
      continue;
    }
    temp_string += yytext[index];
  }

  // Store the processed string in yylval
  yylval.cpp_string = new std::string(temp_string);
  return WORD;
}





[$][(][^\n\$]*[)] {
  // SUBSHELL implementation
  // FUck this shit

  // Get the text from command
  std::string command = yytext;

  // Remove the $ at beginning: $(ls) --> (ls)
  if (command.at(0) == '$') {
    command = command.substr(1, command.size() - 1); // Remove the $
  }


  /* pin[]: (input pipe)
   * Direction: Parent --> Child
   * Purpose: Sends input data frmo parent shell to child subshell,
   *          parent writes to pin[1] and this input is read from,
   *          child process via pin[0].
   *
   * pout[]: (output pipe)
   * Diection: Child --> Parent
   * Purpose: Used to capture output from child subshell. The child writes,
   *          the output to pout[1] and the parent reads this via pout[0].
   *
   */


  /* Two pipes are used for two-way communctation between shell and parent subshell
   * Allosws for communication from parent-->child and child-->parent.
   */


  // Remove the parenteisis: (ls) --> ls
  command = command.substr(1, command.size() - 2);

  // Convert command because compiler yelled at me
  char *sub = const_cast<char*> (command.c_str());

  // Save current stdin, stout for future saving
  int dfin = dup(0);
  int dfout = dup(1);

  // In pipe: used for writing to proccess
  int pin[2];
  pipe(pin);

  // Out pipeL used for reading from proccess
  int pout[2];
  pipe(pout);

  // Write to pin[1] (write end)
  write(pin[1], sub, strlen(sub));
  write(pin[1], "\n", 1);

  // Close pipe
  close(pin[1]);

  // rediret stdin to read-end, pin[0], to pass command,
  // to the child
  dup2(pin[0], 0);
  close(pin[0]);

  // redirect stdout to write-end, pout[1], to get command output
  dup2(pout[1], 1);
  close(pout[1]);

  // create child process
  int pid = fork();
  if (pid == 0) { // Child process

    // Create an array of arguments
     char *args[] = {"/proc/self/exe", sub, NULL};

    // Execute shell with arguments
    execvp("/proc/self/exe", args);

    _exit(1);
  }
  else if (pid < 0) {
    perror("fork");
    exit(1);
  }

  // PARENT PROCESS

  // Restore and close og file descriptores
  dup2(dfin, 0);
  dup2(dfout, 1);
  close(dfin);
  close(dfout);

  // red output from child proccess (written to pout[0])
  char c;
  int i = 0;
  size_t buff_size = 1024; // Start with 1 kb buffer size
  char *buffer = (char *)malloc(buff_size);

  // Handle error
  if (!buffer) {
    perror("malloc");
    exit(1);
  }

  // Read from output pipe till no data 
  while (read(pout[0], &c, 1) > 0) {

    // If buffer is full, resize it
    if (i >= buff_size - 1) {
      buff_size *= 2;
      buffer = (char *)realloc(buffer, buff_size);

      // Handle error
      if (!buffer) {
        perror("realloc");
        exit(1);
      }
    }

    // if char is a newline, replace it with a space
    if (c == '\n') {
      buffer[i++] = ' ';
    } else {
      buffer[i++] = c; // copy char as is
    }
  }

  buffer[i] = '\0'; // Null terminate

  // "unput" each character from buffer
  for (i = strlen(buffer) - 1; i >= 0; i--) {
    myunputc(buffer[i]);
  }

  // Clsoe read end of out pipe since done
  close(pout[0]);

}



[^ \t\n|><][^ \t\n|><]*  {
  /* Assume that file names have only alpha chars */
  yylval.cpp_string = new std::string(yytext);
  return WORD;
}

