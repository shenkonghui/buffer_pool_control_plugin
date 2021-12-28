#include "buffer_pool_control.h"
#include "pfs_variable.h"
#include <item_timefunc.h>
#include "auth/sql_authorization.h"
#include <my_sys.h>
#include <mysql/plugin.h>
#include "my_thread.h"
#include "mysql/psi/mysql_thread.h"
#include <sql_class.h>
#include <sql_db.h>
#include <sql_lex.h>
#include "set_var.h"
#include "item.h"
#include <cstring>
#include "mysql.h"
#include "memory.h"
#include <string>
#include<sql_base.h>
#include "sql_parse.h"
#include "probes_mysql.h"
#include <m_string.h>

#define HEART_STRING_BUFFER 100
    static MYSQL_PLUGIN plugin_info_ptr;
THD* thd = NULL;
/* 系统配置变量 */
// 是否开启
static bool enabled;
// 控制时间间隔, 单位秒
// static uint buffer_pool_control_interval;
// // 最小内存
// static char *buffer_pool_control_min;
// // 最大内存
// static char *buffer_pool_control_max;

static MYSQL_THDVAR_BOOL(enabled,
                         PLUGIN_VAR_RQCMDARG,
                         "buffer_pool_control_enabled",
                         NULL, /* check func. */
                         NULL, /* update func. */
                         1     /* default */
);

// static MYSQL_THDVAR_UINT(buffer_pool_control_interval,
//                          PLUGIN_VAR_RQCMDARG,
//                          "buffer_pool_control_interval",
//                          NULL, /* check func. */
//                          NULL, /* update func. */
//                          30 ,
//                          10,
//                          60 * 60 * 24,
//                          1
// );

static struct st_mysql_sys_var *buffer_pool_control_system_variables[] = {
    MYSQL_SYSVAR(enabled),
    NULL};

struct buffer_pool_control_context
{
    my_thread_handle control_thread;
    THD *thd;
};

PSI_memory_key key_memory_mysql_context;

static mysql_mutex_t g_record_buffer_mutex;

/**
 * @brief 执行命令前处理,
 *
 * @param thd
 * @param query
 * @param db
 * @return my_bool
 */
my_bool prepare_execute_command(THD *thd, const char *query, const char *db)
{
    thd->security_context()->set_master_access(SUPER_ACL);
    Parser_state parser_state;
    struct st_mysql_const_lex_string new_db;
    new_db.str = db;
    new_db.length = strlen(db);
    thd->reset_db(new_db);
    alloc_query(thd, query, strlen(query));

    if (parser_state.init(thd, thd->query().str, thd->query().length))
    {
        return FALSE;
    }
    mysql_reset_thd_for_next_command(thd);
    lex_start(thd);
    thd->m_parser_state = &parser_state;
    thd->m_parser_state = NULL;
    bool err = thd->get_stmt_da()->is_error();

    err = parse_sql(thd, &parser_state, NULL);

    return !err;
}

/**
 * @brief 执行命令后的处理
 *
 * @param bdq_backup_thd
 */
void after_execute_command(THD *bdq_backup_thd)
{
    bdq_backup_thd->mdl_context.release_statement_locks();
    bdq_backup_thd->mdl_context.release_transactional_locks();
    bdq_backup_thd->lex->unit->cleanup(true);
    close_thread_tables(bdq_backup_thd);
    bdq_backup_thd->end_statement();
    bdq_backup_thd->cleanup_after_query();
    bdq_backup_thd->reset_db(NULL_CSTR);
    bdq_backup_thd->reset_query();
    bdq_backup_thd->proc_info = 0;
    // free_root(bdq_backup_thd->mem_root,MYF(MY_KEEP_PREALLOC));
}
void init_thd(){
    /**
     * 注册线程到全局线程, 模拟客户端连接过程
     */
    my_thread_init();
    thd = new THD;
    THD *stack_thd = thd;
    mysql_mutex_lock(&g_record_buffer_mutex);
    thd->set_new_thread_id();
    mysql_mutex_unlock(&g_record_buffer_mutex);
    thd->thread_stack = (char *)&stack_thd;
    thd->store_globals();
    // thd->want_privilege = GRANT_TABLES;
    my_plugin_log_message(&plugin_info_ptr, MY_INFORMATION_LEVEL, "bpc_thd: %p", thd);
}

int exec_command(const char *query)
{
    int result;
    prepare_execute_command(thd, query, "mysql");
    my_plugin_log_message(&plugin_info_ptr, MY_INFORMATION_LEVEL, "sql type: %d", thd->lex->sql_command);
    result = mysql_execute_command(thd, true);
    after_execute_command(thd);
    return result;
}

