#include <iostream>
#include <fstream>
#include <boost/asio.hpp>
#include <stdlib.h>
#include <unistd.h>
#include "np_hardcoded_console-head.hpp"

#define DEBUG

/* !!!!!!!!!!!!!!!!!!!!! NOTE OF BAD IMPLEMENTATION !!!!!!!!!!!!!!!!!!!!! *
 * Since TAs require us to compile only one file (main.cpp) on Windows,   *
 * this file will be included by "main.cpp" to prevent increasing of      *
 * unnecessary code.                                                      *
 * NOTE: implementation here is not thread safe!!                         *
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! */
#ifdef WINDOWS
boost::asio::streambuf global_streambuf;
tcp::socket *__sock;
std::ostream __glob_cout(&global_streambuf);
#define cout __glob_cout
#define WIN_CONSOLE_RENDER_PAGE() {                         \
    size_t len;                                             \
    len = boost::asio::write((*__sock), global_streambuf);  \
    global_streambuf.consume(len);                          \
}


#ifdef DEBUG
#define DBGOUT(msgexpr){                               \
    std::cerr << "console.cgi: " << msgexpr << "\n";   \
}
#else
#define DBGOUT(msgexpr) {}
#endif /* DEBUG */


#else

#define WIN_CONSOLE_RENDER_PAGE() {}
using namespace std;
#ifdef DEBUG
#define DBGOUT(msgexpr){                     \
    cout << "<!--" <<  msgexpr << "-->\n";   \
}
#else
#define DBGOUT(msgexpr) {}
#endif /* DEBUG */

#endif /* WINDOWS */
/* !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! */


struct host {
    int active;
    unsigned short _port;
    std::string host;
    std::string port;
    std::string file;
};
#define MAXHST 5


class qstr_parser {
private:
    enum state { ARG, VALUE, BAD };
    int hst;
    char item; 
    char *qstr;
    int parse2host_arg(char *strp){
        item = strp[0];
        std::string hstnumstr = strp+1;
        hst = std::stoi(hstnumstr, NULL, 10);
        return 0;
    }
    int parse2host_val(char *strp){
        if (hst > MAXHST || hst < 0) {
            std::cerr << "host number out of range" << '\n';
            exit(-1);
        }
        switch (item) {
            case 'h':
                hst_list[hst].host = strp;
                break;
            case 'p':
                hst_list[hst].port = strp;
                break;
            case 'f':
                hst_list[hst].file = strp;
                break;
            default:
            std::cerr << "_val parse error" << '\n';
            exit(-1);
        }
        return 0;
    }
public:
    struct host hst_list[MAXHST];
    
    qstr_parser(char *query){
        qstr = strdup(query);
        hst = 0;
        item = 'h';
        for (size_t i = 0; i < 5; i++) {
            hst_list[i].active = 0;
            hst_list[i]._port = 0;
            hst_list[i].host = "";
            hst_list[i].port = "";
            hst_list[i].file = "";
        }
    }
    
    int start() {
        enum state stat = ARG;
        int strst = 0;
        int len = strlen(qstr);
        for (int i = 0; i < len+1; i++) {
            switch (stat) {
                case ARG:
                    if (qstr[i] == '=') {
                        qstr[i] = '\0';
                        if (strst == i) {
                            cout << "error, empty arg" << '\n';
                            exit(-1);
                        }
                        parse2host_arg(qstr+strst);
                        DBGOUT("arg = " << qstr+strst);
                        stat = VALUE;
                        strst = i+1;
                    }
                    break;
                case VALUE:
                    if (qstr[i] == '&' || i == len) {
                        qstr[i] = '\0';
                        if (strst == i) {
                            DBGOUT("value = (empty)")
                        } else {
                            parse2host_val(qstr+strst);
                            DBGOUT("value = " << qstr+strst)
                        }
                        stat = ARG;
                        strst = i+1;
                    }
                    break;
                case BAD:
                    std::cerr << "error parsing qstr" << '\n';
                    exit(-1);
            }
        }
        return 0;
    }
    int check() {
        for (size_t i = 0; i < MAXHST; i++) {
            if (hst_list[i].host.empty() || hst_list[i].port.empty() ||
                hst_list[i].file.empty()) {
                continue;
            }
            hst_list[i]._port = (unsigned short) std::stoi(hst_list[i].port, NULL, 10);
            if (!hst_list[i]._port) {
                continue;
            }
            hst_list[i].active = 1;
            DBGOUT("host " << i+1 << "is active");
            DBGOUT("name: " << hst_list[i].host);
            DBGOUT("port: " << hst_list[i]._port);
            DBGOUT("file: " << hst_list[i].file);
        }
        return 0;
    }
};

