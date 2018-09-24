/*-------------------------------------------------------------------------*
 *---									---*
 *---		mathServer.c						---*
 *---									---*
 *---	    This file defines a C program that gets file-sys commands	---*
 *---	from client via a socket, executes those commands in their own	---*
 *---	threads, and returns the corresponding output back to the	---*
 *---	client.								---*
 *---									---*
 *---	----	----	----	----	----	----	----	----	---*
 *---									---*
 *---	Version 1.0		2018 August 9		Joseph Phillips	---*
 *---									---*
 *-------------------------------------------------------------------------*/

//	Compile with:
//	$ gcc mathServer.c -o mathServer -lpthread

//---		Header file inclusion					---//

#include	"mathClientServer.h"
#include	<errno.h>	// For perror()
#include	<pthread.h>	// For pthread_create()
#include	<dirent.h>


//---		Definition of constants:				---//

#define		STD_OKAY_MSG		"Okay"

#define		STD_ERROR_MSG		"Error doing operation"

#define		STD_BYE_MSG		"Good bye!"

#define		THIS_PROGRAM_NAME	"mathServer"

#define		FILENAME_EXTENSION	".bc"

#define		OUTPUT_FILENAME		"out.txt"

#define		ERROR_FILENAME		"err.txt"

#define		CALC_PROGNAME		"/usr/bin/bc"

const int	ERROR_FD		= -1;


//---		Definition of functions:				---//

char* openDirectory(){
  char* buffer = malloc(sizeof(char)*(BUFFER_LEN+1));
  buffer[0] = '\0';
  
  struct dirent* dirinfo;
  DIR* dir = opendir(".");
  if(dir == NULL)
	return(NULL);
	
  
  char* temp = malloc(sizeof(char)*(BUFFER_LEN+1));
  while((dirinfo = readdir(dir)) != NULL){
	  strcpy(temp,dirinfo->d_name);
	  if(strlen(buffer) + strlen(temp) + 1 < BUFFER_LEN){
	    strcat(buffer, temp);
	    strcat(buffer, "\n");
	  }
  }
  free(temp);
  closedir(dir);
  return(buffer);
}

char* readFile(int fileNum){
	char* buffer = malloc(sizeof(char)*(BUFFER_LEN+1));
	buffer[0] = '\0';
	char	fileName[BUFFER_LEN];

    snprintf(fileName,BUFFER_LEN,"%d%s",fileNum,FILENAME_EXTENSION);

    FILE* file = fopen(fileName,"r");
    if(file == NULL){
		return NULL;
	}
	
	fgets(buffer,BUFFER_LEN,file);
	fclose(file);
	
	return(buffer);
}

int writeFile(int fileNum, char* text){
	int response;
	char	fileName[BUFFER_LEN];
	
	snprintf(fileName,BUFFER_LEN,"%d%s",fileNum,FILENAME_EXTENSION);
	
	FILE* file = fopen(fileName,"w+");
    if(file == NULL){
		response = 0;
		return response;
	}
	
	if(fprintf(file, "%s\nquit", text) != strlen(text)){
		response = 0;
	}else{
		response = 1;
	}
	
	fclose(file);
	return response;
}

int deleteFile(int fileNum){
	char fileName[BUFFER_LEN];
	
	snprintf(fileName,BUFFER_LEN,"%d%s",fileNum,FILENAME_EXTENSION);
	
	if(unlink(fileName) == 0){
		return 1;
	}else{
		return 0;
	}
}

