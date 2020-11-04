#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <tuple>
#include<signal.h>

using namespace std;

#define LOCAL_HOST "127.0.0.1"
#define MAX_DATA_SIZE 1024
#define ERROR_FLAG -1
#define SERVER_A_PORT 30500
#define SERVER_B_PORT 31500
#define MAIN_UDP_PORT 32500
#define MAIN_TCP_PORT 33500
#define BACKLOG 100 // A country will have at least one user, and at most 100 users
#define BOOT_UP_MSG "Bootup"

int sockfdClient, sockfdUDP; // Parent socket for client and UDP socket
int childSockfdClient; // Child socket for connection with client
struct sockaddr_in mainClientAddr, mainUDPAddr;
struct sockaddr_in destClientAddr, destServerAAddr, destServerBAddr;

char clientInput[MAX_DATA_SIZE]; // Input data from client
char AResult[MAX_DATA_SIZE]; // result returned from server A
char BResult[MAX_DATA_SIZE]; // result returned from server B
char serverResult[MAX_DATA_SIZE]; // final result from server to client
unordered_map<string, int> countryMap; // map stored the information about the country which backend has

//const string BOOT_UP_MSG = "Bootup";
//const string COUNTRY = "jpYsAHXfNwOVKaFk";
//const string USER = "898";
//const string USER_RESULT = "20";

/**
 * creat client TCP socket
 */
void createClientTCPSocket();
void listenClient();
void createUDPSocket();
void connectToServerA();
void connectToServerB();

vector<string> bootUpServerA(vector<string> countryForA);
vector<string> bootUpServerB(vector<string> countryForB);

void printCountryList(vector<string> countryForA, vector<string> countryForB);
void queryServerA(string input);
void queryServerB(string input);
void sendAResultToClient(string userID, string country);
void sendBResultToClient(string userID, string country);
void handleClientQuery();

int main() {

    vector<string> countryForA;
    vector<string> countryForB;
    createUDPSocket();
    createClientTCPSocket();
    listenClient();
    cout << "The Main server is up and running." << endl;
    cout << endl;
    /**********************************************************************************
     ******************************** Phase 1--Boot Up  *******************************
     **********************************************************************************/
    countryForA = bootUpServerA(countryForA);
    countryForB = bootUpServerB(countryForB);

    // print out country list
    printCountryList(countryForA, countryForB);

    /**********************************************************************************
    ******************************** Phase 2--Query  *********************************
    **********************************************************************************/

//    printf("before fork pid = %d\n",getpid());
    while(true) {
        /******    Accept connection from client using child socket   ******/
        socklen_t clientAddrSize = sizeof(destClientAddr);

        // Accept listening socket (parent)
        childSockfdClient = ::accept(sockfdClient, (struct sockaddr *) &destClientAddr, &clientAddrSize);
        if (childSockfdClient == ERROR_FLAG) {
            perror("[ERROR] mainserver: fail to accept connection with client");
            exit(1);
        }
        /**********************************************************************************
        ******************************** Phase 3&4--Recommendation&Reply  *************************
        **********************************************************************************/
        pid_t pid = fork();
        if (pid == 0) {
            close(sockfdClient);
            while(true) {
                handleClientQuery();
            }
        }
    }
    close(sockfdUDP);
    close(childSockfdClient);
    return 0;

}


/**
 * create TCP socket for client
 */
void createClientTCPSocket() {
    sockfdClient = socket(AF_INET, SOCK_STREAM, 0); // Create TCP socket
    if (sockfdClient == ERROR_FLAG) {
        perror("[ERROR] Main server: fail to create socket for client");
        exit(1);
    }

    // Initialize IP address, port number
    memset(&mainClientAddr, 0, sizeof(mainClientAddr));
    mainClientAddr.sin_family = AF_INET;
    mainClientAddr.sin_addr.s_addr = inet_addr(LOCAL_HOST);
    mainClientAddr.sin_port = htons(MAIN_TCP_PORT);

    // Bind socket for client with IP address and port number for client
    if (::bind(sockfdClient, (struct sockaddr *) &mainClientAddr, sizeof(mainClientAddr)) == ERROR_FLAG) {
        perror("[ERROR] Main server: fail to bind client socket");
        exit(1);
    }

}

void listenClient() {
    if (listen(sockfdClient, BACKLOG) == ERROR_FLAG) {
        perror("[ERROR] mainserver: fail to listen for client socket");
        exit(1);
    }
}

