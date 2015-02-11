#ifndef WEBSERVER_H
#define WEBSERVER_H


#include <microhttpd.h>

#include <functional>
#include <string>


class Connection {
    MHD_Connection* connection;
public:
    Connection(MHD_Connection* connection);
    void reply(int status, const std::string& data);
    void basicAuthFailed();
    bool requestBasicAuth(std::string& user, std::string& pass);
};

class WebServer {
    MHD_Daemon *daemon = 0;
    int port = 0;

    static int onRequest_(void*, MHD_Connection*, const char* url, const char* method, const char* version, const char* upload_data, long unsigned int* upload_data_size, void** con_cls);
    static void onComplete_(void *cls, struct MHD_Connection *connection, void **con_cls, enum MHD_RequestTerminationCode toe);
public:
    WebServer(int port);
    bool start(const char key[], const char cert[]);
    void stop();

    std::function<int(Connection* connection, const char* url, const char* method, const char* version, const char* upload_data)> onRequest;
    std::function<void(Connection* connection)> onComplete;
};


#endif // WEBSERVER_H