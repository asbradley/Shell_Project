#ifndef command_hh
#define command_hh

#include "simpleCommand.hh"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <regex.h>
#include <pwd.h>



// Command Data Structure

struct Command {
  std::vector<SimpleCommand *> _simpleCommands;
  std::string * _outFile;
  std::string * _inFile;
  std::string * _errFile;
  bool _background;


  // Enum to track appending to file or overwriting ( >, >>)
  enum RedirectMode { OVERWRITE, APPEND };
  RedirectMode _outMode; // mode for output, just a flag

  Command();
  void insertSimpleCommand( SimpleCommand * simpleCommand );

  void clear();
  void print();
  void execute();


  // Built in function declaration (2.6)
  bool builtIn_cd();

  bool builtIn_setenv();

  bool builtIn_unsetenv();

  static SimpleCommand *_currentSimpleCommand;






  // List of stringg in a vector, like an arrat
  // Stores paths of FIFOs and the temp. directories created during process sub.
  // Makes sure paths are cleaned up to avoid problems
  static std::vector<std::string> _cleanup_paths; 

  // Adds a file path to _cleanup_paths
  static void addCleanupPath(const std::string& path) {
    _cleanup_paths.push_back(path);
  }

  // Function to remove all stored FIFOs and directoreies once not needed
  static void performCleanup() {

    // Loop thorugh _cleanup_paths
    // could look like: /tmp/shell-12345/fifo
    //                  /tmp/shell-12345
    for (const std::string& path : _cleanup_paths) {

      // unlink() deletes the files
      // 'path.c_str()' holds the file path
      unlink(path.c_str());  // Remove FIFO

      // If path contains '/shell-' is is temp. directory
      if (path.find("/shell-") != std::string::npos) {
        // If this is a temp directory path (contains /shell-), try to remove it
        rmdir(path.c_str());
      }
    }

    // clear list for future use
    _cleanup_paths.clear();
  }
};

#endif
