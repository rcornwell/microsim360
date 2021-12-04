/*
 * microsim360 - Model 2030 console keyboard/printer model 1050.
 *
 * Copyright 2021, Richard Cornwell
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include <stdio.h>
#include <stdint.h>
#ifndef _WIN32
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#define closesocket close
#define SOCKET int
#else
#include <winsock.h>
#endif
#include "device.h"
#include "model1050.h"
#include "xlat.h"

/* Telnet protocol constants - negatives are for init'ing signed char data */

#define TN_IAC          -1                              /* protocol delim */
#define TN_DONT         -2                              /* dont */
#define TN_DO           -3                              /* do */
#define TN_WONT         -4                              /* wont */
#define TN_WILL         -5                              /* will */
#define TN_BRK          -13                             /* break */
#define TN_BIN          0                               /* bin */
#define TN_ECHO         1                               /* echo */
#define TN_SGA          3                               /* sga */
#define TN_LINE         34                              /* line mode */
#define TN_CR           015                             /* carriage return */
#define TN_LF           012                             /* line feed */
#define TN_NUL          000                             /* null */

/* Telnet line states */

#define TNS_NORM        000                             /* normal */
#define TNS_IAC         001                             /* IAC seen */
#define TNS_WILL        002                             /* WILL seen */
#define TNS_WONT        003                             /* WONT seen */
#define TNS_SKIP        004                             /* skip next cmd */
#define TNS_CRPAD       005                             /* CR padding */

static SOCKET    sock;
static SOCKET    cons;
static int       key_buf[256];
static char      out_buf;
static int       out_flg;
static int       out_cr;
static int       in_flg;
static int       in_ptr;
static int       out_ptr;
static int       in_len;
static int       out_len;
static int       home_loop;
static int       attn_flg;
static int       cancel_flg;
static int       eob_flg;
static int       eot_flg;
static int       t_state;
static fd_set    fds_socks;
static SDL_Thread *thrd;
static int       model1050_thrd(void *data);
static int       running = 0;
#ifdef _WIN32
WSADATA wsaData = { 0 };
#endif

void
model1050_init()
{
    int                 flags;
    struct sockaddr_in  locAddr;
    int                 on = 1;

#ifdef _WIN32
	int      i;

	i = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (i != 0) {
		printf("WSAStartup failed: %d\n", i);
		return;
	}
#endif
    FD_ZERO(&fds_socks);
    cons = 0;
    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket Open");
        return;
    }

    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    locAddr.sin_family = AF_INET;
    locAddr.sin_port = htons(3270);
    locAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock, (struct sockaddr *)&locAddr, sizeof(locAddr)) < 0) {
        perror("Socked Bind");
        close(sock);
        return;
    }
    FD_SET(sock, &fds_socks);
    listen(sock, 1);
    printf("socket open\n");
    running = 1;
    thrd = SDL_CreateThread(model1050_thrd, "Console", NULL);
    printf("listern created\n");
} 

void
model1050_out(uint16_t out_char)
{
     char    ch = ebcdic_to_ascii[out_char & 0xff];
     out_buf = ch;
     out_flg = 1;
     printf("Cons: out %02x\n", out_char);
}

void
model1050_in(uint16_t *in_char)
{
   char   ch;
   if (in_flg && in_len > 0) {
       ch = key_buf[out_ptr++];
       *in_char = ascii_to_ebcdic[ch];
       *in_char |= odd_parity[*in_char];
       out_ptr &= 0xff;
       in_len--;
printf("Read %c %d\n", ch, in_len);
   }
}

void
model1050_func(uint16_t *tags, uint8_t *tags_out, uint8_t tags_in, int *t_request)
{
    *t_request = 0;
    *tags_out = 0;
    if (cons > 0) {
       /* Set default tags out */
       *tags_out = 0x10;

       /* If we have reset signal, clear pending input and flags */
       if (tags_in & 0x1) {
          in_flg = 0;
          in_len = 0;
          out_ptr = in_ptr;
          cancel_flg = 0;
          eob_flg = 0;
          home_loop = 0;
       }

       /* Clear attention signal */
       if (tags_in & 0x02) {
           attn_flg = 0;
       }

       /* If Output enabled, if input done, signal ready */
       if ((tags_in & 0x80) != 0) {
           home_loop = 1;
       }

       /* If microshare enabled */
       if ((tags_in & 0x20) != 0) {
           *tags_out |= 0x00;
       }

       /* If proceed set, signal that we can accept input */
       if ((tags_in & 0x50) == 0x50) {
          in_flg = 1;
       }

       /* Check if sending characters and finished */
       if (home_loop != 0 && out_flg == 0 && out_cr == 0) {
           *tags_out |= 0x40;
       }

       /* If we have any input ready flag it as available */
       if (in_len > 0) {
           *tags_out |= 0x40;
       } else {
           /* Input all drained check of cancel or EOB set */
           if (cancel_flg) {
               *tags_out |= 0x80;
           }
           if (eob_flg) {
               *tags_out |= 0x20;
           }
       }

       /* If we have attention signal to CPU */
       if (attn_flg) {
           *tags_out |= 0x02;
       }

       /* If request CR signal to send one */
       if ((tags_in & 0x04) != 0) {
           out_cr = 1;
       }

       /* Check if we need to notify the CPU of anything */
       if ((*tags_out & 0xef) != 0) {
          *t_request = 1;
       }

       printf("Cons %04x %02x %02x %d\n", *tags, tags_in, *tags_out, *t_request);
    }
} 

