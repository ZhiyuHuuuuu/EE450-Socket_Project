
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
#define BACKLOG 100 // a country will have at least one user, and at most 100 users

int sockfdClient, sockfdUDP; // parent socket for client and UDP socket
int childSockfdClient; // child socket for connection with client
struct sockaddr_in mainClientAddr, mainUDPAddr;
struct sockaddr_in destClientAddr, destServerAAddr, destServerBAddr; // mainserver works as client, it should be destination

char clientInput[MAX_DATA_SIZE]; // input data from client
char AResult[MAX_DATA_SIZE]; // result returned from server A
char BResult[MAX_DATA_SIZE]; // result returned from server B
char serverResult[MAX_DATA_SIZE]; // final result from server to client
unordered_map<string, int> countryMap; // map stored the information about the country which backend has
const string BOOT_UP_MSG = "Bootup"; // boot up message

/**
 * create client TCP parent socket(create, bind and listen)
 */
void createClientTCPSocket();

/**
 * create UDP socket(create and bind)
 */
void createUDPSocket();

/**
 * connect to server A using UDP
 */
void connectToServerA();

/**
 * connect to server B using UDP
 */
void connectToServerB();

/**
 * boot up operation for server A
 * @param countryForA empty country list
 * @return country list the server A has
 */
vector<string> bootUpServerA(vector<string> countryForA);

/**
 * boot up operation for server B
 * @param countryForB empty country list
 * @return country list the serverB has
 */
vector<string> bootUpServerB(vector<string> countryForB);

/**
 * print the country list in mainserver terminal
 * @param countryForA country list the server A has
 * @param countryForB country list the server B has
 */
void printCountryList(vector<string> countryForA, vector<string> countryForB);

/**
 * send input message to server A to get the recommendation(A must contain the country name,
 * otherwise mainserver will send "country not found" directly to client)
 * @param input the input message of client (format: UserID|CountryName)
 */
void queryServerA(string input);

/**
 * send input message to server B to get the recommendation(B must contain the country name,
 * otherwise mainserver will send "country not found" directly to client)
 * @param input the input message of client (format: UserID|CountryName)
 */

void queryServerB(string input);

/**
 * after mainserver gets the recommendation from A(use method queryServerA()), send the recommendation back to client
 * @param userID the userID that the client inputs
 * @param country the country name that the client inputs
 */
void sendAResultToClient(string userID, string country);

/**
 * after mainserver gets the recommendation from B(use method queryServerB()), send the recommendation back to client
 * @param userID the userID that the client inputs
 * @param country the country name that the client inputs
 */
void sendBResultToClient(string userID, string country);

/**
 * handle the client input, there should be three cases:
 *      case1: country name could not be found in country list, send "countryNF" to client
 *      case2: could not find this user ID in corresponding backend server, send "userIDNF" to client
 *      case3: find this user ID, send userID, send the userID to client
 */
void handleClientQuery();

int main() {
    vector<string> countryForA;
    vector<string> countryForB;

    createUDPSocket();
    createClientTCPSocket();
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

    while(true) {
        // accept listening socket (parent)
        socklen_t clientAddrSize = sizeof(destClientAddr);
        childSockfdClient = ::accept(sockfdClient, (struct sockaddr *) &destClientAddr, &clientAddrSize);
        if (childSockfdClient == ERROR_FLAG) {
            perror("[ERROR] mainserver: fail to accept connection with client");
            exit(1);
        }
        /**********************************************************************************
        ******************************** Phase 3&4--Recommendation&Reply  *************************
        **********************************************************************************/
        // use fork() to deal with two clients' query
        // reference from https://www.youtube.com/watch?v=BIJGSQEipEE
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
 * create TCP socket for client(creat, bind, listen)
 */
void createClientTCPSocket() {
    sockfdClient = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfdClient == ERROR_FLAG) {
        perror("[ERROR] Main server: fail to create socket for client");
        exit(1);
    }

    // from beej's tutorial
    // initialize IP address, port number
    memset(&mainClientAddr, 0, sizeof(mainClientAddr));
    mainClientAddr.sin_family = AF_INET;
    mainClientAddr.sin_addr.s_addr = inet_addr(LOCAL_HOST);
    mainClientAddr.sin_port = htons(MAIN_TCP_PORT);

    // bind socket for client with IP address and port number for client
    if (::bind(sockfdClient, (struct sockaddr *) &mainClientAddr, sizeof(mainClientAddr)) == ERROR_FLAG) {
        perror("[ERROR] Main server: fail to bind client socket");
        exit(1);
    }

    // listen to connections from clients
    if (::listen(sockfdClient, BACKLOG) == ERROR_FLAG) {
        perror("[ERROR] mainserver: fail to listen for client socket");
        exit(1);
    }
}


/**
 * Create UDP socket for backend server(create, bind)
 */
void createUDPSocket() {
    // create UDP socket
    sockfdUDP = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfdUDP == ERROR_FLAG) {
        perror("[ERROR] mainserver: fail to create UDP socket");
        exit(1);
    }

    // from beej's tutorial
    memset(&mainUDPAddr, 0, sizeof(mainUDPAddr));
    mainUDPAddr.sin_family = AF_INET;
    mainUDPAddr.sin_addr.s_addr = inet_addr(LOCAL_HOST);
    mainUDPAddr.sin_port = htons(MAIN_UDP_PORT);

    // bind socket
    if (::bind(sockfdUDP, (struct sockaddr *) &mainUDPAddr, sizeof(mainUDPAddr)) == ERROR_FLAG) {
        perror("[ERROR] mainserver: fail to bind UDP socket");
        exit(1);
    }
}

