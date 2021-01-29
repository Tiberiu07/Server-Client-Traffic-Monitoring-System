#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <time.h>
#include <iostream>
#include <map>
#include <errno.h> 
#include <vector>
#include <sqlite3.h>

//Variabile globale
fd_set readfds; //Desciptorii citire
fd_set readactivefds;
fd_set writefds; //Descriptorii scriere
fd_set subscribedfds; //Descriptorii abonati servicii de informare(toti)
fd_set subscribedMSGfds; //Descriptorii abonati servicii de informare(la care se vor transmite mesaje)
fd_set writeMSGfds; //Descriptorii de scriere folositi pentru trimitere mesaje la intervale de timp
struct timeval tv;
int nfds; //Numarul maxim descriptori
std::map<int,int> getPidByFd; //HashMap ce stocheaza dinamic Pid-urile proceselor copil care ruleaza conform descriptorilor
std::map<int,int> getWritePidByFd; //Pentru writeMSG
char DBData[100]; //Database data
char DBDataEvent[100];

static int callback_function_event(void* unused, int count, char** data, char** columns){
    bzero(DBDataEvent,100);
    strcpy(DBDataEvent,data[0]);
    return 0;
}

static int callback_function(void* unused, int count, char** data, char** columns){
    bzero(DBData,100); //Clear the buffer
    strcpy(DBData,data[0]); //Copy in buffer the result
    return 0;
}

void terminateChildWrite(int fd){
    std::map<int,int>::iterator it = getWritePidByFd.find(fd);
    if(it == getWritePidByFd.end()) {
        printf("[server]Error at terminating child.\n");
        fflush(stdout);
        return;
    }
    else {
        int pid = getWritePidByFd[fd];
        getWritePidByFd.erase(fd);
        kill(pid, SIGKILL); //Terminate child process
    }
}

void terminateChild(int fd){
    //Get the pid of the child
    std::map<int,int>::iterator it = getPidByFd.find(fd);
    if(it == getPidByFd.end()) {
        printf("[server]Error at terminating child.\n");
        fflush(stdout);
        return;
    }
    else {
        int pid = getPidByFd[fd];
        kill(pid, SIGKILL); //Terminate child process
    }
}

//Get speed for each of the 4 streets.
int getSpeed(char address[], sqlite3* DB){
    char query[500] = "SELECT MaxSpeed FROM StreetSpeed WHERE Street = '";
    strcat(query,address);
    strcat(query,"';");
    int dbStatus = 0; //DB status

    dbStatus = sqlite3_exec(DB,query,callback_function,NULL,NULL);

    if(dbStatus != SQLITE_OK){
        printf("[server]Database exec error.\n");
        fflush(stdout);
        return -1;
    }

    int returnSpeed = atoi(DBData); //Convert the data to int value
    return returnSpeed; //return the speed
}

int sendMessageSubscribed(int client, int randomEventID, sqlite3* DB){
    sleep(25);
    char randomEventIDBuff[5];
    sprintf(randomEventIDBuff,"%d",randomEventID); //Convert int to char[]

    char buffer[100];
    char query[500] = "SELECT Data FROM ForSubscribed WHERE ID = ";
    strcat(query,randomEventIDBuff);
    strcat(query,";");

    int dbStatus = 0; //Used for error checking

    dbStatus = sqlite3_exec(DB,query,callback_function_event,NULL,NULL);

    if(dbStatus != SQLITE_OK){
        printf("[server]Database exec error.\n");
        fflush(stdout);
        return -1;
    }

    strcpy(buffer,DBDataEvent);

    if(write(client,buffer,100) == -1){
        return -1;
    }
    return 1;
}

int sendMessage(int client){
    sleep(30);
    char buffer[100] = "[server]Doriti informatii referitoare la viteza de circulatie maxima admisibila pe strada Dvs.?";
    if(write(client, buffer, 100) == -1){ 
        return -1;
    }
    return 1;
}

