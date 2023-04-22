#include <stdio.h>
#include "tcp_protocol.h"
#include "socket.h"
#include "wizchip_conf.h"

// recieve data into buf, return the data byte size, if less than or equal to 0
// means no data receive or connect error
int32_t tcp_data_recv(uint8_t sn, uint8_t *buf, uint16_t port)
{
   int32_t ret;
   uint16_t size = 0, sentsize = 0;

#ifdef _LOOPBACK_DEBUG_
   uint8_t destip[4];
   uint16_t destport;
#endif

   switch (getSn_SR(sn))
   {
   case SOCK_ESTABLISHED:
      if (getSn_IR(sn) & Sn_IR_CON)
      {
#ifdef _LOOPBACK_DEBUG_
         getSn_DIPR(sn, destip);
         destport = getSn_DPORT(sn);

         printf("%d:Connected - %d.%d.%d.%d : %d\r\n", sn, destip[0], destip[1], destip[2], destip[3], destport);
#endif
         setSn_IR(sn, Sn_IR_CON);
         return -10;
      }
      // data receive
      if ((size = getSn_RX_RSR(sn)) > 0) // Don't need to check SOCKERR_BUSY because it doesn't not occur.
      {
         if (size > DATA_BUF_SIZE)
            size = DATA_BUF_SIZE;
         ret = recv(sn, buf, size);
         return ret;
      }
      break;
   case SOCK_CLOSE_WAIT:
#ifdef _LOOPBACK_DEBUG_
      // printf("%d:CloseWait\r\n",sn);
#endif
      if ((ret = disconnect(sn)) != SOCK_OK)
         return -99;
      return -10;
#ifdef _LOOPBACK_DEBUG_
      printf("%d:Socket Closed\r\n", sn);
#endif
      break;
   case SOCK_INIT:
#ifdef _LOOPBACK_DEBUG_
      printf("%d:Listen, TCP server loopback, port [%d]\r\n", sn, port);
#endif
      if ((ret = listen(sn)) != SOCK_OK)
         return -99;
      return -10;
      break;
   case SOCK_CLOSED:
#ifdef _LOOPBACK_DEBUG_
      // printf("%d:TCP server loopback start\r\n",sn);
#endif
      if ((ret = socket(sn, Sn_MR_TCP, port, 0x00)) != sn)
         return -99;
      return -10;
#ifdef _LOOPBACK_DEBUG_
         // printf("%d:Socket opened\r\n",sn);
#endif
      break;
   default:
      break;
   }
   return 0;
}

// if return less than 0, error occurs in send.
int32_t tcp_data_send(uint8_t sn, uint8_t *buf, uint16_t size, uint16_t port)
{
   int32_t ret;

   switch (getSn_SR(sn))
   {
   case SOCK_ESTABLISHED:
      // data send
      ret = send(sn, buf, size);
      if (ret < 0)
      {
         close(sn);
      }
      return ret;
      break;
   default:
      break;
   }
   return -1;
}
