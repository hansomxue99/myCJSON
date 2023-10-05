#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>
#include <limits.h>
#include "cJSON.h"
/* common unity */
static const char *skip(const char *in) {
    while (in && *in && (unsigned char)*in<=32){
        in++; 
    }
    return in;
}

static cJSON* cJSON_New_Item(void) {
    cJSON* node = (cJSON*)malloc(sizeof(cJSON));
    if (node)   memset(node, 0, sizeof(cJSON));
    return node;
}

void cJSON_Delete(cJSON* node) {
    cJSON* next;
    while(node) {
        // 注意cJSON数据结构中含有child、string指针，必须释放，否则会导致内存泄露
        if (node->child)    cJSON_Delete(node->child);
        if (node->valuestring)  free(node->valuestring);
        if (node->string)   free(node->string);
        // 删除当前结点
        next = node->next;
        free(node);
        node = next;
    }
}

/* parse value */
static unsigned parse_hex4(const char* str) {
    unsigned h = 0;
    for (int i=0; i<4; ++i) {
        if (*str>='0' && *str<='9') h += *str-'0';
        else if (*str>='a' && *str<='f')    h += *str-'a';
        else if (*str>='A' && *str<='F')    h += *str-'A';
        else return 0;
        h = h<<4;
        str++;
    }
    return h;
}

static const unsigned char firstByteMark[7] = { 0x00, 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC };
static const char* parse_string(cJSON* item, const char* str) {
    const char* ptr = str + 1;
    char* ptr2;
    int len = 0;
    unsigned uc, uc2;
    while (*ptr!='\"' && *ptr) {
        ++len;
        if (*ptr++=='\\')   ptr++;
    }

    char* cpy = (char*)malloc((len+1) * sizeof(char));
    if (cpy==NULL)  return 0;   // alloc failure

    ptr = str + 1;
    ptr2 = cpy;
    while (*ptr!='\"' && *ptr) {
        if (*ptr!='\\')   *ptr2++ = *ptr++;
        else {
            ptr++;
            switch (*ptr) {
            case 'b': *ptr2++='\b'; break;
            case 'f': *ptr2++='\f'; break;
            case 'n': *ptr2++='\n'; break;
            case 'r': *ptr2++='\r'; break;
            case 't': *ptr2++='\t'; break;
            case 'u': 
                // parse fisrt hex4
                // high: 0xD800~0xDBFF or BMP
                uc = parse_hex4(ptr+1);
                ptr += 4;
                if ((uc>=0xDC00 && uc<=0xDFFF) || uc==0)    break;
                // parse second hex4
                // low: 0xDC00~0xDFFF
                if (uc>=0xD800 && uc<=0xDBFF) {
                    if (ptr[1]!='\\' || ptr[2]!='u')    break;
                    uc2 = parse_hex4(ptr+3);
                    ptr += 6;
                    if (uc2<0xDC00 || uc>0xDFFF)    break;
                    uc = 0x10000 + (((uc&0x3FF)<<10) | (uc2&0x3FF));
                }
                len=4;
                if (uc<0x80)    len = 1;
                else if (uc<0x800)  len = 2;
                else if (uc<0x10000)    len = 3;
                ptr2 += len;
                switch (len) {
                    // 1000 0000    1xxx xxxx
                    // 1011 1111    10xx xxxx
                case 4: *--ptr2 = ((uc | 0x80) & 0xBF); uc >>= 6;
                case 3: *--ptr2 = ((uc | 0x80) & 0xBF); uc >>= 6;
                case 2: *--ptr2 = ((uc | 0x80) & 0xBF); uc >>= 6;
                case 1: *--ptr2 = (uc | firstByteMark[len]);
                }
                ptr2 += len;
                break;
            default: *ptr2++=*ptr; break;
            }
            ptr++;
        }
    }
    *ptr2 = 0;
    if (*ptr=='\"') ptr++;
    item->valuestring = cpy;
    item->type = CJSON_STRING;
    return ptr;
}

