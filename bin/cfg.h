#ifndef __CFG_H__
#define __CFG_H__
typedef struct item_t {
    char *key;
    char *value;
}ITEM;

//去除字符串右端空格
char *strtrimr(char *pstr);

//去除字符串左端空格

char *strtriml(char *pstr);

//去除字符串两端空格
char *strtrim(char *pstr);

//从配置文件的一行读出key或value,返回item指针
//line--从配置文件读出的一行

int  get_item_from_line(char *line, ITEM *item);

//完全解析配置文件
int file_to_items(const char *file, ITEM *items);

//读取value
int read_conf_value(const char *key,char *value,const char *file);

int write_conf_value(const char *key,char *value,const char *file);

//char *转int
int stoi(const char * str);


#endif //__CFG_H__

