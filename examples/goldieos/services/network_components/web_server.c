#include "stdlib.h"
#include "string.h"
#include "web_server.h"
#include "goldie_osal.h"

static WifiService* wifi_service = NULL;
static char html_form2[FORM2_MAX_SIZE];
static ApInfo_t aplist[10];

const char *html_form1 = 
"HTTP/1.1 200 OK\r\n"
"Content-Type: text/html\r\n"
"Connection: close\r\n\r\n"
"<!DOCTYPE html>"
"<html lang='zh-CN'>"
"<head>"
"<title>Device configuration</title>"
"<style>"
"body {"
"font-family: Arial, sans-serif;"
"max-width: 500px;"
"margin: 0 auto;"
"padding: 20px;"
"background-color: #f5f5f5;"
"}"
".container {"
"background-color: white;"
"padding: 20px;"
"border-radius: 8px;"
"box-shadow: 0 2px 10px rgba(0, 0, 0, 0.1);"
"}"
"h2 {"
"text-align: center;"
"color: #333;"
"}"
".form-group {"
"    margin-bottom: 15px;"
"}"
"label {"
"display: block;"
"margin-bottom: 5px;"
"font-weight: bold;"
"}"
"input[type='text'], input[type='password'] {"
"width: 100%;"
"padding: 10px;"
"border: 1px solid #ddd;"
"border-radius: 4px;"
"box-sizing: border-box;"
"}"
"button {"
"background-color: #337ab7;"
"color: white;"
"padding: 10px 15px;"
"border: none;"
"border-radius: 4px;"
"cursor: pointer;"
"width: 100%;"
"font-size: 16px;"
"margin-bottom: 14px;"
"}"
"button:hover {"
"background-color: #286090;"
"}"
"#wifi-list {"
"margin-top: 10px;"
"}"
".wifi-item {"
"padding: 8px;"
"border-bottom: 1px solid #eee;"
"cursor: pointer;"
"color: #808080;"
"}"
".wifi-item:hover {"
"background-color: #f9f9f9;"
"}"
"</style>"
"<script>"
"document.addEventListener('DOMContentLoaded', function() {"
"const scanWifiBtn = document.getElementById('scan-wifi');"
"const wifiList = document.getElementById('wifi-list');"
"const ssidInput = document.getElementById('ssid');"
"const passwordInput = document.getElementById('password');"
"const submitBtn = document.getElementById('submit-btn');";

const char *html_form3 = 
"function displayWiFiNetworks(networks) {"
"wifiList.innerHTML = '';"
"if (networks.length === 0) {"
"wifiList.innerHTML = '<div class=\"wifi-item\">No available WiFi networks were found</div>';"
"return;"
"}"
"networks.forEach(network => {"
"const wifiItem = document.createElement('div');"
"wifiItem.className = 'wifi-item';"
"wifiItem.innerHTML =  network.ssid ;"
"wifiItem.addEventListener('click', () => {"
"ssidInput.value = network.ssid;"
"passwordInput.value = '';"
"if (network.passwd === '0') {"
"}else{"
"passwordInput.value = network.passwd;"
"}"
"});"
"wifiList.appendChild(wifiItem);"
"});"
"}"
"displayWiFiNetworks(mockNetworks);"
"});"
"</script>"
"</head>"
"<body>"
"<div class='container'>"
"<h2>Device configuration</h2>"
"<form method='POST' action='/submit'>"
"<div class='form-group'>"
"<label for='ssid'>Network Name</label>"
"<input type='text' name='network_name'  id='ssid' required>"
"</div>"
"<div class='form-group'>"
"<label for='password'>Password</label>"
"<input type='password' name='password'  id='password' required>"
"</div>"
"<input type='text' name='void' style='display:none' value=' '>"
"<button type='submit' id='submit-btn'>Submit</button>"
"</form>"
"<div class='form-group'>"
"<label for='ssid'>Select 2.4G WiFi from the following list</label>"
"<div id='wifi-list'></div>"
"</div>"
"</div>"
"</body>"
"</html>";

static void store_ssid_key(char* network_name,char* passwd)
{
	printf("store_ssid_key [%s : %s] \n", network_name,passwd);
	if(wifi_service)
	{
		wifi_service->svr_sta_add_account(network_name,passwd);
	}
}

// 解析POST数据函数
static void parse_form_data(char *data, ConfigInfo *config) {
    char *token;
    char *saveptr;
	memset(config->network_name,0,32);
	memset(config->password,0,32);
	memset(config->device_name,0,32);
	memset(config->background,0,256);
	memset(config->cloud_model,0,32);
	
    token = strtok_r(data, "&", &saveptr);
    while (token != NULL) {
        char *key = token;
        char *value = strchr(token, '=');
		printf("value is:%s key is %s \n",value,key);
        if (value) {
            *value++ = '\0';
            // URL解码处理（简单实现）
            for (char *p = value; *p; p++) {
                if (*p == '+') *p = ' ';
            }
            
            if (strcmp(key, "network_name") == 0) {
                strncpy(config->network_name, value, strlen(value));
            } else if (strcmp(key, "password") == 0) {
                strncpy(config->password, value, strlen(value));
            } else if (strcmp(key, "device_name") == 0) {
                strncpy(config->device_name, value, strlen(value));
            } else if (strcmp(key, "background") == 0) {
                strncpy(config->background, value, strlen(value));
            } else if (strcmp(key, "cloud_model") == 0) {
                strncpy(config->cloud_model, value, strlen(value));
            }
        }
        token = strtok_r(NULL, "&", &saveptr);
    }
}

