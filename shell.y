
/*
 * shell.y: parser for shell
 *
 * This parser compiles the following grammar:
 *
 *	cmd [arg]* [> filename]
 *
 */


%define parse.error verbose

%code requires 
{
#include <string>

// Needed for wilcard
#include <vector>
#include <algorithm>
#include <dirent.h>
#include <regex.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>


#if __cplusplus > 199711L
#define register      // Deprecated in C++11 so remove the keyword
#endif
}

%union
{
  char        *string_val;
  // Example of using a c++ type in yacc
  std::string *cpp_string;
}

%token <cpp_string> WORD

// ADDED TOKENS
%token NOTOKEN GREAT NEWLINE PIPE AMPERSAND LESS GREATAMPERSAND GREATGREAT GREATGREATAMPERSAND TWOGREAT

%{
//#define yylex yylex
#include <cstdio> 
#include "shell.hh"
#include <iostream>

void yyerror(const char * s);
int yylex();


/* FLOW OF WILDCARD FOR MY OWN SANITY
 * 1) - Parser will parse WORD token
 *    - expand_wildcards is called on the arg
 *
 * 2) Expand_wildcards():
 *      - Check if there is wildcard present
 *      - Calls recursive function to perform expansion
 *      - sorts results using std::sort
 *
 * 3) Expand_Wildcards_Recursive():
 *      - Splits path into components
 *          - Base directory (directory to search in)
 *          - Pattern (part containing wildcards)
 *          - Remaning path (used for recursion part)
 *      - Calls wildcard_to_regex() to create a regex pattern
 *      - Opens and scans directory
 *      - matches entires against the pattern
 *      - Handles special cases:
 *          - Hidden files (e.g .git)
 *          - Direcotry entires (. and ..)
 *      - Recursively processes nested wildcards
 *      - Sorts (using std::sort) and merges results
 * 4) Wildcards_To_Regex():
 *      - Creates a regex pattern given a wildcard pattern (e.g file*.txt)
 *      - '*' becomes '.*' (matches any sequence of characters)
 *      - '?' becomes '.' (matches any single character)
 *      - Escape sequences are preseved
 *      - Special regex characters are esccped
 *      - Pattern starts with '^' and ends with '$'
 */





// '*' matches zeor or more characters: *.txt --> f.txt, bob.txt, ...ect

// '?' matches exactly one character: file?.txt --> file1.txt, file2.txt, ...ect




/* Converts a wildcard pattern into a regex expression.
 * Used for matching and comparisons, this cannot be done in C/C++ easily,
 * so translating wildcard --> regex makes life easier
 */
std::string wildcard_to_regex(const std::string& wildcard) {

  // Start regex with '^' (means start of the string)
  std::string regex_str = "^";

  // iterate thorugh wildcard characters and build regex
  for (size_t i = 0; i < wildcard.size(); i++) {

    // Get current character
    char c = wildcard[i];

    // Help to build the regex
    switch (c) {
        case '*':
          regex_str += ".*";
          break;
        case '?':
          regex_str += ".";
          break;
        case '.':
          regex_str += "\\.";
          break;
        case '\\':
          regex_str += "\\\\";
          break;
        case '+':
          regex_str += "\\+";
          break;
        case '^':
          regex_str += "\\^";
          break;
        case '$':
          regex_str += "\\$";
          break;
        case '(':
          regex_str += "\\(";
          break;
        case ')':
          regex_str += "\\)";
          break;
        case '{':
          regex_str += "\\{";
          break;
        case '}':
          regex_str += "\\}";
          break;
        case '[':
          regex_str += "[";
          break;
        case ']':
          regex_str += "]";
          break;
        case '|':
          regex_str += "\\|";
          break;
        default:
          regex_str += c;
          break;
    }
  }

  // End regex with '$' (means end of the string)
  regex_str += "$";
  return regex_str;
}


/* Handles recurive expansion of wildcard patterns
 */
void expand_wildcards_recursive(const std::string &path, std::vector<std::string> &expanded_paths) {

  // Find where the first wildcard is at (*?)
  size_t first_wildcard = path.find_first_of("*?");

  // If no wildcards found --> return og path
  if (first_wildcard == std::string::npos) {
    expanded_paths.push_back(path);
    return;
  }

  // Go backwards from the index 'first_wildcar' until the next '/' is found
  // EX: 'src/*/main.txt'  'first_wildcard' = 4    start at index 4 and go backwards until '/' is found
  // This would return and index of 3.
  // Finds the directory to search in.
  size_t last_slash_before_wildcard = path.rfind('/', first_wildcard);

  // Goes forwards from index 'first_wildcard' to find next '/'
  // EX: 'src/*/main.txt    'first_wildcar' = 4   start at index 4 and go forwards until '/' is found
  // Would return index 5
  // Finds where current wildcard section ends
  size_t next_slash_after_wildcard = path.find('/', first_wildcard);

  // Base directory to search in
  // If no '/' before wildcard it searches in current directory '.'
  std::string base_dir;

  // Check if 'last_slash_...' was not found, if so use current dir. '.'
  if (last_slash_before_wildcard == std::string::npos) {
      base_dir = ".";
  }
  else { // '/' exists before wildcard index, extract part before the slash
    base_dir = path.substr(0, last_slash_before_wildcard);
    if (base_dir.empty()){
      base_dir = "/";
    }
  }

  // Pattern to match files in this directory
  // Gets the acutal pattern to use (*.txt, file?) ect
  std::string pattern;
  if (last_slash_before_wildcard == std::string::npos) {
    pattern = path.substr(0, next_slash_after_wildcard);
  }
  else {
    size_t start_index = last_slash_before_wildcard + 1;
    size_t length;

    // Check if there is a '/' or not
    if (next_slash_after_wildcard == std::string::npos) {
      length = std::string::npos;
    }
    else {
      length = next_slash_after_wildcard - last_slash_before_wildcard - 1;
    }

    // get the pattern based on slashes
    pattern = path.substr(start_index, length);
  }

  // Rest of the path (after next slash)
  // Used for recursion ater on if needed
  // EX: /foo.txt in 'src/*/foo.txt
  std::string rest_of_path;
  if (next_slash_after_wildcard != std::string::npos) {
    rest_of_path = path.substr(next_slash_after_wildcard);
  }

  // Convert pattern to a regex expression
  std::string regex_pattern = wildcard_to_regex(pattern);
  regex_t regex;

  // Compile the regular expression string into form that can be used for matching
  int result = regcomp(&regex, regex_pattern.c_str(), REG_EXTENDED | REG_NOSUB);
  if (result != 0) {

    // Error compiling regex
    expanded_paths.push_back(path);
    return;
  }

  // Open the directory that contains the wildcard
  // Ex: src/*.txt, base_dir is src
  DIR* dir = opendir(base_dir.c_str());
  if (!dir) {

    // Error occured. Free the regex and use og path
    regfree(&regex);
    expanded_paths.push_back(path);
    return;
  }

  // This holds info about each file or folder name in the given directory
  struct dirent* entry;

  // temp vector to store list of paths that match current wildard pattern
  std::vector<std::string> matched_entries;

  // Read one entry at a time in the given directory
  // Goes until all directory entries have been read
  while ((entry = readdir(dir)) != nullptr) {

    // Get the current entry name
    std::string filename = entry->d_name;

    // Skip . and .. unless specifically matched
    if ((filename == "." || filename == "..") && pattern[0] != '.') {
      continue;
    }

    // Skip hidden files unless pattern starts with a dot
    if (filename[0] == '.' && pattern[0] != '.') {
      continue;
     }

    // Match against regex
    // regexec tests whether 'filename' matches the regex pattern from earlier, 0 means match
    if (regexec(&regex, filename.c_str(), 0, nullptr, 0) == 0) {

      // String to hold the full path
      std::string full_path;

      // if base directory is '.' just use the current filename
      if (base_dir == ".") {
        full_path = filename;
      }
      else if (base_dir == "/") { // if base_dir is '/' (root) add '/' filename
        full_path = "/" + filename;
      }
      else { // Otherwise join base_dir + '/' and filename
        full_path = base_dir + "/" + filename;
      }

      // If there's more to the path, recursively expand it
      // e.g */example/*.txt
      if (!rest_of_path.empty()) {
 
        // Try to use opendir to open the given path
        DIR* test = opendir(full_path.c_str());
        if (test) {
          closedir(test);

          // Recursively expand
          // e.g 'src/*/file.txt
          // If src/utils is match check if its direcotry
          // if yes, then call the function on 'src/utils/file.txt ...
          expand_wildcards_recursive(full_path + rest_of_path, expanded_paths);
        }
      } else { // Path is empty, just add to the matched_entries vector
         matched_entries.push_back(full_path);
      }
    }
  }

  // Close direcotyr and free regex memory
  closedir(dir);
  regfree(&regex);

  // Sort the matches using std::sort()
  std::sort(matched_entries.begin(), matched_entries.end());

  // Add matches to expanded_paths
  for (const auto& entry : matched_entries) {
    expanded_paths.push_back(entry);
  }

  // If no matches and no further path components, use the original pattern
  if (matched_entries.empty() && expanded_paths.empty()) {
    expanded_paths.push_back(path);
  }
}



/* Entry point for wildcard expansion
 * Param 1: The input path that may contain wildcard characters
 * Param2: A reference to a vector where match paths will be stored
 */
void expand_wildcards(const std::string &path, std::vector<std::string> &expanded_paths) {

  // If no wildcards, just return the path
  if (path.find('*') == std::string::npos && path.find('?') == std::string::npos) {
    expanded_paths.push_back(path);
    return;
  }

  // Clear the vector to ensure we start fresh
  expanded_paths.clear();

  // Expand the wildcards
  expand_wildcards_recursive(path, expanded_paths);

  // If no matches were found, return the original path
  /* EX: 'ls *.xyz' is typed in,
   * but no files match *.xyz. This will result in the path being returned as the command,
   * 'ls *.xyz;
   */
  if (expanded_paths.empty()) {
    expanded_paths.push_back(path);
  }

  // Sort the paths to pass the test cases
  std::sort(expanded_paths.begin(), expanded_paths.end());
}




%}

