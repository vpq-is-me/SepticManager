#include <unistd.h>
#include <sys/socket.h>
#include <iostream>
#include <sys/un.h>
#include <sys/mman.h>

#include "up2web_comm.h"
#include "main.h"
#include "sqlite/sqlite3.h"
#include <string>
#include <math.h>  //for fpclassify(), check float for NaN
#include "time.h"
#include "ble_messaging.h"
#include <fstream>


using namespace std;

int ReceiveNewRow5DB_CB(void*dist,int col_n,char** fields,char**col_names);
int ReceiveAnswer5DB_CB(void*dist,int col_n,char** fields,char**col_names);
ofstream err_db_log_file;


#define BACKLOG 1 // how many pending connections queue will hold
///**************************************************************************************************

static int up2web_serv_socket, client_fd;

void ChildStopHandler(int signum) {
    cerr<<"child stopping"<<endl;
    close(client_fd);
    close(up2web_serv_socket);
    unlink(UP2WEB_SOCKET);
    _exit(0);
}
/********************************************//**
 * \brief
 *
 * \param void
 * \return void
 *
 ***********************************************/
void SigStopHandlerPreset(void) {
    struct sigaction sig;
    sig.sa_handler=ChildStopHandler;
    sigemptyset(&sig.sa_mask);
    sig.sa_flags=0;
    sigaction(SIGINT,&sig,NULL);//user press CTRL+C
}


///*******************************************************************************************************************
void Up2webCommPrc(void) {//chiled process
    SigStopHandlerPreset();
    unlink(UP2WEB_SOCKET);// unbind if previously not properly terminated
    /**< create socket */
    if((up2web_serv_socket=socket(AF_LOCAL,SOCK_STREAM,0))<0) { //UNIX type of socket
        cerr<<"socket not achieved"<<endl;
        _exit(1);
    }
    /**< prepare address structure to bind to socket */
    struct sockaddr_un sock_addr;
    memset(&sock_addr,0,sizeof(struct sockaddr_un));
    sock_addr.sun_family=AF_LOCAL;
    strncpy(sock_addr.sun_path, UP2WEB_SOCKET, sizeof(sock_addr.sun_path)-1);
    sock_addr.sun_path[sizeof(sock_addr.sun_path)-1]='\0';
    /**< bind socket to address and create listening queue */
    size_t sock_addr_length=offsetof(struct sockaddr_un, sun_path)+strlen(sock_addr.sun_path);
    if(bind(up2web_serv_socket, (struct sockaddr*)&sock_addr, sock_addr_length)<0) {
        cerr<<"bind error"<<endl;
        _exit(1);
    }
    if(listen(up2web_serv_socket,BACKLOG)<0) {
        cerr<<"listening error"<<endl;
        _exit(1);
    }

    //now socket is entirely prepared and can start waiting for client connection.
    //In oppose to general approaches with:
    //           (1) forking new process for each new connection or
    //           (2) using 'select()' function for several connection
    //we assume that ONLY one upper (web) client will connect to this program
    //that is why we not actually provide multiprocessing in this socket execution
    int len;
    char buffer[MAX_WEB_BUFFER_LENGTH];
    json_t *root;
    json_error_t jerror;
    while(1) {
        client_fd=accept(up2web_serv_socket,NULL,NULL);
        if(client_fd<0) {
            cerr<<"error in accepting new web client"<<endl;
            continue;
        }
        while(1) {
            len=read(client_fd,buffer,MAX_WEB_BUFFER_LENGTH);
            if(len<0) {
                cerr<<"Error reading from web socket"<<endl;
                //???have to close socket??
            } else if(!len) { //client disconnected
                cerr<<"WEB bro disconnected"<<endl;
                close(client_fd);
                break;//go to outer while(1) waiting new connection
            } else { //new message arived
                cerr<<endl<<"New message from WEB bro with len:"<<len<<endl;
                buffer[len]='\0';
                cerr<<buffer<<endl;
                root=json_loadb(buffer,len,0,&jerror);
                if(!root) {
                    cerr<<"JOSN error in line: "<<jerror.line<<"->"<<jerror.text<<endl;
                    json_object_clear(root);
                    continue;
                }
                if(!json_is_object(root)) {
                    cerr<<"JOSN: received not Object!"<<endl;
                    json_object_clear(root);
                    continue;
                }
                int tag,len;
                tag=json_integer_value(json_object_get(root,"tag"));
                switch(tag) {
                case TAG_REQSNAP:
                    char const* val_name;
                    if((val_name=json_string_value(json_object_get(root,"val")))!=NULL){
                        DEBUG(std::cout<<"request val name="<<val_name<<std::endl;)
                        len=up2web.ServeSnapRequest(buffer,val_name);
                        std::cout<<"send current data snap with length:"<<len<<std::endl;
                        write(client_fd,buffer,len);
                    }
                    break;
                case TAG_CMDRESETALARM: {
                    break;
                }
                case TAG_DB_REQUEST:{
                    break;}
                default:
                    std::cout<<"Received unknown tag No:"<<tag<<std::endl;
                    break;
                }
            }
        }//END while(1){... reading from connected upper client
    }//END while(1){...  trying wait connection new client
    _exit(0);
}
///*******************************************************************************************************************
/**< communication child <-> parent processes block */
void tUp2Web_cl::InitMemory(void) {
    smem=(tShared_mem_st*)mmap(NULL,sizeof(tShared_mem_st),PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1,0);
    smem->active_alarms=0;
    smem->pending_alarms=0;
    smem->last_message_time=time(NULL);
}
tUp2Web_cl::~tUp2Web_cl(void) {
    munmap(smem,sizeof(tShared_mem_st));
    sqlite3_close(log_db);
    err_db_log_file.close();
}

