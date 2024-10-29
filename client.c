#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

char *fifo = "fifo_channel";
char *fifo2_response = "fifo_channel_response";

int write_fd_fifo;
int read_fd_fifo;

char command[256];
char response[256];
int response_length;

char username[256];
char approved_username[256];

void handle_error(const char *message)
{
    perror(message);
    exit(EXIT_FAILURE);
}

void create_fifos()
{
    if ((mkfifo(fifo, 0666) == -1) && errno != EEXIST)
    {
        handle_error("Error when creating the fifo in client\n");
    }

    if ((mkfifo(fifo2_response, 0666) == -1) && errno != EEXIST)
    {
        handle_error("Error when creating the fifo_response in client\n");
    }
}

int main()
{

    create_fifos();

    if (-1 == (write_fd_fifo = open(fifo, O_WRONLY))) // opening writing end of fifo for commands
    {
        handle_error("Client: Error when opening fifo writing end\n");
    }

    while (1) // listen for commands from the user
    {

        printf("Enter a command: ");

        fgets(command, sizeof(command), stdin); // get the command

        if (command[strlen(command) - 1] == '\n') // remove the newline character from the command and add string terminator
            command[strlen(command) - 1] = '\0';

        if (-1 == (write(write_fd_fifo, command, strlen(command) + 1))) // write the command in fifo to be sent to the server
        {
            handle_error("Client: Error when writing in fifo channel\n");
        }

        if (-1 == (read_fd_fifo = open(fifo2_response, O_RDONLY))) // open fifo for reading the response from the server
        {
            handle_error("Client: Error when opening the fifo_response for reading");
        }

        memset(response, 0, sizeof(response));

        read(read_fd_fifo, &response_length, sizeof(int));

        if (-1 == (read(read_fd_fifo, response, response_length))) // read the response from the server
        {
            handle_error("Client: Error when reading response from fifo\n");
        }

        printf("\n");
        printf("%s\n", response); // print the according response

        if (strcmp(response, "Quitting...\n") == 0)
        {
            exit(EXIT_SUCCESS);
        }

        close(read_fd_fifo);
    }

    close(write_fd_fifo); // close everything
    unlink(fifo);
    unlink(fifo2_response);

    return 0;
}