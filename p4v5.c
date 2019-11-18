#include <stdlib.h>
#include <stdio.h>
#include <termios.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>
#include <sys/ioctl.h>

void scroll_more(int);
char* fill_line(char*);
int see_more(FILE*);
char* read_and_return(FILE*);
int set_ticker( int );
void catchINT();

// buffer stores the contents of the input file
char* buffer;

// for changing terminal modes
struct termios orig_info, info;



// terminal window sizes
int ROWS;
int COLS;
int PAGELEN;

// num_of_lines tracks how many lines are waiting to print
int num_of_lines = 0;

int main(int ac, char *av[])
{
  // get window size
  struct winsize ws;
  ioctl(1, TIOCGWINSZ, &ws);
  ROWS = ws.ws_row;
  COLS = ws.ws_col;

  // open connection to keyboard file
  FILE *fp_tty;
  fp_tty = fopen("/dev/tty", "r");
  if ( fp_tty == NULL )               // if open fails  
    exit(1);                          // no use in running

  // taking our "more?" prompt into consideration,
  // the most lines we will ever need to print are ROWS-1
  PAGELEN = ROWS-1;
  
  // open the file
  FILE *fp;
  fp = fopen(av[1], "r");
  if (fp == 0){
    fprintf(stderr,"There was an error opening the file.");
    exit(1);
  }
  // store the contents of the file in local memory
  buffer = read_and_return(fp);
  
  fclose(fp);
    
  // set terminal attibutes to turn off echo and canonical
  tcgetattr(0,&orig_info);   // get attribs (save original values) 
  tcgetattr(0,&info);         // get attribs  
  info.c_lflag &= ~ECHO ;      // turn off echo bit 
  info.c_lflag &= ~ICANON ;     // turn off buffering
  info.c_cc[VMIN] = 1;           // get 1 char at a time
  tcsetattr(0,TCSANOW,&info);     // set attribs 
  

  // ***** the actual meat and potatoes of the loop begin here ***** //
  char* output_line;
  
  int reply;
  int done = 0;
  int scrolling = 0;
  int delay = 2000;

  // install handlers
  signal(SIGALRM, scroll_more);
  signal(SIGINT, catchINT);

  // print first screenfull
  num_of_lines = PAGELEN;
  while (num_of_lines > 0 && buffer != '\0'){
    output_line = fill_line(buffer);
    printf("%s", output_line);
    num_of_lines--;
  }

  while(!done){
    reply = see_more(fp_tty);
    // q = quit
    if (reply == 0) {
      done = 1;
      break;
    }
    // space = output next screenful
    if (reply == PAGELEN && *buffer != '\0'){
      scrolling = 0;
      set_ticker(0);
      num_of_lines = PAGELEN;
      while (num_of_lines > 0 && *buffer != '\0'){
	output_line = fill_line(buffer);
	printf("%s", output_line);
	num_of_lines--;
      }
    }
    // Enter = scroll
    // default delay is 2000, can be changed by pressing f or s
    if (reply == 1){
      if (scrolling == 0){
	set_ticker(delay);
	scrolling = 1;
      }else if (scrolling == 1){
	set_ticker(0);
	scrolling = 0;
      }
    }
    // f = faster
    if (reply == 10){
      delay = delay * 0.8;
      if (scrolling == 1)
	set_ticker(delay);
    }
    // s = slower
    if (reply == 20){
      delay *= 1.2;
      if (scrolling == 1)
	set_ticker(delay);
    }
    
  }

  tcsetattr(0,TCSANOW,&orig_info);     // set attribs back to original
  printf("\b\b\b\b\b\b\b       \b\b\b\b\b\b\b");
  return 0;
}

