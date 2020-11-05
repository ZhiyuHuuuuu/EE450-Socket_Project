#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <tuple>

using namespace std;

#define LOCAL_HOST "127.0.0.1"
#define MAIN_TCP_PORT 33500
#define MAX_DATA_SIZE 1024
#define ERROR_FLAG -1

int sockfdClient; // client socket
struct sockaddr_in mainAddr; // mainserver address for TCP connection
char clientInput[MAX_DATA_SIZE]; // input data from client
char serverResult[MAX_DATA_SIZE];// result from server

/**
 * create client TCP socket
 */
void createClientTCPSocket();

/**
 * connect to mainserver using TCP
 */
void requestMainConnection();

/**
 * get the client input and query the mainserver to get the recommendation
 */
void queryMainserver();

int main() {
    createClientTCPSocket();
    requestMainConnection();

    //booting up successfully
    cout << "Client is up and running"<<endl;
    cout << endl;
    // the first request(without message "-----Start a new request-----")
    queryMainserver();
    while(true) {
        // a new request
        cout << "-----Start a new request-----"<<endl;
        queryMainserver();
    }
    close(sockfdClient);
    return 0;
}

/**
 * create client TCP socket
 */
void createClientTCPSocket() {
    sockfdClient = socket(AF_INET, SOCK_STREAM, 0); // create a TCP socket
    if (sockfdClient == ERROR_FLAG) {
        perror("[ERROR] client: can not open client socket ");
        exit(1);
    }
}

/**
 * connect to mainserver using TCP
 */
void requestMainConnection() {

    // from beej's tutorial
    memset(&mainAddr, 0, sizeof(mainAddr));
    mainAddr.sin_family = AF_INET;
    mainAddr.sin_addr.s_addr = inet_addr(LOCAL_HOST);
    mainAddr.sin_port = htons(MAIN_TCP_PORT);

    if (connect(sockfdClient, (struct sockaddr *) &mainAddr, sizeof(mainAddr)) == ERROR_FLAG) {
        perror("[ERROR] client: fail to connect with AWS server");
        close(sockfdClient);
        exit(1);
    }
}

/**
 * get the client input and query the mainserver to get the recommendation
 */
void queryMainserver() {
    string userID;
    string countryName;

    cout << "Please enter the User ID:";
    cin >> userID;
    cout << "Please enter the Country Name:";
    cin >> countryName;
    cout << endl;

    string str = userID + "|" + countryName;
    memset(clientInput,'\0',sizeof(clientInput));
    strncpy(clientInput, str.c_str(), MAX_DATA_SIZE);
//    cout << "The query is " <<str << endl; // used for test

    // send data to mainserver
    if (send(sockfdClient, clientInput, sizeof(clientInput), 0) == ERROR_FLAG) {
        perror("[ERROR] client: fail to send input data");
        close(sockfdClient);
        exit(1);
    }
    cout << "Client has sent User " << userID << " and " << countryName << " to Main Server using TCP"<< endl;
    cout << endl;

    // get result back from mainserver
    memset(serverResult,'\0',sizeof(serverResult));
    if (recv(sockfdClient, serverResult, sizeof(serverResult), 0) == ERROR_FLAG) {
        perror("[ERROR] client: fail to receive write result from main server");
        close(sockfdClient);
        exit(1);
    }

//    cout << "get response from mainserver:" << serverResult << endl; // used for test

    // deal with the result
    string result = serverResult;
    if (result == "CountryNF") {
        cout << countryName <<" not found" <<endl;
        cout << endl;
    } else if (result == "userIDNF") {
        cout << "User " << userID <<" not found" <<endl;
        cout << endl;
    } else {
        cout << "Client has received results from Main Server: User " << result;
        cout << " is possible friend of User " << userID << " in "  << countryName << endl;
        cout << endl;

    }
}