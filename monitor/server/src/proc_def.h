
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


#ifndef __PROC_DEF_H__
#define __PROC_DEF_H__

const uint32_t MAX_OPEN_FILE = 10000;					/* ���֧��10000���ļ������� */
const uint32_t MAX_CORE_FILE = 3*1024*1024*1024u;		/* ���֧��3G��core file */

struct SConfig
{
	SConfig(): sLogPath("../log/") 
	{
		bDaemon = true;
		bUsingCore = true;
		dwMaxOpenFile = MAX_OPEN_FILE;
		dwMaxCoreFile = MAX_CORE_FILE;
		dwLogFileSize = 5*1024*1024;
		dwLogFileCount = 5;
		bShowCmd = false;
	}

	// ϵͳ��Ϣ
	bool bUsingCore;
	uint32_t dwMaxOpenFile;
	uint32_t dwMaxCoreFile;
	string sLocalIP;	//����յ����ص��ϱ���ʹ�ø�IP
	uint32_t dwThreadPoolSize;

	// ��־��Ϣ
	string sLogPath;
	uint32_t dwLogFileSize;
	uint32_t dwLogFileCount;
	bool bShowCmd;

	// �ź���key
	uint32_t dwSemKey;

	// �����ڴ�
	uint32_t dwMemoryLeft;
	uint16_t wMemoryPercentage;
	uint32_t dwServiceShmKey;
	uint64_t ddwServiceShmSize;
	uint32_t dwDataShmKey;
	uint64_t ddwDataShmSize;

	// tcp��������Ϣ
	string sTcpIp;
	uint16_t wSetTcpPort;
	uint16_t wGetTcpPort;
	uint32_t dwTcpInBufSize;                    /* �������뻺���С */
	uint32_t dwTcpOutBufSize;                   /* ������������С */
	uint32_t dwTcpMaxClient;                    /* ��������û��� */
	uint32_t dwTcpOverTime;                     /* ����ͬ�ͻ���ͨ�Ų�ĳ�ʱʱ�� */

	//�澯DB��Ϣ��ȡ
	bool bAlarm;
	string sAlarmDBHost;
	uint16_t wAlarmDBPort;
	string sAlarmDBUser;
	string sAlarmDBPassword;
	string sAlarmDBName;
	uint32_t dwCheckAlarmDBInterval;
	uint16_t wMaxAlarmNumPerService;
	
	// ��ʱ������
	uint32_t dwCheckSignalInterval;

	// ���� 
	string sProgramName;
	string sCfgFile;
	bool bDaemon;
	string sDumpPath;
};

// һЩͨ�ö���
extern SConfig stConfig;

enum PKG_TYPE
{
	PT_REQ = 0,
	PT_ACK,
};

enum CMD_TYPE
{
	CT_CMD1 = 1,
};

enum RES_TYPE
{
	RT_SUCESS = 0,
	RT_ERROR = 1,
	RT_TIMEOUT = 2,
};

#endif
