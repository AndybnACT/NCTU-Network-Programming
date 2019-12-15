#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/write.hpp>
#include <fstream> 
#include <cstdlib>
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>  

#include "np_hardcoded_panel.hpp"

extern char **environ;

using boost::asio::ip::tcp;

#define ASSERT(expr, msg) {         \
    if (!(expr)) {                  \
        std::cerr << msg << '\n';   \
        assert(expr);               \
    }                               \
}

class http_parser {
private:
    char* get_token(char **buf, int *len, const char *cut) {
        char *tok = NULL;
        char *found;
        int cur;
        int erase = 0;
        
        assert(*len > 0);
        
        found = strstr(buf[0], cut);
        if (!found) {
            std::cerr << "cannot get token" << '\n';
            return NULL;
        }
        
        cur = found - buf[0];
        erase = strlen(cut);
        
        buf[0][cur] = '\0';
        tok = strdup(*buf);
        *buf = *buf + cur + erase;
        *len = *len - cur - erase;

        return tok;
    }

public:
    enum state { READY, BAD, INTERMIDIATE, COMPLETE } stat;
    char *req_method;
    char *req_uri;
    char *query_string;
    char *serv_proto;
    char *http_host;
    char *server_addr;
    char *server_port;

    http_parser () {
        std::cout << "[http_parser] initialized" << '\n';
        stat = READY;
    };

    int do_parse (char *buf, int len) {
        char *hst;
        int namelen;
        char *hst_fullname;
        char *req_content;
        char *uri;
        
        req_method = get_token(&buf, &len, " ");
        ASSERT(req_method, "error getting request method")
        std::cout << "req method: " << req_method << '\n';
        
        req_content = get_token(&buf, &len, " ");
        ASSERT(req_content, "error getting request content");
        std::cout << "query content: " << req_content << '\n';
        
        // parse uri and query_string
        namelen = strlen(req_content);
        uri = get_token(&req_content, &namelen, "?");
        if (uri) {
            req_uri = uri;
            query_string = strdup(req_content);
            std::cout << "qstr: " << query_string << '\n';
        }else {
            req_uri = req_content;
            query_string = NULL;
        }
        std::cout << "uri:  " << req_uri << '\n';
        
        serv_proto = get_token(&buf, &len, "\r\n");
        ASSERT(serv_proto, "error getting server protocol");
        std::cout << "http protocol: " << serv_proto << '\n';
        
        hst = get_token(&buf, &len, " ");
        ASSERT(strstr(hst, "Host") || strstr(hst, "host"), "parse error"); 
        
        hst_fullname = get_token(&buf, &len, "\r\n");
        ASSERT(hst_fullname, "error getting server name");
        
        namelen = strlen(hst_fullname);
        http_host = get_token(&hst_fullname, &namelen, ":");
        ASSERT(http_host, "error getting server addr & port");
        
        server_addr = strdup(http_host);
        server_port = strdup(hst_fullname);
        
        std::cout << "serv addr: " << server_addr << '\n';
        std::cout << "serv port: " << server_port << '\n';

        return COMPLETE;
    }

};

class http_responder {
private:
    struct hdlr {
        std::string method;
        int (http_responder::*func)(void);
    };
    enum method { GET, NRMETHOD };
    struct hdlr http_hdlr[NRMETHOD]{
        {"GET", &http_responder::http_get}
    };
    
    enum file_t { BAD, EXEC, READ} http_file_t;
    char *http_file;
    
    enum http_stat { NOT_FOUND, FORBIDDEN, OK, NOT_IMPLEMENTED, NR_STAT} http_stat;
    std::string basic_headr[NR_STAT] = {
        "HTTP/1.1 404 NOT_FOUND\r\n",
        "HTTP/1.1 403 FORBIDDEN\r\n",
        "HTTP/1.1 200 OK\r\n",
        "HTTP/1.1 501 NOT_IMPLEMENTED\r\n"
    };
    std::string default_header_tail = "Content-Type: text/html\r\nConnection: Closed\r\n";
    
