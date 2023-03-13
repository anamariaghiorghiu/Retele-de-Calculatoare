#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <mysql/mysql.h>

/* portul folosit */
#define PORT 2908
/* codul de eroare returnat de anumite apeluri */
extern int errno;

typedef struct thData
{
    int idThread; //id-ul thread-ului
    int cl;       //descriptorul intors de accept
    int idUser;   //id-ul userului logat
} thData;

thData *clients[100];
int nrClients = 0;

static void *treat(void *); /* functia executata de fiecare thread ce realizeaza comunicarea cu clientii */
void raspunde(void *);

int main()
{
    struct sockaddr_in server; // structura folosita de server
    struct sockaddr_in from;
    int nr; //mesajul primit de trimis la client
    int sd; //descriptorul de socket
    int i = -1;
    pthread_t th[100]; //Identificatorii thread-urilor care se vor crea

    /* crearea unui socket */
    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("[server]Eroare la socket().\n");
        return errno;
    }
    /* utilizarea optiunii SO_REUSEADDR */
    int on = 1;
    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    /* pregatirea structurilor de date */
    bzero(&server, sizeof(server));
    bzero(&from, sizeof(from));

    /* umplem structura folosita de server */
    /* stabilirea familiei de socket-uri */
    server.sin_family = AF_INET;
    /* acceptam orice adresa */
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    /* utilizam un port utilizator */
    server.sin_port = htons(PORT);

    /* atasam socketul */
    if (bind(sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1)
    {
        perror("[server]Eroare la bind().\n");
        return errno;
    }

    /* punem serverul sa asculte daca vin clienti sa se conecteze */
    if (listen(sd, 2) == -1)
    {
        perror("[server]Eroare la listen().\n");
        return errno;
    }
    /* servim in mod concurent clientii...folosind thread-uri */
    while (1)
    {
        int client;
        thData *td; //parametru functia executata de thread
        int length = sizeof(from);

        printf("[server]Asteptam la portul %d...\n", PORT);
        //fflush(stdout);

        /* acceptam un client (stare blocanta pina la realizarea conexiunii) */
        if ((client = accept(sd, (struct sockaddr *)&from, &length)) < 0)
        {
            perror("[server]Eroare la accept().\n");
            continue;
        }

        td = (struct thData *)malloc(sizeof(struct thData));
        td->idThread = ++i;
        td->cl = client;
        td->idUser = -1;
        clients[i] = td;
        nrClients = i;

        pthread_create(&th[i], NULL, &treat, td);
        //printf("ceva"); se afiseaza in parinte dupa pthread_detach()
    } //while
};
static void *treat(void *arg)
{
    struct thData tdL; //declaratie
    //int *p=2;
    //cout<<p --- 2976318731 (adresa);
    //cout<<*p --- 2;
    //void * -> struct thData * ->*
    tdL = *((struct thData *)arg); //conversie a lui arg la tipul de data thData
    printf("[thread]- %d - Asteptam mesajul...\n", tdL.idThread);
    //fflush(stdout);
    pthread_detach(pthread_self()); //despart threadul de proces kind of
    raspunde((struct thData *)arg); //de aici serverul serveste clientul conectat prin thread == concurentialitate
    /* am terminat cu acest client, inchidem conexiunea */
    close((intptr_t)arg);
    return (NULL);
};
void Read(int sd, int len, char *mesaj, thData td)
{
    int lenBytes = 0, mesajBytes = 0;
    if ((lenBytes = read(sd, &len, sizeof(int))) < 0)
        perror("[Parinte]Err...read");
    if ((mesajBytes = read(sd, mesaj, len)) < 0)
        perror("[Parinte]Err...read");
    if (lenBytes == 0 && mesajBytes == 0)
    {
        close(sd);
        close(clients[td.idThread]->cl);
        clients[td.idThread]->cl = -1;
    }
}
bool Write(int sd, int len, char mesaj[300])
{
    int a;
    if (write(sd, &len, sizeof(int)) < 0) //trimit un int care reprezinta lungimea mesajului
        perror("[Parinte]Err...write");
    if ((a = write(sd, mesaj, len)) < 0)
        perror("[Parinte]Err...write");
    return a != 0;
}

