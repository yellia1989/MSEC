
/**
 * Tencent is pleased to support the open source community by making MSEC available.
 *
 * Copyright (C) 2016 THL A29 Limited, a Tencent company. All rights reserved.
 *
 * Licensed under the GNU General Public License, Version 2.0 (the "License"); 
 * you may not use this file except in compliance with the License. You may 
 * obtain a copy of the License at
 *
 *     https://opensource.org/licenses/GPL-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the 
 * License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific language governing permissions
 * and limitations under the License.
 */


#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "jansson.h"
#include "commstruct.h"
#include "comm.h"
#include "policy.h"

int32_t json_parse_server(json_t *json, struct server_info *server)
{
    size_t index;
    const char *type;
    json_t *val, *aval;

    /* Ȩ����Ϣ */
    val = json_object_get(json, "w");
    if (NULL == val || !json_is_integer(val)) {
        return -101; 
    } else {
        server->weight_static = (uint16_t)json_integer_value(val);
        if ((server->weight_static > NLB_WEIGHT_MAX)
            || (server->weight_static < NLB_WEIGHT_MIN)) {
            return -102;
        }
    }

    /* �˿����� */
    val = json_object_get(json, "t");
    if (NULL == val || !json_is_string(val)) {
        return -103; 
    }

    type = json_string_value(val);
    if (!strcmp(type, "udp")) {
        server->port_type = 1;
    } else if (!strcmp(type, "tcp")) {
        server->port_type = 2;
    } else if (!strcmp(type, "all")) {
        server->port_type = 3;
    } else {
        return -104;
    }

    /* IP��ַ */
    val = json_object_get(json, "IP");
    if (NULL == val || !json_is_string(val)) {
        return -105;
    }

    inet_aton(json_string_value(val), (struct in_addr *)&server->server_ip);

    /* �˿��б� */
    aval = json_object_get(json, "ports");
    if (NULL == val || !json_is_array(aval)) {
        return -106;
    }

    if (json_array_size(aval) > NLB_PORT_MAX) {
        return -107;
    }

    json_array_foreach(aval, index, val)
    {
        if (!json_is_integer(val)) {
            return -108;
        }

        server->port[index] = (uint16_t)json_integer_value(val);
    }

    server->port_num = json_array_size(aval);

    return 0;
}

int32_t json_parse_service_param(json_t *json, struct shm_servers *shm_servers)
{
    json_t *val;
    int32_t policy                  = NLB_POLICY_STANDARD;
    int32_t shaping_request_min     = NLB_SHAPING_REQUEST_MIN;      // ͳ��������С������,Ĭ��10��
    float   success_ratio_base      = NLB_SUCCESS_RATIO_BASE;       // �ɹ��ʻ�׼��һ��ϸߣ�Ĭ��98%
    float   success_ratio_min       = NLB_SUCCESS_RATIO_MIN;        // ��С�ɹ��ʣ�С�ڸ�ֵ����Ҫѡ���Ӧ�Ļ��������ѡ����������
    float   resume_weight_ratio     = NLB_RESUME_WEIGHT_RATIO;      // �����ظ������õ�Ȩ�ر�����Ĭ��10%
    float   dead_retry_ratio        = NLB_DEAD_RETRY_RATIO;         // ��������̽�������
    float   weight_low_watermark    = NLB_WEIGHT_LOW_WATERMARK;     // ���ڸ�ֵ��ֻ������Ȩ�أ������д���ƽ���ɹ��ʵļ�Ȩ��
    float   weight_low_ratio        = NLB_WEIGHT_LOW_RATIO;         // ��Ȩ�ػ����������ڸ�ֵ��������Ȩ�أ�ֻ������ƽ���ɹ��ʵĻ�����Ȩ��
    float   weight_incr_ratio       = NLB_WEIGHT_INCR_RATIO;        // ÿ������Ȩ�صı���


    /* ��ȡ���� */
    val = json_object_get(json, "Policy");
    if (val) { 
        if (!json_is_string(val)) {
            return -201;
        }

        policy = str2policy(json_string_value(val));

        if (policy == NLB_POLICY_UNKOWN) {
            return -201;
        }
    }

    /* ��ȡͳ��������С������ */
    val = json_object_get(json, "shaping_request_min");
    if (val) {
        if (!json_is_integer(val)) {
            return -202;
        }

        shaping_request_min = (int32_t)json_integer_value(val);

        if (shaping_request_min < 0) {
            return -202;
        }
    }

    /* ��ȡ��׼�ɹ��� */
    val = json_object_get(json, "success_ratio_base");
    if (val) {
        if (!json_is_string(val)) {
            return -204;
        }

        success_ratio_base = (float)atof(json_string_value(val));

        if (success_ratio_base > 1.0 || success_ratio_base <= 0.00001) {
            return -204;
        }
    }

    /* ��ȡ��С�ɹ��� */
    val = json_object_get(json, "success_ratio_min");
    if (val) {
        if (!json_is_string(val)) {
            return -205;
        }

        success_ratio_min = (float)atof(json_string_value(val));

        if (success_ratio_min > success_ratio_base || success_ratio_min <= 0.00001) {
            return -205;
        }
    }

    /* ��ȡ�����ָ����õ�Ȩ�ر��� */
    val = json_object_get(json, "resume_weight_ratio");
    if (val) {
        if (!json_is_string(val)) {
            return -206;
        }

        resume_weight_ratio = (float)atof(json_string_value(val));

        if (resume_weight_ratio > 1.0 || resume_weight_ratio <= 0.00001) {
            return -206;
        }
    }

    /* ��ȡ����̽���������� */
    val = json_object_get(json, "dead_retry_ratio");
    if (val) {
        if (!json_is_string(val)) {
            return -207;
        }

        dead_retry_ratio = (float)atof(json_string_value(val));

        if (dead_retry_ratio > 1.0 || dead_retry_ratio <= 0.00001) {
            return -207;
        }
    }

    /* ��ȡ��Ȩ��ˮƽ�� */
    val = json_object_get(json, "weight_low_watermark");
    if (val) {
        if (!json_is_string(val)) {
            return -208;
        }

        weight_low_watermark = (float)atof(json_string_value(val));

        if (weight_low_watermark >= 1.0 || weight_low_watermark <= 0.00001) {
            return -208;
        }
    }

    /* ��ȡ��Ȩ�ػ������� */
    val = json_object_get(json, "weight_low_ratio");
    if (val) {
        if (!json_is_string(val)) {
            return -209;
        }

        weight_low_ratio = (float)atof(json_string_value(val));

        if (weight_low_ratio >= 1.0 || weight_low_ratio <= 0.00001) {
            return -209;
        }
    }

    /* ��ȡÿ������Ȩ�صı��� */
    val = json_object_get(json, "weight_incr_ratio");
    if (val) {
        if (!json_is_string(val)) {
            return -210;
        }

        weight_incr_ratio = (float)atof(json_string_value(val));

        if (weight_incr_ratio >= 1.0 || weight_incr_ratio <= 0.00001) {
            return -210;
        }
    }

    shm_servers->policy                 = policy;
    shm_servers->shaping_request_min    = shaping_request_min;
    shm_servers->dead_retry_ratio       = dead_retry_ratio;
    shm_servers->success_ratio_base     = success_ratio_base;
    shm_servers->success_ratio_min      = success_ratio_min;
    shm_servers->resume_weight_ratio    = resume_weight_ratio;
    shm_servers->weight_low_watermark   = weight_low_watermark;
    shm_servers->weight_low_ratio       = weight_low_ratio;
    shm_servers->weight_incr_ratio      = weight_incr_ratio;

    return 0;
}

