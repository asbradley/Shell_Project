#include <cstdio>
#include <cstdlib>

#include <iostream>

#include "command.hh"
#include "shell.hh"

extern char **environ;
void source(const char *); // source builtIn function

int code = 0;
int last_pid = 0;
int exit_code = 0;

std::string last_arg = "";

// Vector of paths for source substitution thingy stuff i hate this
std::vector<std::string> Command::_cleanup_paths;



Command::Command() {
    // Initialize a new vector of Simple Commands
    _simpleCommands = std::vector<SimpleCommand *>();

    _outFile = NULL;
    _inFile = NULL;
    _errFile = NULL;
    _background = false;

    // Initialize enum to default (overwrite)
    _outMode = OVERWRITE;
}

void Command::insertSimpleCommand( SimpleCommand * simpleCommand ) {
    // add the simple command to the vector
    _simpleCommands.push_back(simpleCommand);
}

void Command::clear() {
    // deallocate all the simple commands in the command vector

    for (auto simpleCommand : _simpleCommands) {
        delete simpleCommand;
    }


    // remove all references to the simple commands we've deallocated
    // (basically just sets the size to 0)
    _simpleCommands.clear();

    if ( _outFile ) {
        delete _outFile;
    }
    _outFile = NULL;

    if ( _inFile ) {
        delete _inFile;
    }
    _inFile = NULL;

    if ( _errFile ) {
        delete _errFile;
    }


    _errFile = NULL;

    _background = false;

    _outMode = OVERWRITE;
}



void Command::print() {
    printf("\n\n");
    printf("              COMMAND TABLE                \n");
    printf("\n");
    printf("  #   Simple Commands\n");
    printf("  --- ----------------------------------------------------------\n");

    int i = 0;
    // iterate over the simple commands and print them nicely
    for ( auto & simpleCommand : _simpleCommands ) {
        printf("  %-3d ", i++ );
        simpleCommand->print();
    }

    printf( "\n\n" );
    printf( "  Output       Input        Error        Background     Append\n");
    printf( "  ------------ ------------ ------------ ------------ ------------\n" );
    printf( "  %-12s %-12s %-12s %-12s %-12s\n",
            _outFile?_outFile->c_str():"default",
            _inFile?_inFile->c_str():"default",
            _errFile?_errFile->c_str():"default",
            _background?"YES":"NO",
            _outMode?"YES":"NO");
    printf( "\n\n" );
}



// Built in function to handle 'cd'
bool Command::builtIn_cd() {

  // Store the first and second arg of the command
  // ex: cd something
  std::string *first_arg = _simpleCommands[0]->_arguments[0];
  std::string *second_arg = _simpleCommands[0]->_arguments[1];

  // Verify first arg is 'cd'
  if (*first_arg == "cd") {

    // Check if second arg is null or is ${HOME} literal string
    // If so, go to home directory
    if (second_arg == nullptr || *second_arg == "${HOME}") {

      // Get the home env
      const char *home = getenv("HOME");
      if (home != nullptr) {

        chdir(home); // Change dir. to home dir.
      }
    } else { // Tru and go to given directory from second_arg

      // Get the direcotry from second_arg
      int dir_found = chdir(second_arg->c_str());

      // if 'dir_found' is negative --> directory not found
      // Print error message
      if (dir_found < 0) {

        // Error message
        std::string error = "cd: can't cd to ";
        error.append(second_arg->c_str());
        fprintf(stderr, "%s\n", error.c_str());
      }
    }

    // Clear and print new prompt
    clear();
    Shell::prompt();
    return true;
  }

  // shouldnt be here
  return false;


}

// BuiltIN function to handle setenv
bool Command::builtIn_setenv() {

  // Check for correct size, 3
  // setenv A B
  if (_simpleCommands[0]->_arguments.size() != 3) {
    perror("setenv");
  }

  // setenv(A, B, 1)
  // '1' means to overwrite if the env. var. already exists
  setenv(_simpleCommands[0]->_arguments[1]->c_str(), _simpleCommands[0]->_arguments[2]->c_str(), 1);
  clear();
  Shell::prompt();
  return true;
}


