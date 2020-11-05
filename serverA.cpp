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
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <tuple>

using namespace std;

#define SERVER_A_UDP_PORT 30500
#define LOCAL_HOST "127.0.0.1"
#define MAX_DATA_SIZE 1024
#define ERROR_FLAG -1
#define FILE_NAME "data1.txt"

int sockfdUDP; // UDP socket
struct sockaddr_in serverAddrUDP, clientAddrUDP;  // serverA and mainserver address for UDP connection
char recvBuf[MAX_DATA_SIZE]; // query message from mainserver
unordered_map<string, vector<vector<int>>> fileMap;  // the map that stores the information about social network
                                                    // (key: country, value: adjacency matrix)
unordered_map<string, unordered_map<string, int>> indexMap; // the map that stores the index relation
                                                            // (key: country, value: index map[userID -> reindex])
unordered_map<string, vector<int>> indexToIDMap; // the map that stores the reindex relation
                                                // (key: country, value: reindex[reindex ->userID])
unordered_map<string, unordered_set<string>> eachCountryMap; // used for each iteration to store the information for each country

/**
 * this method converts the social network into graphs(adjacency matrix)
 */
void processFiles();

/**
 * create UDP socket
 */
void createUDPSocket();

/**
 * boot up operation for server A
 */
void bootUpServerA();

/**
 *
 * @param array the array to count the one number of it(get one node's degree)
 * @return the number of one in the array
 */
int getOneNumber(vector<int> array);

/**
 *
 * @param array1 one array to get the common neighbours
 * @param array2 the other array to get the common neighbours
 * @return the number of common neighbours of these two array
 */
int getComNeighbours(vector<int> array1, vector<int> array2);

/**
 *  send recommendation result back to servermain
 * @param result the result will send back to servermain
 * @param userID the userID that the client inputs
 * @param countryName the country name that the client inputs
 */
void sendResult(string result, string userID, string countryName);

/**
 * This method helps to get adjacency matrix
 * @param map key is every country user ID, value is the userID line
 * @return reIndexMap: ID -> index
 */
tuple<unordered_map<string, int>, vector<vector<int>>, vector<int>> getAdjacencyMatrix(unordered_map<string, unordered_set<string>> map);

/**
 *
 * @param countryName the country name that the client inputs
 * @param userID the userID that the client inputs
 * @return the recommendation userID(it should be "None" or userID)
 */
string getRecommendation(string countryName, string userID);


int main() {

    processFiles();
    // create UDP socket for server A
    createUDPSocket();
    while(true)
    {
        // receive data from main server
        memset(recvBuf,'\0',sizeof(recvBuf));
        socklen_t serverAUDPLen = sizeof(clientAddrUDP);
        if (::recvfrom(sockfdUDP, recvBuf, MAX_DATA_SIZE, 0, (struct sockaddr *) &clientAddrUDP, &serverAUDPLen) == ERROR_FLAG) {
            perror("serverA receive failed");
            exit(1);
        }
        string recvMsg = recvBuf;
        // boot up message
        if (recvMsg == "Bootup") {
            bootUpServerA();
        } else {  // query message
            string userID;
            string countryName;
            // strtok function must use char[], so here the input should be recvBuf instead of recvMsg
            char *splitStr = strtok(recvBuf,"|");
            userID = splitStr;
            splitStr = strtok(NULL,"|");
            countryName = splitStr;

            cout << "The server A has received request for finding possible friends of User " << userID << " in " << countryName << endl;
            cout << endl;
            // get recommendation
            string result = getRecommendation(countryName, userID);
            // send query result back to mainserver
            sendResult(result, userID, countryName);
        }
    }
    close(sockfdUDP);
    return 0;

}

/**
 * this method converts the social network into graphs(adjacency matrix)
 *
 */
void processFiles() {
    ifstream inFile(FILE_NAME,ios::in);
    string lineStr;
    string countryName;
    fileMap.clear();
    indexMap.clear();
    indexToIDMap.clear();
    int count = 0;

    // the whole file line by line
    while (getline(inFile,lineStr)) {
        unordered_set<string> lineSet;
        stringstream ss(lineStr);
        string str;
        vector<string> lineArray;

        // each line string by string(split by space)
        while(getline(ss,str,' ')) {
            // deal with spaces
            if (str == "") {
                continue;
            }
            lineArray.push_back(str);
            // use set to for deduplication
            lineSet.insert(str);
        }

        // if this line is country name
        if (lineArray.size() == 1 && !isdigit(lineArray[0][0])) {
            // the first country
            if (count == 0) {
                countryName = lineArray[0];
                count++;

            } else {
                // deal with the previous country and initialize the related parameter
                indexMap[countryName] = get<0>(getAdjacencyMatrix(eachCountryMap));
                fileMap[countryName] = get<1>(getAdjacencyMatrix(eachCountryMap));
                indexToIDMap[countryName] = get<2>(getAdjacencyMatrix(eachCountryMap));
                countryName = lineArray[0];
                eachCountryMap.clear();
                continue;
            }
            // if this line is not country name
        } else {
            lineSet.erase(lineArray[0]);
            eachCountryMap[lineArray[0]] = lineSet;
        }
    }
    indexMap[countryName] = get<0>(getAdjacencyMatrix(eachCountryMap));
    fileMap[countryName] = get<1>(getAdjacencyMatrix(eachCountryMap));
    indexToIDMap[countryName] = get<2>(getAdjacencyMatrix(eachCountryMap));

}