char* show_var(const char *query)
{
    SHOW_VAR *show;
    show = (SHOW_VAR *)thd->alloc(sizeof(SHOW_VAR));
    show->type = SHOW_SYS;
    const char *str = query;
    // var = find_sys_var_ex(thd, str);
    show->name = query;
    show->value = (char *)find_sys_var_ex(thd, str, strlength(str), true, false);
    prepare_execute_command(thd, "", "mysql");

    System_variable system_var(thd, show, OPT_GLOBAL, false);
    after_execute_command(thd);

    my_plugin_log_message(&plugin_info_ptr, MY_INFORMATION_LEVEL, "res: %s", system_var.m_value_str);
    char *str_to_ret = (char*)malloc( system_var.m_value_length);
    memcpy(str_to_ret, system_var.m_value_str, system_var.m_value_length);
    return str_to_ret;
}

void *mysql_heartbeat(void *p)
{
    DBUG_ENTER("mysql_heartbeat");
    // struct mysql_heartbeat_context *con = (struct mysql_heartbeat_context *)p;
    char buffer[HEART_STRING_BUFFER];
    time_t result;
    struct tm tm_tmp;
    sleep(4);
    init_thd();


    while (1)
    {
        sleep(10);
        set_buffer_pool_size_new();
        result = time(NULL);
        localtime_r(&result, &tm_tmp);
        my_snprintf(buffer, sizeof(buffer),
                    "Heartbeat at %02d%02d%02d %2d:%02d:%02d\n",
                    tm_tmp.tm_year % 100,
                    tm_tmp.tm_mon + 1,
                    tm_tmp.tm_mday,
                    tm_tmp.tm_hour,
                    tm_tmp.tm_min,
                    tm_tmp.tm_sec);
        my_plugin_log_message(&plugin_info_ptr, MY_INFORMATION_LEVEL, "%s", buffer);
        // my_write(con->heartbeat_file, (uchar *)buffer, strlen(buffer), MYF(0));

    }

    return 0;
}

// 自动设置buffer pool
void set_buffer_pool_size_new()
{
    // 获取当前系统的内存情况
    Memory mem = getMemoryInfo();

    unsigned long long int currBufferPoolSize = 0;
    // 查询mysql中的innodb_buffer_pool_size指标
    char* result = show_var("innodb_buffer_pool_size");
    currBufferPoolSize = atoll(result);
    my_plugin_log_message(&plugin_info_ptr, MY_INFORMATION_LEVEL, "currBufferPoolSize = %llu", currBufferPoolSize);

    const char *query = "set global innodb_buffer_pool_size=%llu;";
    unsigned long long int setbufferPool = (static_cast<unsigned long long int>(mem.MemTotal * 0.7)) * 1024;

    if (setbufferPool - currBufferPoolSize > 1024 * 1024 * 128 || currBufferPoolSize - setbufferPool  > 1024 * 1024 * 128) {
        char buf[1024];
        sprintf(buf, query, setbufferPool);
        my_plugin_log_message(&plugin_info_ptr, MY_INFORMATION_LEVEL, " MemTotal = %llu, set bufferpool = %llu, currBufferPoolSize= %llu ", mem.MemTotal * 1024, setbufferPool, currBufferPoolSize);
        exec_command(buf);
   }
   else
   {
       my_plugin_log_message(&plugin_info_ptr, MY_INFORMATION_LEVEL, "Nothing, MemTotal = %llu, set bufferpool = %llu, currBufferPoolSize= %llu ", mem.MemTotal * 1024, setbufferPool, currBufferPoolSize);
   }
}

