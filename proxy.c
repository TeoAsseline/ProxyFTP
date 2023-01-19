#include  <stdio.h>
#include  <stdlib.h>
#include  <sys/socket.h>
#include  <netdb.h>
#include  <string.h>
#include  <unistd.h>
#include  <stdbool.h>
#include "./simpleSocketAPI.h"


#define SERVADDR "127.0.0.1"        // Définition de l'adresse IP d'écoute
#define SERVPORT "0"                // Définition du port d'écoute, si 0 port choisi dynamiquement
#define LISTENLEN 1                 // Taille de la file des demandes de connexion
#define MAXBUFFERLEN 1024           // Taille du tampon pour les échanges de données
#define MAXHOSTLEN 64               // Taille d'un nom de machine
#define MAXPORTLEN 64               // Taille d'un numéro de port
#define PORTFTP "21"

void fils();

int main(){
    int ecode;                       // Code retour des fonctions
    char serverAddr[MAXHOSTLEN];     // Adresse du serveur
    char serverPort[MAXPORTLEN];     // Port du server
    int descSockRDV;                 // Descripteur de socket de rendez-vous
    int descSockCOM;                 // Descripteur de socket de communication
    struct addrinfo hints;           // Contrôle la fonction getaddrinfo
    struct addrinfo *res;            // Contient le résultat de la fonction getaddrinfo
    struct sockaddr_storage myinfo;  // Informations sur la connexion de RDV
    struct sockaddr_storage from;    // Informations sur le client connecté
    socklen_t len;                   // Variable utilisée pour stocker les 
				                     // longueurs des structures de socket
    char buffer[MAXBUFFERLEN];      // Tampon de communication entre le client et le serveur
    
    // Initialisation de la socket de RDV IPv4/TCP
    descSockRDV = socket(AF_INET, SOCK_STREAM, 0);
    if (descSockRDV == -1) {
         perror("Erreur création socket RDV\n");
         exit(2);
    }
    // Publication de la socket au niveau du système
    // Assignation d'une adresse IP et un numéro de port
    // Mise à zéro de hints
    memset(&hints, 0, sizeof(hints));
    // Initialisation de hints
    hints.ai_flags = AI_PASSIVE;      // mode serveur, nous allons utiliser la fonction bind
    hints.ai_socktype = SOCK_STREAM;  // TCP
    hints.ai_family = AF_INET;        // seules les adresses IPv4 seront présentées par 
				                      // la fonction getaddrinfo

     // Récupération des informations du serveur
     ecode = getaddrinfo(SERVADDR, SERVPORT, &hints, &res);
     if (ecode) {
         fprintf(stderr,"getaddrinfo: %s\n", gai_strerror(ecode));
         exit(1);
     }
     // Publication de la socket
     ecode = bind(descSockRDV, res->ai_addr, res->ai_addrlen);
     if (ecode == -1) {
         perror("Erreur liaison de la socket de RDV");
         exit(3);
     }
     // Nous n'avons plus besoin de cette liste chainée addrinfo
     freeaddrinfo(res);

     // Récuppération du nom de la machine et du numéro de port pour affichage à l'écran
     len=sizeof(struct sockaddr_storage);
     ecode=getsockname(descSockRDV, (struct sockaddr *) &myinfo, &len);
     if (ecode == -1)
     {
         perror("SERVEUR: getsockname");
         exit(4);
     }
     ecode = getnameinfo((struct sockaddr*)&myinfo, sizeof(myinfo), serverAddr,MAXHOSTLEN, 
                         serverPort, MAXPORTLEN, NI_NUMERICHOST | NI_NUMERICSERV);
     if (ecode != 0) {
             fprintf(stderr, "error in getnameinfo: %s\n", gai_strerror(ecode));
             exit(4);
     }
     printf("L'adresse d'ecoute est: %s\n", serverAddr);
     printf("Le port d'ecoute est: %s\n", serverPort);

     // Definition de la taille du tampon contenant les demandes de connexion
     ecode = listen(descSockRDV, LISTENLEN);
     if (ecode == -1) {
         perror("Erreur initialisation buffer d'écoute");
         exit(5);
     }

	len = sizeof(struct sockaddr_storage);
     
     // Attente connexion du client
     // Lorsque demande de connexion, creation d'une socket de communication avec le client
    while (1)
    {
        descSockCOM = accept(descSockRDV, (struct sockaddr *) &from, &len);
        if (descSockCOM == -1){
            perror("Erreur accept\n");
            exit(6);
        }
        // Echange de données avec le client connecté
        int err;
        pid_t pid;
        int rapport, numSig, status;

        // Creation d'un processus fils afin de gerer les echanges client-serveur

        pid = fork();
        switch (pid)
        {
        case -1:
            perror("Impossible de creer le fils 1");
            exit(1);

        case 0:
            fils(descSockCOM);
            exit(0);
        }
    }
    //On ferme la connection après avoir créer un fils
    close(descSockRDV);
}


