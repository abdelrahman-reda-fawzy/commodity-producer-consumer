#include <iostream>
#include <cstdlib>
#include <ctime>
#include <unistd.h>
#include <cstring>
#include <random>
#include <cstdio>
#include <string>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <chrono>
#include <iomanip>

using namespace std;

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

long long getTimeNs() {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

string formatTimeNs(long long ns) {
    time_t sec = ns / 1000000000LL;
    int ms = (int)((ns / 1000000LL) % 1000);
    struct tm tm_info;
    localtime_r(&sec, &tm_info);
    char buf[64];
    strftime(buf, sizeof(buf), "%m/%d/%Y %H:%M:%S", &tm_info);
    char out[80];
    snprintf(out, sizeof(out), "%s.%03d", buf, ms);
    return std::string(out);
}

int main(int argc, char* argv[]) {
    if(argc != 6) {
        cerr << "Usage: ./producer <COMMODITY> <MU> <SIGMA> <SLEEP_MS> <BUFFER_SIZE>\n";
        return 1;
    }

        char commodity[11];
        strncpy(commodity, argv[1], 10);
        commodity[10] = '\0';

    double mu = atof(argv[2]);
    double sigma = atof(argv[3]);
    int sleepMs = atoi(argv[4]);
    int bufferSize = atoi(argv[5]);
        if(bufferSize > 1000) bufferSize = 1000;

    key_t shmKey = ftok("/tmp", 65);
    key_t semKey = ftok("/tmp", 66);

    int shmid = shmget(shmKey, sizeof(sharedBuffer), 0666 | IPC_CREAT);
    sharedBuffer* shmPtr = (sharedBuffer*) shmat(shmid, nullptr, 0);

    if(shmPtr->size == 0) {
        shmPtr->head = shmPtr->tail = 0;
        shmPtr->size = bufferSize;
    } else if(shmPtr->size != bufferSize) {
        bufferSize = shmPtr->size;
    }

    int semid = semget(semKey, 3, 0666 | IPC_CREAT);
    semUn arg;
    if(semctl(semid, 0, GETVAL, arg) == 0) {
        arg.val = 1; semctl(semid, 0, SETVAL, arg);
        arg.val = bufferSize; semctl(semid, 1, SETVAL, arg);
        arg.val = 0; semctl(semid, 2, SETVAL, arg);
    }

    random_device rd;
    mt19937 gen(rd());
    normal_distribution<> dist(mu, sigma);

    while(true) {
    double price = dist(gen);
    long long ts = getTimeNs();
    std::string tstr = formatTimeNs(ts);
    cerr << "[" << tstr << "] " << commodity << ": generating new value " << fixed << setprecision(2) << price << "\n";

    semWait(semid, 1);
    semWait(semid, 0);
    shmPtr->buffer[shmPtr->tail].price = price;
    strncpy(shmPtr->buffer[shmPtr->tail].commodity, commodity, 10);
    shmPtr->buffer[shmPtr->tail].commodity[10] = '\0';
    shmPtr->buffer[shmPtr->tail].timestamp = ts;
    shmPtr->tail = (shmPtr->tail + 1) % shmPtr->size;

    cerr << "[" << tstr << "] " << commodity << ": placed " << price << " in buffer\n";

    semSignal(semid, 0);
    semSignal(semid, 2);

    cerr << "[" << tstr << "] " << commodity << ": sleeping " << sleepMs << " ms\n";
    usleep(sleepMs * 1000);
    }

    return 0;
}