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
#define MAIN_TCP_PORT 33500
#define MAX_DATA_SIZE 1024
#define ERROR_FLAG -1


int sockfdClient;
struct sockaddr_in mainAddr;
char clientInput[MAX_DATA_SIZE]; // Store input
//char compute_buf[MAX_DATA_SIZE]; // Store input to compute (send to AWS)
char serverResult[MAX_DATA_SIZE];
//char compute_result[MAX_DATA_SIZE]; // Compute result from AWS


const string COUNTRY = "cYLEUu";
const string USER = "468761846";
//const string COUNTRY = "Canada";
//const string USER = "11";

// 1. Create TCP socket
void createClientTCPSocket();

// 2. Initialize TCP connection with mainserver
void initMainConnection();

// 3. Send connection request to mainserver
void requestMainConnection();

int main() {
    createClientTCPSocket();
    initMainConnection();
    requestMainConnection();
    string userID;
    string countryName;

    cout << "Please enter the User ID:";
    cin >> userID;
    cout << "Please enter the Country Name:";
    cin >> countryName;

    string str = userID + "|" + countryName;
    memset(clientInput,'\0',sizeof(clientInput));
    strncpy(clientInput, str.c_str(), MAX_DATA_SIZE);
    cout << "The query is " <<str << endl;

    /******    Step 4:  Send data to mainserver    *******/
    if (send(sockfdClient, clientInput, sizeof(clientInput), 0) == ERROR_FLAG) {
        perror("[ERROR] client: fail to send input data");
        close(sockfdClient);
        exit(1);
    }
    printf("The client sent query to main server \n");

    /******    Step 5:  Get  result back from mainserver    *******/
    memset(serverResult,'\0',sizeof(serverResult));
    if (recv(sockfdClient, serverResult, sizeof(serverResult), 0) == ERROR_FLAG) {
        perror("[ERROR] client: fail to receive write result from main server");
        close(sockfdClient);
        exit(1);
    }

    cout << "get response from mainserver:" << serverResult << endl;
    while(true) {
        cout << "-----Start a new request-----"<<endl;
        cout << "Please enter the User ID:";
        cin >> userID;
        cout << "Please enter the Country Name:";
        cin >> countryName;

        string str = userID + "|" + countryName;
        memset(clientInput,'\0',sizeof(clientInput));
        strncpy(clientInput, str.c_str(), MAX_DATA_SIZE);
        cout << "The query is " <<str << endl;

        /******    Step 4:  Send data to mainserver    *******/
        if (send(sockfdClient, clientInput, sizeof(clientInput), 0) == ERROR_FLAG) {
            perror("[ERROR] client: fail to send input data");
            close(sockfdClient);
            exit(1);
        }
        printf("The client sent query to main server \n");

        /******    Step 5:  Get  result back from mainserver    *******/
        memset(serverResult,'\0',sizeof(serverResult));
        if (recv(sockfdClient, serverResult, sizeof(serverResult), 0) == ERROR_FLAG) {
            perror("[ERROR] client: fail to receive write result from main server");
            close(sockfdClient);
            exit(1);
        }

        cout << "get response from mainserver:" << serverResult << endl;
    }

    close(sockfdClient);
    return 0;
}

void createClientTCPSocket() {
    sockfdClient = socket(AF_INET, SOCK_STREAM, 0); // Create a TCP socket
    if (sockfdClient == ERROR_FLAG) {
        perror("[ERROR] client: can not open client socket ");
        exit(1);
    }

}

void initMainConnection() {
    // Initialize TCP connection between client and AWS server using specified IP address and port number
    memset(&mainAddr, 0, sizeof(mainAddr)); //  make sure the struct is empty
    mainAddr.sin_family = AF_INET; // Use IPv4 address family
    mainAddr.sin_addr.s_addr = inet_addr(LOCAL_HOST); // Source address
    mainAddr.sin_port = htons(MAIN_TCP_PORT); // AWS server port number
}

void requestMainConnection() {
//    connect(sockfdClient, (struct sockaddr *) &mainAddr, sizeof(mainAddr));
    if (connect(sockfdClient, (struct sockaddr *) &mainAddr, sizeof(mainAddr)) == ERROR_FLAG) {
        perror("[ERROR] client: fail to connect with AWS server");
        close(sockfdClient);
        exit(1); // If connection failed, we cannot continue
    }
    // If connection succeed, display boot up message
    cout << "Client is up and running"<<endl;
}