char* calcFile(int fileNum){
  pid_t calcPid = fork();
  char* buffer = malloc(sizeof(char)*(BUFFER_LEN+1));
  buffer[0] = '\0';

  if  (calcPid == -1)
  {
    fprintf(stderr,"fork() error\n");
    exit(EXIT_FAILURE);
  }

  if  (calcPid == 0)
  {
	char	fileName[BUFFER_LEN];

    snprintf(fileName,BUFFER_LEN,"%d%s",fileNum,FILENAME_EXTENSION);

    int	inFd	= open(fileName,O_RDONLY,0);
    int	outFd	= open(OUTPUT_FILENAME,O_WRONLY|O_CREAT|O_TRUNC,0660);
    int	errFd	= open(ERROR_FILENAME, O_WRONLY|O_CREAT|O_TRUNC,0660);

    if  ( (inFd < 0) || (outFd < 0) || (errFd < 0) )
    {
      fprintf(stderr,"Could not open one or more files\n");
      exit(EXIT_FAILURE);
    }

    close(0);
    dup(inFd);
    close(1);
    dup(outFd);
    close(2);
    dup(errFd);
    execl(CALC_PROGNAME, CALC_PROGNAME, fileName, ">>", OUTPUT_FILENAME, NULL);
    fprintf(stderr,"Could not execl %s\n",CALC_PROGNAME);
    exit(EXIT_FAILURE);
  }
  int status;
  wait(&status);
  if(WIFEXITED(status)){
	  switch(WEXITSTATUS(status)){
		  case EXIT_SUCCESS:
			;
		    int fd = open(OUTPUT_FILENAME, O_RDONLY, 0);
		    int errfd = open(ERROR_FILENAME, O_RDONLY, 0);
		    if  (fd < 0 || errfd < 0){
			  fprintf(stderr,"Could not open one or more files\n");
			  exit(EXIT_FAILURE);
			}
			read(fd, buffer, BUFFER_LEN);
			read(errfd, buffer, BUFFER_LEN-strlen(buffer));
			close(fd);
			close(errfd);
			unlink(OUTPUT_FILENAME);
			unlink(ERROR_FILENAME);
			break;
		  case EXIT_FAILURE:
		    free(buffer);
		    return(NULL);
			break;
	  }
	  return(buffer);
  }
  free(buffer);
  return(NULL);
  
}

void* handleClient(void* vPtr){
	//  II.B.  Read command:
  char	buffer[BUFFER_LEN];
  char	command;
  int	fileNum;
  char	text[BUFFER_LEN];
  int 	shouldContinue	= 1;
  
  int* nPtr = (int *) vPtr;
  int threadNum = nPtr[0];
  int socketFd = nPtr[1];

  while  (shouldContinue)
  {
	printf("Beginning of while loop\n");
    text[0]	= '\0';
    read(socketFd,buffer,BUFFER_LEN);
    printf("Thread %d received: %s\n",threadNum,buffer);
    int num_params = 1;
    int spaces_found = 0;
    int inQuote = 0;
    
    for(int i=0; i < strlen(buffer); i++){
		if(buffer[i] == ' ' && !inQuote){
			printf("Space found! %d\n", i);
			num_params++;
			spaces_found++;
		}else if(buffer[i] == '"'){
			inQuote = !inQuote;
		}
	}
	
	if(num_params == 1){
		sscanf(buffer, "%c", &command);
		printf("%c\n", command);
	}else if(num_params == 2){
		sscanf(buffer, "%c %d", &command, &fileNum);
		printf("%c\n", command);
		printf("%d\n", fileNum);
	}else if(num_params == 3){
		sscanf(buffer,"%c %d \"%[^\"]\"",&command,&fileNum,text);
		printf("%c\n", command);
		printf("%d\n", fileNum);
		printf("%s\n", text);
	}else{
		write(socketFd, STD_ERROR_MSG, sizeof(STD_ERROR_MSG));
		command = '\0';
		break;
	}
	
	
	char* response;
	int res;
	switch(command){
		case DIR_CMD_CHAR:
			;
			response = openDirectory();
			if(response != NULL)
				write(socketFd, response, sizeof(char)*(strlen(response)+1));
			else
				write(socketFd, STD_ERROR_MSG, sizeof(STD_ERROR_MSG));
			free(response);
			break;
		case READ_CMD_CHAR:
			;
			response = readFile(fileNum);
			if(response != NULL)
				write(socketFd, response, sizeof(char)*(strlen(response)+1));
			else
				write(socketFd, STD_ERROR_MSG, sizeof(STD_ERROR_MSG));
			free(response);
			break;
		case WRITE_CMD_CHAR:
			;
			res = writeFile(fileNum, text);
			if(res)
				write(socketFd, STD_OKAY_MSG, sizeof(STD_OKAY_MSG));
			else
				write(socketFd, STD_ERROR_MSG, sizeof(STD_ERROR_MSG));
			break;
		case DELETE_CMD_CHAR:
			;
			res = deleteFile(fileNum);
			if(res)
				write(socketFd, STD_OKAY_MSG, sizeof(STD_OKAY_MSG));
			else
				write(socketFd, STD_ERROR_MSG, sizeof(STD_ERROR_MSG));
			break;
		case CALC_CMD_CHAR:
			;
			response = calcFile(fileNum);
			if(response != NULL)
				write(socketFd, response, sizeof(char)*(strlen(response)+1));
			else
				write(socketFd, STD_ERROR_MSG, sizeof(STD_ERROR_MSG));
			free(response);
			break;
		case QUIT_CMD_CHAR:
			write(socketFd, STD_BYE_MSG, sizeof(STD_BYE_MSG));
			shouldContinue = 0;
			break;
	}
  }
  printf("Thread %d quitting.\n",threadNum);
  return(NULL);
}

