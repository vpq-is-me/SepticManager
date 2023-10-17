#ifndef UP2WEB_COMM_H_INCLUDED
#define UP2WEB_COMM_H_INCLUDED

#include <map>
#include <string>
#include <algorithm>
#include <mutex>
#include "sqlite/sqlite3.h"
#include "time.h"
#include "jansson.h"


#define MAX_REQUESTED_ROW_AMOUNT 1000
#define DEFAULT_REQUESTED_ROW_AMOUNT 100
void Up2webCommPrc(void);

#define MAX_WEB_BUFFER_LENGTH 8192
#define MAXLOGROWNUMS 1000000l //maximum size of database, magic number
#define MAXLOG_TRUNCATE_THRESHOLD 100

#define UP2WEB_SOCKET "/tmp/septic_socket.socket"

#define SEPTIC_ALRM_FIRST_TANK_OVF  (0X01<<0)
#define SEPTIC_ALRM_DRAIN_TANK_OVF  (0X01<<1)

#define MAX_TIMEOUT_FROM_LAST_MSG_sec (5*60)
typedef struct{
    std::mutex web_mutex;
    int active_alarms;
    int pending_alarms;
    time_t last_message_time;//timestamp (in epoch, in second) when last received any message from septic
}tShared_mem_st;


class tUp2Web_cl{
public:
    ~tUp2Web_cl();
    void InitMemory(void);
    void UpdateActiveAlarms(uint32_t val);
    void UpdatePendingAlarms(uint32_t val);

    int ServeSnapRequest(char*buf,char const* val_name);
    void DB_Open(void);
    int DB_AddRow(void);
    int DB_ServeTableRequest(json_t*root,char**answ);
private:
    tShared_mem_st* smem;
    sqlite3* log_db=nullptr;
    static const std::map <std::string,std::string> DB_columns_arr;//store array of columns name of database
    bool IsInColumns(const std::string str){
        return DB_columns_arr.find(str)!=DB_columns_arr.end();
    }
};
enum tag_en{TAG_REQSNAP        =1,
            TAG_CMDRESETALARM  =2,
            TAG_DB_REQUEST     =3
            };
extern tUp2Web_cl up2web;






#endif // UP2WEB_COMM_H_INCLUDED
