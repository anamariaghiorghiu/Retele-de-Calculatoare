#include <stdio.h>

#include <stdlib.h>

#include <errno.h>

#include <string.h>

#include <fcntl.h>

#include <sys/types.h>

#include <sys/stat.h>

#include <unistd.h>

#include <sys/wait.h>

#include <string.h>

#include <utmp.h>

#include <sys/socket.h>



#define C_to_S "C_to_S_FIFO"

#define S_to_C "S_to_C_FIFO"



char *get_logged_users()

{

    int CtoP[2];



    if (pipe(CtoP) == -1)

    {

        fprintf(stderr, "pipe\n");

        exit(1);

    }



    switch (fork())

    {

    case -1:

        fprintf(stderr, "fork - 1\n");

        exit(1);

    case 0:

        close(CtoP[0]);

        char v[256];

        struct utmp *var = malloc(sizeof(struct utmp));

        char *response = malloc(1024);

        var = getutent();



        int count = 0;

        while (count < 2)

        {

            snprintf(v, 32, "%s", var->ut_user);

            strcat(response, v);

            strcat(response, ",");



            snprintf(v, 256, "%s", var->ut_host);

            strcat(response, v);

            strcat(response, ",");



            snprintf(v, 30, "%d", var->ut_tv.tv_sec);

            strcat(response, v);

            strcat(response, ",");



            snprintf(v, 30, "%d", var->ut_tv.tv_usec);

            strcat(response, v);

            strcat(response, "\n");



            var = getutent();

            count += 1;

        }



        int len = strlen(response);

        write(CtoP[1], &len, sizeof(int));

        write(CtoP[1], response, len);

        exit(1);

    }



    close(CtoP[1]);

    char *response = malloc(100 * sizeof(char));

    int len;

    read(CtoP[0], &len, sizeof(int));

    read(CtoP[0], response, len);



    while (wait(NULL) != -1)

        ;

    return response;

}

char *get_proc_info(char *pid)

{

    int sockp[2], child;

    char msg[1024];

    char *response2 = malloc(100 * sizeof(char));



    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockp) < 0)

    {

        perror("Err... socketpair");

        exit(1);

    }



    if ((child = fork()) == -1)

        perror("Err...fork");

    else if (child) //parinte

    {

        close(sockp[1]);

        int len;

        if (read(sockp[0], &len, sizeof(int)) < 0) //returneaza nr de bytes pe care i a citit din socket/pipe/fifo (idem write)

            perror("[Parinte]Err...read");

        if (read(sockp[0], response2, len) < 0)

            perror("[Parinte]Err...write");

        close(sockp[0]);

    }

    else //copil

    {

        close(sockp[0]);

        FILE *fp;

        int k = 0;

        char *line = NULL;

        char *file_name = malloc(30 * sizeof(char));

        char *response = malloc(100 * sizeof(char));

        size_t len = 0;

        ssize_t read;

        strcat(file_name, "/proc/");

        strcat(file_name, pid);

        strcat(file_name, "/status");



        fp = fopen(file_name, "r");

        if (fp == NULL)

            exit(EXIT_FAILURE);



        while ((read = getline(&line, &len, fp)) != -1)

        {

            if (k == 0 || k == 2 || k == 6 || k == 8 || k == 17)

            {

                strcat(response, line);

            }

            k++;

        }

        fclose(fp);

        if (line)

            free(line);

        int len2 = strlen(response);

        if (write(sockp[1], &len2, sizeof(int)) < 0)

            perror("[copil]Err...write");

        if (write(sockp[1], response, len2) < 0)

            perror("[copil]Err..write");



        close(sockp[1]);

        exit(1);

    }

    printf("%s", response2);

    return response2;

}

int main()

{

    char *command = malloc(30 * sizeof(char));

    char *response = malloc(100 * sizeof(char));

    int num, CtoS, StoC;

    int responseLength = strlen(response);

    int nr = 0;

    int login = 0;



    mknod(C_to_S, S_IFIFO | 0666, 0);

    mknod(S_to_C, S_IFIFO | 0666, 0);



    CtoS = open(C_to_S, O_RDONLY);

    StoC = open(S_to_C, O_WRONLY);



    while (1)

    {

        //citim comanda

        read(CtoS, command, 30);

        printf("%s\n", command);



        if (strcmp(command, "logout") == 0)

        {

            response = "Ai iesit de pe server!";

            login = 0;

        }

        else if (strcmp(command, "get-logged-users") == 0 && login == 1)

        {

            response = get_logged_users();

        }

        else if (strcmp(command, "get-logged-users") == 0 && login == 0)

        {

            response = "Nu esti autentificat.";

        }



        char *command2 = malloc(15 * sizeof(char));

        char *data = malloc(15 * sizeof(char));



        command2 = strtok(command, " :");

        data = strtok(NULL, " :");

        if (strcmp(command2, "get-proc-info") == 0 && login == 1)

        {

            response = get_proc_info(data);

        }

        if (strcmp(command2, "get-proc-info") == 0 && login == 0)

        {

            response = "Nu esti autentificat.";

        }

        if (strcmp(command2, "login") == 0)

        {

            response = "Esti autentificat pe server!";

            login = 1;

        }



        //trimitem raspuns

        // response="Am primit comanda!";

        responseLength = strlen(response);

        write(StoC, &responseLength, sizeof(responseLength));

        write(StoC, response, responseLength);

    }

}