#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/socket.h>
#include <utmp.h>
#include <time.h>

char *fifo = "fifo_channel";
char *fifo2_response = "fifo_channel_response";
char *config_file = "config.txt";

char command[256];
char username[256];
char approved_username[256];

char response_message[256];
int length;

int is_logged_in = 0;

int read_fd_fifo;
int write_fd_fifo;

int sockets_child1[2], sockets_child2[2], sockets_child3[2], sockets_child4[2];

int pipe_login_status[2];

int child1_pid, child2_pid, child3_pid, child4_pid;

void handle_error(const char *message)
{
    perror(message);
    exit(EXIT_FAILURE);
}

void initilize_sockets()
{
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets_child1) < 0)
    {
        handle_error("Server: Error at initilizing socket for child 1\n");
    }

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets_child2) < 0)
    {
        handle_error("Server: Error at initilizing socket for child 2\n");
    }

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets_child3) < 0)
    {
        handle_error("Server: Error at initilizing socket for child 3\n");
    }

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets_child4) < 0)
    {
        handle_error("Server: Error at initilizing socket for child 4\n");
    }
}

void initilize_children()
{
    if (-1 == (child1_pid = fork()))
    {
        handle_error("Server: child 1 Fork() error\n");
    }

    if (child1_pid != 0)
    {
        if (-1 == (child2_pid = fork()))
        {
            handle_error("Server: child 2 Fork() error\n");
        }
    }

    if ((child1_pid != 0) && (child2_pid != 0))
    {
        if (-1 == (child3_pid = fork()))
        {
            handle_error("Server: child 3 Fork() error\n");
        }
    }

    if ((child1_pid != 0) && (child2_pid != 0) && (child3_pid != 0))
        if (-1 == (child4_pid = fork()))
        {
            handle_error("Server: child 4 Fork() error\n");
        }
}

void create_fifos()
{
    if ((mkfifo(fifo, 0666) == -1) && errno != EEXIST) // create the fifo channel if it doesnt already exist
    {
        handle_error("Server: Error when creating the fifo\n");
    }

    if ((mkfifo(fifo2_response, 0666) == -1) && errno != EEXIST) // create the fifo channel if it doesnt already exist
    {
        handle_error("Server: Error when creating the fifo_response\n");
    }
}