void updateOfflineMessage(char messageId[5])
{
    MYSQL *conn = mysql_init(NULL); //initiem conexiunea la baza de date

    if (conn == NULL)
    {
        fprintf(stderr, "%s\n", mysql_error(conn));
        exit(1);
    }
    if (mysql_real_connect(conn, "localhost", "root", "parola", "OfflineMessenger", 0, NULL, 0) == NULL)
    {
        fprintf(stderr, "Am dat peste o eroare: %s\n", mysql_error(conn));
        mysql_close(conn);
        exit(1);
    }

    //pregatim interogarea pt a face update in OfflineMessages cand clientul citeste mesajele offline, pt a nu le mai trimite la o logare viitoare
    char update[100] = "UPDATE OfflineMessages SET isRead=1 WHERE messageId=";
    strcat(update, messageId);
    printf("%s\n", update);
    if (mysql_query(conn, update) != 0)
        fprintf(stderr, "Am dat peste o eroare la update date OfflineMessages: %s\n", mysql_error(conn));
    else
    {
        int affected_rows = mysql_affected_rows(conn);
        printf("update isRead=1 where messageId= %s,affected_rows=%d\n", messageId, affected_rows);
    }
}

int getMessageId(int sender, int receiver, MYSQL *conn)
{
    //selectam ultimul mesaj dintre doi useri

    //comanda select
    char select[100] = "SELECT id FROM Messages WHERE sender=";
    char buff[20];
    sprintf(buff, "%d", sender);
    strcat(select, buff);
    strcat(select, " AND receiver=");
    bzero(buff, sizeof(buff));
    sprintf(buff, "%d", receiver);
    strcat(select, buff);
    strcat(select, " ORDER BY date DESC LIMIT 1");

    printf("%s", select);

    if (mysql_query(conn, select) != 0)
    {
        fprintf(stderr, "Am dat peste o eroare: %s\n", mysql_error(conn));
        return -1;
    }
    else
    {
        MYSQL_RES *res = mysql_store_result(conn);
        if (res == NULL)
        {
            return -1;
        }
        else
        {
            MYSQL_ROW row = mysql_fetch_row(res);
            int count = atoi(row[0]);

            return count;
        }
    }
}

void sendOfflineMessages(thData td)
{
    char *raspuns = (char *)malloc(50 * sizeof(char));
    MYSQL *conn = mysql_init(NULL);

    if (conn == NULL)
    {
        fprintf(stderr, "%s\n", mysql_error(conn));
        exit(1);
    }
    if (mysql_real_connect(conn, "localhost", "root", "parola", "OfflineMessenger", 0, NULL, 0) == NULL)
    {
        fprintf(stderr, "Am dat peste o eroare: %s\n", mysql_error(conn));
        mysql_close(conn);
        exit(1);
    }

    char select[500] = "SELECT u1.name,m.text,m.date,m.id \
                        FROM Users u1 JOIN Messages m ON u1.id=m.sender \
                        JOIN Users u2 ON u2.id=m.receiver \
                        JOIN OfflineMessages om ON m.id=om.messageId \
                        WHERE m.receiver="; //11 AND om.isRead=0 \
                        ORDER BY m.date ASC;";
    char buff[20];
    sprintf(buff, "%d", td.idUser);
    strcat(select, buff);
    strcat(select, " AND om.isRead=0 ORDER BY m.date ASC");

    printf("%s\n", select);

    if (mysql_query(conn, select) != 0)
    {
        fprintf(stderr, "Am dat peste o eroare: %s\n", mysql_error(conn));
        int one = 1;
        strcpy(raspuns, "Eroare la citirea mesajelor offline. Incearca din nou.\n");
        write(td.cl, &one, sizeof(int));
        Write(td.cl, strlen(raspuns), raspuns);
        return;
    }
    else
    {
        MYSQL_RES *result = mysql_store_result(conn);
        int messagesCount = mysql_num_rows(result);
        write(td.cl, &messagesCount, sizeof(int)); //trimitem numarul de mesaje offline citite din db pe care urmeaza sa le trimitem la client
        MYSQL_ROW offlineMessage = mysql_fetch_row(result);
        //char *mesaj = (char*)malloc(50*sizeof(char));
        while (offlineMessage != NULL)
        {
            char mesaj[300];

            printf("name=%s\n", offlineMessage[0]);
            strcpy(mesaj, offlineMessage[0]);
            strcat(mesaj, "(");
            strcat(mesaj, offlineMessage[2]);
            strcat(mesaj, "): ");
            strcat(mesaj, offlineMessage[1]);
            strcat(mesaj, "\n");

            Write(td.cl, strlen(mesaj), mesaj);

            updateOfflineMessage(offlineMessage[3]);

            offlineMessage = mysql_fetch_row(result);
        }
    }
    mysql_close(conn);
}