static const char* parse_number(cJSON* item, const char *num) {
    double sign = 1;
    double res = 0;
    double scale = 0;
    int subscale = 0;
    int subsign = 1;
    if (*num == '-')  { sign = -1; num++; }
    if (*num == '0')    num++;
    if (*num>='1' && *num<='9') {
        do {
            res = (res*10.0)+(*num++-'0');
        } while (*num>='0' && *num<='9');
    }
    if (*num=='.' && num[1]>='0' && num[1]<='9') {
        num++;
        do {
            res = (res*10.0)+(*num++-'0');
            scale--;
        } while (*num>='0' && *num<='9');
    }
    if (*num=='e' || *num=='E') {
        num++;
        if (*num=='+')  num++;
        else if (*num=='-') { num++; subsign = -1; }
        while (*num>='0' && *num<='9') {
            subscale = (subscale*10)+(*num++-'0');
        }
    }
    res = sign * res * pow(10.0, (scale+subscale*subsign));
    item->valuedouble = res;
    item->valueint = (int)res;
    item->type = CJSON_NUMBER;
    return num;
}

static const char* parse_value(cJSON* item, const char* value);
static const char* parse_array(cJSON* item, const char* value) {
    item->type = CJSON_ARRAY;
    value = skip(value+1);
    if (*value==']')    return value+1;

    cJSON* child = cJSON_New_Item();
    if (!child) return 0;
    item->child = child;
    value = skip(parse_value(child, skip(value)));
    if (!value)  return 0;

    while (*value==',') {
        cJSON* new_item = cJSON_New_Item();
        if(!new_item)   return 0;
        child->next = new_item;
        new_item->prev = child;
        child = new_item;
        value = skip(parse_value(child, skip(value+1)));
        if (!value)    return 0;
    }
    if (*value==']')    return value+1;
    return 0;
}

static const char* parse_object(cJSON* item, const char* value) {
    item->type = CJSON_OBJECT;
    value = skip(value+1);
    if (*value=='}')    return value+1;

    cJSON* child = cJSON_New_Item();
    if (!child) return 0;
    item->child = child;
    value=skip(parse_string(child, skip(value)));
    if (!value)    return 0;
    child->string = child->valuestring;
    child->valuestring = 0;

    if (*value!=':')    return 0;
    value = skip(parse_value(child, skip(value+1)));
    if (!value)    return 0;

    while (*value==',') {
        cJSON* new_item = cJSON_New_Item();
        if(!new_item)   return 0;
        child->next = new_item;
        new_item->prev = child;
        child = new_item;
        value = skip(parse_string(child, skip(value+1)));
        if (!value)    return 0;
        child->string = child->valuestring;
        child->valuestring = 0;
        if (*value!=':')    return 0;
        value = skip(parse_value(child, skip(value+1)));
        if (!value)    return 0;
    }

    if (*value=='}')    return value+1;
    return 0;
}

static const char* parse_value(cJSON* item, const char* value) {
    if (!value) return 0;
    if (!strncmp(value, "null", 4)) {
        item->type = CJSON_NULL;
        return value+4;
    }
    if (!strncmp(value, "false", 5)) {
        item->type = CJSON_FALSE;
        return value+5;
    }
    if (!strncmp(value, "true", 4)) {
        item->type = CJSON_TRUE;
        return value+4;
    }
    if (*value=='\"')   return parse_string(item, value);
    if (*value=='-' || (*value>='0' && *value<='9'))    return parse_number(item, value);
    if (*value=='[')    return parse_array(item, value);
    if (*value=='{')    return parse_object(item, value);

    return 0;
}

cJSON* cJSON_Parse(const char* value) {
    cJSON* head_node = cJSON_New_Item();
    if (head_node==NULL)    return 0;

    const char* end = parse_value(head_node, skip(value));
    --end;
    if (head_node==NULL) {
        cJSON_Delete(head_node);
        return 0;
    }
    return head_node;
}

/* print value */
static char* cJSON_strdup(const char* str) {
    size_t len = strlen(str) + 1;
    char* cpy = (char*)malloc(len);
    if(!cpy)    return 0;
    memcpy(cpy, str, len);
    return cpy;
}

