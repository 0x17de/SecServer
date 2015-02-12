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

struct Message {
    Message(const string& user, const string& message, char messageType, size_t lastMessageTime, size_t lastMessageId) :
            user(user),
            message(message),
            messageType(messageType),
            lastMessageTime(lastMessageTime),
            lastMessageId(lastMessageId)
    {
    }

    string user = "";
    string message = "";
    char messageType = 'm';
    size_t lastMessageTime = 0;
    size_t lastMessageId = 0;
};

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

    list<Message> directMessages;
};

int main() {
    signal(SIGINT, sigInt);

    string key, cert;
    loadFile("cert/newkey.pem", key);
    loadFile("cert/newcert.pem", cert);

    list<Message> messages;
    list<User> users;

    auto addMessage = [&messages](const string& user, const string& messageText, char messageType) {
        size_t currentTime = time(0);
        static size_t lastTime = time(0);
        static size_t currentTimeIndex = 0;
        if (currentTime == lastTime)
            ++currentTimeIndex;
        else
            currentTimeIndex = 0;

        messages.emplace_back(user, messageText, messageType, currentTime, currentTimeIndex);

        while (messages.size() > 100) messages.pop_front();
        lastTime = currentTime;
    };
    auto addDirectMessage = [&users,&messages](User* currentUser, const string& from, const string& to, const string& messageText, char messageType) {
        auto userIt = find_if(begin(users), end(users), [&](User& loggedUser) { return loggedUser.name == to; });
        if (userIt != end(users)) {
            size_t currentTime = time(0);
            userIt->directMessages.emplace_back(from, messageText, messageType, currentTime, 0);
            if (currentUser) {
                stringstream ss;
                ss << "> " << to;
                currentUser->directMessages.emplace_back(ss.str(), messageText, messageType, currentTime, 0);
            }
        }
    };

    WebServer server(12321);
    server.onRequest = [&](Connection* connection, const char* cUrl, const char* method, const char* version, const char* upload_data, long unsigned int* upload_data_size) {
        string user, pass;
        stringstream answer;

        User*& currentUser = connection->currentUser;
        if (currentUser) {
            user = currentUser->name;
        } else {
            bool authSuccess = connection->requestBasicAuth(user, pass);
            if (authSuccess) {
                if (user.size() == 0
                || user.size() > 15
                || user == "System"
                || user == "system"
                || user == "") {
                    authSuccess = false;
                } else {
                    static const string allowedCharacters("/.,<>?\\[]!#$%^*()_+=-");
                    for (char c : user) {
                        if ((c >= 'a' && c <= 'z')
                        || (c >= 'A' && c <= 'Z')
                        || (c >= '0' && c <= '9')
                        || allowedCharacters.find(c) != string::npos) {
                            // Ok.
                        } else {
                            authSuccess = false;
                            break;
                        }
                    }
                }
            }
            if (!authSuccess) {
                connection->basicAuthFailed("SecServer", "Login failed. Choosing this username is forbidden.");
                return MHD_YES;
            }

            auto userIt = find_if(begin(users), end(users), [&](User& loggedUser) { return loggedUser.name == user; });
            if (userIt == users.end()) {
                users.emplace_back(user);
                currentUser = &users.back();

                stringstream ss;
                ss << "User \"" << user << "\" logged on";
                addMessage(user, ss.str(), '+');
            } else {
                currentUser = &*userIt;
            }
        }
        currentUser->updateTime();

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

            auto writeMessage = [&answer] (list<Message>::iterator it) {
                answer << it->messageType << ":" << it->lastMessageTime << ":" << it->lastMessageId << "|" << it->user << ":" << it->message << endl;
            };

            auto dmIt = begin(currentUser->directMessages);
            for (; it != messages.end(); ++it) {
                while (dmIt != end(currentUser->directMessages) && dmIt->lastMessageTime < it->lastMessageTime) {
                    writeMessage(dmIt);
                    dmIt = currentUser->directMessages.erase(dmIt);
                }
                writeMessage(it);
            }
            while (dmIt != end(currentUser->directMessages)) {
                writeMessage(dmIt);
                dmIt = currentUser->directMessages.erase(dmIt);
            }
            connection->reply(200, answer.str());
        } else if (url.substr(0, 6) == "/send/") {
            if (!strcmp(method, "POST")) {
                size_t oldDataRead = connection->dataRead;
                connection->processPostMessage(upload_data, upload_data_size);
                if (oldDataRead > 0 && oldDataRead == connection->dataRead) {
                    string messageText(connection->postData.str());

                    int commandLength;
                    string command = "";
                    if (messageText[0] == '/') {
                        for (commandLength = 1; commandLength < messageText.size(); ++commandLength) {
                            if (messageText[commandLength] < 'a' || messageText[commandLength] > 'z')
                                break;
                        }
                        --commandLength; // started at 1
                        command = messageText.substr(1, commandLength);
                    }
                    if (command == "help") {
                        addDirectMessage(0, "", user, "== This is the help page ==", 'w');
                        addDirectMessage(0, "", user, " /help [text] > Displays this text", 'w');
                        addDirectMessage(0, "", user, " /me [text] > DIY", 'w');
                        addDirectMessage(0, "", user, " /dm [user] [text] > Direct message", 'w');
                        addDirectMessage(0, "", user, " @all > Notify all users", 'w');
                    } else if (command == "dm") {
                        istringstream is(messageText.substr(commandLength + 2));
                        string to;
                        getline(is, to, ' ');
                        if (messageText.size() > commandLength + 3 + to.size())
                            addDirectMessage(currentUser, user, to, messageText.substr(commandLength + 2 + to.size()), 'w');
                    } else {
                        addMessage(user, messageText, 'm');
                    }
                    answer << "OK";
                    connection->reply(200, answer.str());
                }
            } else {
                answer << "Use POST request or restart client.";
                connection->reply(400, answer.str());
            }
        } else if (url.substr(0, 7) == "/users/") {
            for (User& u : users) {
                answer << u.name << endl;
            }
            connection->reply(200, answer.str());
        } else if (url.substr(0, 8) == "/whoami/") {
            connection->reply(200, user);
        } else if (url.substr(0, 8) == "/static/") {
            string fileName(url.substr(8));
            if (fileName.size() > 0 && fileName[0] != '.') {
                bool validFile = true;
                for (auto c : fileName) {
                    if ((c >= 'a' && c <= 'z')
                            || (c >= 'A' && c <= 'Z')
                            || (c >= '0' && c <= '9')
                            || (c == '.' || c == '-')) {
                    } else {
                        validFile = false;
                        break;
                    }
                }
                if (validFile) {
                    string file;
                    if (loadFile(string("static/") + fileName, file))
                        connection->reply(200, file);
                    else {
                        answer << "File \"" << fileName << "\" not found.";
                        connection->reply(404, answer.str());
                    }
                } else {
                    cout << "Bad request: " << fileName << endl;
                    connection->reply(400, "Bad request");
                }
            }
        } else {
            answer << "Page \"" << url << "\" not found.";
            connection->reply(404, answer.str());
        }

        return MHD_YES;
    };
    server.onComplete = [&](Connection* connection) {
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
                if (it->lastOnTime + 15 < currentTime) {
                    stringstream ss;
                    ss << "User \"" << it->name << "\" signed off";
                    addMessage(it->name, ss.str(), '-');
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
