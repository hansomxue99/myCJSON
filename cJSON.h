#ifndef CJSON__H
#define CJSON__H

/*cjson data types*/
#define CJSON_NULL 0
#define CJSON_TRUE 1
#define CJSON_FALSE 2
#define CJSON_NUMBER 3
#define CJSON_STRING 4
#define CJSON_ARRAY 5
#define CJSON_OBJECT 6

/*cjson data structure*/
typedef struct cJSON {
    struct cJSON* next;
    struct cJSON* prev;

    int type;
    char* string;

    int valueint;
    double valuedouble;
    char* valuestring;
    struct cJSON* child;
} cJSON;

/*functions*/
extern cJSON* cJSON_Parse(const char* value);
extern void cJSON_Delete(cJSON* node);
extern char* cJSON_Print(cJSON* item);
#endif