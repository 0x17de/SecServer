#include "webserver.h"

#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <sstream>

#include <sys/types.h>
#include <arpa/inet.h>


using namespace std;


Connection::Connection(MHD_Connection *connection) :
        connection(connection)
{
    ip = getIpAddr();
    parseHeaders();
}

Connection::~Connection()
{
    if (pp) MHD_destroy_post_processor(pp);
}

static int parseHeaders_headerValueIterator(void *t,
        enum MHD_ValueKind kind,
        const char *key, const char *value) {
    Connection* c = (Connection*)t;
    if (!strcmp(key, MHD_HTTP_HEADER_CONTENT_LENGTH)) {
        c->contentLength = atoll(value);
    }
    return MHD_YES;
};

void Connection::parseHeaders() {
    MHD_get_connection_values(connection, MHD_HEADER_KIND, &parseHeaders_headerValueIterator, (void*)this);
}

std::string Connection::getIpAddr() {
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
            return string(result);
    }
}

void Connection::reply(int status, const std::string &data) {
    MHD_Response* response = MHD_create_response_from_buffer(data.size(), (void*)data.c_str(), MHD_RESPMEM_MUST_COPY);
    MHD_queue_response(connection, status, response);
    MHD_destroy_response(response);
}

static int headerValueIterator(void *t,
        enum MHD_ValueKind kind,
        const char *key, const char *value) {
    cout << "HEADER: " << key << ": " << value << endl;
    return MHD_YES;
};

static int postValueIterator(void *t,
        enum MHD_ValueKind kind,
        const char *key, const char *value) {
    cout << "POST: " << key << ": " << value << endl;
    return MHD_YES;
};

void Connection::debug(const char* upload_data, long unsigned int* upload_data_size) {
    cout << "== HEADER DATA ==" << endl;
    MHD_get_connection_values(connection, MHD_HEADER_KIND, &headerValueIterator, (void*)this);
    MHD_get_connection_values(connection, MHD_POSTDATA_KIND, &postValueIterator, (void*)this);
    if (*upload_data_size > 0) {
        cout << "POST_DATA:" << string(upload_data, *upload_data_size) << endl;
    }
    cout << "== END HEADER DATA ==" << endl;
}

bool Connection::processPostMessage(const char* upload_data, long unsigned int* upload_data_size) {
    if (dataRead >= contentLength && *upload_data_size == 0) return true;
    if (*upload_data_size > 0) {
        postData << string(upload_data, *upload_data_size);
        dataRead += *upload_data_size;
        *upload_data_size = 0; // we processed the data
    }
    return dataRead >= contentLength;
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

bool WebServer::start(bool sslEnabled, const char key[], const char cert[]) {
    // MHD_USE_EPOLL_INTERNALLY_LINUX_ONLY
    if (sslEnabled && !MHD_is_feature_supported(MHD_FEATURE_SSL)) {
        cerr << "No SSL support" << endl;
        return false;
    }
    int flags = MHD_USE_SELECT_INTERNALLY;
    if (sslEnabled) {
        daemon = MHD_start_daemon(flags|MHD_USE_SSL, port, nullptr, nullptr, &onRequest_, (void *) this,
                MHD_OPTION_NOTIFY_COMPLETED, &onComplete_, (void *) this,
                MHD_OPTION_HTTPS_MEM_KEY, key,
                MHD_OPTION_HTTPS_MEM_CERT, cert,
                MHD_OPTION_END);
    } else {
        daemon = MHD_start_daemon(flags, port, nullptr, nullptr, &onRequest_, (void *) this,
                MHD_OPTION_NOTIFY_COMPLETED, &onComplete_, (void *) this,
                MHD_OPTION_END);
    }
    return daemon != nullptr;
}

void WebServer::stop() {
    if (daemon)
        MHD_stop_daemon(daemon);
}