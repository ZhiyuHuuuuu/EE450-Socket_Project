#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <vector>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <tuple>

using namespace std;

#define LOCAL_HOST "127.0.0.1"
#define MAX_DATA_SIZE 1024
#define ERROR_FLAG -1
#define SERVER_A_PORT 30500
#define SERVER_B_PORT 31500
#define MAIN_UDP_PORT 32500
#define MAIN_TCP_PORT 33500

int sockfd_client_TCP, sockfd_monitor_TCP, sockfdUDP; // Parent socket for client & for monitor & UDP socket
int child_sockfd_client, child_sockfd_monitor; // Child socket for connection with client and monitor
struct sockaddr_in aws_client_addr, aws_monitor_addr, mainUDPAddr;
struct sockaddr_in dest_client_addr, dest_monitor_addr, destServerAAddr, destServerBAddr; // When AWS works as a client

char clientInput[MAX_DATA_SIZE]; // Input data from client
char AResult[MAX_DATA_SIZE]; // Computed data returned from server A
char BResult[MAX_DATA_SIZE]; // Computed data returned from server B

const string BOOT_UP_MSG = "Bootup";
unordered_map<string, int> countryMap;

const string COUNTRY = "jpYsAHXfNwOVKaFk";
const string USER = "898";

void createUDPSocket();
void connectToServerA();
void connectToServerB();
vector<string> bootUpServerA(vector<string> countryForA);
vector<string> bootUpServerB(vector<string> countryForB);
void queryServerA();
void queryServerB();


void printCountryList(vector<string> countryForA, vector<string> countryForB);

int main() {

    vector<string> countryForA;
    vector<string> countryForB;
    createUDPSocket();
    cout << "The Main server is up and running." << endl;

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
        queryServerA();
        queryServerB();
        break;
    }
    close(sockfdUDP);

}

/**
 * Step 3: Create UDP socket and bind socket
 */
void createUDPSocket() {
    // Create socket
    sockfdUDP = socket(AF_INET, SOCK_DGRAM, 0); // UDP datagram socket
    if (sockfdUDP == ERROR_FLAG) {
        perror("[ERROR] AWS server: fail to create UDP socket");
        exit(1);
    }

    // Initialize IP address, port number
    memset(&mainUDPAddr, 0, sizeof(mainUDPAddr)); //  make sure the struct is empty
    mainUDPAddr.sin_family = AF_INET; // Use IPv4 address family
    mainUDPAddr.sin_addr.s_addr = inet_addr(LOCAL_HOST); // Host IP address
    mainUDPAddr.sin_port = htons(MAIN_UDP_PORT); // Port number for client

    // Bind socket
    if (::bind(sockfdUDP, (struct sockaddr *) &mainUDPAddr, sizeof(mainUDPAddr)) == ERROR_FLAG) {
        perror("[ERROR] AWS server: fail to bind UDP socket");
        exit(1);
    }
}

void connectToServerA() {
    // Info about server A
    memset(&destServerAAddr, 0, sizeof(destServerAAddr));
    destServerAAddr.sin_family = AF_INET;
    destServerAAddr.sin_addr.s_addr = inet_addr(LOCAL_HOST);
    destServerAAddr.sin_port = htons(SERVER_A_PORT);
}

void connectToServerB() {
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
}

vector<string> bootUpServerA(vector<string> countryForA) {
    char dataBuff[MAX_DATA_SIZE];
    strncpy(dataBuff, BOOT_UP_MSG.c_str(), BOOT_UP_MSG.length() + 1);

    // Send to server A to ask for country list
    connectToServerA();
    if (::sendto(sockfdUDP, dataBuff, sizeof(dataBuff), 0, (const struct sockaddr *) &destServerAAddr,
                 sizeof(destServerAAddr)) == ERROR_FLAG) {
        perror("[ERROR] MAIN: fail to send input data to server A");
        exit(1);
    }
    cout << "send boot up query to server A" << endl; // used for test

    // Receive from Server A to get the country list
    socklen_t destServerASize = sizeof(destServerAAddr);
    if (::recvfrom(sockfdUDP, AResult, MAX_DATA_SIZE, 0, (struct sockaddr *) &destServerAAddr,
                   &destServerASize) == ERROR_FLAG) {
        perror("[ERROR] MAIN: fail to receive writing result from Server A");
        exit(1);
    }
//        printf("The MAIN received response from Backend-Server A for writing using UDP over port <%d> \n",
//               MAIN_UDP_PORT);
    cout << "get country list  from server A:" << AResult << endl; // used for test
    cout << "The Main server has received the country list from server A using UDP over port ";
    cout << MAIN_UDP_PORT<< endl;

    char *splitStr = strtok(AResult,"|");
    while(splitStr!=NULL) {
        countryMap[splitStr] = 0;
        countryForA.push_back(splitStr);
//        countryForB.push_back(splitStr); // used for test
        splitStr = strtok(NULL,"|");
    }
//    countryForA.push_back("China"); // used for test
    return countryForA;
}


