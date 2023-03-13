#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <stdbool.h>

/* codul de eroare returnat de anumite apeluri */
extern int errno;
#define WelcomeMenu "Salut!Alege o optiune:\nregister\nlogin\nexit\n optiune:"

/* portul de conectare la server*/
int port;

void Read(int sd, int len, char mesaj[300])
{
    if (read(sd, &len, sizeof(int)) < 0)
        perror("[Parinte]Err...read");
    if (read(sd, mesaj, len) < 0)
        perror("[Parinte]Err...read");
    // return mesaj;
}
bool Write(int sd, int len, char mesaj[300])
{
    int a;
    if (write(sd, &len, sizeof(int)) < 0)
        perror("[Parinte]Err...write");
    if ((a = write(sd, mesaj, len)) < 0)
        perror("[Parinte]Err...write");
    return a != 0;
}
void sendMessage(int sd)
{
    char nume[40], mesaj[100];

    printf("Insereaza userul:");
    fflush(stdout);
    scanf("%s", nume);

    printf("Mesaj:");
    fflush(stdout);
    bzero(mesaj, sizeof(mesaj));
    read(0, mesaj, sizeof(mesaj));
    int size = strlen(mesaj);
    if (mesaj[size - 1] == '\n')
        mesaj[size - 1] = '\0';

    Write(sd, strlen(nume), nume);
    Write(sd, strlen(mesaj), mesaj);
}

void seeAllUsersMenu(int sd)
{
    char users[200];
    int len;
    int option3;

    bzero(users, sizeof(users));
    Read(sd, len, users);
    printf("%s", users);

    printf("\n1.Trimite mesaj 2.Inapoi\nOptiune: "); //meniu afisat dupa selectarea comenzii 'Vezi utilizatori'

    //citim optiunea data de client
    scanf("%d", &option3);
    write(sd, &option3, sizeof(int));

    if (option3 == 1)
        sendMessage(sd); //accesam functia de trimitere a mesajelor
    else if (option3 == 2)
        return; //back
    else printf("Optiunea nu exista!\n");

}
void seeFriendsMenu(int sd)
{
    char friends[200];
    int option2;
    int len;

    bzero(friends, sizeof(friends));
    Read(sd, strlen(friends), friends);
    printf("%s", friends);

    //Vezi utilizatori
    printf("1.Vezi alti utilizatori 2.Trimite mesaj 3.Inapoi\nOptiune:");
    scanf("%d", &option2);
    write(sd, &option2, sizeof(int));

    if (option2 == 1)
    {
        seeAllUsersMenu(sd); //se acceseaza functia de afisare a meniului pentru toti userii
        fflush(stdout);
    }
    else if (option2 == 2)
    {
        sendMessage(sd); //se acceseaza functia de trimitere mesaj
        return;
    }
    else if (option2 == 3)
        return; //back
    else
        printf("Optiunea nu exista!\n");
}
void seeHistory(int sd)
{
    char friends[200];
    int len, option;

    bzero(friends, sizeof(friends));
    Read(sd, strlen(friends), friends);
    printf("%s", friends);

    char menu[100] = "1.Alegeti user 2.Inapoi\noptiune:";
    printf("%s", menu);

    scanf("%d", &option);
    write(sd, &option, sizeof(int));
    if (option == 1)
    {
        char user[40];
        //citim username
        printf("Inserati username:");
        scanf("%s", user);
        Write(sd, strlen(user), user);

        int messagesCount;
        read(sd, &messagesCount, sizeof(int)); //nr de mesaje
        printf("Istoric: %d mesaje\n", messagesCount);
        char mesaj[200];

        for (int i = 0; i < messagesCount; i++)
        {
            bzero(mesaj, sizeof(mesaj));
            Read(sd, strlen(mesaj), mesaj);
            printf("%s", mesaj);
        }
        while (1)
        {
            int optiune;

            printf("Alegeti o optiune: 1.Reply 2.Meniu principal\noptiune:");
            scanf("%d", &optiune);
            write(sd, &optiune, sizeof(int));
            if (optiune == 1)
            {
                int replyId;
                char reply[100];
                printf("Introduceti numarul mesajului pentru reply:");
                scanf("%d", &replyId);
                write(sd, &replyId, sizeof(int));

                printf("Mesaj:");
                fflush(stdout);
                bzero(reply, sizeof(reply));
                read(0, reply, 100);
                int size = strlen(reply);
                //stergem newline de la finalul unui string
                if (reply[size - 1] == '\n')
                    reply[size - 1] = '\0';

                Write(sd, strlen(reply), reply);

                char response[100];
                Read(sd, strlen(response), response);
                printf("%s", response);
            }
            else if (optiune == 2)
                break;
        }
    }
    else if (option == 2)
        return;
    else
    {
        printf("Optiunea nu exista!\n");
        return;
    }
}
void loginMenu(int sd)
{
    //CITIRE SI AFISARE MESAJE OFFLINE
    int messagesCount;
    read(sd, &messagesCount, sizeof(int)); //citim numarul de mesaje offline
    printf("Hei! Mai jos sunt mesajele primite offline: %d\n", messagesCount);
    for (int i = 0; i < messagesCount; i++) //citim si afisam mesajele primite offline
    {
        char mesaj[200];
        bzero(mesaj, sizeof(mesaj));
        int len;
        Read(sd, strlen(mesaj), mesaj);
        printf("%s\n", mesaj);
    }

    //MENIUL DE LOGIN
    char menu[200] = "Alege o optiune: 1.Vezi utilizatori  2.Vezi istoric  3.Logout";
    int option;
    printf("%s\n optiune: ", menu);
    scanf("%d", &option);
    write(sd, &option, sizeof(int));
    while (1) //MENIU LOGIN
    {
        if (option == 3) //back
            return;
        else if (option == 1) //accesam functia ce arata meniul in legatura cu prietenii
            seeFriendsMenu(sd);
        else if (option == 2)
            seeHistory(sd);
        else
            printf("Optiunea nu exista!\n");
            
        printf("%s\n optiune: ", menu);
        scanf("%d", &option);
        write(sd, &option, sizeof(int));
    }
}

