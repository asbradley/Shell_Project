#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>

#define MAX_BUFFER_LINE 2048
#define MAX_HISTORY 100

extern void tty_raw_mode(void);


extern void reset_buffer(void);

struct termios orig_termios;

// Buffer where line is stored
int line_length;
int line_location;

// Temp storeage for current line of user input
char line_buffer[MAX_BUFFER_LINE];


void reset_buffer(void) {
  line_length = 0;
  line_location = 0;
  memset(line_buffer, 0, MAX_BUFFER_LINE);
}

// Simple history array
// This history does not change. 
// Yours have to be updated.
int history_index = 0;
/*
char * history [] = {
  "ls -al | grep x", 
  "ps -e",
  "cat read-line-example.c",
  "vi hello.c",
  "make",
  "ls -al | grep xxx | grep yyy"
};
*/

char *history[MAX_HISTORY];
//int history_length = 0;

int history_length = sizeof(history)/sizeof(char *);

// Helper function to add to history
void add_to_history(const char* cmd) {

  if (cmd == NULL || cmd[0] == '\0') {
    return;
  }


  // If history is full, remove oldest entry
  if (history_length >= MAX_HISTORY) {
    free(history[0]);

     // Shift all entries down to make room for new command
    for (int i = 0; i < history_length - 1; i++) {
      history[i] = history[i + 1];
    }
    history_length--;
  }

  // Allocate and add new entry
  history[history_length] = strdup(cmd); // strdup allocates memory and copies
  if (history[history_length]) {
    history_length++;
    history_index = history_length;
  }
}


// Free all memory used by history
void cleanup_shell(void) {
  for (int i = 0; i < history_length; i++) {
    free(history[i]);
    history[i] = NULL;
  }
  history_length = 0;
  history_index = 0;
}



void read_line_print_usage()
{
  char * usage = "\n"
    " ctrl-?       Print usage\n"
    " Backspace    Deletes last character\n"
    " up arrow     See last command in the history\n";

  write(1, usage, strlen(usage));
}

/* 
 * Input a line with some basic editing.
 */