vector<string> bootUpServerB(vector<string> countryForB) {
    char dataBuff[MAX_DATA_SIZE];
    strncpy(dataBuff, BOOT_UP_MSG.c_str(), BOOT_UP_MSG.length() + 1);

    // Send to server B to ask for country list
    connectToServerB();
    if (::sendto(sockfdUDP, dataBuff, sizeof(dataBuff), 0, (const struct sockaddr *) &destServerBAddr,
                 sizeof(destServerBAddr)) == ERROR_FLAG) {
        perror("[ERROR] MAIN: fail to send input data to server B");
        exit(1);
    }
    cout << "send boot up query to server A" << endl; // used for test

    // Receive from Server B to get the country list
    socklen_t destServerBSize = sizeof(destServerBAddr);
    if (::recvfrom(sockfdUDP, BResult, MAX_DATA_SIZE, 0, (struct sockaddr *) &destServerBAddr,
                   &destServerBSize) == ERROR_FLAG) {
        perror("[ERROR] MAIN: fail to receive writing result from Server B");
        exit(1);
    }
//        printf("The MAIN received response from Backend-Server B for writing using UDP over port <%d> \n",
//               MAIN_UDP_PORT);
    cout << "get country list  from server B:" << BResult << endl; // used for test
    cout << "The Main server has received the country list from server B using UDP over port ";
    cout << MAIN_UDP_PORT<< endl;

    char *splitStr = strtok(BResult,"|");
    while(splitStr!=NULL) {
        countryMap[splitStr] = 1;
//        countryForA.push_back(splitStr);
        countryForB.push_back(splitStr); // used for test
        splitStr = strtok(NULL,"|");
    }
    return countryForB;
}

void queryServerA() {
    char dataBuff[MAX_DATA_SIZE];
    strncpy(dataBuff, clientInput, strlen(clientInput));
    string msg = USER + "|" + COUNTRY;
    strncpy(dataBuff, msg.c_str(), msg.length() + 1);

    // Send to server A
    connectToServerA();
    if (sendto(sockfdUDP, dataBuff, sizeof(dataBuff), 0, (const struct sockaddr *) &destServerAAddr,
               sizeof(destServerAAddr)) == ERROR_FLAG) {
        perror("[ERROR] MAIN: fail to send input data to server A");
        exit(1);
    }
    cout << "send query to server A" << endl;

    // Receive from Server A
    memset(AResult,'\0',sizeof(AResult));
    socklen_t destServerASize = sizeof(destServerAAddr);
    if (::recvfrom(sockfdUDP, AResult, MAX_DATA_SIZE, 0, (struct sockaddr *) &destServerAAddr,
                   &destServerASize) == ERROR_FLAG) {
        perror("[ERROR] MAIN: fail to receive writing result from Server A");
        exit(1);
    }
//        printf("The MAIN received response from Backend-Server A for writing using UDP over port <%d> \n",
//               MAIN_UDP_PORT);
    cout << "get response from server A:" << AResult << endl;
}


void queryServerB() {
    char dataBuff[MAX_DATA_SIZE];
    strncpy(dataBuff, clientInput, strlen(clientInput));
//    string msg = USER + "|" + COUNTRY;
    string msg = "385|TQanqBFWKjJUmhlk";

    strncpy(dataBuff, msg.c_str(), msg.length() + 1);

    // Send to server B
    connectToServerB();
    if (sendto(sockfdUDP, dataBuff, sizeof(dataBuff), 0, (const struct sockaddr *) &destServerBAddr,
               sizeof(destServerBAddr)) == ERROR_FLAG) {
        perror("[ERROR] MAIN: fail to send input data to server B");
        exit(1);
    }
    cout << "send query to server B" << endl;

    // Receive from Server B
    memset(BResult,'\0',sizeof(BResult));
    socklen_t destServerBSize = sizeof(destServerBAddr);
    if (::recvfrom(sockfdUDP, BResult, MAX_DATA_SIZE, 0, (struct sockaddr *) &destServerBAddr,
                   &destServerBSize) == ERROR_FLAG) {
        perror("[ERROR] MAIN: fail to receive writing result from Server A");
        exit(1);
    }
//        printf("The MAIN received response from Backend-Server A for writing using UDP over port <%d> \n",
//               MAIN_UDP_PORT);
    cout << "get response from server B:" << BResult << endl;
}