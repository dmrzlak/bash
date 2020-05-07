#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAX_LINE 80

//used for the last command
char history[1][MAX_LINE + 1];
int historicalCount = 0;


//used for debugging and just to show the last command in general
void displayHistory(){
  int j = 0;
  //print each command as NUMBER     arg[0] ... arg[n]
  printf("\t");
  while(history[0][j] != '\n' && history[0][j] != '\0'){
    printf("%c", history[0][j]);
    j++;
  }
  printf("\n");
}


//Take the whole buffer string, whatever it may be, and parse it into arguments delimitted by spaces, and tabs
//also if we hit any special flags (<, >, |, &), signify them by marking their respective int
void writeArgsFromBuffer(char buff[], char *args[], int *flag, int *IO, int *pipe, int *processSplit){
  int length = strlen(buff); //# of chars in a command
  int i; //indexing for loop
  int start; //index for the beginning of the next command
  int count = 0; //index of where to put the next string
  int hist;
  i = 0;
  start = -1;
  for(i = 0; i < length; i++){
    //printf("\n\tDebug: %s\n", debug);
    switch(buff[i]){
    case ' ':
    case '\t':
      if(start != -1){
	args[count] = &buff[start];
	count++;
      }
      buff[i] = '\0';
      start = -1;
      break;
    case '\n':
      if(start != -1){
	args[count] = &buff[start];
	count++;
      }
      buff[i] = '\0';
      args[count] = NULL;
      break;
    case '&':
      *flag = 1;
      buff[i] = '\0';
      break;
    case '<':
      *IO = -1;
      buff[i] = '\0';
      *processSplit = count;
      break;
    case '>':
      *IO = 1;
      buff[i] = '\0';
      *processSplit = count;
      break;
    case '|':
      *pipe = 1;
      *processSplit = count;
      buff[i] = '\0';
      break;
    default:
      if(start == -1){
	start = i;
      }
    }
  }
  args[count] = NULL;
}


//Read the incoming line, then make 2 copies for it.
//1 for the history, and 1 for the args parsing
//if we hit !!, make a copy of the history buff and parse that instead
int formatCommand(char *args[], int *flag,
		  int *IO, int *pipe, int *processSplit){
  // if(historicalCount > 0)
  // displayHistory();
  int length; //# of chars in a command
  int i; //indexing for loop
  int hist;
  char buff[MAX_LINE];
  
  *flag = 0 ;
  *IO = 0;
  *pipe = 0;
  *processSplit = 0 ;
  
  length = read(STDIN_FILENO, buff, MAX_LINE);
  if(length <= 0){
    printf("Could not read command\n");
    return -1;
  }

  char historyCopy[strlen(history[0])];
  char argsCopy[length];
  char toHistoryCopy[length];
  
  if(buff[0] == '!' && buff[1] == '!'){
    displayHistory();    
    i=0;
    strcpy(historyCopy, history[0]);
    writeArgsFromBuffer(historyCopy, args, flag, IO, pipe, processSplit);
    return 0;
  } 
  else{
    strcpy(argsCopy, buff);
    strcpy(toHistoryCopy, buff);
    writeArgsFromBuffer(argsCopy, args, flag, IO, pipe, processSplit);
    if(args[0] == NULL){
      //User entered no command, return 1 to technically signify a success, but not a useful one
      return 0;
    }
    if (strcmp(args[0], "history") == 0){
      printf("Showing last command\n");
      displayHistory();
      return -1;
    }
    strcpy(history[0], toHistoryCopy);
    historicalCount++;
    if(historicalCount > 1){
      historicalCount = 1;
    } 
  }
  return 0;
}

int main() {
  char *args[MAX_LINE/2 + 1];
  char *argsTemp[MAX_LINE/2 + 1];
  int shouldRun = 1;

  pid_t pid, pipe_pid;
  int i;
  int fileDesc;
  int fd[2];

  while(shouldRun){
    int flag = 0 ;
    int IO = 0;
    int havePipe = 0;
    int processSplit = 0 ;
    printf("osh>");
    fflush(stdout);
    //read user in
    if(0 <= formatCommand(args, &flag, &IO, &havePipe, &processSplit)){
      if(args[0] == NULL){
	//User entered no command
	continue;
      }
      //read for stop
      if(strcmp(args[0], "exit") == 0){
	shouldRun = 0;
      }
      else{
	//fork to make the child process in the background
	pid = fork();
	if(pid < 0){
	  printf("Failed to fork, aborting\n");
	  return(-1);
	}
	else if(pid == 0){
	  //in the child process
	  if(IO == 1){
	    //output
	    //running under the assumption that the second arg is the file
	    i = processSplit;
	    //args[i] will be the respective file, and we need an fd for it, so open the file
	    fileDesc = open(args[i], O_RDWR | O_CREAT | O_CLOEXEC, S_IRWXU | S_IRWXO);
	    //dup2 to bind the file and sys out
	    dup2(fileDesc, STDOUT_FILENO);

	    //any args after the file are useless, so I set them to null to help execvp
	    while(args[i] != NULL){
	      args[i] = NULL;
	      i++;
	    }
	  }
	  if(IO == -1){
	    //input
	    //same process for input, just the other way, so bind sys in to the file
	    i = processSplit;
	    fileDesc = open(args[i], O_RDWR | O_CLOEXEC);
	    dup2(fileDesc, STDIN_FILENO);
	    while(args[i] != NULL){
	      args[i] = NULL;
	      i++;
	    }
	  }
	  if(havePipe){
	    //we read a |, so make a pipe
	    if(pipe(fd) == -1){
	      printf("Creation of the pipe failed\n");
	      return(-1);
	    }
	    //fork for the piped process
	    pipe_pid = fork();
	    //only want respective args for each process, so copy the ones after | and kill them
	    i = processSplit;
	    while(args[i] != NULL){
	      argsTemp[i-processSplit] = args[i];
	      args[i] = NULL;
	      i++;
	    }
	    argsTemp[i-processSplit] = NULL;
	    
	    if(pipe_pid < 0){
	      printf("Failed to fork, aborting\n");
	      return(-1);
	    }
	    if(pipe_pid == 0){
	      //in the grandchild process
	      //we want to read, and not write at all, so close the pipe's write end
	      close(fd[1]);
	      dup2(fd[0], STDIN_FILENO);
	      i = 0;
	      //copy our temp args back over, we need them in the right array for execvp
	      while(argsTemp[i] != NULL){
		args[i] = argsTemp[i];
		i++;
	      }
	      while(args[i] != NULL){
		args[i] = NULL;
		i++;
	      }
	    }
	    else{
	      //We are in the base child process, so close the read end
	      close(fd[0]);
	      dup2(fd[1], STDOUT_FILENO);
	    }
	  }
	  //child process invokes execvp()
	  if(execvp(args[0], args) == -1){
	    //if we've gotten -1, the command was faulty, so let the user know
	    printf("Failed to complete command\n");
	  }
	  exit(0);
	}
	else{
	  //parent will invoke wait unless command includes &
	  //flag == 0, for this case
	  if(flag == 0){
	    wait(NULL);
	  } 
	}
      }
    }
  }
  return 0;
}