/**
 * This method helps to get adjacency matrix
 * @param map key is every country user ID, value is the userID line
 * @return reIndexMap: ID -> index
 */
tuple<unordered_map<string, int>, vector<vector<int>>, vector<int>> getAdjacencyMatrix(unordered_map<string, unordered_set<string>> map) {
    unordered_map<string, int> reIndexMap;
    vector<vector<int>> matrix;
    vector<int> userIDVector;
    int count = 0;
    int size = map.size();
    int userIDArray [size];
    // get reindex map
    for (auto it = map.begin(); it != map.end(); ++it) {
        // cast string to int so that we can use sort()
        userIDArray[count] = stoi(it->first);
        count++;
    }
    sort(userIDArray, userIDArray + size);
    for (int i = 0; i < size; i++) {
        reIndexMap[to_string(userIDArray[i])] = i;
    }

    // get adjacency matrix
    for (int i = 0; i < size; i++) {
        vector<int> temp;
        unordered_set<string> rowSet = map[to_string(userIDArray[i])];
        for (int j = 0; j < size; j++) {
            if (j == i) {
                temp.push_back(0);
            } else if (rowSet.count(to_string(userIDArray[j])) > 0) {
                temp.push_back(1);
            } else {
                temp.push_back(0);
            }
        }
        matrix.push_back(temp);
    }

    // get ID to Index vector(value is ID)
    for (int i = 0; i < size; i++) {
        userIDVector.push_back(userIDArray[i]);
    }

    tuple<unordered_map<string, int>, vector<vector<int>>, vector<int>> result(reIndexMap, matrix, userIDVector);
    return result;

}


/**
 * create an UDP socket
 */
void createUDPSocket() {
    sockfdUDP = socket(AF_INET, SOCK_DGRAM, 0);
    //test if create a socket successfully
    if(sockfdUDP == ERROR_FLAG){
        perror("ServerA UDP socket");
        exit(1);
    }

    // from beej's tutorial
    // initialize IP address, port number
    memset(&serverAddrUDP, 0, sizeof(serverAddrUDP));
    serverAddrUDP.sin_family = AF_INET;
    serverAddrUDP.sin_port   = htons(SERVER_A_UDP_PORT);
    serverAddrUDP.sin_addr.s_addr = inet_addr(LOCAL_HOST);

    // bind socket
    if(::bind(sockfdUDP, (struct sockaddr*) &serverAddrUDP, sizeof(serverAddrUDP)) == ERROR_FLAG){
        perror("ServerA UDP bind");
        exit(1);
    }
    cout << "The Server A is up and running using UDP on port "<< SERVER_A_UDP_PORT << "." << endl;
    cout << endl;

}


/**
 *
 * @param countryName the country name that the client inputs
 * @param userID the userID that the client inputs
 * @return the recommendation userID(it should be "None" or userID)
 */