    int sockfd;
    http_parser &req_;
    tcp::socket &socket_;    
    
    int http_exec(void){
        int rc;
        char *fake_argv[2];
        fake_argv[0] = http_file;
        fake_argv[1] = NULL;
        
        std::cout << "closing file descriptors" << '\n';
        close(0);
        close(1);
        close(2);
        dup2(sockfd, 0);
        dup2(sockfd, 1);
        dup2(sockfd, 2);
        close(sockfd);
        // no return if success
        rc = execve(http_file, fake_argv, environ);
        exit(rc);
    }

    int http_setenv(void){
        // setenv("REQUEST_METHOD" , req_.req_method, 1);
        // if (req_.req_uri)
        //     setenv("REQUEST_URI"    ,req_.req_uri ,1);
        // if (req_.query_string)
        //     setenv("QUERY_STRING"   ,req_.query_string ,1);
        // setenv("SERVER_PROTOCOL",req_.serv_proto ,1);
        // setenv("HTTP_HOST"      ,req_.http_host ,1);
        // setenv("SERVER_ADDR"    ,req_.server_addr ,1);
        // setenv("SERVER_PORT"    ,req_.server_port ,1);
        // setenv("REMOTE_ADDR", socket_.remote_endpoint().address().to_string().c_str(), 1);
        // setenv("REMOTE_PORT", std::to_string(socket_.remote_endpoint().port()).c_str() ,1);
        return 0;
    }
    
    int http_send_header(void){
        int len;
        std::string default_header = basic_headr[http_stat] + default_header_tail;
        std::cout << "ready to send header:" << '\n';
        std::cout << default_header << '\n';
        len = boost::asio::write(socket_, boost::asio::buffer(default_header));
        if (len != default_header.length()) {
            std::cerr << "bug, sending of http header is not completed" << '\n';
            return -1;
        }
        return 0;
    }
    
    int http_send_stream(std::ifstream &stream){
        auto buf = std::make_shared<boost::asio::streambuf>();
        std::ostream(buf.get()) << stream.rdbuf();
        boost::asio::async_write(socket_, *buf, [buf](std::error_code ec, size_t len){});
        return 0;
    }
    
    int http_get(void) {
        std::cout << "http get handler, stat=" << http_stat << '\n';
        if (http_file_t == EXEC) {
            http_exec();
        } else if (http_file_t == READ) {
            std::ifstream is (http_file, std::ifstream::binary);
            if (is) {
                http_send_stream(is);
            }
        }
        return -1;
    }
    
    
    int http_panel(){
        std::cout << "http server is sending panel" << '\n';
        std::string panelstr(PANEL);
        boost::asio::async_write(socket_, boost::asio::buffer(panelstr), 
            [](std::error_code ec, size_t len){});
        return 0;
    }
    
    int http_console(){
        
        return 0;
    }
    
    mode_t _get_filemod(char *f){
        struct stat statbuf;
        if (stat(f, &statbuf) != 0) {
            perror("stat");
        }
        return statbuf.st_mode;
    }
    
public:
    http_responder (tcp::socket &socket, http_parser &req)
    : req_(req),
      socket_(socket)
    {
        char file_buf[255];
        memset(file_buf, 0, 255);
        // memset(http_header, 0, 1024);
        
        sockfd = socket.native_handle();    
        std::cout << "socket fd = " << sockfd << '\n';
        
        strcpy(file_buf, ".");
        strcpy(file_buf+1, req_.req_uri);
        http_file = strdup(file_buf);
        std::cout << "http file = " << http_file << '\n';
        http_file_t = BAD;
    }
    
