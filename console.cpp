#include <iostream>
#include <boost/asio.hpp>
#include <stdlib.h>

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
    struct host {
        int active;
        unsigned short _port;
        std::string host;
        std::string port;
        std::string file;
    } hst_list[MAXHST];
    
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
                        std::cout << "arg = " << qstr+strst << '\n';
                        stat = VALUE;
                        strst = i+1;
                    }
                    break;
                case VALUE:
                    if (qstr[i] == '&' || i == len) {
                        qstr[i] = '\0';
                        if (strst == i) {
                            std::cout << "value = (empty)" << '\n';
                        } else {
                            parse2host_val(qstr+strst);
                            std::cout << "value = " << qstr+strst << '\n';
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
            using namespace std;
            cout << "<pre>" << endl;
            cout << "host " << i+1 << " is active" << endl;
            cout << "name: " << hst_list[i].host << endl;
            cout << "port: " << hst_list[i]._port << endl;
            cout << "file: " << hst_list[i].file << endl;
            cout << "</pre>" << endl;
        }
        return 0;
    }
    
};

// // Friday afternoon + night
// class console {
// private:
//     /* data */
// 
// public:
//     console ();
//     virtual ~console ();
// };
// 
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
    
    std::cout << "[" <<getpid() << "]"<<'\n';
    
    if (!query_string) {
        std::cerr << "error, no environment variable QUERY_STRING found" << '\n';
        exit(-1);
    }
    
    qstr_parser parse(query_string);
    parse.start();
    parse.check();
    
    
    return 0;
}
