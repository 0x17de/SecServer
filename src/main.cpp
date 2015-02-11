#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <list>
#include <cstring>
#include <microhttpd.h>
#include <stdio.h>
#include <signal.h>
#include "webserver/webserver.h"

using namespace std;


bool running = true;
void sigInt(int) {
    running = false;
}

bool loadFile(const std::string& fileName, std::string& into) {
    ifstream fileIn(fileName);
    if (!fileIn) return false;
    fileIn.seekg(0, ios::end);
    size_t keySize = fileIn.tellg();
    into.reserve(keySize);
    fileIn.seekg(0, ios::beg);
    into.assign(istreambuf_iterator<char>(fileIn), istreambuf_iterator<char>());
    return true;
}

struct User {
    User(const string& name) :
    name(name)
    {
        updateTime();
    }

    void updateTime() {
        lastOnTime = time(0);
    }

    string name = "";
    size_t lastOnTime = 0;
};

struct Message {
    Message(const string& user, const string& message, size_t lastMessageTime, size_t lastMessageId) :
    user(user),
    message(message),
    lastMessageTime(lastMessageTime),
    lastMessageId(lastMessageId)
    {
    }

    string user = "";
    string message = "";
    size_t lastMessageTime = 0;
    size_t lastMessageId = 0;
};

int main() {
    signal(SIGINT, sigInt);

    string key, cert;
    loadFile("cert/newkey.pem", key);
    loadFile("cert/newcert.pem", cert);

    list<Message> messages;
    list<User> users;

    auto addMessage = [&messages](const string& user, const string& messageText) {
        size_t currentTime = time(0);
        static size_t lastTime = time(0);
        static size_t currentTimeIndex = 0;
        if (currentTime == lastTime)
            ++currentTimeIndex;
        else
            currentTimeIndex = 0;

        messages.emplace_back(user, messageText, currentTime, currentTimeIndex);

        while (messages.size() > 30) messages.pop_front();
        lastTime = currentTime;
    };

    WebServer server(12321);
    server.onRequest = [&](Connection* connection, const char* cUrl, const char* method, const char* version, const char* upload_data) {
        string user, pass;
        if (!connection->requestBasicAuth(user, pass)) {
            connection->basicAuthFailed();
            return MHD_YES;
        }

        User* currentUser;
        auto userIt = find_if(begin(users), end(users), [&](User& loggedUser) { return loggedUser.name == user; });
        if (userIt == users.end()) {
            users.emplace_back(user);
            currentUser = &users.back();

            stringstream ss;
            ss << "User \"" << user << "\" logged on";
            addMessage("", ss.str());
        } else {
            currentUser = &*userIt;
        }
        currentUser->updateTime();

        stringstream answer;

        string url(cUrl);
        if (url == "/") {
            string pageTemplate;
            loadFile("page/index.html", pageTemplate);
            connection->reply(200, pageTemplate);
        } else if (url.substr(0, 5) == "/get/") {
            size_t messageTime, messageId;
            {
                string messageTimeStr;
                string messageIdStr;
                istringstream messageIn(url.substr(5));

                getline(messageIn, messageTimeStr, ':');
                getline(messageIn, messageIdStr);

                try {
                    messageTime = stoll(messageTimeStr);
                    messageId = stoll(messageIdStr);
                } catch(exception& e) {
                    messageTime = 0;
                    messageId = 0;
                }
            }

            list<Message>::iterator it = messages.begin();
            for (; it != end(messages); ++it) {
                if (it->lastMessageTime > messageTime || (it->lastMessageTime == messageTime && it->lastMessageId > messageId)) {
                    break;
                }
            }
            for (; it != messages.end(); ++it) {
                answer << it->lastMessageTime << ":" << it->lastMessageId << "|" << it -> user << ":" << it->message << endl;
            }
            connection->reply(200, answer.str());
        } else if (url.substr(0, 6) == "/send/") {
            if (!strcmp(method, "GET")) {
                string messageText(url.substr(6));
                addMessage(user, messageText);
            } else {
                // @TODO: POST method
            }

            answer << "OK";
            connection->reply(200, answer.str());
        } else if (url.substr(0, 8) == "/static/") {
            string fileName(url.substr(8));
            bool validFile = true;
            for (auto c : fileName) {
                if((c > 'a' && c < 'z')
                 ||(c > 'A' && c < 'Z')
                 ||(c > '0' && c < '9')
                 ||(c == '.' || c == '-'))
                {} else {
                    validFile = false;
                    break;
                }
            }
            if (validFile) {
                string file;
                if (loadFile(string("static/")+fileName, file))
                    connection->reply(200, file);
                else {
                    answer << "File \"" << fileName << "\" not found.";
                    connection->reply(404, answer.str());
                }
            } else {
                connection->reply(400, "Bad request");
            }
        }

        return MHD_YES;
    };
    server.onComplete = [](Connection* connection) {
    };

    if (server.start(key.c_str(), cert.c_str())) {
        FILE *f = fopen("stopserver.deleteme", "w+");
        fclose(f);
        while (running) {
            f = fopen("stopserver.deleteme", "r");
            if (!f) break;
            fclose(f);
            sleep(2);

            size_t currentTime = time(0);
            for (auto it = begin(users); it != end(users);) {
                if (it->lastOnTime + 10 < currentTime) {
                    stringstream ss;
                    ss << "User \"" << it->name << "\" signed off";
                    addMessage("", ss.str());
                    it = users.erase(it);
                } else {
                    ++it;
                }
            }
        }
        server.stop();
    }
    return 0;
}