string getRecommendation(string countryName, string userID) {
    vector<vector<int>> matrix = fileMap[countryName];
    unordered_map<string, int> reindexMap = indexMap[countryName];
    vector<int> indexToID = indexToIDMap[countryName];

    if (reindexMap.count(userID) == 0) {
        return "userIDNF";
    }
    // case1.a: only user in the graph
    if (matrix.size() == 1) {
        return "None";
    }
    int IDReindex = reindexMap[userID];
    vector<int> userRow = matrix[IDReindex];
    // case1: connected to all the other users
    if (userRow.size() == getOneNumber(userRow) + 1) {
        return "None";
    }

    // get the index set for those users unconnected to u
    unordered_set<int> nonNeighbourIndex;
    for (int i = 0; i < userRow.size(); i++) {
        if (i == IDReindex || userRow[i] == 1) { continue; }
        nonNeighbourIndex.insert(i);
//        cout << "nodes are not neighbour: " << indexToID[i] << endl; // used for test
    }

    // get common neighbours row index
    vector<int> comNeighbourRowIndex;
    for (int i = 0; i < matrix.size(); i++) {
        // nonNeighbourIndex.count(i) == 0 means it is neighbour for u
        if (i == IDReindex || nonNeighbourIndex.count(i) == 0) { continue; }
        if (getComNeighbours(userRow, matrix[i]) != 0) {
            comNeighbourRowIndex.push_back(i);
//            cout << "nodes have common neighbour: " << indexToID[i] << endl; // used for test
        }

    }
    // case2.a: no n shares any common neighbor with u
    if (comNeighbourRowIndex.size() == 0) {
        // get the highest degree node
        int highestDegree = 0;
        int highestDegreeID = 0;
        int mark = 0;
        for (int i = 0; i < matrix.size(); i++) {
            if (i == IDReindex || nonNeighbourIndex.count(i) == 0) {
//                cout << "this is " << indexToID[i] << endl; // used for test
                continue;
            }
            int degree = getOneNumber(matrix[i]);
            // for the condition that all the non neighbour node's degree is zero
            if (mark == 0) {
                highestDegree = degree;
                highestDegreeID = i;
                mark++;
            }
            if (highestDegree < degree) {
                highestDegree = degree;
                highestDegreeID = i;
            }
        }

//        cout << "case 2.a: no n shares any common neighbor with u" << endl; // used for test
        return to_string(indexToID[highestDegreeID]);
    }

    // case2.b: some n shares some common neighbors with u
    int mostComNeighbour = 0;
    int mostComNeighbourID = 0;
    for (int i = 0; i < comNeighbourRowIndex.size(); i++) {
        int comNeighbourIndex = comNeighbourRowIndex[i];
        int comNeighbourNum = getComNeighbours(userRow, matrix[comNeighbourIndex]);
        if (mostComNeighbour < comNeighbourNum) {
            mostComNeighbour = comNeighbourNum;
            mostComNeighbourID = comNeighbourIndex;
        }
    }
//    cout << "case2.b: some n shares some common neighbors with u" << endl; // used for test
    return to_string(indexToID[mostComNeighbourID]);
}

/**
 *
 * @param array the array to count the one number of it(get one node's degree)
 * @return the number of one in the array
 */
int getOneNumber(vector<int> array) {
    int count = 0;
    for (int i = 0; i < array.size(); i++) {
        if (array[i] == 1) {
            count++;
        }
    }
    return count;
}

/**
 *
 * @param array1 one array to get the common neighbours
 * @param array2 the other array to get the common neighbours
 * @return the number of common neighbours of these two array
 */
int getComNeighbours(vector<int> array1, vector<int> array2) {
    int count = 0;
    for (int i = 0; i < array1.size(); i++) {
        if (array1[i] == 1 &&  array2[i] == 1) {
            count++;
        }
    }
    return count;
}

/**
 * boot up operation for server A(send the country list to mainserver)
 */
void bootUpServerA() {
    string countryList = "";
    for (auto it = fileMap.begin(); it != fileMap.end(); ++it) {
        countryList += it->first + "|";
    }
    countryList = countryList.substr(0, countryList.length() - 1);
    if(::sendto(sockfdUDP,countryList.c_str(), MAX_DATA_SIZE, 0, (struct sockaddr *) &clientAddrUDP, sizeof(clientAddrUDP)) == ERROR_FLAG) {
        perror("serverA response failed");
        exit(1);
    }
    cout << "The server A has sent a country list to Main Server" << endl;
    cout << endl;

}

/**
 *  send recommendation result back to servermain
 * @param result the result will send back to servermain
 * @param userID the userID that the client inputs
 * @param countryName the country name that the client inputs
 */
void sendResult(string result, string userID, string countryName) {
    if (result == "userIDNF") {
        cout << "User " << userID << " does not show up in "<< countryName << endl;
        cout << endl;
        // send data to main server
        string message = result;
        if(::sendto(sockfdUDP,message.c_str(), MAX_DATA_SIZE, 0, (struct sockaddr *) &clientAddrUDP, sizeof(clientAddrUDP)) == ERROR_FLAG) {
            perror("serverA response failed");
            exit(1);
        }
        cout << "The server A has sent \"User "<< userID << " not found\" to Main Server" << endl;
        cout << endl;

    } else {
        // send data to main server
        cout << "The server A is searching possible friends for User " << userID << "..." <<endl;
        cout << "Here are the results: User "<< result << endl;
        cout << endl;

        string message = result;
        if(::sendto(sockfdUDP,message.c_str(), MAX_DATA_SIZE, 0, (struct sockaddr *) &clientAddrUDP, sizeof(clientAddrUDP)) == ERROR_FLAG) {
            perror("serverA response failed");
            exit(1);
        }
        cout << "The server A has sent the result(s) to Main Server" << endl;
        cout << endl;

    }
}