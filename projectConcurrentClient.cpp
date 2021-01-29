#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>
#include <netdb.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sqlite3.h>


/* portul de conectare la server*/
int port;
fd_set serverSet; //The fd sets we will use for checking user input and server communication
fd_set inputSet;
fd_set sendSpeedSet;
struct timeval tv;
int nfds; //numarul maxim de descriptori
int sendSpeedPid; //child pid used to terminate it if disconnect
char DBdata[100]; //DB buffer

static int callback_function(void* unused, int count, char** data, char** columns){
  bzero(DBdata,100);
  strcpy(DBdata,data[0]);
  return 0;
}

int handleServer(int fd){
  //Tratare input server
  char msg[1000];
  bzero(msg,1000); //Clear the buffer
  ssize_t len = read(fd,msg,1000);
   if(len == -1){
    printf("[client]Error at read from server.\n");
    fflush(stdout);
    return -1;
  }
  msg[len] = '\0';

  //Print the message from server
  printf("%s\n",msg);
  fflush(stdout);

  return 1;
}

int handleUser(int fd,int sd, sqlite3* DB, char* randomStreet){
  //Tratare input user
  char msg[1000];
  bzero(msg,1000); //Clear the buffer
  ssize_t len = read(fd,msg,1000);
   if(len == -1){
    printf("[client]Error at read from user.\n");
    fflush(stdout);
    return -1;
  }
  msg[len] = '\0';

  //If Da, add the randomStreet
  if(strncmp(msg,"Da",2) == 0){
    strcat(msg,": ");
    strcat(msg, randomStreet);
  }

  //Send the message from user to the server
  if(write(sd,msg,1000) < 0){
    printf("[client]Error at sending the message from user.\n");
    fflush(stdout);
    return -1;
  }

  if(strncmp(msg,"Disconnect",10) == 0){
      /* inchidem conexiunea, am terminat */
      //Kill the sendSpeed child if it is running
      if(FD_ISSET(sd,&sendSpeedSet) == 0){
        kill(sendSpeedPid,SIGKILL);
      }
      close (sd);
      sqlite3_close(DB);
      exit(0);
  }

  return 1;
}

int sendSpeedFunction(int fd, int randSpeed){

  sleep(60);
  char randSpeedBuff[5];
  sprintf(randSpeedBuff, "%d", randSpeed);
  char buffer[100] = "SendSpeed: ";
  strcat(buffer,randSpeedBuff);
  //Send the message to the server
  if(write(fd,buffer,1000) < 0){
    printf("[client]Error at SendSpeed.\n");
    fflush(stdout);
    return -1;
  }

  return 1;
}

int main (int argc, char *argv[])
{
  srand(time(NULL)); //For random function
  sqlite3* DB; //Database stuff
  int dbStatus = 0;
  int sd;			// descriptorul de socket
  struct sockaddr_in server;	// structura folosita pentru conectare 
  char msg[1000];		// mesajul trimis
  int wstatus = -1; //used for waitpid
  char randomStreet[20]; // Random street initialized for each particular client
  int randSpeed; //Used for random speed generation for every client

  //Open our database
  dbStatus = sqlite3_open("projectDatabase.db", &DB);
  
  if(dbStatus){
    printf("[client]Error openning Database.\n");
    fflush(stdout);
    return -1;
  }
    
  printf("[client]Database opened succesfully.\n");
  fflush(stdout);

  //Generate a random ID between 1 and 4 to select a random Street from database, according to ID;
  int dbID = (rand()%4) + 1;
  char dbIDbuff[10];
  sprintf(dbIDbuff, "%d", dbID);
  char query[500] = "SELECT Street FROM StreetSpeed WHERE ID = "; //Database query
  strcat(query, dbIDbuff);
  strcat(query, ";");

  dbStatus = sqlite3_exec(DB,query,callback_function,NULL,NULL);

  if (dbStatus != SQLITE_OK){
    printf("[client]Database exec error.\n");
    fflush(stdout);
    return -1;
  }

  //Store the random generated street from the Database in the randomStreet buffer
  strcpy(randomStreet,DBdata);

  /* exista toate argumentele in linia de comanda? */
  if (argc != 3)
    {
      printf ("[client]Sintaxa: %s <adresa_server> <port>\n", argv[0]);
      return -1;
    }

  /* stabilim portul */
  port = atoi (argv[2]);

  /* cream socketul */
  if ((sd = socket (AF_INET, SOCK_STREAM, 0)) == -1)
    {
      perror ("[client]Eroare la socket().\n");
      return -1;
    }
  

  /* umplem structura folosita pentru realizarea conexiunii cu serverul */
  /* familia socket-ului */
  server.sin_family = AF_INET;
  /* adresa IP a serverului */
  server.sin_addr.s_addr = inet_addr(argv[1]);
  /* portul de conectare */
  server.sin_port = htons (port);
  
  /* ne conectam la server */
  if (connect (sd, (struct sockaddr *) &server,sizeof (struct sockaddr)) == -1)
    {
      perror ("[client]Eroare la connect().\n");
      return -1;
    }

  //Pregatire fd_sets
  FD_ZERO(&serverSet);
  FD_ZERO(&inputSet);
  FD_ZERO(&sendSpeedSet);
  FD_SET(sd,&sendSpeedSet); //Set the server descriptor in sendSpeed set
  nfds = sd;

  tv.tv_sec = 1; //Asteptare 1 sec la select
  tv.tv_usec = 0;


  while (1)
  {
      //Generate randomSpeed for the client to send to the Server
      randSpeed = (rand()%110) + 30; //generate random speed from 30 to 140 km/h

      //Ajustam descriptorii la fiecare iteratie While
      FD_SET(sd, &serverSet);
      if(sd > nfds) nfds = sd;
      FD_SET(fileno(stdin), &inputSet);
      if(fileno(stdin) > nfds) nfds = fileno(stdin);

      //Select to check input from user
      if(select(nfds+1, &inputSet, NULL, NULL, &tv) < 0){
         printf("[client]Select() error.\n");
         fflush (stdout);
         return -1;
       }

       //Select to check input from server
       if(select(nfds+1, &serverSet, NULL, NULL, &tv) < 0){
         printf("[client]Select() error.\n");
         fflush (stdout);
         return -1;
       }

       //Tratam input user
       if(FD_ISSET(fileno(stdin),&inputSet)){
           //Handle the user
            if(handleUser(fileno(stdin),sd,DB,randomStreet) == -1) {
                    printf("[client]Eroare handle input.\n");
                    fflush(stdout);
                    return -1;
            }
       }

        //Tratam input server
        if(FD_ISSET(sd,&serverSet)){
            //Handle the server
            if(handleServer(sd) == -1) {
                    printf("[client]Eroare handle server.\n");
                    fflush(stdout);
                    return -1;
            }
        }

        //SendSpeed at a certain interval
        if(FD_ISSET(sd,&sendSpeedSet)){
            FD_CLR(sd,&sendSpeedSet);
            int pid = fork();
            sendSpeedPid = pid;
            if(pid == 0){
                //Child
                if(sendSpeedFunction(sd,randSpeed) == -1){
                        printf("[client]Eroare SendSpeed.\n");
                        fflush(stdout);
                        return -1;
                    }
                exit(sd);    
            }
        }

        //Check if sendSpeed function terminated
        waitpid(-1, &wstatus, WNOHANG);
        if(WIFEXITED(wstatus)){
            FD_SET(sd,&sendSpeedSet); //server available to send speed
            wstatus = -1; //reset wstatus
        }
  
  
  }
  
}