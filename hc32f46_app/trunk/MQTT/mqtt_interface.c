//*****************************************************************************
//! \file mqtt_interface.c
//! \brief Paho MQTT to WIZnet Chip interface implement file.
//! \details The process of porting an interface to use paho MQTT.
//! \version 1.0.0
//! \date 2016/12/06
//! \par  Revision history
//!       <2016/12/06> 1st Release
//!
//! \author Peter Bang & Justin Kim
//! \copyright
//!
//! Copyright (c)  2016, WIZnet Co., LTD.
//! All rights reserved.
//!
//! Redistribution and use in source and binary forms, with or without
//! modification, are permitted provided that the following conditions
//! are met:
//!
//!     * Redistributions of source code must retain the above copyright
//! notice, this list of conditions and the following disclaimer.
//!     * Redistributions in binary form must reproduce the above copyright
//! notice, this list of conditions and the following disclaimer in the
//! documentation and/or other materials provided with the distribution.
//!     * Neither the name of the <ORGANIZATION> nor the names of its
//! contributors may be used to endorse or promote products derived
//! from this software without specific prior written permission.
//!
//! THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
//! AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
//! IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
//! ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
//! LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
//! CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
//! SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
//! INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
//! CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
//! ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
//! THE POSSIBILITY OF SUCH DAMAGE.
//
//*****************************************************************************

#include "mqtt_interface.h"
#include "wizchip_conf.h"
#include "socket.h"
#include "hc32f46_driver.h"
#include "MQTTClient.h"
#include "http_callback.h"
#include <string.h>
#include "reader_msg.h"
#include "app_conf.h"
//unsigned long MilliTimer;

/*
 * @brief MQTT MilliTimer handler
 * @note MUST BE register to your system 1m Tick timer handler.

void MilliTimer_Handler(void) {
	MilliTimer++;
}
 */
/*
 * @brief Timer Initialize
 * @param  timer : pointer to a Timer structure
 *         that contains the configuration information for the Timer.
 */
void TimerInit(Timer* timer) {
	timer->end_time = 0;
}

/*
 * @brief expired Timer
 * @param  timer : pointer to a Timer structure
 *         that contains the configuration information for the Timer.
 */
char TimerIsExpired(Timer* timer) {
	int left = timer->end_time - getSysTick();
	return (left <= 0);
}

/*
 * @brief Countdown millisecond Timer
 * @param  timer : pointer to a Timer structure
 *         that contains the configuration information for the Timer.
 *         timeout : setting timeout millisecond.
 */
void TimerCountdownMS(Timer* timer, unsigned int timeout) {
	timer->end_time = getSysTick() + timeout;
}

/*
 * @brief Countdown second Timer
 * @param  timer : pointer to a Timer structure
 *         that contains the configuration information for the Timer.
 *         timeout : setting timeout millisecond.
 */
void TimerCountdown(Timer* timer, unsigned int timeout) {
	timer->end_time = getSysTick() + (timeout * 1000);
}

/*
 * @brief left millisecond Timer
 * @param  timer : pointer to a Timer structure
 *         that contains the configuration information for the Timer.
 */
int TimerLeftMS(Timer* timer) {
	long left = timer->end_time - getSysTick();
	return (left < 0) ? 0 : left;
}

/*
 * @brief New network setting
 * @param  n : pointer to a Network structure
 *         that contains the configuration information for the Network.
 *         sn : socket number where x can be (0..7).
 * @retval None
 */
void init_mem_sta(void);
int reinit_mbedtls(void);
extern int gMbedNetFd;
int mbedtls_handshake(void);
void NewNetwork(Network* n, int sn, int istls) {
	n->my_socket = sn;
	n->mqttread = hc32inf_read;
	n->mqttwrite = hc32inf_write;
	n->is_tls = istls;
	if (istls == 1)
	{
		gMbedNetFd = sn;
//		init_mem_sta();
//		init_mbedtls();
	}
//	n->disconnect = hc32inf_disconnect;
}

/*
 * @brief read function
 * @param  n : pointer to a Network structure
 *         that contains the configuration information for the Network.
 *         buffer : pointer to a read buffer.
 *         len : buffer length.
 * @retval received data length or SOCKERR code

int w5x00_read(Network* n, unsigned char* buffer, int len, long time)
{
	
	if((getSn_SR(n->my_socket) == SOCK_ESTABLISHED) && (getSn_RX_RSR(n->my_socket)>0))
		return recv(n->my_socket, buffer, len);

	return SOCK_ERROR;
}
 */