//isOfflineMessage : 0=online message,1=offline message
char *insertMessage(int receiver, int sender, char *mesaj, int isOfflineMessage, int messageIdForReply)
{
    printf("am ajuns in insertMessage\n");

    char *raspuns = (char *)malloc(50 * sizeof(char));
    MYSQL *conn = mysql_init(NULL);

    if (conn == NULL)
    {
        fprintf(stderr, "%s\n", mysql_error(conn));
        exit(1);
    }
    if (mysql_real_connect(conn, "localhost", "root", "parola", "OfflineMessenger", 0, NULL, 0) == NULL)
    {
        fprintf(stderr, "Am dat peste o eroare: %s\n", mysql_error(conn));
        mysql_close(conn);
        exit(1);
    }

    printf("insertMessage text=%s\n", mesaj);

    char insert[500] = "INSERT INTO Messages(sender,receiver,text) VALUES (";
    char buff[20];
    sprintf(buff, "%d", sender);
    strcat(insert, buff);
    strcat(insert, ",");
    bzero(buff, sizeof(buff));

    sprintf(buff, "%d", receiver);
    strcat(insert, buff);
    strcat(insert, ",\"");
    strcat(insert, mesaj);
    strcat(insert, "\")");

    printf("%s", insert);

    if (mysql_query(conn, insert) != 0)
    {
        fprintf(stderr, "Am dat peste o eroare: %s\n", mysql_error(conn));
        strcpy(raspuns, "Eroare la trimiterea mesajului. Incearca din nou.\n");
    }
    else
        strcpy(raspuns, "OK"); //s-a trimis mesajul cu succes

    int messageId = getMessageId(sender, receiver, conn);
    printf("%d", messageId);

    //INSERT MESAJ OFFLINE
    if (isOfflineMessage == 1)
    {
        char insertOfflineMessage[100] = "INSERT INTO OfflineMessages(messageId,isRead) VALUES (";
        bzero(buff, sizeof(buff));
        sprintf(buff, "%d", messageId);
        strcat(insertOfflineMessage, buff);
        strcat(insertOfflineMessage, ",0)");
        printf("%s\n", insertOfflineMessage);

        if (mysql_query(conn, insertOfflineMessage) != 0)
        {
            fprintf(stderr, "Am dat peste o eroare: %s\n", mysql_error(conn));
            strcat(raspuns, "Eroare la trimiterea mesajului. Incearca din nou.\n");
            //mysql_close(conn);
        }
        else
        {
            strcat(raspuns, "OK"); //s-a trimis mesajul cu succes,apendam un OK la raspuns
            //mysql_close(conn);
        }
    }

    if (messageIdForReply > 0)
    {
        char insertReply[100] = "INSERT INTO Replies(messageId,replyId) VALUES (";
        bzero(buff, sizeof(buff));
        sprintf(buff, "%d", messageIdForReply);
        strcat(insertReply, buff);
        strcat(insertReply, ",");
        sprintf(buff, "%d", messageId);
        strcat(insertReply, buff);
        strcat(insertReply, ")");
        printf("%s\n", insertReply);

        if (mysql_query(conn, insertReply) != 0)
        {
            fprintf(stderr, "Am dat peste o eroare: %s\n", mysql_error(conn));
            strcat(raspuns, "Eroare la trimiterea mesajului. Incearca din nou.\n");
            mysql_close(conn);
        }
        else
        {
            printf("ALL gut!\n");
            fflush(stdout);
            strcat(raspuns, "OK"); //s-a trimis mesajul cu succes,apendam un OK la raspuns
            mysql_close(conn);
        }
    }
    printf("Am ajuns pana la final\n");
    fflush(stdout);
    printf("%s\n", raspuns);
    fflush(stdout);
    return raspuns; //daca s-a inserat corect totul -> OK - mesaj online/ OKOK - mesaj offline
}

