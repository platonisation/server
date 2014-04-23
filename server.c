/*
 * server.c
 *
 *  Created on: Apr 16, 2014
 *  Author: Timoté Bonnin
 *
 *
 *  Kill if receive ctrl+c
 *
 *  protocole : [accorche][header][msg]
 *  [header] = [src][dst][size msg]
 *
 * 	Authentification fonctionne par couple IP : ID
 *
 *
 Serveur
**Des notications sont envoyées aux clients à chaque connexion et déconnexion.
**Chaque serveur doit maintenir une liste des membres connectés sur le réseau.
**Un utilisateur p eut créer un compte (pseudo / mot de passe). Ce doit être une notion globale au
réseau : un compte enregistré depuis un serveur doit être reconnu à une pro chaine connexion sur
un autre serveur.
**Un utilisateur enregistré p eut être administrateur du réseau. Il p eut alors expulser bannir un
autre utilisateur
 *
 */

#include <sys/socket.h>       /*  socket definitions        */
#include <sys/types.h>        /*  socket types              */
#include <arpa/inet.h>        /*  inet (3) funtions         */
#include <unistd.h>           /*  misc. UNIX functions      */
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct globalData {
    int       	socketFd;               /*  connection socket         */
    int       	list_s;                 /*  listening socket          */
    short int 	port;                   /*  port number               */
    struct    	sockaddr_in servaddr;	/*  socket address structure  */
    char*		messageToSend;			/* message from server to client*/
    char*		messageToReceive;			/* message from server to client*/
    int			endConnection;//bof bof peut etre enum ?

}context;

//pour les tests. a changer
context ctx;

/*  Global constants  */

#define ECHO_PORT          (2014)
#define MAX_LINE           (1000)
#define LISTENQ				100

int Readline(int sockd, char* buffer, size_t maxlen);
void doAction(char* buffer, int size);
void killsrv(int socketFd);
ssize_t Writeline(int sockd, const void *vptr, size_t n);
char* parseMessage(char* buffer, int size) ;