class console {
private:
    std::string html_head;
    struct host *hst_list;
    
    int console_set_tbl(){
        cout << "<body>" << '\n';
        cout << "<table class=\"table table-dark table-bordered\">" << '\n';
        
        cout << "<thead>" << '\n';
        cout << "<tr>" << '\n';
        for (int i = 0; i < nractive; i++) {
            cout << "<th scope=\"col\">";
            cout << session[i].hst->host;
            cout << ":" << session[i].hst->_port;
            cout << "</th>\n";
        }
        cout << "</tr>" << '\n';
        cout << "</thead>" << '\n';
        
        cout << "<tbody>" << '\n';
        cout << "<tr>" << '\n';
        for (int i = 0; i < nractive; i++) {
            cout << "<td><pre id=\"s";
            cout << i << "\"";
            cout << " class=\"mb-0\"></pre></td>\n";
        }
        cout << "</tr>" << '\n';
        cout << "</tbody>" << '\n';
        
        cout << "</table>" << '\n';
        cout << "</body>" << '\n';
        return 0;
    }
    
    int console_check_sid(int sid) {
        if ( sid < 0 || sid >= nractive) {
            return -1;
        }
        return 0;
    }
    
public:
    struct session {
        int sid;
        struct host *hst;
    } session[MAXHST];
    int nractive;
    
    void output_shell(int sid, std::string str) {
        if (console_check_sid(sid))
            return;
        cout << "<script>document.getElementById('s";
        cout << sid << "').innerHTML += '";
        cout << str << "';</script>\n" << std::flush;;
        WIN_CONSOLE_RENDER_PAGE();
    }
    
    void output_command(int sid, std::string str) {
        if (console_check_sid(sid))
            return;
        cout << "<script>document.getElementById('s";
        cout << sid << "').innerHTML += '<b>";
        cout << str << "&NewLine;</b>';</script>\n" << std::flush;
        WIN_CONSOLE_RENDER_PAGE();
    }
    
    console (struct host *hstp, const char* hardcoded_head){
        hst_list = hstp;
        nractive = 0;
        memset(session, 0, MAXHST*sizeof(struct session));
        
        cout << hardcoded_head;
        
        for (size_t i = 0; i < MAXHST; i++) {
            if (hst_list[i].active) {
                session[nractive].sid = nractive;
                session[nractive].hst = &hst_list[i];
                nractive++;
            }
        }
        
        console_set_tbl();
        cout << "</html>" << std::flush;
        WIN_CONSOLE_RENDER_PAGE();
    };
};

// Friday afternoon + night
using boost::asio::ip::tcp;
#define NPSHELL_ERR(boostec){                               \
    std::string errstr = (boostec).message() + ": " +       \
                         std::to_string((boostec).value()); \
    DBGOUT(errstr);                                         \
    console_.output_command(sid, errstr);                   \
}
#define RBUFSIZE 4096
#define PROMPT "%"

class npshell_conn {
private:
    boost::asio::io_context &io_context_;
    tcp::resolver resolver;
    tcp::socket socket;
    tcp::resolver::results_type endpoint;
    std::ifstream cmdstream;
    console &console_;
    struct host &host;
    int sid;
    
