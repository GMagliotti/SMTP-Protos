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
#define NUM_THREADS 100

void *send_email(void *threadid) {
    long tid;
    tid = (long)threadid;
    printf("Thread #%ld\n", tid);

    int sockfd;
    struct sockaddr_in servaddr, cliaddr;

    // Create socket
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket creation failed");
        pthread_exit(NULL);
    }

    // Set up the client address structure
    memset(&cliaddr, 0, sizeof(cliaddr));
    cliaddr.sin_family = AF_INET;
    cliaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    // Assign a unique port number to each thread, ensuring it's within a valid range
    cliaddr.sin_port = htons(10000 + tid); // Example: starting from port 10000

    // Bind the socket to the client address
    if (bind(sockfd, (struct sockaddr *)&cliaddr, sizeof(cliaddr)) < 0) {
        perror("bind failed");
        close(sockfd);
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

    // Send HELO command
    char *helo = "HELO example.com\r\n";
    write(sockfd, helo, strlen(helo));

    sleep(1);
    // MAIL FROM
    char *mail_from = "MAIL FROM:<sender@example.com>\r\n";
    write(sockfd, mail_from, strlen(mail_from));

    sleep(1);
    // RCPT TO
    char *rcpt_to_one = "RCPT TO:<recipient1@example.com>\r\n";
    write(sockfd, rcpt_to_one, strlen(rcpt_to_one));

    // char *rcpt_to_two = "RCPT TO:<recipient2@example.com>\r\n";
    // write(sockfd, rcpt_to_two, strlen(rcpt_to_two));

    // char *rcpt_to_three = "RCPT TO:<recipient3@example.com>\r\n";
    // write(sockfd, rcpt_to_three, strlen(rcpt_to_three));

    sleep(1);
    // DATA
    char *data = "DATA\r\n";
    write(sockfd, data, strlen(data));

    sleep(1);
    // Email content
    char *email_body = "From: sender@example.com\r\nTo: recipient@example.com\r\nSubject: Test Email\r\n\r\nThis is a test email.\r\n.\r\n";
    write(sockfd, email_body, strlen(email_body));

    close(sockfd);
    pthread_exit(NULL);
}

int main() {
    pthread_t threads[NUM_THREADS];
    int rc;
    long t;

    for(t = 0; t < NUM_THREADS; t++) {
        printf("Creating thread %ld\n", t);
        rc = pthread_create(&threads[t], NULL, send_email, (void *)t);
        if (rc) {
            printf("ERROR; return code from pthread_create() is %d\n", rc);
            exit(-1);
        }
    }

    /* Wait for all threads to complete */
    for(t = 0; t < NUM_THREADS; t++) {
        pthread_join(threads[t], NULL);
    }

    printf("SMTP Stress Test Completed.\n");
    pthread_exit(NULL);
}