int main(int argc, char *argv[]) {

    char      buffer[MAX_LINE];      /*  character buffer          */
    char     *endptr;                /*  for strtol()              */



    pid_t son;

    ctx.endConnection = 0;

    /*  Get port number from the command line, and
        set to default port if no arguments were supplied  */

    if ( argc == 2 ) {
	ctx.port = strtol(argv[1], &endptr, 0);
	if ( *endptr ) {
	    fprintf(stderr, "ECHOSERV: Invalid port number.\n");
	    exit(EXIT_FAILURE);
	}
    }
    else if ( argc < 2 ) {
	ctx.port = ECHO_PORT;
    }
    else {
	fprintf(stderr, "ECHOSERV: Invalid arguments.\n");
	exit(EXIT_FAILURE);
    }


    /*  Create the listening socket  */

    if ( (ctx.list_s = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
	fprintf(stderr, "ECHOSERV: Error creating listening socket.\n");
	exit(EXIT_FAILURE);
    }

    /*  Set all bytes in socket address structure to
        zero, and fill in the relevant data members   */

    memset(&(ctx.servaddr), 0, sizeof(ctx.servaddr));
    ctx.servaddr.sin_family      = AF_INET;
    ctx.servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    ctx.servaddr.sin_port        = htons(ctx.port);


    /*  Bind our socket address to the
	listening socket, and call listen()  */

    if ( bind(ctx.list_s, (struct sockaddr *) &(ctx.servaddr), sizeof(ctx.servaddr)) < 0 ) {
	fprintf(stderr, "ECHOSERV: Error calling bind()\n");
	exit(EXIT_FAILURE);
    }

    if ( listen(ctx.list_s, LISTENQ) < 0 ) {
	fprintf(stderr, "ECHOSERV: Error calling listen()\n");
	exit(EXIT_FAILURE);
    }


    /*  Enter an infinite loop to respond
        to client requests and echo input  */

    while ( 1 ) {

		/*  Wait for a connection, then accept() it  */

		if ( (ctx.socketFd = accept(ctx.list_s, NULL, NULL) ) < 0 ) {
			fprintf(stderr, "ECHOSERV: Error calling accept()\n");
			killsrv(ctx.socketFd);
			exit(EXIT_FAILURE);
		}
		else{
			if((son=fork()==0)){

				while(!(ctx.endConnection)){
					/*  Retrieve an input line from the connected socket
					then simply write it back to the same socket.     */

					Readline(ctx.socketFd, buffer, MAX_LINE-1);
					ctx.messageToSend = parseMessage(buffer,strlen(buffer));
					Writeline(ctx.socketFd, ctx.messageToSend, strlen(ctx.messageToSend));

				}

				/*  Close the connected socket  */

				if ( close(ctx.socketFd) < 0 ) {
				    fprintf(stderr, "ECHOSERV: Error calling close()\n");
				    exit(EXIT_FAILURE);
				}
				printf("LE CLIENT IS DEAD !\n");
			}
		}
    }
}

//do something according to buffer's datas
//commande : PUSH, GET, LIST, CONNECT,
void doAction(char* buffer,int size) {
	//char action[1000]={'a',',','b',',','c',',','d'};
	//strcpy(buffer,action);

	printf("My action : %s\n",buffer);

	//fixme: this function is full of shit
	char commande[6];
	memcpy(commande,buffer,5);
	if(buffer[0] == '?' || (strcmp(commande,"help") == 0)){
		int len=4;
		memcpy(buffer,"PUSH",len);
		buffer[len]='\0';
	}
	else if(strcmp(buffer,"quit") == 0){
		ctx.endConnection = 1;
	}
	else{
		printf("Unknown command\n");
		memcpy(buffer,"Unknown command",14);
	}
}


/*  Write a line to a socket  */

ssize_t Writeline(int sockd, const void *vptr, size_t n) {
    size_t      nleft;
    ssize_t     nwritten;
    const char *buffer;

    buffer = vptr;
    nleft  = n;

    while ( nleft > 0 ) {
	if ( (nwritten = write(sockd, buffer, nleft)) <= 0 ) {
	    if ( errno == EINTR )
		nwritten = 0;
	    else
		return -1;
	}
	nleft  -= nwritten;
	buffer += nwritten;
    }

    return n;
}

void killsrv(int socketFd){
	if ( close(socketFd) < 0 ) {
		    fprintf(stderr, "ECHOSERV: Error calling close()\n");
		    exit(EXIT_FAILURE);
	}
}

/*  Read a line from a socket  */

int Readline(int sockd, char* buffer, size_t maxlen) {

	//data to read
	unsigned char start;
	unsigned char src[4];
	unsigned char dst[4];
	unsigned char size;
	unsigned char data[20];

//	if ( (rc = read(sockd, data, 12)) == 12 ){
//	printf("\n\nCOCOU\n\n");
//	printf("Val : %s\n",data);
//	}

	//read selection
	fd_set read_selector;
	//timeout of read
	struct timeval timeout;
	timeout.tv_sec = 30;
	timeout.tv_usec = 0;
	//return value
	int retval;
	//init read selection
	FD_ZERO(&read_selector);
	FD_SET(sockd,&read_selector);
	retval = select(sockd+1,&read_selector,NULL,NULL,NULL);
	if(retval) {
		//treat data
		read(sockd, &start, 1);
		if (start == 0xFE) {
//			read(sockd,src,4);
//			read(sockd,dst,4);
			read(sockd,&size,1);
			printf("size:%d\n",size);
			//size in bytes
			if(size < 10000000){//10Mo, msg
				if((read(sockd,buffer,size) != size)){
					//Lecture ok
					printf("Cannot read %d datas\n",size);
				}
				else {
					printf("%s",buffer);
					doAction(buffer,size);
				}

			}
			else { // files
				//attention bug ! client envoie coucou trouve un fichier
				printf("This is a file\n");
			}
		}
		else {
			printf("Not a starting sequence\n");
		}
	}
	else if(retval == -1){
		//treat error
		printf("RETVAL==1\n");
	}
	else{
		ctx.endConnection = 1;
		//treat no data found
		printf("NODATA?\n");
	}

	return 1;
}

char* parseMessage(char* buffer, int size) {
	char* s;
	s = malloc(size*sizeof(char) + 2);
	s[0]=0xFE;
	s[1]=strlen(buffer);
	strcat(s,buffer);

	return s;
}
