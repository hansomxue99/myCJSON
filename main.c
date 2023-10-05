#include <stdio.h>
#include <stdlib.h>
#include "cJSON.h"
void doit(char *text)
{
	char *out;
	cJSON *json;
	
	json=cJSON_Parse(text);
	if (!json) {printf("Error\n");}
	else
	{
		out=cJSON_Print(json);
		cJSON_Delete(json);
		printf("%s\n",out);
		free(out);
	}
}
int main() {
    printf("This my cjson project\n");
	char text8[] = "{\"ad_id\":\"tool_ad\",\"create_time\":1551432168000,\"detail\":\"\",\"height\":90,\"href\":\"https://www.oschina.net/action/visit/ad?id=1550\",\"html\":\"\",\"id\":92,\"ident\":\"tool\",\"img\":\"https://static.oschina.net/uploads/cooperation/tool_AGWwZ.jpg\",\"name\":\"在线工具顶部广告位\",\"refer\":\"\",\"script\":\"\",\"status\":0,\"update_time\":1695695089000,\"user\":1046130,\"width\":970}";
	/* Process each json textblock by parsing, then rebuilding: */
	doit(text8);
    return 0;
}