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

#include <string.h>

#include "dependencies/communication/communication.h"

typedef struct globalData {
    int       	socketFd;               /*  connection sockets        */
    int       	mainSocket;             /*  listening socket          */
    short int 	port;                   /*  port number               */
    struct    	sockaddr_in servaddr;	/*  socket address structure  */
    char*		messageToSend;			/* message from server to client*/
    unsigned char*		messageToReceive;		/* message from server to client*/

}contextServer;

//pour les tests. a changer
contextServer ctx;

/*  Global constants  */

#define ECHO_PORT          (2014)
#define MAX_LINE           (1000)
#define LISTENQ				100

void doAction(unsigned char* buffer, char** messageToSend);
void killsrv(int socketFd);
int analyzeData(contextServer* ctx);

int main(int argc, char *argv[]) {

    char     *endptr;                /*  for strtol()              */
    ctx.messageToReceive = malloc(sizeof(unsigned char) * MAX_LINE);
    fd_set read_selector;			 //read selection
    int ret;
    struct timeval timeout;

    timeout.tv_sec = 1000;
    timeout.tv_usec = 0;

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

    	ret = select(FD_SETSIZE,&read_selector,(fd_set *)NULL,(fd_set *)NULL,&timeout);
    	if(ret > 0){
			if(FD_ISSET(ctx.mainSocket,&read_selector)){ //main socket
				debugTrace("Select found something to do ...\n");
				/*  Wait for a connection, then accept() it  */
				ctx.socketFd = accept(ctx.mainSocket, NULL, NULL);
	//    		if ( (ctx.socketFd = accept(ctx.mainSocket, NULL, NULL) ) < 0 ) {
				if(ctx.socketFd == -1) {
					fprintf(stderr, "ECHOSERV: Error calling accept()\n%s", strerror(errno));
					killsrv(ctx.socketFd);
					exit(EXIT_FAILURE);
				}
				debugTrace("New connection established\n");
				FD_SET(ctx.socketFd,&read_selector);
				//stocker les differents FD
			}
			else{//client
				debugTrace("New message incoming");
				Readline(ctx.socketFd, ctx.messageToReceive, MAX_LINE-1);
				debugTrace("Reading done");
				analyzeData(&ctx);
				debugTrace("Analyse done");
				ctx.messageToSend = parseMessage(ctx.messageToSend,strlen(ctx.messageToSend)+1);
				debugTrace("Parsing");
				Writeline(ctx.socketFd, ctx.messageToSend, strlen(ctx.messageToSend)+1);
				debugTrace("Message sent : ");
	//			}

				/*free mallocs*/

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
//    		debugTrace("toto");
    	}
    }
}

//do something according to buffer's datas
//commande : PUSH, GET, LIST, CONNECT,
void doAction(unsigned char* buffer, char** messageToSend) {

	const char* help = "Command list :\nhelp\tpush\tget\texit\0";
	const char* ukCommand = "Unknown command\0";

	if((strcmp((char*)buffer,"help\n") == 0)){
		debugTrace("Help");
		*messageToSend = malloc(sizeof(char)*(strlen(help)+1));
		strcpy(*messageToSend,help);
	}
//	else if(strcmp(buffer,"quit") == 0){
////		ctx.endConnection = 1;
//		strcpy(messageToSend,"Exit understood");
//	}
	else{
		debugTrace("UnknownCommand");
		messageToSend = malloc(sizeof(char)*(strlen(ukCommand)+1));
		strcpy(*messageToSend,ukCommand);
	}
}

int analyzeData(contextServer* ctx){

	unsigned char start = ctx->messageToReceive[0];
	unsigned char size = ctx->messageToReceive[1];

	if (start == 0xFE) {
	//			read(sockd,src,4);
	//			read(sockd,dst,4);
	//size in bytes
		if(size < 10000000){//10Mo, msg
			debugTrace("You got a message\n");
			printf("%s\n",ctx->messageToReceive);
			doAction((unsigned char*)ctx->messageToReceive+2,&(ctx->messageToSend));
		}
		else { // files
			//attention bug ! client envoie coucou trouve un fichier
			debugTrace("You got a file\n");
		}
	}
	else {
		debugTrace("This is not a valid sequence\n");
	}
	return 1;
}


void killsrv(int socketFd){
	if ( close(socketFd) < 0 ) {
		    fprintf(stderr, "ECHOSERV: Error calling close()\n");
		    exit(EXIT_FAILURE);
	}
}