void
model1050_done()
{

    if (running) {
printf("Kill console\n");
        running = 0;
        SDL_WaitThread(thrd, NULL);
    }
    if (cons > 0)
        closesocket(cons);
    if (sock > 0)
        closesocket(sock);
#ifdef _WIN32
	WSACleanup();
#endif
}


static void
push_char(char in_char) {
    if (in_char == '\033') {
       attn_flg = 1;
printf("Set Attn\n");
    } else if (in_flg) {
       if (in_char == '\03') {
           cancel_flg = 1;
printf("Set Cancel\n");
       } else if (in_char == '\r') {
           eob_flg = 1;
printf("Set EOB\n");
       } else {
           key_buf[in_ptr++] = in_char;
           in_ptr &= 0xff;
           in_len++;
           send(cons, &in_char, 1, 0);
       }
    }
}

static char init_string[] = {
        TN_IAC, TN_WILL, TN_LINE,
        TN_IAC, TN_WILL, TN_SGA,
        TN_IAC, TN_WILL, TN_ECHO,
        TN_IAC, TN_WILL, TN_BIN,
        TN_IAC, TN_DO, TN_BIN
};

int
model1050_thrd(void *data)
{
    static char    bell = '\007';
    char           c;
    int            i, j;
    int            maxfd;
    struct timeval tv = {1,0}; 
    fd_set         read_set, write_set;
    struct sockaddr_in client;
    int            size;
    SOCKET         newsock;
    char           buffer[256];
    int            flags;
        printf("Console started\n");

    while(running) {
        maxfd = sock;
        if (cons > 0 && cons > maxfd)
            maxfd = cons;
        read_set = fds_socks;
        write_set = fds_socks;
        tv.tv_sec = 0;
        tv.tv_usec = 33000;
        i = select(maxfd+1, &read_set, NULL, NULL, &tv);

        /* Do accept on socket. */
        if (FD_ISSET(sock, &read_set)) {
            size = sizeof(client);
            newsock = accept(sock, (struct sockaddr *)&client, &size);
            printf("Accept\n\r");
//            flags = fcntl(newsock, F_GETFL, 0);
 //           fcntl(newsock, F_SETFL, flags | O_NONBLOCK);
            if (cons == 0) {
               printf("Connected\n");
               cons = newsock;
               FD_SET(newsock, &fds_socks);
               send(newsock, init_string, 15, 0);
               in_ptr = 0;
               out_ptr = 0;
               in_len = 0;
               t_state = TNS_NORM;
           } else {
               static char *msg = "Console already connected\n\r";
               send(newsock, msg, sizeof(msg), 0);
               closesocket(newsock);
           } 
        }

        /* Send any data ready to send */
            if (out_flg) {
                send(cons, &out_buf, 1, 0);
                out_flg = 0;
   printf("Write %c\n", out_buf);
                if (out_buf == '\r')
                   send(cons, "\n", 1, 0);
            }
            if (out_cr) {
                send(cons, "\r\n", 2, 0);
                out_cr = 0;
            }

        /* Collect any waiting input */
        if (FD_ISSET(cons, &read_set)) {
            int j, k;
            j = recv(cons, buffer, 256, 0);
   printf("Read returned %d\n", j);
            if (j == 0) {
             printf("Disonnected\n");
               FD_CLR(cons, &fds_socks);
               close(cons);
               cons = -1;
            }
            k = 0;
            while(k < j && in_len < 256) {
               char t;

               t = buffer[k++];
               printf("Recieved %o %o\n", in_len, t & 0377);
               switch(t_state) {
               case TNS_NORM:
                    if (t == TN_IAC)
                       t_state = TNS_IAC;
                    else
                       push_char(t);
                    break;
               case TNS_IAC:
                    if (t == TN_IAC) {
                       push_char(t);
                       t_state = TNS_NORM;
                    } else if (t == TN_BRK) {
                       t_state = TNS_NORM;
                    } else if (t == TN_WILL) {
                       t_state = TNS_WILL;
                    } else if (t == TN_WONT) {
                       t_state = TNS_WONT;
                    } else 
                       t_state = TNS_SKIP;
                    break;
                case TNS_WILL:
                case TNS_WONT:
                case TNS_SKIP:
                    t_state = TNS_NORM;
                    break;
                }
            }
        }
   }
   return 0;
}

