#include <my_sys.h>
#include <mysql/plugin.h>
#include "my_thread.h"
#include "sql_class.h"
#include "set_var.h"
#include "item.h"
#include <cstring>
#include "mysql.h"
#include "buffer_pool_control.h"
#include "memory.h"

#define HEART_STRING_BUFFER 100
static MYSQL_PLUGIN plugin_info_ptr;

/* 系统配置变量 */
// 是否开启
static bool enabled;
// 控制时间间隔, 单位秒
// static int buffer_pool_control_interval;
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

static struct st_mysql_sys_var *buffer_pool_control_system_variables[] = {
    MYSQL_SYSVAR(enabled),
    NULL};

struct buffer_pool_control_context
{
    my_thread_handle control_thread;
};

PSI_memory_key key_memory_mysql_context;

void *mysql_heartbeat(void *p)
{

    // struct mysql_heartbeat_context *con = (struct mysql_heartbeat_context *)p;
    char buffer[HEART_STRING_BUFFER];
    time_t result;
    struct tm tm_tmp;

    while (1)
    {
        sleep(10);
        set_buffer_pool_size();

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

void set_buffer_pool_size(){
    // // 最大允许的bufffer_pool
    // uint64_t max_innodb_buffer_pool_size = 2048ULL * 1024 * 1024;
    // // 最小允许的bufffer_pool
    // uint64_t min_innodb_buffer_pool_size = 128ULL * 1024 * 1024;
    // // 内存少于256MB触发缩容防止OOM
    // uint64_t min_avaliable_mem = 256ULL * 1024 * 1024;
    MYSQL mysql = MYSQL();
    MYSQL *res = mysql_init(&mysql);
    if (!(mysql_real_connect(res, "127.0.0.1", "root",
                             "123456", "mysql", 3307,
                             NULL, 0)))
    {
        my_plugin_log_message(&plugin_info_ptr, MY_ERROR_LEVEL, "mysql_close");
        mysql_close(res);
    }
    Memory mem = getMemoryInfo();

    const char *seletBufferPool = "select @ @innodb_buffer_pool_size;" if (!mysql_query(res, seletBufferPool))
    {
        my_plugin_log_message(&plugin_info_ptr, MY_ERROR_LEVEL, "select @ @innodb_buffer_pool_size failed");
    }

    const char *query = "ser global innodb_buffer_pool_size=%llu;";
    unsigned long long int setbufferPool = (static_cast<unsigned long long int>(mem.MemTotal * 0.7)) * 1024;
    char buf[1024];
    sprintf(buf,query,setbufferPool);
    my_plugin_log_message(&plugin_info_ptr, MY_INFORMATION_LEVEL, "MemTotal = %llu, set bufferpool = %llu", mem.MemTotal * 1024, setbufferPool);

    if (!mysql_query(res, buf))
    {
        my_plugin_log_message(&plugin_info_ptr, MY_ERROR_LEVEL, "set innodb_buffer_pool_size failed");
    }
    mysql_close(res);
}

static int buffer_pool_control_plugin_init(MYSQL_PLUGIN plugin_info)
{
    plugin_info_ptr = plugin_info;
    my_plugin_log_message(&plugin_info_ptr, MY_INFORMATION_LEVEL, "buffer_pool_control_plugin_init");
    my_thread_attr_t attr; /* Thread attributes */

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