%%

goal:
  commands
  ;

commands:
  command
  | commands command
  ;

command: simple_command
       ;

simple_command: 
  pipe_list iomodifier_list background_flag NEWLINE {

    //printf("   Yacc: Execute command\n");


    Shell::_currentCommand.execute();
  }
  | NEWLINE {
    Shell::prompt();
  }
  | error NEWLINE { yyerrok; }
  ;




// Adding pipe list here
pipe_list:
  command_and_args
  | pipe_list PIPE command_and_args
  //| command_and_args
  ;



command_and_args:
  command_word argument_list {
    Shell::_currentCommand.
    insertSimpleCommand( Command::_currentSimpleCommand );
  }
  ;

argument_list:
  argument_list argument
  | /* can be empty */
  ;

argument:
  WORD {
    //printf("   Yacc: insert argument \"%s\"\n", $1->c_str());


    // Create a vector to store results for wildcard expansion
    std::vector<std::string> expanded_paths;

    // Call the wildcard expansion function ($1 refers to WORD token)
    // 'expaned_paths' vector is populated with the results
    expand_wildcards($1->c_str(), expanded_paths);

    // iterate thorugh each expaned path
    for (size_t i = 0; i < expanded_paths.size(); i++) {

      // Create a new string object for each path
      std::string *arg = new std::string(expanded_paths[i]);

      // Add each path as an argumenet like before
      Command::_currentSimpleCommand->insertArgument(arg);
    }

    // If expansion led to multiple matches OR one match differes from original,
    // Delete the original token since expanded version is being used now
    /* EXAMPLES FOR MY BRAIN TO REMEMBER HOW I DID THIS.
     * 1st case in if statement:
     *  - ls *.txt expands to [file.txt, file2.txt, notes.txt... ect)
     * 2nd case in if statement:
     *  - ls test?.txt expands to test1.txt
     * In both scenarios the og WORD token is not the same, so it must be deleted,
     * and the new version is inserted instead
     */

    if (expanded_paths.size() > 1 ||
        (expanded_paths.size() == 1 && expanded_paths[0] != *$1)) {
      delete $1;
    }
    else if (expanded_paths.empty()) { // if no expanded paths (not a wildcard), then insert og token

      // No expansion happened, insert original argument
      Command::_currentSimpleCommand->insertArgument($1);
      delete $1;
    }
  }
  ;