void tUp2Web_cl::UpdateAlarms(uint32_t val,mes_type_en mes_type) {
    uint32_t prev;
    time_t tout;
    {
        std::lock_guard<std::mutex> lg(smem->web_mutex);
        tout=(time_t)difftime(time(NULL),smem->last_message_time);
        if(tout<MAX_TIMEOUT_FROM_LAST_MSG_sec)tout=0;
        int*p=mes_type==ACTIVEen ? &smem->active_alarms : &smem->pending_alarms;
        prev=*p;
        *p=val;
        smem->last_message_time=time(NULL);
    }
    if(prev!=val || tout>=MAX_TIMEOUT_FROM_LAST_MSG_sec)DB_AddRow(tout);
}

///*******************************************************************************************************************
unsigned int constexpr str_hash(char const* str,int i=0) {
    return !str[i] ? 5381 : (str_hash(str,i+1)*33)^str[i];
}
unsigned int str_hash_vol(char const*str,int max_len=256) {
    unsigned int res=5381;
    int i;
    for(i=0; i<max_len; i++)
        if(!str[i])break;
    while(i) {
        res=(res*33)^str[--i];
    }
    return res;
}

int tUp2Web_cl::ServeSnapRequest(char*buf,char const* val_name) {
    int len;
    std::lock_guard<std::mutex> lg(smem->web_mutex);
    json_t *root;
//    json_error_t jerror;
    root=json_object();
    json_object_set_new(root,"tag",json_integer(TAG_REQSNAP));
    switch(str_hash_vol(val_name)){
        case str_hash("act_alarm"):
            json_object_set_new(root,"act_alarm",json_integer(smem->active_alarms));
        break;
        case str_hash("pend_alarm"):
            json_object_set_new(root,"pend_alarm",json_integer(smem->active_alarms));
        break;
        case str_hash("msg_timer"):{
            time_t l_m_tout=(time_t)difftime(time(NULL),smem->last_message_time);
            json_object_set_new(root,"msg_timer",json_integer(l_m_tout));
        break;}
        default:
            json_object_set_new(root,"error",json_string("error"));
        break;
    }
    len=json_dumpb(root,buf,MAX_WEB_BUFFER_LENGTH,JSON_REAL_PRECISION(3));
    json_decref(root);
    return len;
}

const std::vector<std::pair<std::string,std::string>> tUp2Web_cl::DB_columns_arr={
    {"id","INTEGER PRIMARY KEY"},
    {"date","INTEGER NOT NULL"},
    {"act_alarm","INTEGER"},
    {"pend_alarm","INTEGER"},
    {"msg_max_tout","INTEGER"}
    };