int hc32inf_read(Network* n, unsigned char* buffer, int len, long time)
{
	int ret;
	int timems = time;
	
	if (n->is_tls == 1)
	{
		int pos = 0;
		mbedtls_set_readTimeout(time);
		while (1)
		{
			ret =  mbedtls_ssl_read(g_ssl_context, buffer+pos, len-pos);
			if (ret > 0)
				pos += ret;
			else if (MBEDTLS_ERR_SSL_TIMEOUT == ret)
				return 0;
			else 
				return -1;
			if (pos == len)
				break;
		}
	}
	else
	{
		ioctl(n->my_socket, COMMON_INTERFACE_SET_TIMEOUT, &timems);
		ret = read_n(n->my_socket, buffer, len);
	}
	if (ret > 0)
		return ret;
	else if (ret == -2)
		return 0;
	else
		return -1;
}

/*
 * @brief write function
 * @param  n : pointer to a Network structure
 *         that contains the configuration information for the Network.
 *         buffer : pointer to a read buffer.
 *         len : buffer length.
 * @retval length of data sent or SOCKERR code

int w5x00_write(Network* n, unsigned char* buffer, int len, long time)
{
	if(getSn_SR(n->my_socket) == SOCK_ESTABLISHED)
		return send(n->my_socket, buffer, len);

	return SOCK_ERROR;
}
 */
int hc32inf_write(Network* n, unsigned char* buffer, int len, long time)
{
	if (n->is_tls == 1)
	{
		if (mbedtls_ssl_write(g_ssl_context, buffer, len) != len)
		{
			TRACE("mbedtls_ssl_write error\n");
			return -1;
		}
		else
			return len;
	}
	else
		return write_n(n->my_socket, buffer, len);
}


/*
 * @brief disconnect function
 * @param  n : pointer to a Network structure
 *         that contains the configuration information for the Network.

void w5x00_disconnect(Network* n)
{
	disconnect(n->my_socket);
}
 */
void DisconnectNetwork(Network* n)
{
	if (n->my_socket <= COMMON_INTERFACE_SOCKET3)
	{
		disconnect(n->my_socket);
		close(n->my_socket);
	}
}

/*
 * @brief connect network function
 * @param  n : pointer to a Network structure
 *         that contains the configuration information for the Network.
 *         ip : server iP.
 *         port : server port.
 * @retval SOCKOK code or SOCKERR code

int ConnectNetwork(Network* n, uint8_t* ip, uint16_t port)
{
	uint16_t myport = 12345;

	if(socket(n->my_socket, Sn_MR_TCP, myport, 0) != n->my_socket)
		return SOCK_ERROR;

	if(connect(n->my_socket, ip, port) != SOCK_OK)
		return SOCK_ERROR;

	return SOCK_OK;
}
 */
int ConnectNetwork(Network* n, uint8_t* ip, uint16_t port)
{
//	TRACE("fd:%d, ip:%d.%d.%d.%d, port:%d\n", n->my_socket, ip[0], ip[1], ip[2], ip[3], port);
	if (n->my_socket <= COMMON_INTERFACE_SOCKET3)
	{
		if(socket(n->my_socket, Sn_MR_TCP, 0, SF_TCP_NODELAY) != n->my_socket)
			return -1;

		if(connect(n->my_socket, ip, port) != SOCK_OK)
			return -1;
	}
	else
	{
//		if (wlan_reconn_ser() != 0)
//			return -1; 
	}
	if (n->is_tls == 1)
	{
		if (mbedtls_handshake() != 0)
			return -1;
	}
	return 0;
}

int gMqttConnected = 0;

void messageArrived(MessageData* md)
{
	MQTTMessage* message = md->message;
	if (message->payloadlen > (CmdRecvBufLen - 100))
	{
		TRACE("message->payloadlen toot long:%d\n",(char*)message->payload);
		return;
	}
	((char*)message->payload)[(int)message->payloadlen] = 0;
//	TRACE("recv msg:%s\r\n",(char*)message->payload);
	json_remote_cmd((char*)message->payload, message->payloadlen);
}