command_word:
  WORD {

    //printf("   Yacc: insert command \"%s\"\n", $1->c_str());
    Command::_currentSimpleCommand = new SimpleCommand();
    Command::_currentSimpleCommand->insertArgument( $1 );
  }
  ;

// Add list for iomodifier
// Allows for correct parsing with multiple iomodifiers in same command
iomodifier_list:
  iomodifier_list iomodifier_opt
  | iomodifier_opt
  | /* can be empty */
  ;



iomodifier_opt:
  GREAT WORD {
    //printf("   Yacc: insert output \"%s\"\n", $2->c_str());
    if (Shell::_currentCommand._outFile != NULL) {
      printf("Ambiguous output redirect.\n");
      exit(0);
    }
    // Shell::_currentCommand._outMode = Command::OVERWRITE; // Set overwrite flag
    Shell::_currentCommand._outFile = $2;

  }

  // Add >&
  | GREATAMPERSAND WORD {
    if (Shell::_currentCommand._outFile != NULL) {
      printf("Ambiguous output redirect.\n");
      exit(0);
    }

    // Use differnt string for err and out tob avoid the stupid double free seg fault I keep getting over and over
    std::string *errFile = new std::string($2->c_str());
    Shell::_currentCommand._outFile = $2;
    Shell::_currentCommand._errFile = errFile;
  }

  // Add >>
  | GREATGREAT WORD {
    if (Shell::_currentCommand._outFile != NULL) {
      printf("Ambiguous output redirect.\n");
      exit(0);
    }
    Shell::_currentCommand._outMode = Command::APPEND; // Use enum to track >>
    Shell::_currentCommand._outFile = $2;
    // Does not need ._errFile, because >> Does not redirect stderr

  }

  // >>&
  | GREATGREATAMPERSAND WORD {
    if (Shell::_currentCommand._outFile != NULL) {
      printf("Ambiguous output redirect.\n");
      exit(0);
    }

    // Use different string for err to avoid dumb doubel free seg fault
    std::string *errFile = new std::string($2->c_str());
    Shell::_currentCommand._outMode = Command::APPEND;
    Shell::_currentCommand._outFile = $2;
    Shell::_currentCommand._errFile = errFile;
  }

  // <
  | LESS WORD {
    if (Shell::_currentCommand._inFile != NULL) {
      printf("Ambiguous output redirect.\n");
      exit(0);
    }

    Shell::_currentCommand._inFile = $2;
  }

  // 2>
  | TWOGREAT WORD {
    Shell::_currentCommand._errFile = $2;
  }
  //| /* can be empty */ 
  //;


background_flag:
  AMPERSAND {
    Shell::_currentCommand._background = true;
  }
  | /* Can be empty */
  ;


%%

void
yyerror(const char * s)
{
  fprintf(stderr,"%s", s);
}

#if 0
main()
{
  yyparse();
}
#endif