/**
 * connect to server A using UDP, mainserver should connect to serverA every time it query
 */
void connectToServerA() {
    // from beej's tutorial
    // info about server A
    memset(&destServerAAddr, 0, sizeof(destServerAAddr));
    destServerAAddr.sin_family = AF_INET;
    destServerAAddr.sin_addr.s_addr = inet_addr(LOCAL_HOST);
    destServerAAddr.sin_port = htons(SERVER_A_PORT);
}

/**
 * connect to server B using UDP, mainserver should connect to serverA every time it query
 */
void connectToServerB() {
    // from beej's tutorial
    // info about server B
    memset(&destServerBAddr, 0, sizeof(destServerBAddr));
    destServerBAddr.sin_family = AF_INET;
    destServerBAddr.sin_addr.s_addr = inet_addr(LOCAL_HOST);
    destServerBAddr.sin_port = htons(SERVER_B_PORT);
}

/**
 * print the country list in mainserver terminal
 * @param countryForA country list the server A has
 * @param countryForB country list the server B has
 */
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

/**
 * boot up operation for server A
 * @param countryForA the empty country list
 * @return country list the server A has
 */
vector<string> bootUpServerA(vector<string> countryForA) {
    char dataBuff[MAX_DATA_SIZE];
    memset(dataBuff,'\0',sizeof(dataBuff));
    strncpy(dataBuff, BOOT_UP_MSG.c_str(), BOOT_UP_MSG.length() + 1);

    // send to server A to ask for country list
    connectToServerA();
    if (::sendto(sockfdUDP, dataBuff, sizeof(dataBuff), 0, (const struct sockaddr *) &destServerAAddr,
                 sizeof(destServerAAddr)) == ERROR_FLAG) {
        perror("[ERROR] mainserver: fail to send input data to server A");
        exit(1);
    }
//    cout << "send boot up query to server A" << endl; // used for test

    // receive from Server A to get the country list
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

/**
 * boot up operation for server B
 * @param countryForB the empty country list
 * @return country list the server B has
 */
vector<string> bootUpServerB(vector<string> countryForB) {
    char dataBuff[MAX_DATA_SIZE];
    memset(dataBuff,'\0',sizeof(dataBuff));
    strncpy(dataBuff, BOOT_UP_MSG.c_str(), BOOT_UP_MSG.length() + 1);

    // send to server B to ask for country list
    connectToServerB();
    if (::sendto(sockfdUDP, dataBuff, sizeof(dataBuff), 0, (const struct sockaddr *) &destServerBAddr,
                 sizeof(destServerBAddr)) == ERROR_FLAG) {
        perror("[ERROR] mainserver: fail to send input data to server B");
        exit(1);
    }
//    cout << "send boot up query to server A" << endl; // used for test
    memset(BResult,'\0',sizeof(BResult));
    // receive from Server B to get the country list
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

/**
 * send input message to server A to get the recommendation(A must contain the country name,
 * otherwise mainserver will send "country not found" directly to client)
 * @param input the input message of client (format: UserID|CountryName)
 */
void queryServerA(string input) {
    char dataBuff[MAX_DATA_SIZE];
    memset(dataBuff,'\0',sizeof(dataBuff));
//    cout << "dataBuff in queryServerA: "<< dataBuff << endl;// used for test
    strncpy(dataBuff, input.c_str(), strlen(input.c_str()));

    // send to server A
    connectToServerA();
    if (sendto(sockfdUDP, dataBuff, sizeof(dataBuff), 0, (const struct sockaddr *) &destServerAAddr,
               sizeof(destServerAAddr)) == ERROR_FLAG) {
        perror("[ERROR] mainserver: fail to send input data to server A");
        exit(1);
    }

    // receive from Server A
    memset(AResult,'\0',sizeof(AResult));
    socklen_t destServerASize = sizeof(destServerAAddr);
    if (::recvfrom(sockfdUDP, AResult, MAX_DATA_SIZE, 0, (struct sockaddr *) &destServerAAddr,
                   &destServerASize) == ERROR_FLAG) {
        perror("[ERROR] mainserver: fail to receive writing result from Server A");
        exit(1);
    }
//    cout << "get response from server A:" << AResult << endl;// used for test
}

/**
 * send input message to server B to get the recommendation(B must contain the country name,
 * otherwise mainserver will send "country not found" directly to client)
 * @param input the input message of client (format: UserID|CountryName)
 */
void queryServerB(string input) {
    char dataBuff[MAX_DATA_SIZE];
    memset(dataBuff,'\0',sizeof(dataBuff));
    strncpy(dataBuff, input.c_str(), strlen(input.c_str()));

    // send to server B
    connectToServerB();
    if (sendto(sockfdUDP, dataBuff, sizeof(dataBuff), 0, (const struct sockaddr *) &destServerBAddr,
               sizeof(destServerBAddr)) == ERROR_FLAG) {
        perror("[ERROR] mainserver: fail to send input data to server B");
        exit(1);
    }
    // receive from Server B
    memset(BResult,'\0',sizeof(BResult));
    socklen_t destServerBSize = sizeof(destServerBAddr);
    if (::recvfrom(sockfdUDP, BResult, MAX_DATA_SIZE, 0, (struct sockaddr *) &destServerBAddr,
                   &destServerBSize) == ERROR_FLAG) {
        perror("[ERROR] mainserver: fail to receive writing result from Server A");
        exit(1);
    }
//    cout << "get response from server B:" << BResult << endl;// used for test
}

/**
 * after mainserver gets the recommendation from A(use method queryServerA()), send the recommendation back to client
 * @param userID the userID that the client inputs
 * @param country the country name that the client inputs
 */
void sendAResultToClient(string userID, string country) {
    cout << "The Main Server has sent request from User ";
    cout <<  userID << " to server A ";
    cout << "using UDP over port " <<  MAIN_UDP_PORT << endl;
    cout << endl;
    string input = userID + "|" + country;
    queryServerA(input);
    string result = AResult;
    // case2: could not find this user ID, send "userIDNF"
    if (result == "userIDNF") {
        cout << "The Main server has received \"User ID: Not found\" from server A" << endl;
        cout << endl;
        memset(&serverResult, 0, sizeof(serverResult));
        strncpy(serverResult, result.c_str(), result.length() + 1);
        // send userIDNF result to client
        if (sendto(childSockfdClient, serverResult, sizeof(serverResult),
                   0,(struct sockaddr *) &destClientAddr,
                   sizeof(destClientAddr)) == ERROR_FLAG) {
            perror("[ERROR] mainserver: fail to send computed result to client");
            exit(1);
        }
        cout << "The Main Server has sent error to client using TCP over "<< MAIN_TCP_PORT << endl;
        cout << endl;
    } else {  // case3:  find this user ID, send userID
        cout << "The Main server has received searching result(s) of User " << userID << " from server A" << endl;
        cout << endl;
        memset(&serverResult, 0, sizeof(serverResult));
        strncpy(serverResult, result.c_str(), result.length() + 1);
        // send userID result to client
        if (sendto(childSockfdClient, serverResult, sizeof(serverResult),
                   0,(struct sockaddr *) &destClientAddr,
                   sizeof(destClientAddr)) == ERROR_FLAG) {
            perror("[ERROR] mainserver: fail to send computed result to client");
            exit(1);
        }
        cout << "The Main Server has sent searching result(s) to client using TCP over port "<< MAIN_TCP_PORT << endl;
        cout << endl;
    }
}

/**
 * after mainserver gets the recommendation from B(use method queryServerB()), send the recommendation back to client
 * @param userID the userID that the client inputs
 * @param country the country name that the client inputs
 */
void sendBResultToClient(string userID, string country) {
    cout << "The Main Server has sent request from User ";
    cout <<  userID << " to server B ";
    cout << "using UDP over port " <<  MAIN_UDP_PORT << endl;
    cout << endl;
    string input = userID + "|" + country;
//    cout << "input in sendBResultToClient is: "<< input<< endl; // used for test

    queryServerB(input);
    string result = BResult;
    // case2: could find this user ID, send "userIDNF"
    if (result == "userIDNF") {
        cout << "The Main server has received \"User ID: Not found\" from server B" << endl;
        cout << endl;
        memset(&serverResult, 0, sizeof(serverResult));
        strncpy(serverResult, result.c_str(), result.length() + 1);
        // send userIDNF result to client
        if (sendto(childSockfdClient, serverResult, sizeof(serverResult),
                   0,(struct sockaddr *) &destClientAddr,
                   sizeof(destClientAddr)) == ERROR_FLAG) {
            perror("[ERROR] mainserver: fail to send computed result to client");
            exit(1);
        }
        cout << "The Main Server has sent error to client using TCP over "<< MAIN_TCP_PORT << endl;
        cout << endl;
    } else {  // case3:  find this user ID, send userID
        cout << "The Main server has received searching result(s) of User " << userID << " from server B" << endl;
        cout << endl;
        memset(&serverResult, 0, sizeof(serverResult));
        strncpy(serverResult, result.c_str(), result.length() + 1);
        // send userID result to client
        if (sendto(childSockfdClient, serverResult, sizeof(serverResult),
                   0,(struct sockaddr *) &destClientAddr,
                   sizeof(destClientAddr)) == ERROR_FLAG) {
            perror("[ERROR] mainserver: fail to send computed result to client");
            exit(1);
        }
        cout << "The Main Server has sent searching result(s) to client using TCP over port "<< MAIN_TCP_PORT << endl;
        cout << endl;
    }
}

/**
 * handle the client input, there should be three cases:
 *      case1: country name could not be found in country list, send "countryNF" to client
 *      case2: could not find this user ID in corresponding backend server, send "userIDNF" to client
 *      case3: find this user ID, send userID, send the userID to client
 */
void handleClientQuery() {

    /******    Receive input from client  ******/
    // from beej's tutorial
    memset(&clientInput, 0, sizeof(clientInput));
    // receive through child socket
    if (recv(childSockfdClient, clientInput, MAX_DATA_SIZE, 0) == ERROR_FLAG) {
        perror("[ERROR] mainserver: fail to receive input data from client");
        exit(1);
    }

    char *splitStr = strtok(clientInput,"|");
    string userID = splitStr;
    splitStr = strtok(NULL,"|");
    string country = splitStr;

    cout << "The Main server has received the request on User " << userID;
    cout << " in " << country << " from client using TCP over port " << MAIN_TCP_PORT << endl;
    cout << endl;

    /**********************************************************************************
    ******************************** Phase 4--Reply  *************************
    **********************************************************************************/
    // case1: country name could not be found
    if (countryMap.count(country) == 0) {
        cout << country << " does not show up in server A&B"<< endl;
        cout << endl;
        string CountryNFResult = "CountryNF";
        memset(&serverResult, 0, sizeof(serverResult));
        strncpy(serverResult, CountryNFResult.c_str(), CountryNFResult.length() + 1);
        // send CountryNF result to client
        if (sendto(childSockfdClient, serverResult, sizeof(serverResult),
                   0,(struct sockaddr *) &destClientAddr,
                   sizeof(destClientAddr)) == ERROR_FLAG) {
            perror("[ERROR] mainserver: fail to send computed result to client");
            exit(1);
        }
        cout << "The Main Server has sent \"Country Name: Not found\" to client using TCP over port ";
        cout << MAIN_TCP_PORT << endl;
        cout << endl;

    }
    else {

        if (countryMap[country] == 0) {
            cout << country << " shows up in server A"<< endl;
            cout << endl;
            sendAResultToClient(userID, country);
        } else {
            cout << country << " shows up in server B"<< endl;
            cout << endl;
            sendBResultToClient(userID, country);
        }
    }
}