    int exec(void){
        mode_t mode;
        http_setenv();
        
        if (strcmp(http_file, "./panel.cgi") == 0) {
            http_stat = OK;
            http_file_t = EXEC;
            http_send_header();
            http_panel();
            return 0;
        }else if (strcmp(http_file, "./console.cgi") == 0) {
            http_stat = OK;
            http_file_t = EXEC;
            http_send_header();
            http_console();
            return 0;
        }
        
        std::cout << "checking file existence and permissions" << '\n';
        http_file_t = BAD;
        if (access(http_file, F_OK) == -1) {
            std::cout << "==>file not found" << '\n';
            http_stat = NOT_FOUND;
            http_file_t = BAD;
            goto bad;
        }
        
        mode = _get_filemod(http_file);
        if (!S_ISREG(mode)) {
            std::cout << "==>file is not a regular file" << '\n';
            http_stat = FORBIDDEN;
            http_file_t = BAD;
            goto bad;
        }
        
        if (access(http_file, R_OK) == 0) {
            std::cout << "==>regular file" << '\n';
            http_file_t = READ;
            http_stat = OK;
        }else {
            if (errno == EACCES) {
                http_file_t = BAD;
                http_stat = FORBIDDEN;
                goto bad;
            }else {
                http_stat = NOT_FOUND;
                perror("==>bug, access");
                goto bad;
            }
        }
        
        for (size_t i = 0; i < NRMETHOD; i++) {
            if (req_.req_method ==  http_hdlr[i].method) {
                http_send_header();
                (this->*http_hdlr[i].func)();
                return 0;
            }
        }
        http_stat = NOT_IMPLEMENTED;
bad:
        http_send_header();
        boost::asio::write(socket_, boost::asio::buffer("\r\n"));
        return -1;
    }

};

class server
{
public:
    server(boost::asio::io_context& io_context, unsigned short port)
    : io_context_(io_context),
      acceptor_(io_context, {tcp::v4(), port}),
      socket_(io_context),
      req_parser()
    {
        memset(rbuf, 0, 1024);
        rlen = 0;
        accept();
    }

private:
    void accept(){
        acceptor_.async_accept(
            [this](boost::system::error_code ec, tcp::socket new_socket){
                if (!ec){
                    std::cout << "[accept]" << '\n';
                    // Take ownership of the newly accepted socket.
                    socket_ = std::move(new_socket);
                    read();
                    accept();
                }else {
                    std::cerr << "Accept error: " << ec.message() << std::endl;
                    accept();
                }
        });
    }

    void read() {
        socket_.async_read_some(boost::asio::buffer(data_, 1024),
            [this](boost::system::error_code ec, std::size_t length)
            {
                char *ptr = NULL;
                std::cout << "[read-" << getpid() << "]:\n";
                if (rlen + length >= 4096) {
                    std::cerr << "Error, header too long" << '\n';
                }else{
                    memcpy(rbuf+rlen, &data_[0], length);
                    rlen += length;
                }
                ptr = strstr(rbuf, "\r\n\r\n");
                if (ptr) {
                    std::cout << "[read] ready to parse http header:" << '\n';
                    std::cout << rbuf << '\n';
                    
                    if (req_parser.do_parse(rbuf, rlen) == http_parser::COMPLETE) {
                        memset(rbuf, 0, 4096);
                        rlen = 0;
                        http_responder req_responder(socket_, req_parser);
                        req_responder.exec();
                        socket_.close();
                    }
                } else if (!ec){
                    read();
                }
            });
    }

    char rbuf[4096];
    int rlen;
    boost::asio::io_context& io_context_;
    tcp::acceptor acceptor_;
    tcp::socket socket_;
    std::array<char, 1024> data_;
    http_parser req_parser;
};

int main(int argc, char* argv[], char *envp[]) {
    try {
        if (argc != 2) {
            std::cerr << "Usage: process_per_connection <port>\n";
            return 1;
        }

        boost::asio::io_context io_context;

        using namespace std; // For atoi.
        server s(io_context, atoi(argv[1]));
        
        io_context.run();
        
    }catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
}