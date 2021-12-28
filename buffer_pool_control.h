void set_buffer_pool_size();
void set_buffer_pool_size_new();
char* show_var(const char *query);
#ifndef MYSQL_SERVER
#define MYSQL_SERVER
#endif

#ifndef SAFE_MUTEX
#define SAFE_MUTEX
#endif