/**
 * Step 3: Create UDP socket and bind socket
 */
void createUDPSocket() {
    // Create socket
    sockfdUDP = socket(AF_INET, SOCK_DGRAM, 0); // UDP datagram socket
    if (sockfdUDP == ERROR_FLAG) {
        perror("[ERROR] mainserver: fail to create UDP socket");
        exit(1);
    }

    // From beej's tutorial
    memset(&mainUDPAddr, 0, sizeof(mainUDPAddr));
    mainUDPAddr.sin_family = AF_INET;
    mainUDPAddr.sin_addr.s_addr = inet_addr(LOCAL_HOST);
    mainUDPAddr.sin_port = htons(MAIN_UDP_PORT);

    // Bind socket
    if (::bind(sockfdUDP, (struct sockaddr *) &mainUDPAddr, sizeof(mainUDPAddr)) == ERROR_FLAG) {
        perror("[ERROR] mainserver: fail to bind UDP socket");
        exit(1);
    }
}

void connectToServerA() {
    // From beej's tutorial
    // Info about server A
    memset(&destServerAAddr, 0, sizeof(destServerAAddr));
    destServerAAddr.sin_family = AF_INET;
    destServerAAddr.sin_addr.s_addr = inet_addr(LOCAL_HOST);
    destServerAAddr.sin_port = htons(SERVER_A_PORT);
}

void connectToServerB() {
    // From beej's tutorial
    // Info about server B
    memset(&destServerBAddr, 0, sizeof(destServerBAddr));
    destServerBAddr.sin_family = AF_INET;
    destServerBAddr.sin_addr.s_addr = inet_addr(LOCAL_HOST);
    destServerBAddr.sin_port = htons(SERVER_B_PORT);
}

void printCountryList(vector<string> countryForA, vector<string> countryForB) {
    int column = max(countryForA.size(),countryForB.size());
    cout << endl;
    cout << "Server A  |" << "Server B"<< endl;
    for (int i = 0; i < column; i++) {
        if (i >= countryForA.size()) {
            cout << "       |";
        } else {
            cout << countryForA[i] << " |";
        }

        if (i >= countryForB.size()) {
            cout << "         " << endl;
        } else {
            cout << countryForB[i] << endl;
        }
    }
    cout << endl;
}

vector<string> bootUpServerA(vector<string> countryForA) {
    char dataBuff[MAX_DATA_SIZE];
    memset(dataBuff,'\0',sizeof(dataBuff));
    strncpy(dataBuff, BOOT_UP_MSG.c_str(), BOOT_UP_MSG.length() + 1);

    // Send to server A to ask for country list
    connectToServerA();
    if (::sendto(sockfdUDP, dataBuff, sizeof(dataBuff), 0, (const struct sockaddr *) &destServerAAddr,
                 sizeof(destServerAAddr)) == ERROR_FLAG) {
        perror("[ERROR] mainserver: fail to send input data to server A");
        exit(1);
    }
//    cout << "send boot up query to server A" << endl; // used for test

    // Receive from Server A to get the country list
    memset(AResult,'\0',sizeof(AResult));
    socklen_t destServerASize = sizeof(destServerAAddr);
    if (::recvfrom(sockfdUDP, AResult, MAX_DATA_SIZE, 0, (struct sockaddr *) &destServerAAddr,
                   &destServerASize) == ERROR_FLAG) {
        perror("[ERROR] mainserver: fail to receive writing result from Server A");
        exit(1);
    }

//    cout << "get country list from server A:" << AResult << endl; // used for test
    cout << "The Main server has received the country list from server A using UDP over port ";
    cout << MAIN_UDP_PORT<< endl;

    char *splitStr = strtok(AResult,"|");
    while(splitStr!=NULL) {
        countryMap[splitStr] = 0;
        countryForA.push_back(splitStr);
        splitStr = strtok(NULL,"|");
    }
    return countryForA;
}


