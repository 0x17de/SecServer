#ifndef WEBSERVER_H
#define WEBSERVER_H


#include <microhttpd.h>

#include <functional>
#include <string>
#include <sstream>


class User;

class Connection {
    MHD_Connection* connection;
    MHD_PostProcessor* pp = 0;

    std::string getIpAddr();
    void parseHeaders();
public:
    Connection(MHD_Connection* connection);
    ~Connection();

    bool postMessageProcessed = false;
    std::string type = "";
    std::string ip = "";
    std::stringstream postData;
    size_t dataRead = 0;
    size_t contentLength = 0;
    User* currentUser = 0;

    void debug(const char* upload_data, long unsigned int* upload_data_size);
    void reply(int status, const std::string& data);
    bool processPostMessage(const char* upload_data, long unsigned int* upload_data_size);
    void basicAuthFailed(const std::string& realm, const std::string& reason);
    bool requestBasicAuth(std::string& user, std::string& pass);
};

class WebServer {
    MHD_Daemon *daemon = 0;
    int port = 0;

    static int onRequest_(void*, MHD_Connection*, const char* url, const char* method, const char* version, const char* upload_data, long unsigned int* upload_data_size, void** con_cls);
    static void onComplete_(void *cls, struct MHD_Connection *connection, void **con_cls, enum MHD_RequestTerminationCode toe);
public:
    WebServer(int port);
    bool start(bool sslEnabled, const char key[], const char cert[]);
    void stop();

    std::function<int(Connection* connection, const char* url, const char* method, const char* version, const char* upload_data, long unsigned int* upload_data_size)> onRequest;
    std::function<void(Connection* connection)> onComplete;
};


#endif // WEBSERVER_H