void set_buffer_pool_size(){
    // // 最大允许的bufffer_pool
    // uint64_t max_innodb_buffer_pool_size = 2048ULL * 1024 * 1024;
    // // 最小允许的bufffer_pool
    // uint64_t min_innodb_buffer_pool_size = 128ULL * 1024 * 1024;
    // // 内存少于256MB触发缩容防止OOM
    // uint64_t min_avaliable_mem = 256ULL * 1024 * 1024;

    MYSQL mysql = MYSQL();
    MYSQL *res = mysql_init(&mysql);
    MYSQL_RES *result;
    MYSQL_ROW row;
    unsigned long long int currBufferPoolSize = 0;
    static char *opt_unix_socket = 0;
    if (!(mysql_real_connect(res, "127.0.0.1", "root",
                             "123456", "mysql", 3307,
                             opt_unix_socket, 0)))
    {
        my_plugin_log_message(&plugin_info_ptr, MY_ERROR_LEVEL, "mysql  connect failed , close");
        mysql_close(res);
        return;
    }
    Memory mem = getMemoryInfo();

    // 查询当前bufferpool的值
    const char *seletBufferPool = "select @@innodb_buffer_pool_size;";
    mysql_query(res, seletBufferPool);
    result = mysql_store_result(res);
    int num_fields = mysql_num_fields(result);
    while ((row = mysql_fetch_row(result)))
    {
        ulong *lengths = mysql_fetch_lengths(result);
        for (int i = 0; i < num_fields; i++)
        {
            if (row[i]){
                currBufferPoolSize = atoll(row[i]);
                break;
            }
            my_plugin_log_message(&plugin_info_ptr, MY_INFORMATION_LEVEL, "[%.*s] ", (int)lengths[i],
                                    row[i] ? row[i] : "NULL");
        }
    }

    my_plugin_log_message(&plugin_info_ptr, MY_INFORMATION_LEVEL, "currBufferPoolSize = %llu", currBufferPoolSize);
    mysql_free_result(result);

    // 设置bufferpool
    const char *query = "set global innodb_buffer_pool_size=%llu;";
    unsigned long long int setbufferPool = (static_cast<unsigned long long int>(mem.MemTotal * 0.7)) * 1024;
    if (setbufferPool > currBufferPoolSize )
    {
        char buf[1024];
        sprintf(buf, query, setbufferPool);
        my_plugin_log_message(&plugin_info_ptr, MY_INFORMATION_LEVEL, " MemTotal = %llu, set bufferpool = %llu, currBufferPoolSize= %llu ", mem.MemTotal * 1024, setbufferPool, currBufferPoolSize);
        mysql_query(res, buf);
        mysql_close(res);
    }else{
        my_plugin_log_message(&plugin_info_ptr, MY_INFORMATION_LEVEL, "Nothing, MemTotal = %llu, set bufferpool = %llu, currBufferPoolSize= %llu ", mem.MemTotal * 1024, setbufferPool, currBufferPoolSize);
    }
}

static int buffer_pool_control_plugin_init(MYSQL_PLUGIN plugin_info)
{
    mysql_mutex_init(PSI_NOT_INSTRUMENTED,
                     &g_record_buffer_mutex,
                     MY_MUTEX_INIT_FAST);

    plugin_info_ptr = plugin_info;
    my_plugin_log_message(&plugin_info_ptr, MY_INFORMATION_LEVEL, "buffer_pool_control_plugin_init");
    my_thread_attr_t attr; /* Thread attributes */

    // bpc_thd = (THD *)my_get_thread_local(THR_THD);
    struct buffer_pool_control_context *con;

    my_plugin_log_message(&plugin_info_ptr, MY_INFORMATION_LEVEL, "create thread");
    plugin_info_ptr = plugin_info;
    con = (struct buffer_pool_control_context *)
        my_malloc(key_memory_mysql_context,
                  sizeof(struct buffer_pool_control_context), MYF(0));
    my_plugin_log_message(&plugin_info_ptr, MY_INFORMATION_LEVEL,
                          "buffer_pool_control_enabled is %d", enabled);

    my_thread_attr_init(&attr);
    my_thread_attr_setdetachstate(&attr, MY_THREAD_CREATE_JOINABLE);
    if (my_thread_create(&con->control_thread, &attr, mysql_heartbeat,
                         (void *)con) != 0)
    {
        fprintf(stderr, "Could not create heartbeat thread!\n");
        exit(0);
    }
    return 0;
}

static int buffer_pool_control_plugin_deinit(void *p)
{
    return 0;
}

struct st_mysql_daemon buffer_pool_control_plugin =
    {MYSQL_DAEMON_INTERFACE_VERSION};

mysql_declare_plugin(buffer_pool_control){
    MYSQL_DAEMON_PLUGIN,
    &buffer_pool_control_plugin,
    "buffer_pool_control",
    "konghui shen",
    "auto adjust buffer pool",
    PLUGIN_LICENSE_GPL,
    buffer_pool_control_plugin_init,   /* Plugin Init */
    buffer_pool_control_plugin_deinit, /* Plugin Deinit */
    0x0100 /* 1.0 */,
    NULL,                                 /* status variables                */
    buffer_pool_control_system_variables, /* system variables                */
    NULL,                                 /* config options                  */
    0,                                    /* flags                           */
} mysql_declare_plugin_end;