static void rebuild_web(void)
{
	if(!wifi_service)
		return;
	int apnum = wifi_service->svr_sta_get_ap_list(aplist);
	if(apnum <=0)
		return;

	AccountList_t *acc_list = goldie_malloc(sizeof(AccountList_t)*apnum);
	memset(acc_list,0,sizeof(AccountList_t)*apnum);
	
	for (int i = 0; i < apnum; i++)
    {   
		sprintf(acc_list[i].ssid,"{ssid:'%s',passwd:'******'},",aplist[i].ssid);
	}
    char *str_temp  = goldie_malloc(sizeof(AccountList_t)*apnum+1);
	memset(str_temp,0,sizeof(AccountList_t)*apnum+1);
    *str_temp='\0';
    for (int j = 0; j < apnum; j++)
    {
       
        if(strlen(acc_list[j].ssid) != 0){
        	strcat(str_temp,acc_list[j].ssid);
        }
        
    }

	memset(html_form2,0,FORM2_MAX_SIZE);
	if(strlen(str_temp) > (FORM2_MAX_SIZE-30)){
		printf("FORM2_MAX_SIZE is too small ! \r\n");
	}
    sprintf(html_form2,"const mockNetworks = [%s];",str_temp);
    goldie_free(str_temp);
    goldie_free(acc_list);
    return;

	
}

// HTTP����������
static void web_server(void* para) {
    struct netbuf *inbuf;
    char *buf;
    u16_t len;
    err_t err;
    struct netconn *conn = (struct netconn *)para;
	rebuild_web();
    if ((err = netconn_recv(conn, &inbuf)) == ERR_OK) {
        netbuf_data(inbuf, (void **)&buf, &len);
       
        // ����GET����
        if (strstr(buf, "GET / ") != NULL) {
             char *html_form = goldie_malloc(3800);
            sprintf(html_form,"%s%s%s",html_form1,html_form2,html_form3);
            netconn_write(conn, html_form, strlen(html_form), NETCONN_NOCOPY);
            goldie_free(html_form);

        }
        // ����POST�ύ
        else if (strstr(buf, "POST /submit")) {
            char *body = strstr(buf, "\r\n\r\n");
            if (body) {
                body += 4; // ����ͷ��
                ConfigInfo config = {0};
                parse_form_data(body, &config);
                
                // ��ӡ������Ϣ
                printf("\n=== Configuration Received ===\n");
                printf("Network Name: %s\n", config.network_name);
                printf("Password: %s\n", config.password);
                printf("==============================\n");
                store_ssid_key(config.network_name,config.password);
                // 返回确认页面
                const char *confirm = 
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/html\r\n\r\n"
                "<html><body>"
                "<h2>Configuration Saved!</h2>"
                "<a href='/'>Back</a>"
                "</body></html>";
                netconn_write(conn, confirm, strlen(confirm), NETCONN_NOCOPY);
            }
        }
        netbuf_delete(inbuf);
    }
    netconn_close(conn);
    netconn_delete(conn);
}

static void web_server_task(void *arg) {
    struct netconn *conn, *newconn;
	wifi_service = wait_service(WIFI_SERVICE_INDEX);
    conn = netconn_new(NETCONN_TCP);
    netconn_bind(conn, IP_ADDR_ANY, WEB_PORT);
    netconn_listen(conn);
	if(arg!=NULL){
		conn = netconn_new(NETCONN_TCP);
	}
    while (1) {
		(void)goldie_msleep(30);
        err_t err = netconn_accept(conn, &newconn);
        if (err == ERR_OK) {
           // sys_thread_new("web_handler", web_server, newconn, DEFAULT_THREAD_STACKSIZE, 24);
			goldie_thread_lock();
			void* task_handle = goldie_thread_create(web_server, newconn, "web_handler", DEFAULT_THREAD_STACKSIZE);
			if (task_handle != NULL) {
				goldie_thread_set_priority(task_handle, 24);
			}
			goldie_thread_unlock();

			
        }
    }
}

// ������������ HTTP ����
void web_server_start(void) {
    //sys_thread_new("web_server", web_server_task, NULL, DEFAULT_THREAD_STACKSIZE, 24);
	goldie_thread_lock();
    void * task_handle = goldie_thread_create(web_server_task, 0, "web_server", DEFAULT_THREAD_STACKSIZE);
    if (task_handle != NULL) {
        goldie_thread_set_priority(task_handle, 24);
    }
    goldie_thread_unlock();
}