void fils (int descSockCOM) {

    char buffer[MAXBUFFERLEN];       // Tampon de communication entre le client et le serveur
    int ecode;                       // Code retour des fonctions

    //0 Envoi du message de connexion au proxy réussi
    strcpy(buffer, "220 Connexion au serveur réussi\n");
    write(descSockCOM, buffer, strlen(buffer));

    //1 Lecture du message envoyé par le client, le nom et l'adresse du serveur visé (normalement anonymous@ftp.fau.de)
    ecode=read(descSockCOM, buffer, MAXBUFFERLEN-1);
    if (ecode == -1){
        perror("Erreur lecture dans socket\n");
        exit(7);
    }
    buffer[ecode]='\0';
    printf("Message recu: %s\n", buffer);

     //2 Séparation du message reçu pour isoler le login et l'adresse 
    char login[50], serverAddr[50];
    sscanf(buffer,"%[^@]@%s",login,serverAddr);
    printf("Login : %s\nServeur : %s\n",login,serverAddr);

    //3 création du socket de connnexion avec le serveur
    int sockServeurCMD;
    ecode = connect2Server(serverAddr, PORTFTP, &sockServeurCMD);
    if (ecode == -1){
        perror("Erreur connexion au serveur\n");
        exit(8);
    }
    printf("Connexion au serveur réussie\n");

    //4 lecture du message de retour du serveur (demande de login)
    ecode=read(sockServeurCMD, buffer, MAXBUFFERLEN-1);
    if (ecode == -1){
        perror("Erreur lecture dans socket\n");
        exit(9);
    }
    buffer[ecode]='\0';
    printf("Message recu du serveur : %s\n", buffer);

    //5 renvoi du login du client au serveur
    sprintf(buffer,"%s\r\n",login);
    ecode = write(sockServeurCMD, buffer, strlen(buffer));
    if (ecode == -1){
        perror("Erreur écriture dans socket\n");
        exit(10);
    }
    printf("Message envoyé au serveur : %s\n", buffer);

    //6 lecture du message de retour du serveur
    ecode=read(sockServeurCMD, buffer, MAXBUFFERLEN-1);
    if(ecode == -1){
        perror("Erreur lecture dans socket\n");
        exit(9);
    }
    buffer[ecode]='\0';
    printf("Message recu du serveur : %s\n", buffer);

    //7 transmition du message du serveur au client 
    ecode = write(descSockCOM, buffer, strlen(buffer));
    if(ecode == -1){
        perror("Erreur écriture dans socket\n");
        exit(10);
    }
    printf("Message envoyé au client : %s\n", buffer);

    // boucle le transmission des des information jusqu'au prochain quit
    while ( strstr(buffer, "221 Goodbye.") == NULL) {
        
        //lecture du message du client 
        ecode=read(descSockCOM, buffer, MAXBUFFERLEN-1);
        if (ecode == -1){
            perror("Erreur lecture dans socket\n");
            exit(7);
        }
        buffer[ecode]='\0';
        printf("Message recu : %s\n", buffer);
            
        // Si le client entre ls    
        if (strstr(buffer, "PORT") != NULL) {
            // séparation de adresse IP et num de port
            int n1, n2, n3, n4, n5, n6;
            char adrIPClt[MAXHOSTLEN];
            char portClt[MAXHOSTLEN];
            sscanf(buffer,"PORT %d,%d,%d,%d,%d,%d", &n1, &n2, &n3, &n4, &n5, &n6);
            sprintf(adrIPClt,"%d.%d.%d.%d",n1,n2,n3,n4);
            n5 = n5 * 256 + n6;
            sprintf(portClt,"%d",n5);

            // connexion avec le client
            int SockActif;
            ecode = connect2Server(adrIPClt, portClt, &SockActif);

            //19 envoie PASV au serveur
            char pasv [50] = "PASV\r\n";
            ecode = write(sockServeurCMD, pasv, strlen(pasv));
            if(ecode == -1){
                perror("Erreur écriture dans socket\n");
                exit(10);
            }
            printf("Message envoyé au serveur : %s\n", pasv);

            // lecture du message de retour du serveur
            ecode=read(sockServeurCMD, buffer, MAXBUFFERLEN-1);
            if(ecode == -1){
                perror("Erreur lecture dans socket\n");
                exit(9);
            }
            buffer[ecode]='\0';
            printf("Message recu du serveur : %s\n", buffer);

            // séparation de adresse IP et num de port du serveur
            char adrIPSvr[MAXHOSTLEN];
            char portSvr[MAXHOSTLEN];
            sscanf(buffer,"227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)", &n1, &n2, &n3, &n4, &n5, &n6);
            sprintf(adrIPSvr,"%d.%d.%d.%d",n1,n2,n3,n4);
            n5 = n5 * 256 + n6;
            sprintf(portSvr,"%d",n5);
            printf("Adresse IP du serveur : %s ,Port du serveur : %s\n",adrIPSvr, portSvr);

            // connexion avec le serveur
            int SockPassif;
            ecode = connect2Server(adrIPSvr, portSvr, &SockPassif);

            // envoie de la confirmation au client 
            char message[50] = "200 OK\r\n";
            write(descSockCOM, message, strlen(message));

            // lecture message du client (LIST)
            ecode=read(descSockCOM, buffer, MAXBUFFERLEN-1);
            if (ecode == -1){
                perror("Erreur lecture dans socket\n");
                exit(7);
            }
            buffer[ecode]='\0';
            printf("Message recu : %s\n", buffer);

            // transmition du message au serveur (LIST)
            ecode = write(sockServeurCMD, buffer, strlen(buffer));
            if(ecode == -1){
                perror("Erreur écriture dans socket\n");
                exit(10);
            }
            printf("Message envoyé au serveur : %s\n", buffer);

            // lecture du message de retour du serveur (150)
            ecode=read(sockServeurCMD, buffer, MAXBUFFERLEN-1);
            if(ecode == -1){
                perror("Erreur lecture dans socket\n");
                exit(9);
            }
            buffer[ecode]='\0';
            printf("Message recu du serveur : %s\n", buffer);

            // transmition du message du serveur au client (150) 
            ecode = write(descSockCOM, buffer, strlen(buffer));
            if(ecode == -1){
                perror("Erreur écriture dans socket\n");
                exit(10);
            }
            printf("Message envoyé au client : %s\n", buffer);
            
            // boucle afin de lire l'entierete du ls
            do 
            {
                // lecture de donnees du serveur
                ecode = read(SockPassif, buffer, MAXBUFFERLEN - 1);
                if (ecode == -1)
                {
                    perror("probleme de lecture");
                    exit(1);
                }
                buffer[ecode] = '\0';
                printf("%s", buffer);

                // envoie des donnees au client
                write(SockActif, buffer, strlen(buffer));
            } while (read(SockPassif, buffer, MAXBUFFERLEN - 1) != 0);

            close(SockActif);
            close(SockPassif);

            // lecture du message de confirmation du serveur (226)
            ecode=read(sockServeurCMD, buffer, MAXBUFFERLEN-1);
            if(ecode == -1){
                perror("Erreur lecture dans socket\n");
                exit(9);
            }
            buffer[ecode]='\0';
            printf("Message recu du serveur : %s\n", buffer);

            // transmition du message du serveur au client (226) 
            ecode = write(descSockCOM, buffer, strlen(buffer));
            if(ecode == -1){
                perror("Erreur écriture dans socket\n");
                exit(10);
            }
            printf("Message envoyé au client : %s\n", buffer);
       
        } else {

            // transmition du message au serveur 
            ecode = write(sockServeurCMD, buffer, strlen(buffer));
            if(ecode == -1){
                perror("Erreur écriture dans socket\n");
                exit(10);
            }
            printf("Message envoyé au serveur : %s\n", buffer);

            // lecture du message de retour du serveur
            ecode=read(sockServeurCMD, buffer, MAXBUFFERLEN-1);
            if(ecode == -1){
                perror("Erreur lecture dans socket\n");
                exit(9);
            }
            buffer[ecode]='\0';
            printf("Message recu du serveur : %s\n", buffer);

            // transmition du message du serveur au client
            ecode = write(descSockCOM, buffer, strlen(buffer));
            if(ecode == -1){
                perror("Erreur écriture dans socket\n");
                exit(10);
            }
            printf("Message envoyé au client : %s\n", buffer);
        }
    }

    //Fermeture de la connexion
    close(sockServeurCMD);
    close(descSockCOM);
    printf("La connection a bien été coupée.\n");
}