vector<string> bootUpServerB(vector<string> countryForB) {
    char dataBuff[MAX_DATA_SIZE];
    memset(dataBuff,'\0',sizeof(dataBuff));
    strncpy(dataBuff, BOOT_UP_MSG.c_str(), BOOT_UP_MSG.length() + 1);

    // Send to server B to ask for country list
    connectToServerB();
    if (::sendto(sockfdUDP, dataBuff, sizeof(dataBuff), 0, (const struct sockaddr *) &destServerBAddr,
                 sizeof(destServerBAddr)) == ERROR_FLAG) {
        perror("[ERROR] mainserver: fail to send input data to server B");
        exit(1);
    }
//    cout << "send boot up query to server A" << endl; // used for test
    memset(BResult,'\0',sizeof(BResult));
    // Receive from Server B to get the country list
    socklen_t destServerBSize = sizeof(destServerBAddr);
    if (::recvfrom(sockfdUDP, BResult, MAX_DATA_SIZE, 0, (struct sockaddr *) &destServerBAddr,
                   &destServerBSize) == ERROR_FLAG) {
        perror("[ERROR] mainserver: fail to receive writing result from Server B");
        exit(1);
    }

//    cout << "get country list  from server B:" << BResult << endl; // used for test
    cout << "The Main server has received the country list from server B using UDP over port ";
    cout << MAIN_UDP_PORT<< endl;

    char *splitStr = strtok(BResult,"|");
    while(splitStr!=NULL) {
        countryMap[splitStr] = 1;
        countryForB.push_back(splitStr);
        splitStr = strtok(NULL,"|");
    }
    return countryForB;
}

void queryServerA(string input) {
    char dataBuff[MAX_DATA_SIZE];
    memset(dataBuff,'\0',sizeof(dataBuff));
//    cout << "dataBuff in queryServerA: "<< dataBuff << endl;
    strncpy(dataBuff, input.c_str(), strlen(input.c_str()));

    // Send to server A
    connectToServerA();
    if (sendto(sockfdUDP, dataBuff, sizeof(dataBuff), 0, (const struct sockaddr *) &destServerAAddr,
               sizeof(destServerAAddr)) == ERROR_FLAG) {
        perror("[ERROR] mainserver: fail to send input data to server A");
        exit(1);
    }
    cout << "send query to server A" << endl;

    // Receive from Server A
    memset(AResult,'\0',sizeof(AResult));
    socklen_t destServerASize = sizeof(destServerAAddr);
    if (::recvfrom(sockfdUDP, AResult, MAX_DATA_SIZE, 0, (struct sockaddr *) &destServerAAddr,
                   &destServerASize) == ERROR_FLAG) {
        perror("[ERROR] mainserver: fail to receive writing result from Server A");
        exit(1);
    }
//    cout << "get response from server A:" << AResult << endl;
}


void queryServerB(string input) {
    char dataBuff[MAX_DATA_SIZE];
    memset(dataBuff,'\0',sizeof(dataBuff));
    strncpy(dataBuff, input.c_str(), strlen(input.c_str()));

    // Send to server B
    connectToServerB();
    if (sendto(sockfdUDP, dataBuff, sizeof(dataBuff), 0, (const struct sockaddr *) &destServerBAddr,
               sizeof(destServerBAddr)) == ERROR_FLAG) {
        perror("[ERROR] mainserver: fail to send input data to server B");
        exit(1);
    }
    cout << "send query to server B" << endl;

    // Receive from Server B
    memset(BResult,'\0',sizeof(BResult));
    socklen_t destServerBSize = sizeof(destServerBAddr);
    if (::recvfrom(sockfdUDP, BResult, MAX_DATA_SIZE, 0, (struct sockaddr *) &destServerBAddr,
                   &destServerBSize) == ERROR_FLAG) {
        perror("[ERROR] mainserver: fail to receive writing result from Server A");
        exit(1);
    }
//    cout << "get response from server B:" << BResult << endl;
}


void sendAResultToClient(string userID, string country) {
    cout << "The Main Server has sent request from User ";
    cout <<  userID << " to server A ";
    cout << "using UDP over port " <<  MAIN_UDP_PORT << endl;
    string input = userID + "|" + country;
    queryServerA(input);
    string result = AResult;
    // case2: could find this user ID, send "userIDNF"
    if (result == "userIDNF") {
        cout << "The Main server has received \"User ID: Not found\" from server A" << endl;
        memset(&serverResult, 0, sizeof(serverResult));
        strncpy(serverResult, result.c_str(), result.length() + 1);
        // Send userIDNF result to client
        if (sendto(childSockfdClient, serverResult, sizeof(serverResult),
                   0,(struct sockaddr *) &destClientAddr,
                   sizeof(destClientAddr)) == ERROR_FLAG) {
            perror("[ERROR] mainserver: fail to send computed result to client");
            exit(1);
        }
        cout << "The Main Server has sent error to client using TCP over "<< MAIN_TCP_PORT << endl;
    } else {  // case3:  find this user ID, send userID
        cout << "The Main server has received searching result(s) of User " << userID << " from server A" << endl;
        memset(&serverResult, 0, sizeof(serverResult));
        strncpy(serverResult, result.c_str(), result.length() + 1);
        // Send userID result to client
        if (sendto(childSockfdClient, serverResult, sizeof(serverResult),
                   0,(struct sockaddr *) &destClientAddr,
                   sizeof(destClientAddr)) == ERROR_FLAG) {
            perror("[ERROR] mainserver: fail to send computed result to client");
            exit(1);
        }
        cout << "The Main Server has sent searching result(s) to client using TCP over port "<< MAIN_TCP_PORT << endl;
    }
}