static char* print_number(cJSON* item) {
	char *str=0;
	double d=item->valuedouble;
	if (d==0) {
		str=(char*)malloc(2);	/* 0 */
		if (str) strcpy(str,"0");
	} else if (fabs(((double)item->valueint)-d)<=DBL_EPSILON && d<=INT_MAX && d>=INT_MIN) {
		str=(char*)malloc(21);	/* 整数 */
		if (str)	sprintf(str,"%d",item->valueint);
	} else {
		str=(char*)malloc(64);	/* 浮点数 */
		if (str) {
			if (fabs(floor(d)-d)<=DBL_EPSILON && fabs(d)<1.0e60)sprintf(str,"%.0f",d);
			else if (fabs(d)<1.0e-6 || fabs(d)>1.0e9)			sprintf(str,"%e",d);
			else												sprintf(str,"%f",d);
		}
	}
	return str;
}

static char* print_string(const char* str) {
	const char* ptr;
    char* ptr2;
    char* out;
    int len=0;
    int flag=0;
    unsigned char token;
    
    /* 检查是否含有特殊字符 */
	for (ptr=str; *ptr; ptr++) {
        flag |= ((*ptr>0 && *ptr<32)||(*ptr=='\"')||(*ptr=='\\'))?1:0;
    } 
	if (!flag) {
		len=ptr-str;
		out=(char*)malloc(len+3);   // 1个末尾、2个引号
		if (!out) return 0;
		ptr2=out;
        *ptr2++='\"';
		strcpy(ptr2,str);
		ptr2[len]='\"';
		ptr2[len+1]=0;
		return out;
	}

	ptr=str;
    while ((token=*ptr)) {
        ++len;
        if (strchr("\"\\\b\f\n\r\t",token)) len++; 
        else if (token<32) len+=5;  // \xhh
        ptr++;
    }
	
	out=(char*)malloc(len+3);
	if (!out) return 0;

    ptr=str;
	ptr2=out;
	*ptr2++='\"';
	while (*ptr)
	{
		if ((unsigned char)*ptr>31 && *ptr!='\"' && *ptr!='\\') *ptr2++=*ptr++;
		else {
			*ptr2++='\\';
			switch (token=*ptr++) {
            case '\\':	*ptr2++='\\';	break;
            case '\"':	*ptr2++='\"';	break;
            case '\b':	*ptr2++='b';	break;
            case '\f':	*ptr2++='f';	break;
            case '\n':	*ptr2++='n';	break;
            case '\r':	*ptr2++='r';	break;
            case '\t':	*ptr2++='t';	break;
            default: sprintf(ptr2,"u%04x",token);ptr2+=5;	break;	/* escape and print */
			}
		}
	}
	*ptr2++='\"';*ptr2++=0;
	return out;
}

static char* print_value(cJSON* item, int depth);
static char* print_array(cJSON* item, int depth) {
	char** entries; // 存放每个子元素的地址
	char* out=0;    // 存放输出指针
    char* ptr;      // 指向指针
    char* ret;      // 子元素的指针
    int len=0;      // 输出长度
	
    int i=0;
    int fail=0;
	size_t tmplen=0;
    int numentries=0;
    cJSON* child=item->child;
	
	/* 统计子元素的数量 */
	while (child) {
        numentries++;
        child=child->next;
    }
	/* 没有子元素 */
	if (!numentries) {
		out=(char*)malloc(3);
		if (out) strcpy(out,"[]");
		return out;
	}

    /* 为每个子元素分配一个指针 */
    entries = (char**)malloc(numentries*sizeof(char*));
    if (!entries) return 0;
    memset(entries, 0, numentries*sizeof(char*));

    /* 遍历所有子元素 */
    child = item->child;
    while (child && !fail) {
        ret = print_value(child, depth+1);
        entries[i++] = ret;
        if (ret) len += strlen(ret)+2+1; 
        else fail = 1;
        child = child->next;
    }
    
    if (!fail)	out=(char*)malloc(len);
    if (!out) fail=1;

    /* 处理failure */
    if (fail) {
        for (i=0;i<numentries;i++) {
            if (entries[i]) free(entries[i]);
        }
        free(entries);
        return 0;
    }
    
    /* 合并array */
    *out='[';
    ptr=out+1;
    *ptr=0;
    for (i=0; i<numentries; i++) {
        tmplen=strlen(entries[i]);
        memcpy(ptr, entries[i], tmplen);
        ptr += tmplen;
        if (i != numentries-1) {
            *ptr++=',';
            *ptr++=' ';
            *ptr=0;
        }
        free(entries[i]);
    }
    free(entries);
    *ptr++=']';
    *ptr++=0;
	return out;	
}