char *sendMessage(int sd, int receiver, int sender, char mesaj[300], int messageIdForReply)
{
    bool userFound = false;
    char *raspuns = (char *)malloc(100 * sizeof(char));

    //check for users in current threads to see if the user is offline
    for (int i = 0; i <= nrClients; i++)
    {
        if (clients[i]->idUser == receiver) //gasim userul
        {
            userFound = true;
            if (clients[i]->cl != -1) //file descriptor/socket =-1 daca clientul respectiv e offline
            {
                printf("mesaj offline"); //send mesaj offline
                strcpy(raspuns, insertMessage(receiver, sender, mesaj, 1, messageIdForReply));
            }
            else
            {
                printf("mesaj online");
                strcpy(raspuns, insertMessage(receiver, sender, mesaj, 0, messageIdForReply));
            }
        }
        if (userFound)
            break;
    }
    if (userFound == false)
    { //userul nu s-a mai conectat la server => mesaj offline
        //send offline mesaj
        printf("mesaj offline");
        strcpy(raspuns, insertMessage(receiver, sender, mesaj, 1, messageIdForReply));
    }
    printf("%s", raspuns);
    return raspuns;
}

void sendFriends(thData td) //utilizatorii cu care a mai avut userul logat conversatii
{
    char buf[20];
    char raspuns[50];
    char select[300] = "select distinct u2.name,u2.id from Users u1 join Messages m on u1.id=m.receiver join Users u2 on u2.id=m.sender where m.receiver=";
    sprintf(buf, "%d", td.idUser);
    strcat(select, buf);
    strcat(select, " union select distinct u1.name,u1.id from Users u1 join Messages m on u1.id=m.receiver join Users u2 on u2.id=m.sender where m.sender=");
    strcat(select, buf);

    printf("%s", select);

    MYSQL *conn = mysql_init(NULL);
    if (conn == NULL)
    {
        fprintf(stderr, "%s\n", mysql_error(conn));
        exit(1);
    }

    if (mysql_real_connect(conn, "localhost", "root", "parola", "OfflineMessenger", 0, NULL, 0) == NULL)
    {
        fprintf(stderr, "Am dat peste o eroare: %s\n", mysql_error(conn));
        mysql_close(conn);
        exit(1);
    }

    if (mysql_query(conn, select) != 0)
    {
        fprintf(stderr, "Am dat peste o eroare: %s\n", mysql_error(conn));
        strcpy(raspuns, "Eroare la baza de date. Incearca din nou.\n");
        mysql_close(conn);
        Write(td.cl, strlen(raspuns), raspuns);
        return;
    }
    else
    {
        MYSQL_RES *result = mysql_store_result(conn);
        MYSQL_ROW row;

        char users[200];
        bzero(users, sizeof(users));
        strcpy(users, "Utilizatori:\n");
        while ((row = mysql_fetch_row(result)) != NULL)
        {
            strcat(users, row[0]);
            strcat(users, "\n");
        }
        Write(td.cl, strlen(users), users);
    }
}
void sendAll(thData td) //utilizatori cu care nu a mai avut conversatii
{
    char buf[20];
    char raspuns[50];
    char select[300] = "select name from Users where id!=";
    sprintf(buf, "%d", td.idUser);
    strcat(select, buf);

    printf("%s\n", select);

    MYSQL *conn = mysql_init(NULL);
    if (conn == NULL)
    {
        fprintf(stderr, "%s\n", mysql_error(conn));
        exit(1);
    }

    if (mysql_real_connect(conn, "localhost", "root", "parola", "OfflineMessenger", 0, NULL, 0) == NULL)
    {
        fprintf(stderr, "Am dat peste o eroare: %s\n", mysql_error(conn));
        mysql_close(conn);
        exit(1);
    }

    if (mysql_query(conn, select) != 0)
    {
        fprintf(stderr, "Am dat peste o eroare: %s\n", mysql_error(conn));
        strcpy(raspuns, "Eroare la baza de date. Incearca din nou.\n");
        mysql_close(conn);
        Write(td.cl, strlen(raspuns), raspuns);
        return;
    }
    else
    {
        MYSQL_RES *result = mysql_store_result(conn);
        MYSQL_ROW row;

        char users[200];
        bzero(users, sizeof(users));
        strcpy(users, "Utilizatori:\n");
        while ((row = mysql_fetch_row(result)) != NULL)
        {
            strcat(users, row[0]);
            strcat(users, "\n");
        }
        printf("%s", users);
        Write(td.cl, strlen(users), users);
    }
}
int checkIfUserExists(char user[40])
{
    MYSQL *conn = mysql_init(NULL);

    if (conn == NULL)
    {
        fprintf(stderr, "%s\n", mysql_error(conn));
        return true;
    }
    if (mysql_real_connect(conn, "localhost", "root", "parola", "OfflineMessenger", 0, NULL, 0) == NULL)
    {
        fprintf(stderr, "Am dat peste o eroare: %s\n", mysql_error(conn));
        mysql_close(conn);
        return true;
    }

    printf("checkIfUserExists nume=%s\n", user);
    char select[200] = "SELECT id from Users WHERE name=\"";
    strcat(select, user);
    strcat(select, "\"");

    printf("%s\n", select);
    //fflush(stdout);

    if (mysql_query(conn, select) != 0)
    {

        fprintf(stderr, "Am dat peste o eroare: %s\n", mysql_error(conn));
        mysql_close(conn);
    }
    else
    {
        MYSQL_RES *res = mysql_store_result(conn);
        if (res == NULL)
        {
            return -1;
        }
        else
        {
            MYSQL_ROW row = mysql_fetch_row(res);
            if (mysql_num_rows(res) == 0)
                return -1;
            int id = atoi(row[0]);
            mysql_close(conn);
            return id;
        }
        free(res);
        mysql_close(conn);
    }
    return -1;
}
void Register(int sd, thData td)
{
    char *username = (char *)malloc(40 * sizeof(char));
    char *password = (char *)malloc(30 * sizeof(char));
    char *raspuns = (char *)malloc(50 * sizeof(char));

    bzero(username, sizeof(username));
    bzero(password, sizeof(password));
    bzero(raspuns, sizeof(raspuns));

    Read(sd, strlen(username), username, td);
    Read(sd, strlen(password), password, td);
    if (username == NULL || password == NULL)
        strcpy(raspuns, "Completati username si parola!\n");

    MYSQL *conn = mysql_init(NULL);
    if (conn == NULL)
    {
        fprintf(stderr, "%s\n", mysql_error(conn));
        exit(1);
    }
    if (mysql_real_connect(conn, "localhost", "root", "parola", "OfflineMessenger", 0, NULL, 0) == NULL)
    {
        fprintf(stderr, "Am dat peste o eroare: %s\n", mysql_error(conn));
        mysql_close(conn);
        exit(1);
    }

    if (checkIfUserExists(username) > -1)
    {
        strcpy(raspuns, "Userul exista deja! Incearca alt username.\n");
        Write(sd, strlen(raspuns), raspuns);
        mysql_close(conn);
        return;
    }
    else
    {
        char insert[100] = "INSERT INTO Users(name,password) VALUES (\"";
        strcat(insert, username);
        strcat(insert, "\",\"");
        strcat(insert, password);
        strcat(insert, "\")");

        printf("%s", insert);
        //fflush(stdout);

        if (mysql_query(conn, insert) != 0)
        {
            fprintf(stderr, "Am dat peste o eroare: %s\n", mysql_error(conn));
            strcpy(raspuns, "Eroare la baza de date. Incearca din nou.\n");
            mysql_close(conn);
        }
        else
        {
            strcpy(raspuns, "Te-ai inregistrat cu succes!\n");
            mysql_close(conn);
        }
    }
    Write(sd, strlen(raspuns), raspuns);
}
int login(int sd, thData data)
{
    char *username = (char *)malloc(40 * sizeof(char));
    char *password = (char *)malloc(30 * sizeof(char));
    char *raspuns = (char *)malloc(50 * sizeof(char));

    bzero(username, sizeof(username));
    bzero(password, sizeof(password));
    bzero(raspuns, sizeof(raspuns));

    Read(sd, strlen(username), username, data);
    Read(sd, strlen(password), password, data);

    MYSQL *conn = mysql_init(NULL);
    if (conn == NULL)
    {
        fprintf(stderr, "%s\n", mysql_error(conn));
        exit(1);
    }
    if (mysql_real_connect(conn, "localhost", "root", "parola", "OfflineMessenger", 0, NULL, 0) == NULL)
    {
        fprintf(stderr, "Am dat peste o eroare: %s\n", mysql_error(conn));
        mysql_close(conn);
        exit(1);
    }
    char select[200] = "SELECT id from Users WHERE name=\"";
    strcat(select, username);
    strcat(select, "\" and password=\"");
    strcat(select, password);
    strcat(select, "\"");

    printf("%s\n", select);
    //fflush(stdout);

    if (mysql_query(conn, select) != 0)
    {
        fprintf(stderr, "Am dat peste o eroare: %s\n", mysql_error(conn));
        strcpy(raspuns, "Eroare la baza de date. Incearca din nou.\n");
        mysql_close(conn);
        return -1;
    }
    else
    {
        MYSQL_RES *result = mysql_store_result(conn);
        if (mysql_num_rows(result) == 0)
        {
            strcpy(raspuns, "Username/parola gresita!");
            Write(sd, strlen(raspuns), raspuns);
            mysql_close(conn);
            return -1;
        }
        else if (mysql_num_rows(result) == 1)
        {
            strcpy(raspuns, "Te-ai logat cu succes!");
            Write(sd, strlen(raspuns), raspuns);
            MYSQL_ROW row = mysql_fetch_row(result);
            //data.idUser = atoi(row[0]);                   //salvam local,pe threadul pe care e logat userul id-ul acestuia.
            clients[data.idThread]->idUser = atoi(row[0]); //salvam global id-ul userului, ca sa-l putem accesa si din alte threaduri
            mysql_close(conn);
            return atoi(row[0]);
        }
        else
        {
            strcpy(raspuns, "Eroare la baza de date. Incearca din nou.\n");
            Write(sd, strlen(raspuns), raspuns);
            mysql_close(conn);
            return -1;
        }
    }
}

