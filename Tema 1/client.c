#include <stdio.h>

#include <stdlib.h>

#include <errno.h>

#include <string.h>

#include <fcntl.h>

#include <sys/types.h>

#include <sys/stat.h>

#include <unistd.h>



#define C_to_S "C_to_S_FIFO"

#define S_to_C "S_to_C_FIFO"



int main()

{

    char *command = malloc(30 * sizeof(char));



    int num, CtoS, StoC;

    int responseLength;

    int login = 0;



    CtoS = open(C_to_S, O_WRONLY);

    StoC = open(S_to_C, O_RDONLY);



    while (1)

    {

        printf("Comanda: ");

        scanf("%s", command);



        if (strcmp(command, "quit") == 0)

            break;

        else

            write(CtoS, command, 30);



        char *response = malloc(100 * sizeof(char));

        read(StoC, &responseLength, sizeof(responseLength));

        read(StoC, response, responseLength);



        printf("Raspuns: %s\n", response);

        if (strcmp(response, "Esti autentificat pe server!") == 0)

            login = 1;

    }

}