// Auto-scroll feature which is activated by pressing "Enter"
void scroll_more(int signum){
  char* output_line;
  num_of_lines++;
  while (num_of_lines > 0 && *buffer != '\0'){
    printf("\b\b\b\b\b\b\b       \b\b\b\b\b\b\b");	
    output_line = fill_line(buffer);
    printf("%s", output_line);
    num_of_lines--;
    printf("\033[7m more? \033[m");
    fflush(stdout);
    
  }
}

// this function reads the FILE into internal memory as one long string
char* read_and_return(FILE *fp){
  char *source = NULL;
  if (fp != NULL) {
    // Get size of the file
    if (fseek(fp, 0L, SEEK_END) == 0) {
      long bufsize = ftell(fp);
      if (bufsize == -1) { /* Error */ }

      // Allocate memory with malloc
      source = malloc(sizeof(char) * (bufsize + 1));

      // Reset file pointer
      if (fseek(fp, 0L, SEEK_SET) != 0) { /* Error */ }

      // Read file into memory
      size_t newLen = fread(source, sizeof(char), bufsize, fp);
      if ( ferror( fp ) != 0 ) {
	fputs("Error reading file", stderr);
      } else {
	source[newLen++] = '\0'; // Safety first
      }
    }
  }
  return source;
}


/* takes a pointer to a string, and copies up to COLS chars to that string, 
   where COLS is the width of the console window. 
   If a '\n' is encountered, or we have COLS chars, return the string.
 */

char* fill_line(char* input){

  int i = 0; // iterative variable
  int n = COLS; // console width
  char* newline = malloc(n*sizeof(char));

  // save the initial memory location of newline in
  //                                 "newline_start"
  char* newline_start = newline; 
  
  while(i < COLS && *buffer != '\0'){
    if(*input == '\n'){
      //copy the char, return the string
      *newline++ = *input++;
      newline = newline_start;
      buffer += ++i;
      return newline;

    }else{
      //copy the char
      *newline++ = *input++;
      i++;
    }
  }
  *newline = '\n';
  buffer += i;
  newline = newline_start;
  
  return newline;
  
}

int see_more(FILE *cmd)        
{
 
  
  int   c;
  if (*buffer != '\0')
    printf("\033[7m more? \033[m");  // reverse on a vt100
  else printf("\033[7m done! \033[m");
  while( (c=getc(cmd)) != EOF )    // NEW: reads from tty 
    {
      if ( c == 'f' ){
	printf("\b\b\b\b\b\b\b       \b\b\b\b\b\b\b");
	return 10;
      }
      if ( c == 's' ){
	printf("\b\b\b\b\b\b\b       \b\b\b\b\b\b\b");
	return 20;
      }
      if ( c == 'q' ){               
	printf("\b\b\b\b\b\b\b       \b\b\b\b\b\b\b");	
	return 0;
      }
      if ( c == ' ' ){              
	printf("\b\b\b\b\b\b\b       \b\b\b\b\b\b\b");	
	return PAGELEN;
      }           
      if ( c == '\n' ){
	printf("\b\b\b\b\b\b\b       \b\b\b\b\b\b\b");
	return 1;      
      }

  }
  
  return 0;
}

/* set_ticker( number_of_milliseconds )
 * arranges for interval timer to issur SIGALRM;s at regular intervals
 * returns -1 on error, 0 for ok
 * arg in milliseconds,converted into whole seconds and microseconds
 * note:set_ticker(0) turns off ticker
 */

int set_ticker( int n_msecs ){
    struct itimerval new_timeset;
    long n_sec, n_usecs;
    
    n_sec = n_msecs/1000;
    n_usecs = ( n_msecs % 1000) * 1000L;

    new_timeset.it_interval.tv_sec = n_sec;
    new_timeset.it_interval.tv_usec = n_usecs;

    new_timeset.it_value.tv_sec = n_sec;
    new_timeset.it_value.tv_usec = n_usecs;

    return setitimer(ITIMER_REAL, &new_timeset, NULL);
}

void catchINT(){
    tcsetattr(0, TCSANOW, &orig_info);
    exit(0);
}