char * read_line() {

  // Set terminal in raw mode
  tty_raw_mode();

  line_length = 0;
  line_location = line_length; // Cursor position in the line

  // Set history entires to be NULL
  for (int i = 0; i < MAX_HISTORY; i++) {
    history[i] = NULL;
}

  // Read one line until enter is typed
  while (1) {

    // Read one character in raw mode.
    char ch;
    read(0, &ch, 1);

    if (ch>=32 && ch != 127) {
      // It is a printable character

      if (line_length == MAX_BUFFER_LINE-2) break;

      // Check if cursor is at end of line
      if (line_location == line_length) {

        // Echo character to screen
        write(1, &ch, 1);

        // Save this to the buffer
        line_buffer[line_length] = ch;

        // Extne line and move cursor forward
        line_length++;
        line_location++;
      } else {

        // Case for inserting chars. in middle of line

        // if typing in middle of line, everytning after cursor shifts right
        for (int i = line_length; i > line_location; i--) {
          line_buffer[i] = line_buffer[i-1]; // Shift char to right
        }

        line_buffer[line_location] = ch;
        line_length++;

        // Echo the character
        write(1, &ch, 1);

        // Write the rest of the line from the cursor position
        write(1, &line_buffer[line_location + 1], line_length - line_location - 1);

        // Move cursor back to the position after inserted character
        for (int i = 0; i < line_length - line_location - 1; i++) {
          char back = 8; // backspace

          // 8 is ASCII for <backspace char> '\b'
          // Moves terminal curose back one
          write(1, &back, 1);
        }

        // Move cursor position forward
        line_location++;
      }
    }

    else if (ch==10) {
      // <Enter> was typed. Return line
      // Print newline
      write(1,&ch,1);
      break;
    }
    else if (ch == 31) {
      // ctrl-?
      read_line_print_usage();
      line_buffer[0]=0;
      break;
    }
    else if (ch == 8 || ch == 127) {
      // <backspace> or <delete> was typed. Remove previous character
      // Go back one character

        // Check to make sure not at beginning of line
      if (line_location > 0) {
        line_location--; // Move cursor back one position

        // Shift characters to the left
        for (int i = line_location; i < line_length - 1; i++) {
          line_buffer[i] = line_buffer[i + 1];
        }

        line_length--; // Reduce line length

        // Go back one character
        char back = 8;
        write(1, &back, 1);

        // Redraw the rest of the line
        write(1, &line_buffer[line_location], line_length - line_location);

        // Add space to erase the last character
        char space = ' ';
        write(1, &space, 1);

        // Move cursor back to the correct position
        for (int i = 0; i <= line_length - line_location; i++) {
          write(1, &back, 1);
        }
      }
    }
    else if (ch == 4) { // DELETE KEY (ctrl+D): remove character form currnet location

      // Make sure not at end of line
      if (line_location < line_length) {

        // shift character left
        for (int i = line_location; i < line_length - 1; i++) {
          line_buffer[i] = line_buffer[i + 1];
        }

        // Redraw from cursor position
        write(1, &line_buffer[line_location], line_length - line_location - 1);
 
        // Add space to erase last character
        char space = ' ';
        write(1, &space, 1);

        // Move cursor back to original position
        char back = 8;
        for (int i = 0; i <= line_length - line_location - 1; i++) {
          write(1, &back, 1);
        }

        line_length--;
      }
    } // END OF DELETE KEY
    else if (ch == 1) { // HOME KEY (ctrl-A): moves cursor to beginning of line

      // Move cursor back until at start of line
      while (line_location > 0) {
        char back = 8;
        write(1, &back, 1);
        line_location--;
      }
    } // END OF HOME KEY (ctrl-A)
    else if (ch == 5) { // END key (ctr-E): move curose to end

      // Move cursor forward while not at end of line
      while (line_location < line_length) {

        // send ANSI escape sequence (ESC [ C) to move right
        char esc = 27;
        write(1, &esc, 1);

        char bracket = 91;
        write(1, &bracket, 1);

        char forward = 67;
        write(1, &forward, 1);
        line_location++;
      }
    }

    else if (ch==27) {
      // Escape sequence. Read two chars more
      //
      // see the ascii code for the different chars typed.
      //
      char ch1;
      char ch2;
      read(0, &ch1, 1);
      read(0, &ch2, 1);
      if (ch1==91 && ch2==65) {
        // Up arrow. Print next line in history.

        // Make sure history_length is positive
        if (history_length > 0) {

          // Erase old line
          // Print backspaces
          int i = 0;
          for (i =0; i < line_location; i++) {
            ch = 8;
            write(1,&ch,1);
          }

          // Print spaces on top
          for (i =0; i < line_length; i++) {
            ch = ' ';
            write(1,&ch,1);
          }

          // Print backspaces
          for (i =0; i < line_length; i++) {
            ch = 8;
            write(1,&ch,1);
          }

          // Navigate history and copy line
          if (history_index > 0) {
            history_index--;
          } else {
            //history_index = history_length - 1;
            history_index = 0;
          }

          // Load and print the command in *histroy[]
          // Checks so my stupid code does not seg fault
          if (history_index >= 0 && history_index < history_length && history[history_index] != NULL) {

            // Coies command from *history into line_buffer
            strcpy(line_buffer, history[history_index]);

            // Adjust the line_length and location
            line_length = strlen(line_buffer);
            line_location = line_length; // Set cursor to end

            // Display the history command
            write(1, line_buffer, line_length);
          }
          else { // No histroy --> clear the line
            line_length = 0;
            line_location = 0;
          }
        }
      } // END OF UP ARROW 
      else if (ch1 == 91 && ch2 == 68) { // LEFT ARROW

        // Check to see if current location is not at beginning
        if (line_location > 0) {
          ch = 8;
          write(1, &ch, 1);
          line_location--;
        }
      } // END OF LEFT ARROW
      else if (ch1 == 91 && ch2 == 67) { // RIGHT ARROW

        // Check to make sure not at end of line
        if (line_location < line_length) {

          // Moves the cursor to the right
          ch = 27;
          write(1, &ch, 1);

          ch = 91;
          write(1, &ch, 1);

          ch = 67;
          write(1, &ch, 1);
          line_location++;
        }
      } // END OF RIGHT ARROW
      else if (ch1 == 91 && ch2 ==66) { // DOWN ARROW
        // Eras old lines
        // Print backspaces


        // Check and make sure the history_length is positive
        if (history_length > 0) {
          int i = 0;
          for (i = 0; i < line_length; i++) {
            ch = 8;
            write(1, &ch, 1);
          }

          // Print spaces on top
          for (i = 0; i < line_length; i++) {
            ch = ' ';
            write(1, &ch, 1);
          }

          // Print backspaces
          for (i = 0; i < line_length; i++) {
            ch = 8;
            write(1, &ch, 1);
          }

          // Navigate to next history entry or empty line
          history_index++;

          // Check to see if at end of the history entires
          if (history_index >= history_length) {

            // Past the end of history, clear the line
            line_buffer[0] = '\0';
            line_length = 0;
            line_location = 0;
          } 
          else if (history_index >= 0 && history_index < history_length
                   && history[history_index] != NULL) { // Valid history entry exists

            // Copy history entry into line_buffer
            strcpy(line_buffer, history[history_index]);

            // Adjust lenght of line and cursor position
            line_length = strlen(line_buffer);
            line_location = line_length;

            // Display the history command
            write(1, line_buffer, line_length);

          } // END DOWN ARROW
        }
      }
    }
  }

  // Add eol and null char at the end of string
  line_buffer[line_length]=10;
  line_length++;
  line_buffer[line_length]=0;

  // If user enters something more than pressing enter, store it
  if (line_length > 1) {

    // Allocate memory for new history entry
    // Plus 1 for the null terminator
    history[history_length] = (char*)malloc(strlen(line_buffer) + 1);

    // If the malloc works (it fucking better)
    if (history[history_length]) {

      // Copy the command into the history[]
      strcpy(history[history_length], line_buffer);

      // Remove the newline character
      history[history_length][strlen(line_buffer) - 1] = '\0';

      // Increase the history size since new command is stored
      history_length++;
      history_index = history_length;

      // Restore th terminal to its origincal state
      tty_restore_mode();
    }
  }

  return line_buffer;
}