//  PURPOSE:  To run the server by 'accept()'-ing client requests from
//	'listenFd' and doing them.
void		doServer	(int		listenFd
				)
{
  //  I.  Application validity check:

  //  II.  Server clients:
  pthread_t		threadId;
  pthread_attr_t	threadAttr;
  int threadCount = 0;
  struct sockaddr_in address;
  
  pthread_attr_init(&threadAttr);
  pthread_attr_setdetachstate(&threadAttr, PTHREAD_CREATE_DETACHED);
  
  int addrlen = sizeof(address);
  int socketFd;
  int info[2];
  pthread_t thread;
  while((socketFd = accept(listenFd, &address, &addrlen)) > 0){
	  info[0] = threadId;
	  info[1] = socketFd;
	  if((thread = pthread_create(&threadId, &threadAttr, handleClient, info)) == 0){
		threadCount += 1;
	  }else{
		printf("Bad thread\n");
	  }
	  printf("Thread count: %d\n",threadCount);
  }
  printf("Done listening for socket. Destroying thread.\n");
  pthread_attr_destroy(&threadAttr);
  
}




//  PURPOSE:  To decide a port number, either from the command line arguments
//	'argc' and 'argv[]', or by asking the user.  Returns port number.
int		getPortNum	(int	argc,
				 char*	argv[]
				)
{
  //  I.  Application validity check:

  //  II.  Get listening socket:
  int	portNum;

  if  (argc >= 2)
    portNum	= strtol(argv[1],NULL,0);
  else
  {
    char	buffer[BUFFER_LEN];

    printf("Port number to monopolize? ");
    fgets(buffer,BUFFER_LEN,stdin);
    portNum	= strtol(buffer,NULL,0);
  }

  //  III.  Finished:  
  return(portNum);
}


//  PURPOSE:  To attempt to create and return a file-descriptor for listening
//	to the OS telling this server when a client process has connect()-ed
//	to 'port'.  Returns that file-descriptor, or 'ERROR_FD' on failure.
int		getServerFileDescriptor
				(int		port
				)
{
  //  I.  Application validity check:

  //  II.  Attempt to get socket file descriptor and bind it to 'port':
  //  II.A.  Create a socket
  int socketDescriptor = socket(AF_INET, // AF_INET domain
			        SOCK_STREAM, // Reliable TCP
			        0);

  if  (socketDescriptor < 0)
  {
    perror(THIS_PROGRAM_NAME);
    return(ERROR_FD);
  }

  //  II.B.  Attempt to bind 'socketDescriptor' to 'port':
  //  II.B.1.  We'll fill in this datastruct
  struct sockaddr_in socketInfo;

  //  II.B.2.  Fill socketInfo with 0's
  memset(&socketInfo,'\0',sizeof(socketInfo));

  //  II.B.3.  Use TCP/IP:
  socketInfo.sin_family = AF_INET;

  //  II.B.4.  Tell port in network endian with htons()
  socketInfo.sin_port = htons(port);

  //  II.B.5.  Allow machine to connect to this service
  socketInfo.sin_addr.s_addr = INADDR_ANY;

  //  II.B.6.  Try to bind socket with port and other specifications
  int status = bind(socketDescriptor, // from socket()
		    (struct sockaddr*)&socketInfo,
		    sizeof(socketInfo)
		   );

  if  (status < 0)
  {
    perror(THIS_PROGRAM_NAME);
    return(ERROR_FD);
  }

  //  II.B.6.  Set OS queue length:
  listen(socketDescriptor,5);

  //  III.  Finished:
  return(socketDescriptor);
}


int		main		(int	argc,
				 char*	argv[]
				)
{
  //  I.  Application validity check:

  //  II.  Do server:
  int 	      port	= getPortNum(argc,argv);
  int	      listenFd	= getServerFileDescriptor(port);
  int	      status	= EXIT_FAILURE;

  if  (listenFd >= 0)
  {
    doServer(listenFd);
    close(listenFd);
    status	= EXIT_SUCCESS;
  }

  //  III.  Finished:
  return(status);
}
