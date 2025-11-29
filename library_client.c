/*===============================================
*   文件名称：library_client.c
*   创 建 者：超
*   创建日期：2025年07月20日
*   描    述：
================================================*/
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sqlite3.h>
#include <string.h>
#include <malloc.h>
#include <stdlib.h>
#define port 6666

int main(int argc, char *argv[])
{

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == sockfd)
    {
        perror("socket");
        return -1;
    }
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr("0.0.0.0");
    socklen_t len = sizeof(addr);

    int ret = connect(sockfd, (struct sockaddr *)&addr, len);
    if (-1 == ret)
    {
        perror("connect");
        return -1;
    }

    char buf[1024];
    while (1) // 登录
    {
        char name[100];
        char passwd[100];
        puts("input name");
        fgets(name, sizeof(name), stdin);
        *(strstr(name, "\n")) = 0;

        puts("input passwd");
        fgets(passwd, sizeof(passwd), stdin);
        *(strstr(passwd, "\n")) = 0;
        sprintf(buf, "login:%s:%s", name, passwd);
        send(sockfd, buf, strlen(buf) + 1, 0); // 14
        recv(sockfd, buf, 15, 0);
        // buf[strcspn(buf, "\0")] = '\0'; // 确保只读取到 \0 为止，不包含后续数据：
        puts(buf);
        if (0 == strcmp(buf, "login finished"))
        {
            printf("登录成功\n");
            // 接收书籍表

            unsigned int book_len;
            char book_data[2048];
            if (4 != recv(sockfd, &book_len, 4, 0))
            {
                printf("接收失败\n");
                perror("recv buf");
            }
            book_len = ntohl(book_len); // 158
            if (book_len != recv(sockfd, book_data, 158, 0))
            {
                printf("接收数据不完整\n");
                perror("recv books");
                return -1;
            }
            book_data[book_len] = '\0';
            puts("书籍列表:");
            puts(book_data);
            break;
        }
        else
        {
            printf("登陆失败，请重新输入:\n");
            memset(buf, 0, sizeof(buf));
        }
    }

    while (1)
    {
        printf("==========图书管理系统==========\n");
        printf("1.查看所有书籍'1'\n");
        printf("2.借书(borrow:id)\n");
        printf("3.还书(return:id)\n");
        printf("4.退出(quit)\n");
        printf("5.查看我的书籍(mybooks)\n");
        printf("请输入指令\n");
        char cmd[1024];
        int len;
        fgets(cmd, sizeof(cmd), stdin);
        *(strstr(cmd, "\n")) = 0;
        send(sockfd, cmd, strlen(cmd) + 1, 0);
        // 退出指令
        if (0 == strcmp(cmd, "4") || 0 == strcmp(cmd, "quit"))
        {
            printf("已退出系统\n");
            close(sockfd);
            return 0;
        }
        // 查看所有书籍指令
        else if (0 == strcmp(cmd, "1"))
        {
            unsigned int book_len;
            if (4 != recv(sockfd, &book_len, 4, 0))
            {
                perror("recv 1len");
                break;
            }
            book_len = ntohl(book_len);
            printf("书籍长度:%d\n", book_len);
            char book_data[2048] = {0};
            if (book_len != recv(sockfd, book_data, book_len, 0))
            {
                perror("recv 1data");
                break;
            }
            book_data[book_len] = '\0';
            printf("\n最新书籍列表\n");
            puts(book_data);
            continue;
        }
        // 借书还书指令
        else if (NULL != strstr(cmd, "borrow:") || NULL != strstr(cmd, "return:"))
        {
            unsigned int book_len;
            if (4 != recv(sockfd, &book_len, 4, 0))
            {
                perror("recv 2lb");
                break;
            }
            book_len = ntohl(book_len);
            char respose[2048] = {0};
            if (book_len != recv(sockfd, respose, book_len, 0))
            {
                perror("respose default");
                break;
            }
            respose[book_len] = '\0';
            printf("结果:%s\n", respose);
            continue;
        }
        else if (NULL!= strstr(cmd, "mybooks"))
        {
            unsigned int len;
            if (4 != recv(sockfd, &len, 4, 0))
            {
                perror("recv mybooks");
                break;
            }
            len = ntohl(len);
            char mybooks[2048];
            if(len!=recv(sockfd,mybooks,len,0))
            {
                perror("recv mybooksdata");
                break;
            }
            mybooks[len]='\0';
            printf("结果:%s\n",mybooks);

        }
    }
}