int handle(int client, sqlite3* DB){
    //Read the message from client
    char fromClient[10000];
    ssize_t len = read(client, fromClient, 1000);
    if (len == -1) { //client disconnect
        return -1;
    }
    fromClient[len] = '\0';

    if(strncmp(fromClient, "ReportCarCrash",14) == 0){
        char address[1000];
        strcpy(address, strchr(fromClient,':')); //Copy address
        memmove(address,address+1,strlen(address+1)+1); //Delete : at the beginning
        char message[1000];
        strcpy(message, "[server]Atentie, accident rutier pe strada");
        strcat(message, address);
        strcat(message, "Aveti grija de Dvs. si circulati cu prudenta.");
        //Send back to all the connected clients the info regarding this carcrash
        for(int fd = 0; fd <= nfds; ++fd){
            if(FD_ISSET(fd, &writefds)){
                if(write(fd, message, 1000) == -1) 
                return -1;
            }
        }
        return 1;
    }

    else if(strncmp(fromClient, "Subscribe",9) == 0){
        if(FD_ISSET(client, &subscribedfds)){
            if(write(client,"[server]You are already subscribed",strlen("[server]You are already subscribed")) == -1) return -1;
            return 1;
        }
        else
        {
            FD_SET(client, &subscribedfds);
            FD_SET(client, &subscribedMSGfds);
            if(write(client,"[server]You have been subscribed to our news/information channel", strlen("[server]You have been subscribed to our news/information channel")) == -1) return -1;
            return 1;
        }
        
    }

    else if(strncmp(fromClient,"Unsubscribe",11) == 0){
        if(FD_ISSET(client, &subscribedfds)){
            FD_CLR(client,&subscribedfds);
            if(FD_ISSET(client, &subscribedMSGfds)) FD_CLR(client, &subscribedMSGfds);
            else{
                //Daca nu e setat in subscribed MSG, dar e setat subscribed, inseamna ca ruleaza procesul copil
                //Trebuie distrus procesul copil, pentru a nu mai trimite informatii catre client
                terminateChild(client);
            }
            if(write(client,"[server]You have been unsubscribed from our news/information channel", strlen("[server]You have been unsubscribed from our news/information channel")) == -1) return -1;
            return 1;
        }
        else{
            if(write(client,"[server]You are not subscribed to our news/information channel",strlen("[server]You are not subscribed to our news/information channel")) == -1) return -1;
            return 1;
        }
    }

    else if(strncmp(fromClient,"Da",2) == 0){

        //Send info regarding maxSpeed
        char address[1000];
        strcpy(address,strchr(fromClient,':'));
        memmove(address,address+1,strlen(address+1)+1); //Delete : at the beginning
        if(address[0] == ' ') memmove(address,address+1,strlen(address+1)+1); //If first character space, delete it
        int speed = getSpeed(address,DB);
        char speedString[10];
        sprintf(speedString,"%d",speed);
        char msg[1000];
        strcpy(msg, "[server]Dvs. circulati pe strada ");
        strcat(msg, address);
        strcat(msg,"\n");
        strcat(msg, "Viteza admisibila de circulatie este de ");
        strcat(msg, speedString);
        strcat(msg, " km/h.");
        if(write(client, msg, strlen(msg)) == -1) return -1;
        return 1;
    }

    else if(strncmp(fromClient,"SendSpeed",9) == 0){
        char msg[100];
        char speed[4];
        strcpy(speed,strchr(fromClient,':'));
        memmove(speed,speed+1,strlen(speed+1)+1); //Delete the first :
        if(speed[0] == ' ') memmove(speed,speed+1,strlen(speed+1)+1); //Delete the first space if it`s the case
        speed[strlen(speed)] = ' '; //Replace null terminating char
        char clientDescriptor[5];
        sprintf(clientDescriptor,"%d",client);
        strcpy(msg,"[server]Automobilul cu numarul de identificare ");
        strcat(msg, clientDescriptor);
        strcat(msg," circula cu viteza de ");
        strcat(msg, speed);
        strcat(msg, "km/h.");
        printf("%s\n",msg);
        fflush(stdout);
        return 1;
    }

    else if(strncmp(fromClient, "Disconnect",10) == 0){
        //Delete the client everywhere
        //First of all, if subscribed, then unsubscribe
        if(FD_ISSET(client, &subscribedfds)){
            FD_CLR(client,&subscribedfds);
            if(FD_ISSET(client, &subscribedMSGfds)) FD_CLR(client, &subscribedMSGfds);
            else{
                //Daca nu e setat in subscribed MSG, dar e setat subscribed, inseamna ca ruleaza procesul copil
                //Trebuie distrus procesul copil, pentru a nu mai trimite informatii catre client
                terminateChild(client);
            }           
        }

        if(FD_ISSET(client, &writeMSGfds) == 0){
            //If fd not in writeMSGfds
            terminateChildWrite(client);
        }  
        if(FD_ISSET(client,&writeMSGfds)) FD_CLR(client, &writeMSGfds); 
        if(FD_ISSET(client, &readactivefds)) FD_CLR(client, &readactivefds);
        FD_CLR(client, &writefds);
        close(client);
        printf("[server]S-a deconectat o masina de la server.\n");
	    fflush (stdout);
    }

    // write(client, "[server]Mesaj nerecunoscut \n", strlen("[server]Mesaj nerecunoscut \n"));
    return 1;
}