void sendConvo(thData tdL, int id2)
{
    char *raspuns = (char *)malloc(50 * sizeof(char));
    MYSQL *conn = mysql_init(NULL);

    if (conn == NULL)
    {
        fprintf(stderr, "%s\n", mysql_error(conn));
        exit(1);
    }
    if (mysql_real_connect(conn, "localhost", "root", "parola", "OfflineMessenger", 0, NULL, 0) == NULL)
    {
        fprintf(stderr, "Am dat peste o eroare: %s\n", mysql_error(conn));
        mysql_close(conn);
        exit(1);
    }

    char select[500] = "select m1.id,u1.name,m1.date,m1.text,\
    m2.id,u2.name,m2.date,m2.text \
    from Users u1 join Messages m1 on u1.id=m1.sender \
    left join Replies r on r.messageId=m1.id \
    left join Messages m2 on r.replyId=m2.id \
    left join Users u2 on m2.sender=u2.id \
    where (m1.sender=";

    char buff1[20], buff2[20];
    sprintf(buff1, "%d", tdL.idUser);
    sprintf(buff2, "%d", id2);
    strcat(select, buff1);
    strcat(select, " && m1.receiver=");
    strcat(select, buff2);
    strcat(select, ") || (m1.sender=");
    strcat(select, buff2);
    strcat(select, "  && m1.receiver=");
    strcat(select, buff1);
    strcat(select, ") order by m1.date asc");

    printf("%s\n", select);

    if (mysql_query(conn, select) != 0)
    {
        fprintf(stderr, "Am dat peste o eroare: %s\n", mysql_error(conn));
        int one = 1;
        strcpy(raspuns, "Eroare la citirea mesajelor offline. Incearca din nou.\n");
        write(tdL.cl, &one, sizeof(int));
        Write(tdL.cl, strlen(raspuns), raspuns);
        return;
    }
    else
    {
        MYSQL_RES *result = mysql_store_result(conn);
        int messagesCount = mysql_num_rows(result);
        printf("number of messages=%d", messagesCount);
        write(tdL.cl, &messagesCount, sizeof(int)); //trimitem numarul de mesaje offline citite din db pe care urmeaza sa le trimitem la client
        char messages[100][200];
        int messagesIds[100];
        MYSQL_ROW row = mysql_fetch_row(result);
        int i = 0;
        char buff[20];
        while (row != NULL)
        {
            sprintf(buff, "%d", i);
            if (row[4] == NULL)
            { //mesaj simplu
                strcpy(messages[i], buff);
                strcat(messages[i], ".");
                strcat(messages[i], row[1]);
                strcat(messages[i], "(");
                strcat(messages[i], row[2]);
                strcat(messages[i], "): ");
                strcat(messages[i], row[3]);
                strcat(messages[i], "\n");
                messagesIds[i] = atoi(row[0]); //pastram id-ul mesajului
            }
            else
            { //reply la un mesaj
                strcpy(messages[i], buff);
                strcat(messages[i], ".");
                strcat(messages[i], "as reply to: ");
                strcat(messages[i], row[1]);
                strcat(messages[i], "(");
                strcat(messages[i], row[2]);
                strcat(messages[i], "): ");
                strcat(messages[i], row[3]);
                strcat(messages[i], "\nanswered: ");
                strcat(messages[i], row[5]);
                strcat(messages[i], "(");
                strcat(messages[i], row[6]);
                strcat(messages[i], "): ");
                strcat(messages[i], row[7]);
                strcat(messages[i], "\n");
                messagesIds[i] = atoi(row[4]);
                //row=mysql_fetch_row(result);
            }
            printf("%s", messages[i]);
            Write(tdL.cl, sizeof(messages[i]), messages[i]);
            i = i + 1;
            row = mysql_fetch_row(result);
        }
        mysql_close(conn);

        while (1)
        {
            int optiune;
            read(tdL.cl, &optiune, sizeof(int));
            if (optiune == 1)
            {
                int replyId;
                read(tdL.cl, &replyId, sizeof(int));
                char reply[100];
                Read(tdL.cl, strlen(reply), reply, tdL);
                printf("%d %s", replyId, reply);
                if (replyId >= 0 && replyId < messagesCount)
                {
                    char *ok = (char *)malloc(100 * sizeof(char));
                    strcpy(ok, sendMessage(tdL.cl, id2, tdL.idUser, reply, messagesIds[replyId]));
                    //verificam daca id-ul e corect
                    printf("%s\n", ok);
                    if (strcmp("OKOKOK", ok) == 0)
                        Write(tdL.cl, strlen("Reply trimis!\n"), "Reply trimis!\n");
                    else
                        Write(tdL.cl, strlen("Eroare la trimitere reply..\n"), "Eroare la trimitere reply..\n");
                }
                else
                    Write(tdL.cl, strlen("Numarul mesajului este gresit.\n"), "Numarul mesajului este gresit.\n");
            }
            else if (optiune == 2)
                break;
        }
    }
    //mysql_close(conn);
}

