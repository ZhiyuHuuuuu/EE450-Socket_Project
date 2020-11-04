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
#include <cstring>

using namespace std;

#define SERVER_B_UDP_PORT 31500
#define LOCAL_HOST "127.0.0.1"
#define MAX_DATA_SIZE 1024
#define ERROR_FLAG -1
#define FILE_NAME "data2.txt"



//const string FILE_NAME = "testcases/testcase3/data2.txt";
//const string FILE_NAME = "test2.txt";

int sockfdUDP;
struct sockaddr_in serverBddrUDP, clientBddrUDP;
char recvBuf[MAX_DATA_SIZE];

unordered_map<string, vector<vector<int>>> fileMap;
unordered_map<string, unordered_map<string, int>> indexMap;
unordered_map<string, vector<int>> indexToIDMap;
unordered_map<string, unordered_set<string>> eachCountryMap;

int getOneNumber(vector<int> array);
int getComNeighbours(vector<int> array1, vector<int> array2);


void createUDPSocket();
//void processFiles(unordered_map<string, vector<vector<int>>> fileMap,
//                  unordered_map<string, unordered_map<string, int>> indexMap,
//                  unordered_map<string, unordered_set<string>> eachCountryMap);
void processFiles();
void bootUpServerB();
void sendResult(string result, string userID, string countryName);
tuple<unordered_map<string, int>, vector<vector<int>>, vector<int>> getAdjacencyMatrix(unordered_map<string, unordered_set<string>> map);
string getRecommendation(string countryName, string userID);


int main() {


    processFiles();
    // create UDP socket for server B
    createUDPSocket();

    while(true)
    {
        // receive data from main server
        memset(recvBuf,'\0',sizeof(recvBuf));
        socklen_t serverBUDPLen = sizeof(clientBddrUDP);
        if (::recvfrom(sockfdUDP, recvBuf, MAX_DATA_SIZE, 0, (struct sockaddr *) &clientBddrUDP, &serverBUDPLen) == ERROR_FLAG) {
            perror("serverB receive failed");
            exit(1);
        }
//        printf("The Server B received input <%s>\n", recvBuf); // debug Test
        string recvMsg = recvBuf;
        // boot up message
        if (recvMsg == "Bootup") {
            bootUpServerB();
        } else {  // query message
            string userID;
            string countryName;
            // strtok function must use char[], so here the input should be recvBuf instead of recvMsg
            char *splitStr = strtok(recvBuf,"|");
            userID = splitStr;
            splitStr = strtok(NULL,"|");
            countryName = splitStr;

//            /* debug info */
//            cout << endl;
//            cout << "userID is: " << userID << endl;
//            cout << "countryName is: "<< countryName << endl;
//            cout << endl;
//            /*  debug info */

            cout << "The server B has received request for finding possible friends of User " << userID << " in " << countryName << endl;
            // get recommendation
            string result = getRecommendation(countryName, userID);
//            cout << "recommendation is: " << result << endl;
            // send query result back to mainserver
            sendResult(result, userID, countryName);
        }
    }
    close(sockfdUDP);
    return 0;

}

