#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#define BUFFER_LENGTH 256

int main(int argc, char *argv[])
{
    int ret, fd, len;
    char receive[BUFFER_LENGTH];
    char stringToSend[BUFFER_LENGTH];
    char *processName;

    if (argc < 2)
    {
        printf("Usage: %s <process_name>\n", argv[0]);
        return 1;
    }

    processName = (char *)malloc(strlen(argv[1]) + 1);
    strncpy(processName, argv[1], strlen(argv[1]));
    processName[strlen(argv[1])] = '\0';

    printf("Starting device test code example as process: %s\n", processName);

    fd = open("/dev/mq_driver", O_RDWR);
    if (fd < 0)
    {
        perror("Failed to open the device...");
        return errno;
    }

    char *command = (char *)malloc(strlen(processName) + 6);
    sprintf(command, "/reg %s", processName);
    if (write(fd, command, strlen(command)) < 0)
    {
        perror("Failed to register process");
        free(command);
        close(fd);
        return errno;
    }
    free(command);

    printf("Process registered as: %s\n", processName);

    free(processName);

    while (1)
    {
        printf("Type in '/<process_name> <message>' to send message to process, or\njust ENTER to read messages, or\n'/unreg <process_name>' to exit):\n");

        memset(stringToSend, 0, BUFFER_LENGTH);
        fgets(stringToSend, BUFFER_LENGTH - 1, stdin);
        len = strnlen(stringToSend, BUFFER_LENGTH);
        stringToSend[len - 1] = '\0';

        if (strncmp(stringToSend, "/unreg ", 7) == 0)
        {
            ret = write(fd, stringToSend, strlen(stringToSend));
            if (ret < 0)
            {
                perror("Failed to unregister process");
                continue;
            }
            break;
        }

        if (stringToSend[0] == '/')
        {
            ret = write(fd, stringToSend, strlen(stringToSend));
            if (ret < 0)
            {
                perror("Failed to send message");
            }
            else
            {
                printf("Message sent\n");
            }
            continue;
        }

        if (stringToSend[0] == '\0')
        {
            ret = read(fd, receive, BUFFER_LENGTH);
            if (ret < 0)
            {
                perror("Failed to read the message");
            }
            else if (ret == 0)
            {
                printf("No messages to read\n");
            }
            else
            {
                printf("Received message: %s\n", receive);
            }
            continue;
        }
    }

    close(fd);
    printf("End of the program\n");
    return 0;
}