void readMessage(thData tdL)
{
    char user[40], mesaj[100];
    bzero(mesaj, sizeof(mesaj));
    bzero(user, sizeof(user));
    Read(tdL.cl, strlen(user), user, tdL);
    Read(tdL.cl, strlen(mesaj), mesaj, tdL);
    printf("user=%s\nmesaj=%s\n", user, mesaj);

    int receiver = checkIfUserExists(user);
    if (receiver > -1)
        sendMessage(tdL.cl, receiver, tdL.idUser, mesaj, -1);
}

void SeeAllUsersMenu(thData tdL)
{
    sendAll(tdL);
    int option;
    //luam toti utilizatorii; daca utilizatorul alege 1, se citeste mesajul
    read(tdL.cl, &option, sizeof(int));
    if (option == 1)
        readMessage(tdL);
    if (option == 2)
        return; //back
}

void seeFriendsMenu(thData tdL)
{
    sendFriends(tdL);
    int option;
    //luam toti utilizatorii
    read(tdL.cl, &option, sizeof(int));

    if (option == 1)
    {
        SeeAllUsersMenu(tdL);
        printf("am iesit din SeeAllUsersMenu!\n");
        return;
    }
    else if (option == 2)
    {
        printf("\ntrebuie sa citesc un mesaj\n");
        readMessage(tdL); //se trimite mesajul
        return;
    }
    else return;
}

