/*===============================================
*   文件名称：library_server.c
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
#include <stdlib.h>
#include <signal.h>
#define port 6666
#define MAX_LIENTS 10
typedef struct
{
    int fd;
    int status_logged;
    char username[100]; // 保存当前登录用户名
} ClientState;
typedef struct
{
    char data[2048];
    int rout;
} BOOKBOF;
int book_back(void *data, int argc, char **acgv, char **azconame)
{
    // 处理标志数据
    BOOKBOF *buf = (BOOKBOF *)data;
    int i;
    if (0 == buf->rout)
    {
        for (i = 0; i < argc; i++)
        {
            strcat(buf->data, azconame[i]);
            strcat(buf->data, "|");
        }
        buf->rout = 1;
    }
    strcat(buf->data, "\n");
    for (i = 0; i < argc; i++)
    {
        strcat(buf->data, acgv[i] ? acgv[i] : "NULL");
        strcat(buf->data, "|");
    }

    return 0;
}

int callback(void *data, int argc, char **acgv, char **azcolname)
{
    *(int *)data = 1;
    return 0;
}
int main(int argc, char *argv[])
{
    ClientState clients[MAX_LIENTS] = {0};
    for (int j = 0; j < MAX_LIENTS; j++)
    {
        clients[j].fd = -1;
        clients[j].status_logged = 0;
    }
    signal(SIGPIPE, SIG_IGN);
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

    if (-1 == bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)))
    {
        perror("bind");
        return -1;
    }
    if (-1 == listen(sockfd, 7))
    {
        perror("listen");
        return -1;
    }

    // 设置位图
    fd_set readfds, tempfds;
    FD_ZERO(&readfds);
    FD_SET(sockfd, &readfds);
    int nfds = sockfd + 1;

    // 设置数据表
    sqlite3 *db;
    if (0 != sqlite3_open("my.db", &db))
    {
        printf("open fault\n");
        return -1;
    }
    // 创建账户表和书籍表以及借阅表
    char sql[300] = "create table  if not exists account (name char,passwd char);";
    char *err;
    if (0 != sqlite3_exec(db, sql, NULL, NULL, &err))
    {
        printf("exec:%s\n", err);
        return -1;
    }
    strcpy(sql, "create table  if not exists books (id int,title char,author char,status char);");
    if (0 != sqlite3_exec(db, sql, NULL, NULL, &err))
    {
        printf("exec1:%s\n", err);
        return -1;
    }
    strcpy(sql, "create table if not exists borrow_records (id integer primary key autoincrement,username char,book_id int,borrow_time datetime default current_timestamp,return_time datetime);");
    if (0 != sqlite3_exec(db, sql, NULL, NULL, &err))
    {
        printf("exec2:%s\n", err);
        return -1;
    }

    char buf[1024];
    while (1)
    {
        tempfds = readfds;
        if (-1 == select(nfds, &tempfds, NULL, NULL, NULL))
        {
            perror("select");
            // return -1;
            continue; // 继续下一次select
        }
        int i;
        for (i = 0; i < nfds; i++)
            if (FD_ISSET(i, &tempfds))
            {
                if (i == sockfd)
                {
                    int connfd = accept(sockfd, (struct sockaddr *)&addr, &len);
                    if (-1 == connfd)
                    {
                        perror("accept");
                        // return -1;
                        continue; // 跳过当前连接，继续处理其他请求
                    }
                    printf("connected\n");
                    int location = -1; // 查找空闲位置存储客户端状态
                    for (int j = 0; j < MAX_LIENTS; j++)
                    {
                        if (-1 == clients[j].fd)
                        {
                            location = j;
                            break;
                        }
                    }
                    if (-1 == location)
                    {
                        printf("客户端连接已达到上限\n");
                        close(connfd);
                        continue;
                    }
                    clients[location].fd = connfd;
                    clients[location].status_logged = 0; // 标记为未登录
                    FD_SET(connfd, &readfds);
                    nfds = nfds < connfd + 1 ? connfd + 1 : nfds;
                }
                else
                {
                    int state = -1; // 1,查找当前客户端对应的状态
                    for (int j = 0; j < MAX_LIENTS; j++)
                    {
                        if (i == clients[j].fd)
                        {
                            state = j;
                            break;
                        }
                    }
                    if (-1 == state)
                    {
                        printf("没有找到客户端状态,fd=%d\n", i);
                        continue;
                    }
                    // 2,处理登录（未登录状态）
                    if (0 == clients[state].status_logged)
                    {
                        // log:wxc:123
                        unsigned int nflag = 0;
                        int ret;
                        char sql[300] = {0};
                        char buf[1024] = {0};
                        char *err = NULL;
                        ret = recv(i, buf, 14, 0);
                        if (-1 == ret)
                        {
                            perror("recv");
                            FD_CLR(i, &readfds); // 移除当前客户端
                            close(i);
                            continue; // 继续处理其他客户端
                            // return -1;
                        }
                        else if (0 == ret)
                        {
                            printf("a client quit\n");
                            FD_CLR(i, &readfds);
                            close(i);
                            continue;
                        }
                        char name[200]; // 1. 空指针风险
                        int name_len;
                        char passwd[200];
                        int passwd_len;
                        char *name_ptr = NULL;
                        char *passwd_ptr = NULL;
                        name_ptr = strstr(buf, ":") + 1;
                        passwd_ptr = strstr(name_ptr, ":") + 1;
                        name_len = (passwd_ptr - name_ptr) - 1;
                        *(passwd_ptr - 1) = 0;
                        strcpy(name, name_ptr);
                        name[name_len] = '\0';
                        strcpy(passwd, passwd_ptr);
                        sprintf(sql, "select *from account where name='%s' and passwd='%s';", name_ptr, passwd_ptr);
                        puts(sql);
                        puts(name);
                        int flag = 0;
                        if (0 != sqlite3_exec(db, sql, callback, &flag, &err))
                        {
                            printf("error:%s", err);
                            sqlite3_free(err);
                            FD_CLR(i, &readfds);
                            close(i);
                            continue;
                        }
                        if (1 == flag)
                        {

                            strcpy(buf, "login finished");
                            send(i, buf, strlen(buf) + 1, 0); // 15
                            puts(name);
                            strcpy(clients[state].username, name); // 保存用户名
                            printf("成功连接客户端:%s\n", clients[state].username);
                            clients[state].status_logged = 1; // 标记为已登录
                            unsigned int book_len;
                            strcpy(sql, "select *from books;");
                            BOOKBOF book_buf = {0};
                            // 清空
                            memset(book_buf.data, 0, sizeof(book_buf.data));
                            book_buf.rout = 0;
                            sqlite3_exec(db, sql, book_back, &book_buf, &err);
                            // 发送长度前检查连接是否有效
                            book_len = strlen(book_buf.data);
                            printf("%d\n", book_len);
                            book_len = htonl(book_len); // 158
                            if (4 != send(i, &book_len, 4, 0))
                            {
                                perror("send book_len");
                            }
                            send(i, book_buf.data, ntohl(book_len), 0);
                            puts(book_buf.data);
                        }
                        else
                        {
                            strcpy(buf, "login default");
                            send(i, buf, strlen(buf) + 1, 0);
                        }
                    }
                    else
                    {
                        char cmd[1024] = {0};
                        int ret = recv(i, cmd, 10, 0);
                        if (0 >= ret)
                        {
                            if (ret == 0)
                                printf("客户端断开连接fd:%d\n", i);
                            else
                                perror("接收指令错误");
                            FD_CLR(i, &readfds);
                            close(i);
                            clients[state].fd = -1; // 释放状态
                            clients[state].status_logged = 0;
                            continue;
                        }

                        cmd[ret] = '\0';                    // 添加结束符，避免字符串操作异常
                        if (strstr(cmd, "borrow:") != NULL) // 借书
                        {
                            // borrow:2
                            char *id_borrow = strstr(cmd, ":") + 1;
                            int book_id = atoi(id_borrow);
                            char update_sql[300];
                            sprintf(update_sql, "update books set status='borrowed' where id=%d", book_id);
                            sqlite3_exec(db, update_sql, NULL, NULL, &err);
                            char insert_sql[300];
                            sprintf(insert_sql, "insert into borrow_records (username,book_id)"
                                                "values('%s',%d);",
                                    clients[state].username, book_id);
                            sqlite3_exec(db, insert_sql, NULL, NULL, &err);
                            char rme[200] = "借书成功";
                            unsigned int rme_len = htonl(strlen(rme));
                            send(i, &rme_len, 4, 0);
                            send(i, rme, ntohl(rme_len), 0);
                        }
                        else if (strstr(cmd, "return:") != NULL) // 还书
                        {
                            char *id_return = strstr(cmd, ":") + 1;
                            int book_id = atoi(id_return);
                            char update_sql[300];
                            sprintf(update_sql, "update books set status='available' where id=%d", book_id);
                            sqlite3_exec(db, update_sql, NULL, NULL, &err);
                            char return_sql[300];
                            snprintf(return_sql, sizeof(return_sql),
                                     "update borrow_records set return_time=current_timestamp where username='%s' and book_id=%d and return_time is null;",
                                     clients[state].username, book_id);
                            sqlite3_exec(db, return_sql, NULL, NULL, &err);
                            printf("还书操作:%s\n", return_sql);
                            char rbm[200] = "还书成功";
                            unsigned int rbm_len = htonl(strlen(rbm));
                            send(i, &rbm_len, 4, 0);
                            send(i, rbm, strlen(rbm), 0);
                        }
                        else if (strcmp(cmd, "quit") == 0 || 0 == strcmp(cmd, "4")) // 退出
                        {
                            char quit[200] = "已退出";
                            unsigned int quit_len = htonl(strlen(quit));
                            send(i, &quit_len, 4, 0);
                            send(i, quit, strlen(quit), 0);
                            FD_CLR(i, &readfds);
                            close(i);
                            clients[state].fd = -1; // 释放状态
                            clients[state].status_logged = 0;
                        }
                        else if (0 == strcmp(cmd, "1")) // 查看书籍状态
                        {
                            BOOKBOF new_book = {0};
                            strcpy(sql, "select *from books");
                            sqlite3_exec(db, sql, book_back, &new_book, &err);
                            unsigned int new_len = htonl(strlen(new_book.data));
                            send(i, &new_len, 4, 0);
                            send(i, new_book.data, ntohl(new_len), 0);
                            continue;
                        }
                        else if (0 == strcmp(cmd, "mybooks")) // 查询当前用户的借阅记录
                        {
                            char inquire_sql[300];
                            snprintf(inquire_sql, sizeof(inquire_sql),
                                     "select b.id, b.title, r.borrow_time, r.return_time "
                                     "from borrow_records r "
                                     "join books b on r.book_id = b.id "
                                     "where r.username='%s';",
                                     clients[state].username); // 关联查询图书信息和借阅记录
                            BOOKBOF records = {0};
                            records.rout = 0;
                            sqlite3_exec(db, inquire_sql, book_back, &records, &err);
                            unsigned int len = htonl(strlen(records.data));
                            send(i, &len, 4, 0);
                            send(i, records.data, ntohl(len), 0);
                        }
                    }
                }
            }
    }
    return 0;
}
// 创建一个表存书籍目录
// 发送给客户端
// 客户端选择借书或者是还书
// 服务器接收以后借书就是增加还书就是减少在之前还要创建账号登陆成功才能实现借书或者是还书，账号存到数据库里面
// 也可以选择查询有什么书
