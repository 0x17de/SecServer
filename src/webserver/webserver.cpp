#include "webserver.h"

#include <iostream>
#include <string>
#include <vector>


using namespace std;


Connection::Connection(MHD_Connection *connection) :
connection(connection)
{
}

void Connection::reply(int status, const std::string &data) {
    MHD_Response* response = MHD_create_response_from_buffer(data.size(), (void*)data.c_str(), MHD_RESPMEM_MUST_COPY);
    MHD_queue_response(connection, status, response);
    MHD_destroy_response(response);
}

void Connection::basicAuthFailed() {
    const string& page = "<html><body>User auth failed.</body></html>";
    MHD_Response* response = MHD_create_response_from_buffer(page.size(), (void*)page.c_str(), MHD_RESPMEM_PERSISTENT);
    MHD_queue_basic_auth_fail_response(connection, "myRealm", response);
    MHD_destroy_response(response);
}

bool Connection::requestBasicAuth(std::string& user, std::string& pass) {
    char *retuser = 0, *retpass = 0;
    retuser = MHD_basic_auth_get_username_password(connection, &retpass);
    bool success = retuser != nullptr && retpass != nullptr;

    if (success) {
        user = string(retuser);
        pass = string(retpass);
    }

    free(retuser);
    free(retpass);

    return success;
}

// ------

WebServer::WebServer(int port) :
port(port)
{
}

int WebServer::onRequest_(void* t,
        struct MHD_Connection *connection,
        const char *url,
        const char *method,
        const char *version,
        const char *upload_data,
        size_t *upload_data_size,
        void **con_cls) {
    Connection* cxn = new Connection(connection);
    *con_cls = (void*)cxn;
    return ((WebServer*)t)->onRequest(cxn, url, method, version, upload_data);
}

void WebServer::onComplete_(void *t, struct MHD_Connection *connection, void **con_cls, enum MHD_RequestTerminationCode toe) {
    if (con_cls) {
        Connection *cxn = (Connection *) *con_cls;
        ((WebServer*)t)->onComplete(cxn);
        delete cxn;
    }
}

bool WebServer::start(const char key[], const char cert[]) {
    // MHD_USE_EPOLL_INTERNALLY_LINUX_ONLY
    if (!MHD_is_feature_supported(MHD_FEATURE_SSL))
        cerr << "No SSL support" << endl;
    daemon = MHD_start_daemon(MHD_USE_SELECT_INTERNALLY|MHD_USE_SSL, port, nullptr, nullptr, &onRequest_, (void*)this,
            MHD_OPTION_NOTIFY_COMPLETED, &onComplete_, (void*)this,
            MHD_OPTION_HTTPS_MEM_KEY, key,
            MHD_OPTION_HTTPS_MEM_CERT, cert,
            MHD_OPTION_END);
    return daemon != nullptr;
}

void WebServer::stop() {
    MHD_stop_daemon(daemon);
}