    char rbuf[RBUFSIZE];
    
    int npshell_outputl(char *buf, size_t len)
    {
        int cnt = 0;
        std::string outstr;
        for (size_t i = 0; i < len; i++) {
            if (buf[i] == '\r' || buf[i] == '\n') {
                outstr += "&NewLine;";
                cnt++;
            }else if (buf[i] == '\'') {
                outstr += "\\'";
            }else {
                outstr += buf[i];
            }
        }
        console_.output_shell(sid, outstr);
        return cnt;
    }
    
    void npshell_recvRes()
    {
        DBGOUT("read");
        socket.async_read_some(boost::asio::buffer(rbuf, RBUFSIZE-1),
            [this](boost::system::error_code ec, std::size_t len)
            {
                if (!ec) {
                    DBGOUT("read: " << rbuf);
                    npshell_outputl(rbuf, len);
                    
                    if (strstr(rbuf, PROMPT)) {
                        memset(rbuf, 0, RBUFSIZE);
                        npshell_sendCmd();
                    }else {
                        memset(rbuf, 0, RBUFSIZE);
                        npshell_recvRes();
                    }
                }else {
                    NPSHELL_ERR(ec);
                }
            });
    }  
    
    void npshell_sendCmd()
    {
        std::string line;
        if (std::getline(cmdstream, line)) {
            if ((int)(line.size()) - 1 >= 0) {
                if (line[line.size() - 1] == '\r') {
                    line.erase(line.size() - 1);
                }
            }
            DBGOUT("write: " << line);
            console_.output_command(sid, line);
            line = line + "\n";
            socket.async_write_some(boost::asio::buffer(line), 
                [this](boost::system::error_code ec, std::size_t len)
                {
                    if (!ec) {
                        npshell_recvRes();
                    }else {
                        NPSHELL_ERR(ec);
                    }
                });
        }
    }
    
    int npshell_connect(tcp::resolver::results_type &endpoint)
    {
        // auto self(shared_from_this());
        boost::asio::async_connect(socket, endpoint, 
            [this](boost::system::error_code ec, tcp::endpoint ep)
            {
                if (!ec) {
                    DBGOUT("connected!!!");
                    console_.output_command(sid, "connected!");
                    npshell_recvRes();
                }else {
                    NPSHELL_ERR(ec);
                }
            });
        return -1;
    }
    
public:
    npshell_conn (boost::asio::io_context &io_context, struct host &h, console &console, int id)
    :   io_context_(io_context),
        resolver(io_context),
        socket(io_context),
        console_(console),
        host(h),
        sid(id)
    {
        endpoint = resolver.resolve(host.host, host.port);
        DBGOUT(sid);
        cmdstream = std::ifstream("./test_case/" + h.file, std::ifstream::binary);
        
        npshell_connect(endpoint);
        return;
    };
};


#ifdef WINDOWS
int http_console_start(char *query_string) {
#else
int main(int argc, char const *argv[]) {
    char *query_string = getenv("QUERY_STRING");
#endif /* WINDOWS */
    boost::asio::io_context io_context;

    cout << "\r\n";
    npshell_conn* npshell[MAXHST];
    
    DBGOUT("[" <<getpid() << "]");
    
    if (!query_string) {
        std::cerr << "error, no environment variable QUERY_STRING found" << '\n';
        exit(-1);
    }
    
    qstr_parser parse(query_string);
    parse.start();
    parse.check();
    
    console console(parse.hst_list, CONSOLE_HEAD);
    
    size_t nr = console.nractive;
    for (size_t i = 0; i < nr ; i++) {
        npshell[i] = new npshell_conn(io_context, *console.session[i].hst, console, i);
    }
    
    io_context.run();
    
    for (size_t i = 0; i < nr; i++) {
        delete npshell[i];
    }
    
    return 0;
}
