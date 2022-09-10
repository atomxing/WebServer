
#ifndef SQLCONNPOOL_H
#define SQLCONNPOOL_H

#include <mysql/mysql.h>
#include <string>
#include <queue>
#include <mutex>
#include <semaphore.h>
#include <thread>
#include "../log/log.h"

class SqlConnPool {
public:
    static SqlConnPool *Instance();

    MYSQL *GetConn();
    void FreeConn(MYSQL * conn);
    int GetFreeConnCount();

    void Init(const char* host, int port,
              const char* user,const char* pwd, 
              const char* dbName, int connSize);
    void ClosePool();

private:
    SqlConnPool();
    ~SqlConnPool();

    int MAX_CONN_;  // ����������
    int useCount_;  // ��ǰ���û���
    int freeCount_; // ���е��û���

    std::queue<MYSQL *> connQue_;   // ���У�����MYSQL *����������MYSQL���ݿ�
    std::mutex mtx_;    // ������
    sem_t semId_;   // �ź���
};


#endif // SQLCONNPOOL_H