static char* print_object(cJSON* item, int depth) {
	char** entries=0;
    char** names=0;
	char* out=0;
    char* ptr;
    char* ret;
    char* str;
    int len=7;
    int i=0,j;
	cJSON *child=item->child;
	int numentries=0;
    int fail=0;
	size_t tmplen=0;
	/* 统计子元素个数 */
	while (child) {
        numentries++;
        child=child->next;
    }
	/* 空object */
	if (!numentries) {
		out=(char*) malloc(depth+4);
		if (!out)	return 0;
		ptr=out;
        *ptr++='{';
        *ptr++='\n';
        for (i=0; i < depth; i++) *ptr++='\t';
		*ptr++='}';
        *ptr++=0;
		return out;
	}
    /* Allocate space for the names and the objects */
    entries=(char**)malloc(numentries*sizeof(char*));
    if (!entries) return 0;
    names=(char**)malloc(numentries*sizeof(char*));
    if (!names) {free(entries);return 0;}
    memset(entries,0,sizeof(char*)*numentries);
    memset(names,0,sizeof(char*)*numentries);

    /* Collect all the results into our arrays: */
    child=item->child;
    depth++;
    len += depth;
    while (child) {
        names[i]=str=print_string(child->string);
        entries[i++]=ret=print_value(child, depth);
        if (str && ret) len += strlen(ret) + strlen(str) + 2 + (2+depth);
        else fail=1;
        child=child->next;
    }
    
    /* Try to allocate the output string */
    if (!fail)	out=(char*)malloc(len);
    if (!out) fail=1;

    /* Handle failure */
    if (fail) {
        for (i=0;i<numentries;i++) {
            if (names[i]) free(names[i]);
            if (entries[i]) free(entries[i]);
        }
        free(names);free(entries);
        return 0;
    }
    
    /* Compose the output: */
    *out='{';
    ptr=out+1;
    *ptr++='\n';
    *ptr=0;
    for (i=0;i<numentries;i++)
    {
        for (j=0;j<depth;j++) *ptr++='\t';
        tmplen=strlen(names[i]);
        memcpy(ptr,names[i],tmplen);
        ptr+=tmplen;
        *ptr++=':';
        *ptr++='\t';
        strcpy(ptr,entries[i]);
        ptr+=strlen(entries[i]);
        if (i!=numentries-1) *ptr++=',';
        *ptr++='\n';*ptr=0;
        free(names[i]);free(entries[i]);
    }
    
    free(names);free(entries);
    for (i=0; i<depth-1; i++) *ptr++='\t';
    *ptr++='}';
    *ptr++=0;
	return out;	
}


static char* print_value(cJSON* item, int depth) {
    if (!item)  return 0;
    char* out;
    switch (item->type) {
    case CJSON_NULL:	out=cJSON_strdup("null");	break;
    case CJSON_FALSE:	out=cJSON_strdup("false");break;
    case CJSON_TRUE:	out=cJSON_strdup("true"); break;
    case CJSON_NUMBER:	out=print_number(item);break;
    case CJSON_STRING:	out=print_string(item->valuestring);break;
    case CJSON_ARRAY:	out=print_array(item, depth);break;
    case CJSON_OBJECT:	out=print_object(item, depth);break;
    }
    return out;
}

char* cJSON_Print(cJSON* item) {
    return print_value(item, 0);
}