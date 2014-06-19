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

typedef struct usersDatas {
	char	name[MAX_USR_LENGTH];
	int 	group;
	int 	rights;
}userDatas;

typedef struct globalData {
    int       			socketFd[LISTENQ];      /*  connection sockets        */
    int       			mainSocket;             /*  listening socket          */
    short int 			port;                   /*  port number               */
    struct    			sockaddr_in servaddr;	/*  socket address structure  */
    char*				messageToSend;			/* message from server to client*/
    char* 				parsedMessage;
    unsigned char*		messageToReceive;		/* message from server to client*/


}contextServer;

contextServer ctx;
userDatas usrDatas[LISTENQ];

char* doAction(unsigned char* buffer, char* messageToSend, int actuel);
void sendTo(char* message, int to, int from);
void killsrv();
void setUserDatas(int id);
int isFile(int messageSize);
void deconnectClient(int sockd);
int initServer(int argc, char** argv);
void sendAll(unsigned char* message, int actuel);
void printError(int err);
void sendFileTo(char* message, int to, int from);

int main(int argc, char *argv[]) {

    fd_set read_selector;			 //read selection
    int ret;
    int i = 0;
    int messageSize;

    initServer(argc, argv);

    /*  Enter an infinite loop to respond
        to client requests and echo input  */

    while ( 1 ) {

        FD_ZERO(&read_selector);
    //    	Update FD's : En sortie, les ensembles sont modifiés pour  indiquer  les
    //    	descripteurs qui ont changé de statut.
    	 /* Remember the main socket */
        FD_SET(ctx.mainSocket,&read_selector);
    	for(i=0;i<LISTENQ;i++){
    		if(ctx.socketFd[i] != -1){
    			FD_SET(ctx.socketFd[i],&read_selector);
    		}
    	}

    	ret = select(FD_SETSIZE,&read_selector,(fd_set *)NULL,(fd_set *)NULL,NULL);
    	if(ret > 0){
			if(FD_ISSET(ctx.mainSocket,&read_selector)){ //main socket
				debugTrace("New connection detected\n");
				/*  Wait for a connection, then accept() it  */
				int sock = accept(ctx.mainSocket, NULL, NULL);
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
							setUserDatas(i);
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
							FD_CLR(ctx.socketFd[i],&read_selector);
							ctx.socketFd[i] = -1;
							break;
//							FD_CLR() clear le fd
						}
						debugTrace("Reading done");
//						if(isFile(messageSize) == 1){
							ctx.messageToSend = doAction((unsigned char*)ctx.messageToReceive,ctx.messageToSend,i);
//						}
//						else {
							// treat files
//						}
						debugTrace("Analyze done");
						//Le message a déjà été envoyé
						if(strcmp(ctx.messageToSend,"sentToALl")){
							ctx.parsedMessage = parseMessage(ctx.messageToSend,strlen(ctx.messageToSend));
							debugTrace("Parsing");
							printf("msg : %s\n",ctx.parsedMessage);
							if (Writeline(ctx.socketFd[i], ctx.parsedMessage, strlen(ctx.parsedMessage)+1) < 0){
								debugTrace("Message issue");
							}
							else
								debugTrace("Message sent\n");
						}

//						FD_CLR(ctx.socketFd[i],&active_read_selector);
					}
				}
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
char* doAction(unsigned char* buffer, char* messageToSend, int actuel) {

	char* help = "Command list :\nhelp\tsend\tpush\texit";
	char* ukCommand = "Unknown command";
	char* receptionOk = "Well received";
	char* sentToAll = "sentToALl";
	char* push = "pushed";
	char* failed = "failed";
	int i = 0;

	if((strcmp((char*)buffer,"help\n") == 0)){
		debugTrace("Help");
//		*messageToSend = malloc(sizeof(char)*((strlen(help)) + 1));
//		strcpy(*messageToSend,help);
		return help;
	}
	else if(strstr((char*)buffer,"push") != NULL){
		debugTrace("push");
		buffer+=5;
		char* name;
		char* sv;
		name = strtok_r((char*)buffer," ",&sv);
		for(i=0;i<LISTENQ;i++){
			if(!strcmp(name,usrDatas[i].name)){
				sendFileTo(sv,i,actuel);
				return push;
			}
		}
//		*messageToSend = malloc(sizeof(char)*((strlen(help)) + 1));
//		strcpy(*messageToSend,help);
		return failed;//NON
	}
	else if(strstr((char*)buffer,"send") != NULL){  //command exemple : send coco channel
		buffer+=5; //glide pointer to message (send ) carefull, dont forget the blankspace
		if(buffer[0] == '-' && buffer[1] == 'a'){
			buffer+=2;
			debugTrace("Sending to everyone");
			sendAll(buffer,actuel);
			return sentToAll;
		}
		char* name;
		char* sv;
		name = strtok_r((char*)buffer," ",&sv);

		for(i=0;i<LISTENQ;i++){
			if(!strcmp(name,usrDatas[i].name)){
				sendTo(sv,i,actuel);
				return sentToAll;
			}
		}
		return receptionOk;
	}
	else{
		debugTrace("UnknownCommand");
//		printf("%s\n",buffer);
//		*messageToSend = malloc(sizeof(char)*((strlen(ukCommand) + 1)));
//		strcpy(*messageToSend,ukCommand);
		return ukCommand;
	}
}

void sendFileTo(char* message, int to, int from){
	char *file_contents;
	long input_file_size;
	//remove \n at end of string
	message[strlen(message)-1]=0;
	FILE* input_file = fopen(message,"r");

	if(input_file != NULL){
		fseek(input_file, 0, SEEK_END);
		input_file_size = ftell(input_file);
		rewind(input_file);
		file_contents = malloc(input_file_size * (sizeof(char)));
		fread(file_contents, sizeof(char), input_file_size, input_file);
		fclose(input_file);

		char* buf;
		int size = input_file_size+sizeof(message)+MAX_USR_LENGTH + 30;
		char tmp[size];
		memset(tmp,0,size);
		strcat(tmp,message);
		strcat(tmp," ; ");
		strcat(tmp,file_contents);

		buf = parseMessage(tmp,strlen(tmp));
		printf("N%s\n",buf);
		printf("Envoie au client : %d\n",to);
		if (Writeline(ctx.socketFd[to], buf, strlen(buf)+1) < 0){
			debugTrace("Message issue");
		}
		else {
			debugTrace("Message sent\n");
		}
		free(buf);
	}
}

void sendTo(char* message, int to, int from){

	char* buf;
	int size = strlen((char*)message)+MAX_USR_LENGTH + 20;
	char tmp[size];
	memset(tmp,0,size);
	strcat(tmp,usrDatas[from].name);
	strcat(tmp," : ");
	strcat(tmp,(char*)message);
	buf = parseMessage(tmp,strlen(tmp));
	printf("Envoie au client : %d\n",to);
	if (Writeline(ctx.socketFd[to], buf, strlen(buf)+1) < 0){
		debugTrace("Message issue");
	}
	else {
		debugTrace("Message sent\n");
	}
	free(buf);
}

void sendAll(unsigned char* message, int actuel){

	int i = 0;
	char* buf;
	int size = strlen((char*)message)+MAX_USR_LENGTH + 20;
	char tmp[size];
	for(i=0;i<LISTENQ;i++){
		memset(tmp,0,size);
		if(ctx.socketFd[i] != -1  && (i != actuel)){
			strcat(tmp,usrDatas[actuel].name);
			strcat(tmp," : ");
			strcat(tmp,(char*)message);
			buf = parseMessage(tmp,strlen(tmp));
			printf("Envoie au client : %d\n",i);
			if (Writeline(ctx.socketFd[i], buf, strlen(buf)+1) < 0){
				debugTrace("Message issue");
			}
			else {
				debugTrace("Message sent\n");
			}
			free(buf);
		}

	}
}

int isFile(int messageSize){

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


//bug: deux utilisateurs peuvent avoir le meme nom
// nom trop grand
void setUserDatas(int id){
	char* buf = malloc(sizeof(char)*(MAX_USR_LENGTH + 4)); //+4 : <name(10 characters)> + < > + <group> + < > + <rights>
	int size = Readline(ctx.socketFd[id], buf, MAX_LINE-10);
	int sizeName = strlen(buf)-4;
	printf("Myname : %s\n",buf);
	if(size > 0){
		printf("In size\n");
		strncpy(usrDatas[id].name,buf,sizeName);
		buf+=sizeName;
		printf("In group : %c\n",buf[0]);
		usrDatas[id].group = buf[0];
		buf+=2; //blankspace
		printf("in right: %c\n",buf[0]);
		usrDatas[id].rights = buf[0];
	}
	else {
		strcpy(usrDatas[id].name,"User");
		usrDatas[id].rights = 0;
		usrDatas[id].group = 0;
	}
	printf("%s is my name read from the srv\n",usrDatas[id].name);
}

int initServer(int argc, char** argv){
	int i;
	char     *endptr;                /*  for strtol()              */

	ctx.messageToReceive = NULL;
	ctx.messageToSend = NULL;
	ctx.parsedMessage =NULL;
	ctx.messageToReceive = malloc(sizeof(unsigned char) * MAX_LINE);

    for(i=0;i<LISTENQ;i++){
    	ctx.socketFd[i]=-1;
    }

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

    int optval = 1;
    setsockopt(ctx.mainSocket,SOL_SOCKET,SO_REUSEADDR,&optval,sizeof(optval));

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
}

void printError(int err){
	switch(err){
		case 0:
			debugTrace("deconnection");
		break;
		case -1:
			debugTrace("Known error");
		break;
		case -2:
			debugTrace("Is this possible ?");
		break;
	}
}

void killsrv(){
//	int i = 0;
//	for(i = 0; i < LISTENQ ; i++){
//		if(close(ctx.socketFd[i]) < 0){
//		    fprintf(stderr, "ECHOSERV: Error calling close()\n");
//		    exit(EXIT_FAILURE);
//		}
//	}
//	exit(EXIT_SUCCESS);
}