MQTTClient gCliMqtt;
Network gNwkMqtt;
MQTTPacket_connectData gConData = MQTTPacket_connectData_initializer;
extern uint8 gSerIp[4];
extern uint16 gSerPort;
extern uint8 *SockSendBuffer;
extern uint8 *SockRecvBuffer;
extern int TagSendBufLen;
extern int CmdRecvBufLen;
extern uint8 *gMqttSendBuf;
uint16 gContiMqttFaidedCnt = 0;
void mqtt_task(uint8 *SBuffer, int dlen)
{
	int rc = 0;
	int rtimeout = gRtSetting->upload.recv_timeout*1000;
	
Reconn:
	if (gContiMqttFaidedCnt >= MaxUpsendFailedCntBefReset)
		reset_uart1_ex_dev(&gContiMqttFaidedCnt);

	if (gMqttConnected == 0)
	{
		TRACE("start ConnectNetwork ......\n");	
		DisconnectNetwork(&gNwkMqtt);
		if (ConnectNetwork(&gNwkMqtt, gSerIp, gSerPort) != 0)
		{
			sleep_ms(2000);
			goto Reconn;
		}
	
		if (gIsTlsConn == 0 && gRtSetting->upload.hw_inf != Upload_Inf_Ethernet)
			ioctl(gNwkMqtt.my_socket, COMMON_INTERFACE_CLEAR_REVBUF, NULL);
	
		MQTTClientInit(&gCliMqtt, &gNwkMqtt, rtimeout, gMqttSendBuf, 
			TagSendBufLen+100, SockRecvBuffer, CmdRecvBufLen);
		TRACE("before MQTTConnect\n");
		rc = MQTTConnect(&gCliMqtt, &gConData);
		if (rc != 0)
		{			
			reinit_mbedtls();
			gContiMqttFaidedCnt++;
			goto Reconn;
		}
		gContiMqttFaidedCnt = 0;
		TRACE("MQTTClientInit ok\n");
		///////////
		/*
		rc = MQTTSubscribe(&gCliMqtt, gRtSetting->upload.sw_potl_params.mqtt.pub_topic, 
			(enum QoS)gRtSetting->upload.sw_potl_params.mqtt.pub_qos, messageArrived);
		if (rc != 0)
			goto Reconn;
		*/
		////////////

		if (gRtSetting->upload.sw_potl_params.mqtt.sub_u_topic[0] != 0)
		{
			rc = MQTTSubscribe(&gCliMqtt, gRtSetting->upload.sw_potl_params.mqtt.sub_u_topic, 
				(enum QoS)gRtSetting->upload.sw_potl_params.mqtt.sub_u_qos, messageArrived);
			if (rc != 0)
			{
				reinit_mbedtls();
				goto Reconn;
			}
			else
				TRACE("MQTTSubscribe sub_u_topic ok\n");
		}

		if (gRtSetting->upload.sw_potl_params.mqtt.sub_b_topic[0] != 0)
		{
			if (strcmp(gRtSetting->upload.sw_potl_params.mqtt.sub_b_topic, 
					gRtSetting->upload.sw_potl_params.mqtt.sub_u_topic) != 0)
			{
				rc = MQTTSubscribe(&gCliMqtt, gRtSetting->upload.sw_potl_params.mqtt.sub_b_topic, 
					(enum QoS)gRtSetting->upload.sw_potl_params.mqtt.sub_b_qos, messageArrived);
				if (rc != 0)
				{
					reinit_mbedtls();
					goto Reconn;
				}
				else
					TRACE("MQTTSubscribe sub_b_topic ok\n");
			}
		}
		
		gMqttConnected = 1;
		sleep_ms(1000);
	}
		
	if (SBuffer == NULL)
	{
		rc = MQTTYield(&gCliMqtt, 10);
		if (rc < 0)
		{
			TRACE("MQTTYield failed---- rc:%d\n", rc);
			reinit_mbedtls();
			gMqttConnected = 0;
			goto Reconn;
		}
	}
	else
	{
		MQTTMessage msg;
		msg.qos = (enum QoS)gRtSetting->upload.sw_potl_params.mqtt.pub_qos;
		msg.retained = 0;
		msg.dup = 0;
		msg.payload = SBuffer;
		msg.payloadlen = dlen;
		SBuffer[dlen] = 0;

		rc = MQTTPublish(&gCliMqtt, gRtSetting->upload.sw_potl_params.mqtt.pub_topic, &msg);
		if (rc != 0)
		{
			TRACE("MQTTPublish failed ++++++++++++++++++++++++\n");
			reinit_mbedtls();
			gMqttConnected = 0;
			goto Reconn;
		}
	}
}



