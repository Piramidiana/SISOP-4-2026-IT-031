#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 9000
#define BUF 4096

int main() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return 1; }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        return 1;
    }

    char buf[BUF];
    int n = recv(sock, buf, BUF - 1, 0);
    if (n > 0) { buf[n] = '\0'; printf("%s", buf); }

    while (1) {
        printf("db > ");
        fflush(stdout);
        if (!fgets(buf, BUF, stdin)) break;
        send(sock, buf, strlen(buf), 0);
        if (strncmp(buf, "EXIT", 4) == 0) break;
        n = recv(sock, buf, BUF - 1, 0);
        if (n <= 0) break;
        buf[n] = '\0';
        printf("%s", buf);
    }

    close(sock);
    return 0;
}
