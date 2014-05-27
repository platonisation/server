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

/*  Global constants  */

#define ECHO_PORT          (2014)
#define MAX_LINE           (1000)
#define LISTENQ				100


typedef struct globalData {
    int       	socketFd[LISTENQ];      /*  connection sockets        */
    int       	mainSocket;             /*  listening socket          */
    short int 	port;                   /*  port number               */
    struct    	sockaddr_in servaddr;	/*  socket address structure  */
    char*		messageToSend;			/* message from server to client*/
    char* 		parsedMessage;
    unsigned char*		messageToReceive;		/* message from server to client*/

}contextServer;

//pour les tests. a changer
contextServer ctx;


char* doAction(unsigned char* buffer, char* messageToSend);
void killsrv(int socketFd);
int analyzeData(contextServer* ctx, int messageSize);
void deconnectClient(int sockd);
int initServer();
void printError(int err);

int main(int argc, char *argv[]) {

	ctx.messageToReceive = NULL;
	ctx.messageToSend = NULL;
	ctx.parsedMessage =NULL;


    char     *endptr;                /*  for strtol()              */
    ctx.messageToReceive = malloc(sizeof(unsigned char) * MAX_LINE);
    fd_set read_selector;			 //read selection
    int ret;
    int i = 0;
    int messageSize;

    FD_ZERO(&read_selector);
    /*  Get port number from the command line, and
        set to default port if no arguments were supplied  */

    for(i=0;i<LISTENQ;i++){
    	ctx.socketFd[i]=-1;
    }

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

    	FD_ZERO(&read_selector);
//    	Update FD's : En sortie, les ensembles sont modifiés pour  indiquer  les
//    	descripteurs qui ont changé de statut.
    	FD_SET(ctx.mainSocket,&read_selector);
    	for(i=0;i<LISTENQ;i++){
			if(ctx.socketFd[i] != -1){
				printf("%d cleint\n",i);
				FD_SET(ctx.socketFd[i],&read_selector);
			}
		}

    	ret = select(FD_SETSIZE,&read_selector,(fd_set *)NULL,(fd_set *)NULL,NULL);
    	if(ret > 0){
			if(FD_ISSET(ctx.mainSocket,&read_selector)){ //main socket
				debugTrace("New connection detected\n");
				/*  Wait for a connection, then accept() it  */
				int sock = accept(ctx.mainSocket, NULL, NULL);
	//    		if ( (ctx.socketFd = accept(ctx.mainSocket, NULL, NULL) ) < 0 ) {
				if(sock == -1) {
					fprintf(stderr, "ECHOSERV: Error calling accept()\n%s", strerror(errno));
//					killsrv(ctx.socketFd);
					exit(EXIT_FAILURE);
				}
				debugTrace("New connection established\n");

				for(i=0;i<LISTENQ;i++){
					if(ctx.socketFd[i] == -1){
							ctx.socketFd[i]=sock;
							FD_SET(ctx.socketFd[i],&read_selector);
							break;
					}
				}
			}
			else{//client
				//effectuer une lecture des FDs avec une rotation sinon c'est toujours le premier client qui aura le token
//				i = ((i+1) % LISTENQ);
				for(i=0;i<LISTENQ;i++){
					if(ctx.socketFd[i] != -1 && FD_ISSET(ctx.socketFd[i],&read_selector)){
						debugTrace("New message incoming");
						messageSize = Readline(ctx.socketFd[i], ctx.messageToReceive, MAX_LINE-10);
						if(messageSize <= 0){
							printError(messageSize);
							deconnectClient(ctx.socketFd[i]);
							ctx.socketFd[i] = -1;
//							FD_CLR() clear le fd
						}
						debugTrace("Reading done");
						if(analyzeData(&ctx,messageSize) == 1){
							ctx.messageToSend = doAction((unsigned char*)ctx.messageToReceive,ctx.messageToSend);
						}
						else {
							// treat files
						}
						debugTrace("Analyse done");
						ctx.parsedMessage = parseMessage(ctx.messageToSend,strlen(ctx.messageToSend));
						debugTrace("Parsing");
						printf("msg : %s\n",ctx.parsedMessage);
						if (Writeline(ctx.socketFd[i], ctx.parsedMessage, strlen(ctx.parsedMessage)+1) < 0){
							debugTrace("Message issue");
						}
						else
							debugTrace("Message sent\n");

//						free(ctx.parsedMessage);
					}
				}
				/*free mallocs*/

				printf("Over\n");
			}
    	}
    	else{
    		debugTrace("nothing to read");
    	}
    }

    free(ctx.messageToReceive);

    /*  Close the connected socket  */
    for(i=0;i<LISTENQ;i++){
    	if ( close(ctx.socketFd[i]) < 0 ) {
			fprintf(stderr, "ECHOSERV: Error calling close()\n");
			exit(EXIT_FAILURE);
			debugTrace("LE CLIENT IS DEAD !\n");
		}
    }

}

//do something according to buffer's datas
//commande : PUSH, GET, LIST, CONNECT,
char* doAction(unsigned char* buffer, char* messageToSend) {

	char* help = "Command list :\nhelp\tpush\tget\texit";
	char* ukCommand = "Unknown command";

	if((strcmp((char*)buffer,"help\n") == 0)){
		debugTrace("Help");
//		*messageToSend = malloc(sizeof(char)*((strlen(help)) + 1));
//		strcpy(*messageToSend,help);
		return help;
	}
//	else if(strcmp((char*)buffer,"quit\n") == 0){
//		ctx.endConnection = 1;
//		strcpy(messageToSend,"Close connection");
//		debugTrace("Terminate connection with client");
//		exit(0);
//	}
	else{
		debugTrace("UnknownCommand");
//		printf("%s\n",buffer);
//		*messageToSend = malloc(sizeof(char)*((strlen(ukCommand) + 1)));
//		strcpy(*messageToSend,ukCommand);
		return ukCommand;
	}
}

int analyzeData(contextServer* ctx, int messageSize){

	if(messageSize < 10000000){//10Mo, msg
		debugTrace("This is a message");
		return 1;
	}
	else{
		debugTrace("This is a file");
		return 2;
	}

//	unsigned char start = ctx->messageToReceive[0];
//	unsigned char size = ctx->messageToReceive[1];
//
//	if (start == 0xFE) {
//	//			read(sockd,src,4);
//	//			read(sockd,dst,4);
//	//size in bytes
//		if(size < 10000000){//10Mo, msg
//			debugTrace("You got a message\n");
//			printf("%s\n",ctx->messageToReceive);
//			doAction((unsigned char*)ctx->messageToReceive+2,&(ctx->messageToSend));
//		}
//		else { // files
//			//attention bug ! client envoie coucou trouve un fichier
//			debugTrace("You got a file\n");
//		}
//	}
//	else {
//		debugTrace("This is not a valid sequence\n");
//	}
//	return 1;
}

void deconnectClient(int sockd){
	if(close(sockd) != 0){
//do something
		debugTrace("FAIL DECONNECTING CLIENT");
	}
}

void printError(int err){
	switch(err){
		case 0:
			debugTrace("Nothing to read, timeout");
		break;
		case -1:
			debugTrace("Known error");
		break;
		case -2:
			debugTrace("Is this possible ?");
		break;
	}
}

void killsrv(int socketFd){
	if ( close(socketFd) < 0 ) {
		    fprintf(stderr, "ECHOSERV: Error calling close()\n");
		    exit(EXIT_FAILURE);
	}
}

