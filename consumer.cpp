#include <iostream>
#include <iomanip>
#include <cstdio>
#include <unistd.h>
#include <cstring>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <map>
#include <vector>
#include <string>

using namespace std;

vector<string> type = {"ALUMINIUM","COTTON","COPPER","GOLD","LEAD","MENTHAOIL","NATURALGAS","NICKEL","CRUDEOIL","SILVER","ZINC"};

struct item {
    char commodity[11];
    double price;
    long long timestamp;
};

struct sharedBuffer {
    int head, tail;
    int size;
    item buffer[1000];
};

union semUn {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

void semWait(int semid, int semnum) {
    struct sembuf sb = {(unsigned short)semnum, -1, 0};
    semop(semid, &sb, 1);
}

void semSignal(int semid, int semnum) {
    struct sembuf sb = {(unsigned short)semnum, 1, 0};
    semop(semid, &sb, 1);
}

void initShared(int &bufferSize, sharedBuffer*& shmPtr, int &semid) {
    key_t shmKey = ftok("/tmp", 65);
    key_t semKey = ftok("/tmp", 66);
    int shmid = shmget(shmKey, sizeof(sharedBuffer), 0666 | IPC_CREAT);
    shmPtr = (sharedBuffer*) shmat(shmid, nullptr, 0);

    if(shmPtr->size == 0) {
        shmPtr->size = bufferSize;
        shmPtr->head = shmPtr->tail = 0;
    } else if(shmPtr->size != bufferSize) {
        bufferSize = shmPtr->size;
    }

    semid = semget(semKey, 3, 0666 | IPC_CREAT);
}

void printTable(map<string, vector<double>>& history, vector<double>& prevAvg) {
    printf("\033[1;1H\033[2J");
    int cWidth = 12;
    int pNumWidth = 7;
    int avgCellWidth = 8;

    const char* colNeut = "\033[36m";
    const char* colUp = "\033[32m";
    const char* colDown = "\033[31m";
    const char* colReset = "\033[0m";

    string top = "+" + string(cWidth + 2, '-') + "+" + string(pNumWidth + 2, '-') + "+" + string(avgCellWidth + 2, '-') + "+\n";
    string sep = top;

    printf("%s", top.c_str());
    printf("| %-*s | %*s | %*s |\n", cWidth, "Currency", pNumWidth, "Price", avgCellWidth, "AvgPrice");
    printf("%s", sep.c_str());

    for(size_t i = 0; i < type.size(); ++i) {
        string &commodity = type[i];
        auto it = history.find(commodity);
        vector<double> emptyVec;
        const vector<double> &vec = (it != history.end()) ? it->second : emptyVec;

        double current = vec.empty() ? 0.0 : vec.back();
        double avg = 0;
        for(double v: vec) avg += v;
        if(!vec.empty()) avg /= vec.size();

        char arrow = ' ';
        if(avg > prevAvg[i]) arrow = '^';
        else if(avg < prevAvg[i]) arrow = 'v';
        prevAvg[i] = avg;

        const char* valCol = (arrow == '^') ? colUp : (arrow == 'v') ? colDown : colNeut;

        char avgBuf[32];
        if(arrow == '^') snprintf(avgBuf, sizeof(avgBuf), "%7.2lf^", avg);
        else if(arrow == 'v') snprintf(avgBuf, sizeof(avgBuf), "%7.2lfv", avg);
        else snprintf(avgBuf, sizeof(avgBuf), "%7.2lf ", avg);

        printf("| %-*s | %s%7.2lf%s | %s%*s%s |\n",
               cWidth, commodity.c_str(),
               valCol, current, colReset,
               valCol, avgCellWidth, avgBuf, colReset);
    }

    printf("%s", top.c_str());
}

int main(int argc, char* argv[]) {
    if(argc != 2) {
        cerr << "Usage: ./consumer <BUFFER_SIZE>\n";
        return 1;
    }

    int bufferSize = atoi(argv[1]);
    if(bufferSize > 1000) bufferSize = 1000;

    sharedBuffer* shmPtr = nullptr;
    int semid = -1;
    initShared(bufferSize, shmPtr, semid);

    map<string, vector<double>> history;
    for(auto &c: type) history[c] = vector<double>();

    vector<double> prevAvg(type.size(), 0.0);

    while(true) {
    semWait(semid, 2);
    semWait(semid, 0);

    item item = shmPtr->buffer[shmPtr->head];
        shmPtr->head = (shmPtr->head + 1) % shmPtr->size;

    semSignal(semid, 0);
    semSignal(semid, 1);

    string comm(item.commodity);
    history[comm].push_back(item.price);
        if(history[comm].size() > 5) history[comm].erase(history[comm].begin());

        printTable(history, prevAvg);

        usleep(100000);
    }

    return 0;
}
