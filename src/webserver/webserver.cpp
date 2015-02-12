#include "webserver.h"

#include <iostream>
#include <string>
#include <vector>
#include <sstream>

#include <sys/types.h>
#include <arpa/inet.h>


using namespace std;


Connection::Connection(MHD_Connection *connection) :
        connection(connection)
{
    {
        const MHD_ConnectionInfo* info = MHD_get_connection_info(connection, MHD_CONNECTION_INFO_CLIENT_ADDRESS);
        sockaddr *sa = info->client_addr;

        void* addr = 0;
        size_t bufferLength;
        if (sa->sa_family == AF_INET) {
            sockaddr_in* s = (sockaddr_in*)sa;
            addr = &s->sin_addr;
            bufferLength = INET_ADDRSTRLEN;
        } else if (sa->sa_family == AF_INET6) {
            sockaddr_in6* s = (sockaddr_in6*)sa;
            addr = &s->sin6_addr;
            bufferLength = INET6_ADDRSTRLEN;
        }

        if (addr) {
            vector<char> buffer(bufferLength);

            const char *result = inet_ntop(sa->sa_family, addr, buffer.data(), bufferLength);
            if (result != 0)
                ip = string(result);
        }
    }
}

Connection::~Connection()
{
    if (pp) MHD_destroy_post_processor(pp);
}

void Connection::reply(int status, const std::string &data) {
    MHD_Response* response = MHD_create_response_from_buffer(data.size(), (void*)data.c_str(), MHD_RESPMEM_MUST_COPY);
    MHD_queue_response(connection, status, response);
    MHD_destroy_response(response);
}

void Connection::processPostMessage(const char* upload_data, long unsigned int* upload_data_size) {
    if (*upload_data_size > 0) {
        postData << string(upload_data, *upload_data_size);
        dataRead += *upload_data_size;
        *upload_data_size = 0; // we processed the data
    }
}

void Connection::basicAuthFailed(const std::string& realm, const std::string& reason) {
    stringstream page;
    page << "<html><body>" << reason << "</body></html>";
    string pageStr = page.str();
    MHD_Response* response = MHD_create_response_from_buffer(pageStr.size(), (void*)pageStr.c_str(), MHD_RESPMEM_PERSISTENT);
    MHD_queue_basic_auth_fail_response(connection, realm.c_str(), response);
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
    if (*con_cls == 0)
        *con_cls = new Connection(connection);
    return ((WebServer*)t)->onRequest((Connection*)*con_cls, url, method, version, upload_data, upload_data_size);
}

void WebServer::onComplete_(void *t, struct MHD_Connection *connection, void **con_cls, enum MHD_RequestTerminationCode toe) {
    if (*con_cls != 0) {
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