// Function to handle unsetenv builtIn Function
bool Command::builtIn_unsetenv() {

  // Checkk for correct size
  // 'unsetenv A'
  if (_simpleCommands[0]->_arguments.size() != 2) {
    perror("unsetenv");
  }

  // Call: 'unsetenv(A)
  if (unsetenv(_simpleCommands[0]->_arguments[1]->c_str())) {
    perror("unsetenv");
  }

  clear();
  Shell::prompt();
  return true;
}


// TODO
// Need to handle source and printenv

void Command::execute() {
    // Don't do anything if there are no simple commands
    if (_simpleCommands.empty()) {
        Shell::prompt();
        return;
    }

    // print();


    // get the first command
    // Used for builtIn functions
    std::string *cmd = _simpleCommands[0]->_arguments[0];

    // Handle exit command
    if (*cmd == "exit") {
      printf("Good bye!!\n");
      exit(1);
    }

    /* Handle the 'cd' command
     * This is handled in the parent process, because it affects current,
     * working directory. If the child handled this then the changes (changing directories),
     * would not persist after the child terminates.
     */
    if (*cmd == "cd") {
      builtIn_cd();
      clear();
      return;
    }

    /* Handle the 'setenv' builtin function
     * 'setenv' 'unsetenv both modify environmental variables,
     * which are normally stored in the parent process. Once again,
     * if handled in the child process, the changes would not persist,
     * after the child terminates.
     */
    if (*cmd == "setenv") {
      builtIn_setenv();
      return;
    }

    // Handle the 'unsetenv' command
    if (*cmd == "unsetenv") {
      builtIn_unsetenv();
      return;
    }

    /* Handle the 'source' command
     * If this was handled in the child process the changes made would,
     * not persist after the child process termiantes.
     */
    if (*cmd == "source") {
      _simpleCommands.clear(); // Clear commands
      source(_simpleCommands[0]->_arguments[1]->c_str()); // Call source() on file provided
      return;
    }



    // Save default file descriptors
    int defaultin = dup(0); // stdin
    int defaultout = dup(1); // stdout
    int defaulterr = dup(2); // stderr

    // Initialize file descriptors
    // These vars. will be set to point to files or pipes
    int fdin = 0;
    int fdout = 0; 
    int fderr = 0;

    // print comamnd table
    //print();



    /*
     * Why is _infile and _errfile set up before for loop?
     * _inFile: To see if input file is provided, so shell can read from that file
     * _errFile: Applies globally, all commands' stderr go there,
                 Each chlid process inherits this file descriptor
     */

     /* Why is _outFile not used before for loop?
      * _outfile only applies to last command in pipeline
      */



    // Set up input redirection
    if (_inFile) { // inpute file was given
        fdin = open(_inFile->c_str(), O_RDONLY);
        if (fdin < 0) {

            // open failed: print error and resotre og file descriptores
            perror("Error opening input file");
            dup2(defaultin, 0);
            dup2(defaultout, 1);
            dup2(defaulterr, 2);
            close(defaultin);
            close(defaultout);
            close(defaulterr);
            clear();
            Shell::prompt();
            return;
        }
    } else {
        fdin = dup(defaultin); // No input file --> use stdin
    }

    // Set up error redirection
    if (_errFile) { // if error file is given
        if (_outMode == APPEND) { // Check if append or not based on enum value
            fderr = open(_errFile->c_str(), O_CREAT | O_WRONLY | O_APPEND, 0664);
        } else { // truncate
            fderr = open(_errFile->c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0664);
        }
        if (fderr < 0) {
            perror("Error opening error file");
            dup2(defaultin, 0);
            dup2(defaultout, 1);
            dup2(defaulterr, 2);
            close(defaultin);
            close(defaultout);
            close(defaulterr);
            close(fdin);
            clear();
            Shell::prompt();
            return;
        }
    } else {
        fderr = dup(defaulterr); // no error file --> use stderr
    }

    // Apply error redirection for the entire pipeline
    // Makes sure any error output from command in pipeline goes to correct destination
    dup2(fderr, 2);
    close(fderr);


    // Process each command in the pipeline
    pid_t lastPid;
    for (size_t i = 0; i < _simpleCommands.size(); i++) {

        // Redirect input to so current command can read from it
        dup2(fdin, 0);
        close(fdin);

        // Set up output
        if (i == _simpleCommands.size() - 1) { // last command in the pipeline

            // Last command: use specified output file or default
            if (_outFile) {
                if (_outMode == APPEND) {
                    fdout = open(_outFile->c_str(), O_CREAT | O_WRONLY | O_APPEND, 0664);
                } else {
                    fdout = open(_outFile->c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0664);
                }
                if (fdout < 0) {
                    perror("Error opening output file");
                    dup2(defaultin, 0);
                    dup2(defaultout, 1);
                    dup2(defaulterr, 2);
                    close(defaultin);
                    close(defaultout);
                    close(defaulterr);
                    clear();
                    Shell::prompt();
                    return;
                }
            } else {
                fdout = dup(defaultout); // no output file -- >use stdout
            }
        } else {

           // Not the last command: create a pipe
            int fdpipe[2];
            if (pipe(fdpipe) < 0) {
                perror("pipe");
                exit(1);
            }
            fdout = fdpipe[1];  // Write end of pipe
            fdin = fdpipe[0];   // Read end for next command
        }

        // Redirect output
        dup2(fdout, 1);
        close(fdout);


        // Fork and execute
        pid_t pid = fork();

        // Get size of arguement. Used of expand_var_underscore test
        size_t argsize = _simpleCommands[i]->_arguments.size();
        if (pid == -1) {
            perror("fork");
            exit(2);
        }

        if (pid == 0) {
          // Child process

          // Handle printenv function
          // Handle in child process as it just prints environmental variables of the shell.
          // It does not modify anything so is can be done in child process
          if (!strcmp(_simpleCommands[i]->_arguments[0]->c_str(), "printenv")) {

            // Loop through and print the env variables
            for (char **env = environ; *env; env++) {
              printf("%s\n", *env);
            }
            exit(0);
          }

            // Build argv vector for execvp
            char **args = new char*[argsize + 1];
            for (size_t j = 0; j < argsize; j++) {
                args[j] = const_cast<char*>(_simpleCommands[i]->_arguments[j]->c_str());
                args[j][strlen(_simpleCommands[i]->_arguments[j]->c_str())] = '\0';

            }
            args[argsize] = NULL;

            // Close file descriptors
            close(defaultin);
            close(defaultout);
            close(defaulterr);

            // Execute command
            execvp(args[0], args);
            perror("execvp");
            _exit(1);  // Use _exit in child process
        }

        // Store last process ID for waiting
        lastPid = pid;

        last_arg = strdup(_simpleCommands[i]->_arguments[argsize-1]->c_str());

    }

    // Restore default file descriptors
    dup2(defaultin, 0);
    dup2(defaultout, 1);
    dup2(defaulterr, 2);
    close(defaultin);
    close(defaultout);
    close(defaulterr);

    // If not running in background --> wait for last process to finish
    int stat = 0;
    if (!_background) {
      waitpid(lastPid, &stat, 0);
      code = WEXITSTATUS(stat); // USED FOR ${?}
      exit_code = code; // USED FOR EXTRA CREDIT
    } else {
      last_pid = lastPid;
      exit_code = 1;
    }

/*
    if (!_simpleCommands.empty()) {
      for (int i = _simpleCommands.size() - 1; i >= 0; i--) {
        printf("%s\n", _simpleCommands[i]->_arguments.back()->c_str());
        if (*(_simpleCommands[i]->_arguments.back()) == "../shell") continue;
        SimpleCommand * last_cmd = _simpleCommands[i];
        if (!last_cmd->_arguments.empty()) {
          Shell::last_arg = *(last_cmd->_arguments.back());
        }
      }
    }*/


    // Clean up proccess substitution resources
    performCleanup();

    // clear table and print new prompt (myshell> )
    clear();
    Shell::prompt();
}


SimpleCommand * Command::_currentSimpleCommand;
