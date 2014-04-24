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

#include "dependencies/communication/communication.h"

#define debugTrace(string) printf("\n%s\n",string);

typedef struct globalData {
    int       	socketFd;               /*  connection sockets        */
    int       	mainSocket;             /*  listening socket          */
    short int 	port;                   /*  port number               */
    struct    	sockaddr_in servaddr;	/*  socket address structure  */
    char*		messageToSend;			/* message from server to client*/
    char*		messageToReceive;		/* message from server to client*/

}contextServer;

//pour les tests. a changer
contextServer ctx;

/*  Global constants  */

#define ECHO_PORT          (2014)
#define MAX_LINE           (1000)
#define LISTENQ				100

//int Readline(int sockd, char* buffer, size_t maxlen);
void doAction(char* buffer, int size);
void killsrv(int socketFd);
//ssize_t Writeline(int sockd, const void *vptr, size_t n);
//char* parseMessage(char* buffer, int size) ;

int main(int argc, char *argv[]) {

    char      buffer[MAX_LINE];      /*  character buffer          */
    char     *endptr;                /*  for strtol()              */
    fd_set read_selector;			 //read selection
    int ret;

    FD_ZERO(&read_selector);
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

    if ( (ctx.mainSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
	fprintf(stderr, "ECHOSERV: Error creating listening socket.\n");
	exit(EXIT_FAILURE);
    }

    /* Remember the main socket */
    FD_SET(ctx.mainSocket,&read_selector);

    /*  Set all bytes in socket address structure to
        zero, and fill in the relevant data members   */

    memset(&(ctx.servaddr), 0, sizeof(ctx.servaddr));
    ctx.servaddr.sin_family      = AF_INET;
    ctx.servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    ctx.servaddr.sin_port        = htons(ctx.port);


    /*  Bind our socket address to the
	listening socket, and call listen()  */

    if ( bind(ctx.mainSocket, (struct sockaddr *) &(ctx.servaddr), sizeof(ctx.servaddr)) < 0 ) {
	fprintf(stderr, "ECHOSERV: Error calling bind()\n");
	exit(EXIT_FAILURE);
    }

    if ( listen(ctx.mainSocket, LISTENQ) < 0 ) {
	fprintf(stderr, "ECHOSERV: Error calling listen()\n");
	exit(EXIT_FAILURE);
    }


    /*  Enter an infinite loop to respond
        to client requests and echo input  */

    while ( 1 ) {

    	ret = select(FD_SETSIZE,&read_selector,(fd_set *)NULL,(fd_set *)NULL,NULL);
    	if(ret > 0){
			if(FD_ISSET(ctx.mainSocket,&read_selector)){ //main socket
				debugTrace("ISSET");
				printf("ISSET\n");
				/*  Wait for a connection, then accept() it  */
				ctx.socketFd = accept(ctx.mainSocket, NULL, NULL);
	//    		if ( (ctx.socketFd = accept(ctx.mainSocket, NULL, NULL) ) < 0 ) {
				if(ctx.socketFd == -1) {
					fprintf(stderr, "ECHOSERV: Error calling accept()\n%s", strerror(errno));
					killsrv(ctx.socketFd);
					exit(EXIT_FAILURE);
				}
				FD_SET(ctx.socketFd,&read_selector);
				//stocker les differents FD
			}
			else{//client
				Readline(ctx.socketFd, ctx.messageToReceive, MAX_LINE-1);
				ctx.messageToSend = parseMessage(buffer,strlen(buffer));
				Writeline(ctx.socketFd, ctx.messageToSend, strlen(ctx.messageToSend));

	//			}

				/*  Close the connected socket  */

				if ( close(ctx.socketFd) < 0 ) {
					fprintf(stderr, "ECHOSERV: Error calling close()\n");
					exit(EXIT_FAILURE);
	//				}
					debugTrace("LE CLIENT IS DEAD !\n");
				}
			}
    	}
    	else{
    		debugTrace("toto");
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
//	else if(strcmp(buffer,"quit") == 0){
//		ctx.endConnection = 1;
//	}
	else{
		printf("Unknown command\n");
		memcpy(buffer,"Unknown command",14);
	}
}


void killsrv(int socketFd){
	if ( close(socketFd) < 0 ) {
		    fprintf(stderr, "ECHOSERV: Error calling close()\n");
		    exit(EXIT_FAILURE);
	}
}

