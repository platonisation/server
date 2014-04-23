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


/*  Global constants  */

#define ECHO_PORT          (2014)
#define MAX_LINE           (1000)
#define LISTENQ				100


/*  Read a line from a socket  */

int Readline(int sockd, char* buffer, size_t maxlen) {

	//data to read
	unsigned char start;
	unsigned char src[4];
	unsigned char dst[4];
	unsigned int size;
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
	retval = select(sockd+1,&read_selector,NULL,NULL,&timeout);
	if(retval) {
		//treat data
		read(sockd, &start, 1);
		if (start == 0xFE) {
//			read(sockd,src,4);
//			read(sockd,dst,4);
			read(sockd,&size,1);
			//size in bytes
			if(size < 10000000){//10Mo, msg
				if((read(sockd,buffer,size) != size)){
					//Lecture ok
					printf("Cannot read datas\n");
				}
				else {
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
		//treat no data found
		printf("NODATA?\n");
	}

	return 1;
}

void doAction(char* buffer, int size);
void killsrv(int conn_s);
ssize_t Writeline(int sockd, const void *vptr, size_t n);

int main(int argc, char *argv[]) {
    int       list_s;                /*  listening socket          */
    int       conn_s;                /*  connection socket         */
    short int port;                  /*  port number               */
    struct    sockaddr_in servaddr;  /*  socket address structure  */
    char      buffer[MAX_LINE];      /*  character buffer          */
    char     *endptr;                /*  for strtol()              */


    /*  Get port number from the command line, and
        set to default port if no arguments were supplied  */

    if ( argc == 2 ) {
	port = strtol(argv[1], &endptr, 0);
	if ( *endptr ) {
	    fprintf(stderr, "ECHOSERV: Invalid port number.\n");
	    exit(EXIT_FAILURE);
	}
    }
    else if ( argc < 2 ) {
	port = ECHO_PORT;
    }
    else {
	fprintf(stderr, "ECHOSERV: Invalid arguments.\n");
	exit(EXIT_FAILURE);
    }


    /*  Create the listening socket  */

    if ( (list_s = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
	fprintf(stderr, "ECHOSERV: Error creating listening socket.\n");
	exit(EXIT_FAILURE);
    }

    /*  Set all bytes in socket address structure to
        zero, and fill in the relevant data members   */

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port        = htons(port);


    /*  Bind our socket address to the
	listening socket, and call listen()  */

    if ( bind(list_s, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0 ) {
	fprintf(stderr, "ECHOSERV: Error calling bind()\n");
	exit(EXIT_FAILURE);
    }

    if ( listen(list_s, LISTENQ) < 0 ) {
	fprintf(stderr, "ECHOSERV: Error calling listen()\n");
	exit(EXIT_FAILURE);
    }


    /*  Enter an infinite loop to respond
        to client requests and echo input  */

    while ( 1 ) {

	/*  Wait for a connection, then accept() it  */

	if ( (conn_s = accept(list_s, NULL, NULL) ) < 0 ) {
	    fprintf(stderr, "ECHOSERV: Error calling accept()\n");
	    killsrv(conn_s);
	    exit(EXIT_FAILURE);
	}


	/*  Retrieve an input line from the connected socket
	    then simply write it back to the same socket.     */

	Readline(conn_s, buffer, MAX_LINE-1);
	Writeline(conn_s, buffer, strlen(buffer));


	/*  Close the connected socket  */

	if ( close(conn_s) < 0 ) {
	    fprintf(stderr, "ECHOSERV: Error calling close()\n");
	    exit(EXIT_FAILURE);
	}
    }
}

//do something according to buffer's datas
void doAction(char* buffer,int size) {
	//char action[1000]={'a',',','b',',','c',',','d'};
	//strcpy(buffer,action);
	char commande[5];
	memcpy(commande,buffer,5);
	if(buffer[0] == '?' || (strcmp(commande,"help") == 0)){
		memcpy(buffer,"PUSH,GET",8);
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

void killsrv(int conn_s){
	if ( close(conn_s) < 0 ) {
		    fprintf(stderr, "ECHOSERV: Error calling close()\n");
		    exit(EXIT_FAILURE);
	}
}