void seeHistoryMenu(thData tdL)
{
    sendFriends(tdL);
    int option;
    //luam toti utilizatorii
    read(tdL.cl, &option, sizeof(int));

    if (option == 1)
    {
        char user[40];
        Read(tdL.cl, sizeof(user), user, tdL);
        int ok = checkIfUserExists(user);
        if (ok > 0)
            sendConvo(tdL, ok);
    }
    else return;
}
int loginMenu(thData tdL)
{
    sendOfflineMessages(tdL); //ii trimitem mesajele primite offline
    //citim optiunea de la user
    int option;
    read(tdL.cl, &option, sizeof(int));
    while (1)
    {
        if (option == 3)
            return option;
        else if (option == 1)
            seeFriendsMenu(tdL);
        else if (option == 2) //vezi istoric
            seeHistoryMenu(tdL);
        read(tdL.cl, &option, sizeof(int));
    }
    return -1;
}

void raspunde(void *arg)
{
    int nr, i = 0;
    struct thData tdL;
    tdL = *((struct thData *)arg);
    char command[30];
    char response[30];
    int len;
    while (1)
    {
        bzero(response, sizeof(response));
        Read(tdL.cl, strlen(response), response, tdL);
        if (strlen(response) == 0)
            break;
        printf("[Thread %d]Mesajul a fost receptionat...%s\n", tdL.idThread, response);

        //sendMessage(tdL.cl, 11,7);

        if (strcmp(response, "register") == 0)
        {
            Register(tdL.cl, tdL);
        }
        if (strcmp(response, "login") == 0)
        {
            tdL.idUser = login(tdL.cl, tdL);
        }
        if (strcmp(response, "exit") == 0)
            break;

        if (tdL.idUser > -1) //userul s-a logat la iteratia curenta
        {
            loginMenu(tdL);
            tdL.idUser = -1;
        }
    }
}