/**
 * this method converts the social network into graphs
 *
 * @param fileMap is the map that stores the information about social network,
 *        key is country name and value is the graph for every country
 * @param indexMap is the map that stores the index relation,
 *        key is country name and value is the index map for every country
 * @param eachCountryMap the network infomation for each country
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

//
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

//Create a UDP socket
void createUDPSocket()
{
    sockfdUDP = socket(AF_INET, SOCK_DGRAM, 0);
    //test if create a socket successfully
    if(sockfdUDP == ERROR_FLAG){
        perror("ServerB UDP socket");
        exit(1);
    }
    // make sure the struct is empty
    memset(&serverBddrUDP, 0, sizeof(serverBddrUDP));
    serverBddrUDP.sin_family = AF_INET;
    serverBddrUDP.sin_port   = htons(SERVER_B_UDP_PORT);
    serverBddrUDP.sin_addr.s_addr = inet_addr(LOCAL_HOST);

    // bind socket
    if(::bind(sockfdUDP, (struct sockaddr*) &serverBddrUDP, sizeof(serverBddrUDP)) == ERROR_FLAG){
        perror("ServerB UDP bind");
        exit(1);
    }
    cout << "The Server B is up and running using UDP on port "<< SERVER_B_UDP_PORT << "." << endl;
}


string getRecommendation(string countryName, string userID) {
    vector<vector<int>> matrix = fileMap[countryName];
    unordered_map<string, int> reindexMap = indexMap[countryName];
    vector<int> indexToID = indexToIDMap[countryName];

    if (reindexMap.count(userID) == 0) {
        return "userIDNF";
    }
    // case1.a: only user in the graph
    if (matrix.size() == 1) {
//        cout << "case1.a: only user in the graph" << endl; // used for test
        return "None";
    }
    int IDReindex = reindexMap[userID];
    vector<int> userRow = matrix[IDReindex];
    // case1: connected to all the other users
    if (userRow.size() == getOneNumber(userRow) + 1) {
//        cout << "case1: connected to all the other users" << endl; // used for test
        return "None";
    }

    // get the index set for those users unconnected to u
    unordered_set<int> nonNeighbourIndex;
    for (int i = 0; i < userRow.size(); i++) {
        if (i == IDReindex || userRow[i] == 1) { continue; }
        nonNeighbourIndex.insert(i);
//        cout << "nodes are not neighbour: " << indexToID[i] << endl;
    }

    // get common neighbours row index
    vector<int> comNeighbourRowIndex;
    for (int i = 0; i < matrix.size(); i++) {
        // nonNeighbourIndex.count(i) == 0 means it is neighbour for u
        if (i == IDReindex || nonNeighbourIndex.count(i) == 0) { continue; }
        if (getComNeighbours(userRow, matrix[i]) != 0) {
            comNeighbourRowIndex.push_back(i);
//            cout << "nodes have common neighbour: " << indexToID[i] << endl;
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


int getOneNumber(vector<int> array) {
    int count = 0;
    for (int i = 0; i < array.size(); i++) {
        if (array[i] == 1) {
            count++;
        }
    }
    return count;
}

int getComNeighbours(vector<int> array1, vector<int> array2) {
    int count = 0;
    for (int i = 0; i < array1.size(); i++) {
        if (array1[i] == 1 &&  array2[i] == 1) {
            count++;
        }
    }
    return count;
}

void bootUpServerB() {
    string countryList = "";
    for (auto it = fileMap.begin(); it != fileMap.end(); ++it) {
//        cout << "debug info: " << countryList << endl;
        countryList += it->first + "|";
    }
    countryList = countryList.substr(0, countryList.length() - 1);
//    cout << "country list is: "<< countryList << endl;
    if(::sendto(sockfdUDP,countryList.c_str(), MAX_DATA_SIZE, 0, (struct sockaddr *) &clientBddrUDP, sizeof(clientBddrUDP)) == ERROR_FLAG) {
        perror("serverB response failed");
        exit(1);
    }
    cout << "The server B has sent a country list to Main Server" << endl;
}

void sendResult(string result, string userID, string countryName) {
    if (result == "userIDNF") {
        cout << "User " << userID << " does not show up in "<< countryName << endl;
        // send data to main server
        string message = result;
        if(::sendto(sockfdUDP,message.c_str(), MAX_DATA_SIZE, 0, (struct sockaddr *) &clientBddrUDP, sizeof(clientBddrUDP)) == ERROR_FLAG) {
            perror("serverB response failed");
            exit(1);
        }
        cout << "The server B has sent User "<< userID << " not found to Main Server" << endl;

    } else {
        // send data to main server
        cout << "The server B is searching possible friends for User " << userID << "..." <<endl;
        cout << "Here are the results: User "<< result << endl;
        string message = result;
        if(::sendto(sockfdUDP,message.c_str(), MAX_DATA_SIZE, 0, (struct sockaddr *) &clientBddrUDP, sizeof(clientBddrUDP)) == ERROR_FLAG) {
            perror("serverB response failed");
            exit(1);
        }
        cout << "The server B has sent the result(s) to Main Server" << endl;
    }
}