void sendBResultToClient(string userID, string country) {
    cout << "The Main Server has sent request from User ";
    cout <<  userID << "to server B ";
    cout << "using UDP over port" <<  MAIN_UDP_PORT << endl;
    string input = userID + "|" + country;
//    cout << "input in sendBResultToClient is: "<< input<< endl; // used for test

    queryServerB(input);
    string result = BResult;
    // case2: could find this user ID, send "userIDNF"
    if (result == "userIDNF") {
        cout << "The Main server has received \"User ID: Not found\" from server B" << endl;
        memset(&serverResult, 0, sizeof(serverResult));
        strncpy(serverResult, result.c_str(), result.length() + 1);
        // Send userIDNF result to client
        if (sendto(childSockfdClient, serverResult, sizeof(serverResult),
                   0,(struct sockaddr *) &destClientAddr,
                   sizeof(destClientAddr)) == ERROR_FLAG) {
            perror("[ERROR] mainserver: fail to send computed result to client");
            exit(1);
        }
        cout << "The Main Server has sent error to client using TCP over "<< MAIN_TCP_PORT << endl;
    } else {  // case3:  find this user ID, send userID
        cout << "The Main server has received searching result(s) of User " << userID << " from server B" << endl;
        memset(&serverResult, 0, sizeof(serverResult));
        strncpy(serverResult, result.c_str(), result.length() + 1);
        // Send userID result to client
        if (sendto(childSockfdClient, serverResult, sizeof(serverResult),
                   0,(struct sockaddr *) &destClientAddr,
                   sizeof(destClientAddr)) == ERROR_FLAG) {
            perror("[ERROR] mainserver: fail to send computed result to client");
            exit(1);
        }
        cout << "The Main Server has sent searching result(s) to client using TCP over port "<< MAIN_TCP_PORT << endl;
    }
}

void handleClientQuery() {

    /******    Receive input from client  ******/
    // From beej's tutorial
    memset(&clientInput, 0, sizeof(clientInput));
    // Receive through child socket
    if (recv(childSockfdClient, clientInput, MAX_DATA_SIZE, 0) == ERROR_FLAG) {
        perror("[ERROR] mainserver: fail to receive input data from client");
        exit(1);
    }
//    cout << "The client input is " << clientInput << endl;

    char *splitStr = strtok(clientInput,"|");
    string userID = splitStr;
    splitStr = strtok(NULL,"|");
    string country = splitStr;

    cout << "The Main server has received the request on User " << userID;
    cout << " in " << country << " from client using TCP over port " << MAIN_TCP_PORT << endl;

    /**********************************************************************************
    ******************************** Phase 4--Reply  *************************
    **********************************************************************************/
    // case1: country name could not be found
    if (countryMap.count(country) == 0) {
        cout << country << " does not show up in server A&B"<< endl;
        string CountryNFResult = "CountryNF";
        memset(&serverResult, 0, sizeof(serverResult));
        strncpy(serverResult, CountryNFResult.c_str(), CountryNFResult.length() + 1);
        // Send CountryNF result to client
        if (sendto(childSockfdClient, serverResult, sizeof(serverResult),
                   0,(struct sockaddr *) &destClientAddr,
                   sizeof(destClientAddr)) == ERROR_FLAG) {
            perror("[ERROR] mainserver: fail to send computed result to client");
            exit(1);
        }
        cout << "The Main Server has sent \"Country Name: Not found\" to client using TCP over port ";
        cout << MAIN_TCP_PORT << endl;

    }
    else {
        cout << country << " shows up in server A/B"<< endl;
        if (countryMap[country] == 0) {
            sendAResultToClient(userID, country);
        } else {
            sendBResultToClient(userID, country);
        }
    }
}