void tUp2Web_cl::DB_Open(void) {
    int res;
    char *zErrMsg;
    err_db_log_file.open("septic_error_db_log");
    err_db_log_file<<"header"<<endl;
    res = sqlite3_open("septic_log.db", &log_db);
    if(res!=SQLITE_OK)
        err_db_log_file<<"error opening DB with err:"<<res<<endl;
    res = sqlite3_exec(log_db, "SELECT * FROM alarm_table LIMIT 1", 0, 0, &zErrMsg);/* Create SQL statement */
    if( res != SQLITE_OK ) {
        cerr<<"error to SELECT at opening with error:"<<zErrMsg<<endl;
        err_db_log_file<<"error to SELECT at opening with error:"<<zErrMsg<<endl;
        if(strncmp(zErrMsg,"no such table:",14)==0) {
            sqlite3_free(zErrMsg);
            string sql="CREATE TABLE alarm_table(";
            char colon=' ';
            for(const auto pr:DB_columns_arr){
                sql+=colon;
                colon=',';
                sql+=pr.first + " " + pr.second;
            }
            sql+=");";
            cerr<<endl<<sql<<endl;
            res = sqlite3_exec(log_db, sql.c_str(), 0, 0, &zErrMsg);/* Create SQL statement */
            cerr<<"Create DB table with result:"<<zErrMsg<<endl;
        }
        sqlite3_free(zErrMsg);
    }
}
///*******************************************************************************************************************
int tUp2Web_cl::DB_AddRow(time_t tout) {
    int res;
    char *zErrMsg;
    string sql;
    {
        std::lock_guard<std::mutex> lg(smem->web_mutex);
        sql="INSERT INTO alarm_table (";
        char colon=' ';
        for(unsigned int i=1;i<DB_columns_arr.size();i++){//start iterating from second cell, because 'id' inserted automatically
            sql+=colon;
            colon=',';
            sql+=DB_columns_arr[i].first;
        }
        sql+=") VALUES (unixepoch('now','localtime'),";
        sql+=std::to_string(smem->active_alarms) +"," + std::to_string(smem->pending_alarms) + "," + std::to_string(tout)+");";
    }
    /* Execute SQL statement */
    DEBUG(cout<<endl<<"sql req: "<<sql<<endl;)
    res = sqlite3_exec(log_db, sql.c_str(), 0, 0, &zErrMsg);
    if( res != SQLITE_OK ) {
        cout<<endl<<zErrMsg<<endl;
        err_db_log_file<<"error to INSERT new row to DB:"<<zErrMsg<<endl;
        sqlite3_free(zErrMsg);
        cerr<<"error to INSERT new row to DB"<<endl;
        return 1;
    }
    /**< Check if number of row in database exceed predefined limit */
    int64_t row_count;
    res = sqlite3_exec(log_db, "SELECT count(*) FROM alarm_table", &ReceiveAnswer5DB_CB, (void*)&row_count, &zErrMsg);
    if( res != SQLITE_OK ) {
        sqlite3_free(zErrMsg);
        cerr<<"error to find row count in DB"<<endl;
        return 2;
    }
    if (row_count<(MAXLOGROWNUMS+MAXLOG_TRUNCATE_THRESHOLD))return 0;
    /**< Delete band of most older rows */
    int64_t min_id;
    res = sqlite3_exec(log_db, "SELECT min(id) FROM alarm_table", &ReceiveAnswer5DB_CB, (void*)&min_id, &zErrMsg);
    if( res != SQLITE_OK ) {
        sqlite3_free(zErrMsg);
        cerr<<"error to find min ID in DB"<<endl;
        return 3;
    }
    sql="DELETE FROM alarm_table WHERE id<"+std::to_string(min_id+MAXLOG_TRUNCATE_THRESHOLD);
    res = sqlite3_exec(log_db, sql.c_str(), 0, 0, &zErrMsg);
    if( res != SQLITE_OK ) {
        sqlite3_free(zErrMsg);
        cerr<<"error to delete old rows from DB"<<endl;
        return 4;
    }
    return 0;
}
///*******************************************************************************************************************
#define CheckRequestOk(r,str,errmsg) if(r!=SQLITE_OK){\
    err_db_log_file<<str<<errmsg<<endl; \
    sqlite3_free(errmsg); \
    cerr<<str<<endl;\
    return -2; \
}

/**<
NOTE: for request from database we have to remove all rows with same WF counter. Which occurs due to appering any alarm.
I.e. when alarm arise new row inserted to DB, but for pump performance evaluation this duplicate row are waste
From group of rows with same WF couter reading we have to select earliest row, i.e. earliest date/time or lowest id.
So despite request fields we first of all select rows with mandatory columns 'MIN(id)' and 'wf_counter and from result select only required columns
 */
int tUp2Web_cl::DB_ServeTableRequest(json_t* root,char**answ) {
    return 0;
}
///*******************************************************************************************************************
tUp2Web_cl up2web;

///*******************************************************************************************************************
int ReceiveNewRow5DB_CB(void*json_ptr,int col_n,char** fields,char**col_names) {
    //if(col_n!=json_array_size(array))return 1;
    json_t* col_arr_obj;
    for(auto i=0; i<col_n; i++ ) {
        col_arr_obj=json_object_get(*(json_t**)json_ptr,col_names[i]);
        json_array_append_new(col_arr_obj,json_string(fields[i]));
    }
    return 0;
}
///*******************************************************************************************************************
int ReceiveAnswer5DB_CB(void*dist,int col_n,char** fields,char**col_names) {
    if(fields[0]){
        *(int64_t*)dist=atol(fields[0]);
        return 0;
    }else return -1;
}
///*******************************************************************************************************************