int main()
{

    if (-1 == pipe(pipe_login_status))
    {
        handle_error("Server: Error when initilizing pipe");
    }

    initilize_sockets();

    initilize_children();

    create_fifos(); // initilize everthing

    if ((child1_pid > 0) && (child2_pid > 0) && (child3_pid > 0) && (child4_pid > 0)) // Parent
    {
        close(sockets_child1[0]);
        close(sockets_child2[0]);
        close(sockets_child3[0]);
        close(sockets_child4[0]);

        if (-1 == (read_fd_fifo = open(fifo, O_RDONLY))) // open fifo for reading the commands from the client
        {
            handle_error("Server: Error when opening the fifo for reading in parent");
        }

        while (1)
        {

            if (read(read_fd_fifo, command, sizeof(command)) == -1) // read the command
            {
                handle_error("Server: Error when reading command received from client\n");
            }

            if (strncmp(command, "login:", 6) == 0) // if the command is "login:"" => send to child1
            {

                if (is_logged_in == 0) // Nobody is logged in at the moment
                {
                    if (-1 == write(sockets_child1[1], command, sizeof(command))) // write it into the socket of the child that handles login
                    {
                        handle_error("Server: Error when sending command to child1 with socket\n");
                    }

                    read(sockets_child1[1], &length, sizeof(int));

                    if (-1 == read(sockets_child1[1], response_message, length))
                    {
                        handle_error("Server: Error when reading response from child1\n");
                    }
                }
                else if (is_logged_in == 1)
                {
                    strcpy(response_message, "Somebody is already logged in! Try again later!\n");
                    length = strlen(response_message);
                }

                response_message[length] = '\0';
                if (strcmp(response_message, "Successful login!") == 0)
                {
                    is_logged_in = 1;
                }

                if (-1 == (write_fd_fifo = open(fifo2_response, O_WRONLY))) // open fifo for writing
                {
                    handle_error("Server: Error when opening the fifo_response for writing");
                }

                write(write_fd_fifo, &length, sizeof(int));

                if (-1 == (write(write_fd_fifo, response_message, length))) // send the response to the client
                {
                    handle_error("Server: Error when sending response in child1\n");
                }

                close(write_fd_fifo);

            } // end of login command

            else if (strcmp(command, "logout") == 0)
            {
                if (is_logged_in == 1)
                {
                    if (-1 == (write_fd_fifo = open(fifo2_response, O_WRONLY)))
                    {
                        handle_error("Server: Error when opening fifo channel for logout");
                    }

                    is_logged_in = 0;

                    strcpy(response_message, "You are now logged out!\n");
                    length = strlen(response_message);

                    if (-1 == (write(write_fd_fifo, &length, sizeof(int))))
                    {
                        handle_error("Server: Error when writing response for length for logout if user is logged in");
                    }

                    if (-1 == (write(write_fd_fifo, response_message, length)))
                    {
                        handle_error("Server: Error when writing response message for logout if user is logged in");
                    }

                    close(write_fd_fifo);
                }
                else
                {
                    if (-1 == (write_fd_fifo = open(fifo2_response, O_WRONLY)))
                    {
                        handle_error("Server: Error when opening fifo channel for logout");
                    }

                    strcpy(response_message, "Already logged out!\n");
                    length = strlen(response_message);

                    if (-1 == (write(write_fd_fifo, &length, sizeof(int))))
                    {
                        handle_error("Server: Error when writing response for length for logout if user is not logged in");
                    }

                    if (-1 == (write(write_fd_fifo, response_message, length)))
                    {
                        handle_error("Server: Error when writing response message for logout if user is not logged in");
                    }

                    close(write_fd_fifo);
                }
            }

            else if (strcmp(command, "quit") == 0)
            {
                if (-1 == (write_fd_fifo = open(fifo2_response, O_WRONLY)))
                {
                    handle_error("Server: Error when opening fifo channel for logout");
                }

                strcpy(response_message, "Quitting...\n");
                length = strlen(response_message);

                write(write_fd_fifo, &length, sizeof(int));

                write(write_fd_fifo, response_message, length);

                exit(EXIT_SUCCESS);

                close(write_fd_fifo);
            }

            else if (strcmp(command, "get-logged-users") == 0)
            {
                close(pipe_login_status[0]);
                // if (-1 == write(sockets_child2[1], &is_logged_in, sizeof(int)))
                // {
                //    handle_error("Server: Error when writing in sockets_child2 in parent\n");
                //}

                if (-1 == write(pipe_login_status[1], &is_logged_in, sizeof(int)))
                {
                    handle_error("Server: Error when writing in pipe in parent for get-logged-users");
                }

                read(sockets_child2[1], &length, sizeof(int));

                read(sockets_child2[1], response_message, length);

                if (-1 == (write_fd_fifo = open(fifo2_response, O_WRONLY)))
                {
                    handle_error("Server: Error when opening fifo channel for logout");
                }

                write(write_fd_fifo, &length, sizeof(int));

                write(write_fd_fifo, response_message, length);

                close(write_fd_fifo);
            }

            else if (strncmp(command, "get-proc-info:", 14) == 0)
            {
                char pid[32] = {0};

                strcpy(pid, command + 14);
                length = strlen(pid);

                write(sockets_child3[1], &is_logged_in, sizeof(int));

                write(sockets_child3[1], &length, sizeof(int));

                write(sockets_child3[1], pid, length);

                read(sockets_child3[1], &length, sizeof(int));

                read(sockets_child3[1], response_message, length);

                write_fd_fifo = open(fifo2_response, O_WRONLY);

                write(write_fd_fifo, &length, sizeof(int));

                write(write_fd_fifo, response_message, length);

                close(write_fd_fifo);
            }

            else
            {
                strcpy(response_message, "Not a valid command!\n");
                length = strlen(response_message);

                write_fd_fifo = open(fifo2_response, O_WRONLY);

                write(write_fd_fifo, &length, sizeof(int));

                write(write_fd_fifo, response_message, length);
            }

        } // end of while(1) for reading commands

        exit(EXIT_SUCCESS);

        // END OF PARENT
    }
    else if (child1_pid == 0)
    { // CHILD1  //LOGIN: COMMAND

        close(sockets_child1[1]);
        int username_found = 0;

        while (1)
        {

            username_found = 0;

            if (-1 == (read(sockets_child1[0], username, sizeof(username)))) // read the username from the pipe
            {
                handle_error("Server: Error when reading username in child1\n");
            }

            strcpy(username, username + 6);

            FILE *file = fopen(config_file, "r"); // open the file of approved users
            if (file == NULL)
            {
                handle_error("Server: Error when opening config file in child1\n");
            }

            while (fgets(approved_username, sizeof(approved_username), file)) // Get each of the approved usernames
            {

                if (approved_username[strlen(approved_username) - 1] == '\n') // remove the newline character from the username and add string terminator
                    approved_username[strlen(approved_username) - 1] = '\0';

                if (strcmp(username, approved_username) == 0) // if we found the username in the approved usernames file
                {
                    strcpy(response_message, "Successful login!");
                    length = strlen(response_message);
                    username_found = 1;

                    break; // dont need to look for other usernames
                }
            } // end of while looking through approved usernames

            fclose(file);

            if (username_found == 0)
            {
                memset(response_message, 0, sizeof(response_message));
                strcpy(response_message, "Username not found!\n");
                length = strlen(response_message);
            }

            write(sockets_child1[0], &length, sizeof(int));

            if (-1 == write(sockets_child1[0], response_message, length))
            {
                handle_error("Server: sending response to parent from c1\n");
            }
        }
        memset(response_message, 0, sizeof(response_message));

        close(write_fd_fifo);

        exit(EXIT_SUCCESS);

        // END OF CHILD 1 LOGIN:
    }
    else if (child2_pid == 0)
    { // child2    GET-LOGGED_USERS

        close(pipe_login_status[1]);
        while (1)
        {

            if (-1 == read(pipe_login_status[0], &is_logged_in, sizeof(int)))
            {
                handle_error("Server: Error when reading sockets_child2 in child2\n");
            }

            if (is_logged_in == 1)
            {

                struct utmp *ut;

                char user[32] = {0};
                char all_users[1024] = {0};

                char host[256] = {0};
                char login_time[64] = {0};

                setutent();

                while ((ut = getutent()) != NULL)
                {

                    strncpy(user, ut->ut_user, sizeof(user));
                    strncpy(host, ut->ut_host, sizeof(host));

                    sprintf(login_time, "%u", ut->ut_tv.tv_sec);

                    strcat(all_users, "User: ");
                    strcat(all_users, user);

                    strcat(all_users, ", ");

                    strcat(all_users, "Host: ");
                    strcat(all_users, host);

                    strcat(all_users, ", ");

                    strcat(all_users, "Login time: ");
                    strcat(all_users, login_time);

                    strcat(all_users, "\n");
                }

                length = strlen(all_users);

                write(sockets_child2[0], &length, sizeof(int));

                write(sockets_child2[0], all_users, length);

                endutent();

                memset(all_users, 0, sizeof(all_users));
            }
            else
            {
                strcpy(response_message, "You must be logged in to use this command\n");

                int length = strlen(response_message);

                write(sockets_child2[0], &length, sizeof(int));

                if (-1 == write(sockets_child2[0], response_message, length))
                {
                    handle_error("Server: Error when sending response message from child2 in case user is not logged in\n");
                }
            }
        }

        exit(EXIT_SUCCESS);
    }
    else if (child3_pid == 0)
    { // child3    GET-PROC-INFO
        char pid[256] = {0};
        char info[512] = {0};
        char all_proc_info[1024] = {0};
        char proc_name[256] = "/proc/";

        while (1)
        {

            read(sockets_child3[0], &is_logged_in, sizeof(int));

            read(sockets_child3[0], &length, sizeof(int));

            read(sockets_child3[0], pid, length);

            pid[length] = '\0';

            strcpy(proc_name, "/proc/");

            if (is_logged_in == 1)
            {
                strcat(proc_name, pid);

                if (access(proc_name, F_OK) == 0)
                {
                    strcat(proc_name, "/status");

                    FILE *file = fopen(proc_name, "r");

                    if (file == NULL)
                    {
                        handle_error("Server: Error when opening proc file in child3");
                    }

                    while (fgets(info, sizeof(info), file))
                    {
                        if (strncmp(info, "Name:", 5) == 0)
                        {
                            strcat(all_proc_info, info);
                        }

                        if (strncmp(info, "State:", 6) == 0)
                        {
                            strcat(all_proc_info, info);
                        }

                        if (strncmp(info, "PPid:", 5) == 0)
                        {
                            strcat(all_proc_info, info);
                        }

                        if (strncmp(info, "Uid:", 4) == 0)
                        {
                            strcat(all_proc_info, info);
                        }

                        if (strncmp(info, "VmSize:", 7) == 0)
                        {
                            strcat(all_proc_info, info);
                        }
                    }

                    length = strlen(all_proc_info);
                    write(sockets_child3[0], &length, sizeof(int));

                    write(sockets_child3[0], all_proc_info, length);

                    memset(all_proc_info, 0, sizeof(all_proc_info));
                    fclose(file);
                }
                else
                {
                    strcpy(response_message, "Proc does not exist\n");
                    length = strlen(response_message);

                    write(sockets_child3[0], &length, sizeof(int));

                    write(sockets_child3[0], response_message, length);
                }
            }
            else
            {
                strcpy(response_message, "You must be logged in to use this command!\n");

                length = strlen(response_message);

                write(sockets_child3[0], &length, sizeof(int));

                write(sockets_child3[0], response_message, length);
            }
        }

        exit(EXIT_SUCCESS);
    }

    return 0;
}