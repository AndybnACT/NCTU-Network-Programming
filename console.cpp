#include <iostream>
#include <fstream>
#include <boost/asio.hpp>
#include <stdlib.h>
#include <unistd.h>

#define DEBUG
#ifdef DEBUG
#define DBGOUT(msgexpr){                        \
    std::cout << "<!--" <<  msgexpr << "-->\n";   \
}
#else
#define DBGOUT(msgexpr) {}
#endif /* DEBUG */

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
        size_t len = strlen(qstr);
        for (size_t i = 0; i < len+1; i++) {
            switch (stat) {
                case ARG:
                    if (qstr[i] == '=') {
                        qstr[i] = '\0';
                        if (strst == i) {
                            std::cout << "error, empty arg" << '\n';
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
        using namespace std;
        cout << "<body>" << '\n';
        cout << "<table class=\"table table-dark table-bordered\">" << '\n';
        
        cout << "<thead>" << '\n';
        cout << "<tr>" << '\n';
        for (size_t i = 0; i < nractive; i++) {
            cout << "<th scope=\"col\">";
            cout << session[i].hst->host;
            cout << ":" << session[i].hst->_port;
            cout << "</th>\n";
        }
        cout << "</tr>" << '\n';
        cout << "</thead>" << '\n';
        
        cout << "<tbody>" << '\n';
        cout << "<tr>" << '\n';
        for (size_t i = 0; i < nractive; i++) {
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
        std::cout << "<script>document.getElementById('s";
        std::cout << sid << "').innerHTML += '";
        std::cout << str << "';</script>\n" << std::flush;;
    }
    
    void output_command(int sid, std::string str) {
        if (console_check_sid(sid))
            return;
        std::cout << "<script>document.getElementById('s";
        std::cout << sid << "').innerHTML += '<b>";
        std::cout << str << "&NewLine;</b>';</script>\n" << std::flush;
    }
    
    console (struct host *hstp, std::string header_file){
        using namespace std;
        stringstream  buf;
        hst_list = hstp;
        nractive = 0;
        memset(session, 0, MAXHST*sizeof(struct session));
        
        ifstream is (header_file, ifstream::binary);
        buf << is.rdbuf();
        cout << buf.str();
        
        for (size_t i = 0; i < MAXHST; i++) {
            if (hst_list[i].active) {
                session[nractive].sid = nractive;
                session[nractive].hst = &hst_list[i];
                nractive++;
            }
        }
        
        console_set_tbl();
        cout << "</html>";
    };
};

// // // Friday afternoon + night
// class npshell_conn {
// private:
//     /* data */
// 
// public:
//     char *host;
//     char *port;
//     char *testfile;
//     npshell_conn ();
//     virtual ~npshell_conn ();
// };
// /////////////////////////////

int main(int argc, char const *argv[]) {
    std::cout << "\r\n";
    char *query_string = getenv("QUERY_STRING");
    
    DBGOUT("[" <<getpid() << "]");
    
    if (!query_string) {
        std::cerr << "error, no environment variable QUERY_STRING found" << '\n';
        exit(-1);
    }
    
    qstr_parser parse(query_string);
    parse.start();
    parse.check();
    
    console console(parse.hst_list, "console-head.html");
    
    return 0;
}