int main(int argc, char *argv[])
{
    int sd;                    // descriptorul de socket
    struct sockaddr_in server; // structura folosita pentru conectare
                               // mesajul trimis
    int nr = 0;
    char buf[10];
    char command[30];
    char response[30];
    int len;
    /* exista toate argumentele in linia de comanda? */
    if (argc != 3)
    {
        printf("Sintaxa: %s <adresa_server> <port>\n", argv[0]);
        return -1;
    }

    /* stabilim portul */
    port = atoi(argv[2]);

    /* cream socketul */
    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("Eroare la socket().\n");
        return errno;
    }

    /* umplem structura folosita pentru realizarea conexiunii cu serverul */
    /* familia socket-ului */
    server.sin_family = AF_INET;
    /* adresa IP a serverului */
    server.sin_addr.s_addr = inet_addr(argv[1]);
    /* portul de conectare */
    server.sin_port = htons(port);

    /* ne conectam la server */
    if (connect(sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1)
    {
        perror("[client]Eroare la connect().\n");
        return errno;
    }
    while (1)
    {
        /* citirea mesajului */
        printf("%s", WelcomeMenu);

        //fflush(stdout);
        //read(0, command, sizeof(command));
        bzero(command, sizeof(command));
        scanf("%s", command);

        //printf("[client] Am citit %s  %ld  %ld\n", command,strlen(command),sizeof(command));
        // /* trimiterea mesajului la server */

        Write(sd, strlen(command), command);

        if (strcmp(command, "register") == 0 || strcmp(command, "login") == 0)
        {
            printf("username:");
            char username[40];
            char password[30];
            scanf("%s", username);
            printf("password:");
            scanf("%s", password);

            Write(sd, strlen(username), username);
            Write(sd, strlen(password), password);

            bzero(response, sizeof(response));
            Read(sd, strlen(response), response);
            printf(response);

            if (strcmp(response, "Te-ai logat cu succes!") == 0)
            {
                loginMenu(sd);
            }
            /* afisam mesajul primit */
            //printf("[client]Mesajul primit este: %s  %ld  %ld \n", response, strlen(response), sizeof(response));
        }

        /* citirea raspunsului dat de server 
     (apel blocant pina cind serverul raspunde) */
    }
    /* inchidem conexiunea, am terminat */
    close(sd);
}