int32_t json_parse_service(const char *json_buf, int32_t buf_len, struct shm_servers **svrs)
{
    int32_t result;
    int32_t len;
    size_t  index;
    json_t *json = NULL;
    json_t *val, *aval;
    json_error_t error;
    const char *buff = json_buf;
    struct shm_servers *shm_svrs = NULL;

    /* ȷ��json���ݵĳ��� */
    if (buff[buf_len - 1] == '\0') {
        len = strlen(buff);
    } else {
        len = buf_len;
    }

    /* ����json�ַ�����json���� */
    json = json_loadb(buff, len, 0, &error);
    if (!json) {
        result = -2;
        goto ERR_RET;
    }

    /* ��ȡIPInfo json���� */
    aval = json_object_get(json, "IPInfo");
    if (NULL == aval || !json_is_array(aval)) {
        result = -4;
        goto ERR_RET;
    }

    if (json_array_size(aval) == 0 || json_array_size(aval) >= NLB_SERVER_MAX) {
        result = -5;
        goto ERR_RET;
    }

    shm_svrs = calloc(1, sizeof(struct shm_servers) + json_array_size(aval)*sizeof(struct server_info));
    if (NULL == shm_svrs) {
        result = -6;
        goto ERR_RET;
    }

    /* ѭ����ȡ����IP��Ϣ */
    json_array_foreach(aval, index, val)
    {
        if (json_typeof(val) != JSON_OBJECT) {
            result = -7;
            goto ERR_RET;
        }
    
        result = json_parse_server(val, &shm_svrs->svrs[index]);
        if (result) {
            goto ERR_RET;
        }

        shm_svrs->weight_static_total += shm_svrs->svrs[index].weight_static;
    }

    /* ����ҵ���ѡ���ò��� */
    result = json_parse_service_param(json, shm_svrs);
    if (result) {
        goto ERR_RET;
    }

    shm_svrs->server_num    = json_array_size(aval);
    shm_svrs->version       = NLB_SHM_VERSION1;

    *svrs = shm_svrs;
    json_decref(json);

    return 0;

ERR_RET:

    if (json)
        json_decref(json);

    if (shm_svrs)
        free(shm_svrs);

    return result;
}

#if 0
int main()
{
    int32_t ret;
    struct shm_servers *servers;
    char json_str[] = "{\"IPInfo\":[{\"IP\": \"1.1.1.1\", \"ports\": [1,2,3], \"t\":\"all\", \"w\":100}, {\"IP\":\"2.2.2.2\",\"ports\":[11],\"t\":\"all\", \"w\":200}]}";
    ret = json_parse_service(json_str, sizeof(json_str), &servers);

    printf("%d\n", ret);

    return 0;
}
#endif