int main(int argc, char *argv[]) {

    srand(time(NULL)); //For random function

    //Database stuff
    sqlite3* DB;
    int dbStatus = 0;
    
    //Open the database
    dbStatus = sqlite3_open("projectDatabase.db",&DB);

    //Error check database
    if(dbStatus){
        printf("[server]Error openning Database.\n");
        fflush(stdout);
        return -1;
    }

    printf("[server]Database opened succesfully.\n");
    fflush(stdout);    


    //Validare sintaxa
    if (argc != 2) {
        printf("[server]Sintaxa: %s <port>\n", argv[0]);
        return -1;
    }

    //Definirea variabilelor
    int port = atoi(argv[1]);
    int optval=1; 			/* optiune folosita pentru setsockopt()*/ 
    struct sockaddr_in server;
    struct sockaddr_in from;
    char msg[100];
    char msgrasp[100] = " ";
    int sd; //Descriptor socket server
    int client; //Descriptor socket client
    int fd; //Parcurgere lista descriptori
    unsigned int length; //Lungimea structurii sockaddr
    int wstatus = -1, wstatus2 = -1; //used for waitpid()
    std::vector<int> writeMSGpids; //vectors for storing pids
    std::vector<int> subsribedMSGpids;
    int randomEventNumber; //Used for selecting random events from database

    //Creare socket
    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("[server]Eroare la socket().\n");
        return -1;
    }

      /*setam pentru socket optiunea SO_REUSEADDR */ 
    if(setsockopt(sd, SOL_SOCKET, SO_REUSEADDR,&optval,sizeof(optval)) == -1){
        perror("[server]Eroare la setsocketopt.\n");
        return -1;
    }

    bzero(&server, sizeof(server)); //Pregatire structuri
    bzero(&from, sizeof(from));

    //Populam structura folosita de server
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl (INADDR_ANY);
    server.sin_port = htons (port);

    //Bind la socket
    if (bind(sd, (struct sockaddr *) &server, sizeof(struct sockaddr)) == -1) {
        perror("[server]Eroare la bind().\n");
        return -1;
    }

    //Ascultare clienti
    if (listen(sd, 5) == -1) {
        perror("[server]Eroare la listen().\n");
        return -1;
    }

    //Completari multimi necesare select
    FD_ZERO(&writeMSGfds);
    FD_ZERO(&writefds);
    FD_ZERO(&subscribedMSGfds);
    FD_ZERO(&subscribedfds);
    FD_ZERO(&readactivefds);
    FD_SET(sd, &readactivefds); //Includem socketul creat in readactivefds (pentru a monitoriza clientii care se vor conecta)


    nfds = sd; //Momentam, descriptorul de ordin maxim este sd

    tv.tv_sec = 1; //Asteptare 1 sec la select
    tv.tv_usec = 0;

    //Mesaj informativ
    printf("[server]Asteptam la portul %d...\n", port);
    fflush (stdout);

    int ok = 1;
    writeMSGpids.clear();
    subsribedMSGpids.clear();

    while (ok) {
        randomEventNumber = (rand()%10) + 1; //Random number between 1 and 10
        //Ajustam multimea descriptori activi
        bcopy ((char *) &readactivefds, (char *) &readfds, sizeof (readfds));
        //Pentru subsribedfds si writefds nu ajustam, deoarece nu vom apela functia select pentru aceste multimi
        //writefds si subsribedfds vor fi doar pentru stocarea descriptorilor tuturor clientilor si, respectiv, a celor abonati

        //Apel select
        if(select(nfds+1, &readfds, NULL, NULL, &tv) < 0){
            printf("[server]Select() error.\n");
            fflush (stdout);
            return -1;
        }

        //Tratam cereri conectare clienti noi
        if(FD_ISSET(sd, &readfds)){
            length = sizeof(from);

            //Acceptam client
            client = accept(sd, (struct sockaddr*) &from, &length);

            //Tratare eroare accept
            if(client < 0){
                printf("[server]Eroare la accept client.\n");
                fflush(stdout);
                continue;
            }

            //Ajustare nfds (cel mai mare descriptor)
            if(client > nfds) nfds = client;

            //Includem clientul conectat in lista de writefds si readfds(nu si subsribedfds)
            FD_SET(client, &writefds);
            FD_SET(client, &readactivefds);
            FD_SET(client, &writeMSGfds);

            //Mesaj informativ
            printf("[server]S-a conectat o noua masina la server cu descriptorul %d.\n",client);
	        fflush (stdout);
        }

        //Check inbox de la clienti
        for(fd = 0; fd <= nfds; ++fd){
            if(fd != sd && FD_ISSET(fd, &readfds)){
                //If client made a server request, handle it
                if(handle(fd,DB) == -1) {
                    printf("[server]Eroare citire mesaj.\n");
                    fflush(stdout);
                    return -1;
                }
            }
        }

        //Trimitere mesaje la clientii disponibili/abonati
        //La un interval constant de timp
        for(fd = 0; fd <= nfds; ++fd){

            if(FD_ISSET(fd, &writeMSGfds)){
                FD_CLR(fd, &writeMSGfds);//FD not available to send message now, sendMessage() executing
                int pid = fork();
                if (pid == 0) { //child
                   if(sendMessage(fd) == -1){
                       printf("[server]Eroare trimitere mesaj.\n");
                       fflush(stdout);
                       return -1;
                   }
                   exit(fd);             
                }
                else{
                    getWritePidByFd.insert(std::pair<int,int>(fd,pid));
                    writeMSGpids.push_back(pid);
                }
                
            }

            if(FD_ISSET(fd, &subscribedMSGfds)){
                FD_CLR(fd, &subscribedMSGfds);
                int pid2 = fork();
                //Child
                if(pid2 == 0){
                    if(sendMessageSubscribed(fd,randomEventNumber,DB) == -1){
                        printf("[server]Eroare trimitere mesaj.\n");
                        fflush(stdout);
                        return -1;
                    }
                    exit(fd);
                }
                else{
                    getPidByFd.insert(std::pair<int,int>(fd,pid2));
                    subsribedMSGpids.push_back(pid2);
                }
            }
        }//for

        //Check for terminated children
        for(int it = 0; it < writeMSGpids.size(); ++it){

            waitpid(writeMSGpids[it],&wstatus, WNOHANG); 

            if(WIFEXITED(wstatus)){
                std::map<int,int>::iterator it2 = getWritePidByFd.find(WEXITSTATUS(wstatus));
                if(it2 == getWritePidByFd.end()){
                        printf("[server]Eroare la getWritePidByFd.\n");
                        fflush(stdout);
                    }
                else{
                    getWritePidByFd.erase(WEXITSTATUS(wstatus));
                }
                //writeMSG
                FD_SET(WEXITSTATUS(wstatus), &writeMSGfds);
                wstatus = -1; //clear status for next iterations
                writeMSGpids.erase(writeMSGpids.begin() + it);
            }

            }

            for(int it2 = 0; it2 < subsribedMSGpids.size(); ++it2){

                waitpid(subsribedMSGpids[it2],&wstatus2, WNOHANG);

                if(WIFEXITED(wstatus2)){
                    std::map<int,int>::iterator it = getPidByFd.find(WEXITSTATUS(wstatus2));
                    if(it == getPidByFd.end()){
                        printf("[server]Eroare la getPidByFd.\n");
                        fflush(stdout);
                    }
                    else{
                        getPidByFd.erase(WEXITSTATUS(wstatus2));
                    }
                    FD_SET(WEXITSTATUS(wstatus2), &subscribedMSGfds);
                    wstatus2 = -1;
                    subsribedMSGpids.erase(subsribedMSGpids.begin() + it2);
                }
            }

    

    }
}