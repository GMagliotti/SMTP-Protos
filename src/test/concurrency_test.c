#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SMTPD_PORT 2525
#define SMTPD_SERVER "127.0.0.1"
#define NUM_THREADS 500

void *send_email(void *threadid) {
    long tid = (long)threadid;

    int sockfd;
    struct sockaddr_in servaddr;
    char buffer[1024];
    ssize_t n;

    // Create socket
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket creation failed");
        pthread_exit(NULL);
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(SMTPD_PORT);
    servaddr.sin_addr.s_addr = inet_addr(SMTPD_SERVER);

    // Connect to the server
    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("connect failed");
        close(sockfd);
        pthread_exit(NULL);
    }

    // Read server response
    n = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
    if (n < 0) {
        perror("recv failed");
        close(sockfd);
        pthread_exit(NULL);
    }
    buffer[n] = '\0'; // Null-terminate the received string
    // random sleep
    
    //sleep(rand() % 1);

    // Construct the email message in a single buffer
    const char *email_message = 
        "HELO example.com\r\n"
        "MAIL FROM:<sender@localhost>\r\n"
        "RCPT TO:<recv@localhost>\r\n"
        "DATA\r\n"
        "From: sender@example.com\r\n"
        "To: recipient@example.com\r\n"
        "Subject: Test Email\r\n"
        "\r\n"
        "This is a test email.\r\n"
        ".\r\n";
        
    // Send the entire email message in one send
    if (send(sockfd, email_message, strlen(email_message), 0) < 0) {
        perror("send failed");
        close(sockfd);
        pthread_exit(NULL);
    }
    
    // random sleep

    //sleep(rand() % 1);


    while( (n = recv(sockfd, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[n] = '\0'; // Null-terminate the received string
        printf("Thread #%ld : %s\n",tid, buffer);
    }
    close(sockfd);
    pthread_exit(NULL);
}

int main() {
    pthread_t threads[NUM_THREADS];
    int rc;
    long t;

    for (t = 0; t < NUM_THREADS; t++) {
        rc = pthread_create(&threads[t], NULL, send_email, (void *)t);
        if (rc) {
            printf("ERROR; return code from pthread_create() is %d\n", rc);
            exit(-1);
        }
    }

    /* Wait for all threads to complete */
    for (t = 0; t < NUM_THREADS; t++) {
        pthread_join(threads[t], NULL);
    }

    printf("SMTP Stress Test Completed.\n");
    return 0;
}