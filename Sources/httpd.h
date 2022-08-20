/*
 * Copyright (c) 2001-2003 Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 *
 * Author: Adam Dunkels <adam@sics.se>
 *
 * This version of the file has been modified by Texas Instruments to offer
 * simple server-side-include (SSI) and Common Gateway Interface (CGI)
 * capability.
 */

#ifndef __HTTPD_H__
#define __HTTPD_H__

////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////

#include "Events.h"
#include "netif/etharp.h"
#include "ethernetif.h"
#include "lwip/opt.h"
#include "lwip/err.h"
#include "lwip/pbuf.h"
#include "fs.h"

///////////////////////////////////////////////////////////////////////////////
// Definitions
///////////////////////////////////////////////////////////////////////////////

#define DHCP_TIMEOUT 10

// Set this to 1 to support CGI
#ifndef LWIP_HTTPD_CGI
#define LWIP_HTTPD_CGI            1
#endif

// Set this to 1 to support SSI (Server-Side-Includes)
#ifndef LWIP_HTTPD_SSI
#define LWIP_HTTPD_SSI            1
#endif

// Set this to 1 to support HTTP POST
#ifndef LWIP_HTTPD_SUPPORT_POST
#define LWIP_HTTPD_SUPPORT_POST   0
#endif

#if LWIP_HTTPD_CGI

/*!
 * Function pointer for a CGI script handler.
 *
 * This function is called each time the HTTPD server is asked for a file
 * whose name was previously registered as a CGI function using a call to
 * http_set_cgi_handler. The iIndex parameter provides the index of the
 * CGI within the ppcURLs array passed to http_set_cgi_handler. Parameters
 * pcParam and pcValue provide access to the parameters provided along with
 * the URI. iNumParams provides a count of the entries in the pcParam and
 * pcValue arrays. Each entry in the pcParam array contains the name of a
 * parameter with the corresponding entry in the pcValue array containing the
 * value for that parameter. Note that pcParam may contain multiple elements
 * with the same name if, for example, a multi-selection list control is used
 * in the form generating the data.
 *
 * The function should return a pointer to a character string which is the
 * path and filename of the response that is to be sent to the connected
 * browser, for example "/thanks.htm" or "/response/error.ssi".
 *
 * The maximum number of parameters that will be passed to this function via
 * iNumParams is defined by LWIP_HTTPD_MAX_CGI_PARAMETERS. Any parameters in the incoming
 * HTTP request above this number will be discarded.
 *
 * Requests intended for use by this CGI mechanism must be sent using the GET
 * method (which encodes all parameters within the URI rather than in a block
 * later in the request). Attempts to use the POST method will result in the
 * request being ignored.
 *
 */
typedef const char *(*tCGIHandler)(int iIndex, int iNumParams, char *pcParam[], char *pcValue[]);

/*!
 * Structure defining the base filename (URL) of a CGI and the associated
 * function which is to be called when that URL is requested.
 */
typedef struct
{
    const char *pcCGIName;
    tCGIHandler pfnCGIHandler;
} tCGI;

void http_set_cgi_handlers(const tCGI *pCGIs, int iNumHandlers);

// The maximum number of parameters that the CGI handler can be sent.
#ifndef LWIP_HTTPD_MAX_CGI_PARAMETERS
#define LWIP_HTTPD_MAX_CGI_PARAMETERS 16
#endif

#endif // LWIP_HTTPD_CGI

#if LWIP_HTTPD_SSI

/*! 
 * LWIP_HTTPD_SSI_MULTIPART==1: SSI handler function is called with 2 more
 * arguments indicating a counter for insert string that are too long to be
 * inserted at once: the SSI handler function must then set 'next_tag_part'
 * which will be passed back to it in the next call. 
 */
#ifndef LWIP_HTTPD_SSI_MULTIPART
#define LWIP_HTTPD_SSI_MULTIPART    0
#endif

/*!
 * Function pointer for the SSI tag handler callback.
 *
 * This function will be called each time the HTTPD server detects a tag of the
 * form <!--#name--> in a .shtml, .ssi or .shtm file where "name" appears as
 * one of the tags supplied to http_set_ssi_handler in the ppcTags array.  The
 * returned insert string, which will be appended after the the string
 * "<!--#name-->" in file sent back to the client,should be written to pointer
 * pcInsert.  iInsertLen contains the size of the buffer pointed to by
 * pcInsert.  The iIndex parameter provides the zero-based index of the tag as
 * found in the ppcTags array and identifies the tag that is to be processed.
 *
 * The handler returns the number of characters written to pcInsert excluding
 * any terminating NULL or a negative number to indicate a failure (tag not
 * recognized, for example).
 *
 * Note that the behavior of this SSI mechanism is somewhat different from the
 * "normal" SSI processing as found in, for example, the Apache web server.  In
 * this case, the inserted text is appended following the SSI tag rather than
 * replacing the tag entirely.  This allows for an implementation that does not
 * require significant additional buffering of output data yet which will still
 * offer usable SSI functionality.  One downside to this approach is when
 * attempting to use SSI within JavaScript.  The SSI tag is structured to
 * resemble an HTML comment but this syntax does not constitute a comment
 * within JavaScript and, hence, leaving the tag in place will result in
 * problems in these cases.  To work around this, any SSI tag which needs to
 * output JavaScript code must do so in an encapsulated way, sending the whole
 * HTML <script>...</script> section as a single include.
 */
typedef u16_t (*tSSIHandler)(int iIndex, char *pcInsert, int iInsertLen
#if LWIP_HTTPD_SSI_MULTIPART
                             , u16_t current_tag_part, u16_t *next_tag_part
#endif // LWIP_HTTPD_SSI_MULTIPART
#if LWIP_HTTPD_FILE_STATE
                             , void *connection_state
#endif // LWIP_HTTPD_FILE_STATE
                             );

void http_set_ssi_handler(tSSIHandler pfnSSIHandler,
                          const char **ppcTags, int iNumTags);

// The maximum length of the string comprising the tag name
#ifndef LWIP_HTTPD_MAX_TAG_NAME_LEN
#define LWIP_HTTPD_MAX_TAG_NAME_LEN 8
#endif

// The maximum length of string that can be returned to replace any given tag
#ifndef LWIP_HTTPD_MAX_TAG_INSERT_LEN
#define LWIP_HTTPD_MAX_TAG_INSERT_LEN 192
#endif

#endif // LWIP_HTTPD_SSI

#if LWIP_HTTPD_SUPPORT_POST

/* These functions must be implemented by the application */

/*!
 * @brief Called when a POST request has been received. The application can decide
 * whether to accept it or not.
 *
 * @param connection Unique connection identifier, valid until httpd_post_end
 *        is called.
 * @param uri The HTTP header URI receiving the POST request.
 * @param http_request The raw HTTP request (the first packet, normally).
 * @param http_request_len Size of 'http_request'.
 * @param content_len Content-Length from HTTP header.
 * @param response_uri Filename of response file, to be filled when denying the
 *        request
 * @param response_uri_len Size of the 'response_uri' buffer.
 * @param post_auto_wnd Set this to 0 to let the callback code handle window
 *        updates by calling 'httpd_post_data_recved' (to throttle rx speed)
 *        default is 1 (httpd handles window updates automatically)
 * @return ERR_OK: Accept the POST request, data may be passed in
 *         another err_t: Deny the POST request, send back 'bad request'.
 */
err_t httpd_post_begin(void *connection, const char *uri, const char *http_request,
                       u16_t http_request_len, int content_len, char *response_uri,
                       u16_t response_uri_len, u8_t *post_auto_wnd);

/*! 
 * @brief Called for each pbuf of data that has been received for a POST.
 * ATTENTION: The application is responsible for freeing the pbufs passed in!
 *
 * @param connection Unique connection identifier.
 * @param p Received data.
 * @return ERR_OK: Data accepted.
 *         another err_t: Data denied, http_post_get_response_uri will be called.
 */
err_t httpd_post_receive_data(void *connection, struct pbuf *p);

/*! 
 * @brief Called when all data is received or when the connection is closed.
 * The application must return the filename/URI of a file to send in response
 * to this POST request. If the response_uri buffer is untouched, a 404
 * response is returned.
 *
 * @param connection Unique connection identifier.
 * @param response_uri Filename of response file, to be filled when denying the request
 * @param response_uri_len Size of the 'response_uri' buffer.
 */
void httpd_post_finished(void *connection, char *response_uri, u16_t response_uri_len);

#ifndef LWIP_HTTPD_POST_MANUAL_WND
#define LWIP_HTTPD_POST_MANUAL_WND  0
#endif

#if LWIP_HTTPD_POST_MANUAL_WND
void httpd_post_data_recved(void *connection, u16_t recved_len);
#endif // LWIP_HTTPD_POST_MANUAL_WND

#endif // LWIP_HTTPD_SUPPORT_POST

#ifndef HTTPD_DEBUG
#define HTTPD_DEBUG         LWIP_DBG_OFF
#endif

// Set this to 1 and add the next line to lwippools.h to use a memp pool
// for allocating struct http_state instead of the heap:
// LWIP_MEMPOOL(HTTPD_STATE, 20, 100, "HTTPD_STATE")
#ifndef HTTPD_USE_MEM_POOL
#define HTTPD_USE_MEM_POOL  0
#endif

// The server port for HTTPD to use
#ifndef HTTPD_SERVER_PORT
#define HTTPD_SERVER_PORT                   80
#endif

// Maximum retries before the connection is aborted/closed.
// - number of times pcb->poll is called -> default is 4*500ms = 2s;
// - reset when pcb->sent is called
#ifndef HTTPD_MAX_RETRIES
#define HTTPD_MAX_RETRIES                   4
#endif

// The poll delay is X*500ms
#ifndef HTTPD_POLL_INTERVAL
#define HTTPD_POLL_INTERVAL                 4
#endif

// Priority for tcp pcbs created by HTTPD (very low by default).
// Lower priorities get killed first when running out of memroy.
#ifndef HTTPD_TCP_PRIO
#define HTTPD_TCP_PRIO                      TCP_PRIO_MIN
#endif

// Set this to 1 to enabled timing each file sent
#ifndef LWIP_HTTPD_TIMING
#define LWIP_HTTPD_TIMING                   0
#endif
#ifndef HTTPD_DEBUG_TIMING
#define HTTPD_DEBUG_TIMING                  LWIP_DBG_OFF
#endif

// Set this to 1 on platforms where strnstr is not available
#ifndef LWIP_HTTPD_STRNSTR_PRIVATE
#define LWIP_HTTPD_STRNSTR_PRIVATE          1
#endif

// Set this to one to show error pages when parsing a request fails instead
// of simply closing the connection.
#ifndef LWIP_HTTPD_SUPPORT_EXTSTATUS
#define LWIP_HTTPD_SUPPORT_EXTSTATUS        0
#endif

// Set this to 0 to drop support for HTTP/0.9 clients (to save some bytes)
#ifndef LWIP_HTTPD_SUPPORT_V09
#define LWIP_HTTPD_SUPPORT_V09              1
#endif

// Set this to 1 to enable HTTP/1.1 persistent connections.
// ATTENTION: If the generated file system includes HTTP headers, these must
// include the "Connection: keep-alive" header (pass argument "-11" to makefsdata).
#ifndef LWIP_HTTPD_SUPPORT_11_KEEPALIVE
#define LWIP_HTTPD_SUPPORT_11_KEEPALIVE     0
#endif

// Set this to 1 to support HTTP request coming in in multiple packets/pbufs
#ifndef LWIP_HTTPD_SUPPORT_REQUESTLIST
#define LWIP_HTTPD_SUPPORT_REQUESTLIST      1
#endif

#if LWIP_HTTPD_SUPPORT_REQUESTLIST
// Number of rx pbufs to enqueue to parse an incoming request (up to the first newline)
#ifndef LWIP_HTTPD_REQ_QUEUELEN
#define LWIP_HTTPD_REQ_QUEUELEN             5
#endif

// Number of (TCP payload-) bytes (in pbufs) to enqueue to parse and incoming
// request (up to the first double-newline)
#ifndef LWIP_HTTPD_REQ_BUFSIZE
#define LWIP_HTTPD_REQ_BUFSIZE              LWIP_HTTPD_MAX_REQ_LENGTH
#endif

// Defines the maximum length of a HTTP request line (up to the first CRLF,
// copied from pbuf into this a global buffer when pbuf- or packet-queues
// are received - otherwise the input pbuf is used directly)
#ifndef LWIP_HTTPD_MAX_REQ_LENGTH
#define LWIP_HTTPD_MAX_REQ_LENGTH           LWIP_MIN(1023, (LWIP_HTTPD_REQ_QUEUELEN * PBUF_POOL_BUFSIZE))
#endif
#endif // LWIP_HTTPD_SUPPORT_REQUESTLIST

// Maximum length of the filename to send as response to a POST request,
// filled in by the application when a POST is finished.
#ifndef LWIP_HTTPD_POST_MAX_RESPONSE_URI_LEN
#define LWIP_HTTPD_POST_MAX_RESPONSE_URI_LEN 63
#endif

// Set this to 0 to not send the SSI tag (default is on, so the tag will
// be sent in the HTML page
#ifndef LWIP_HTTPD_SSI_INCLUDE_TAG
#define LWIP_HTTPD_SSI_INCLUDE_TAG           1
#endif

// Set this to 1 to call tcp_abort when tcp_close fails with memory error.
// This can be used to prevent consuming all memory in situations where the
// HTTP server has low priority compared to other communication.
#ifndef LWIP_HTTPD_ABORT_ON_CLOSE_MEM_ERROR
#define LWIP_HTTPD_ABORT_ON_CLOSE_MEM_ERROR  1
#endif

// Set this to 1 to kill the oldest connection when running out of
// memory for 'struct http_state' or 'struct http_ssi_state'.
// ATTENTION: This puts all connections on a linked list, so may be kind of slow.
#ifndef LWIP_HTTPD_KILL_OLD_ON_CONNECTIONS_EXCEEDED
#define LWIP_HTTPD_KILL_OLD_ON_CONNECTIONS_EXCEEDED 0
#endif

// Minimum length for a valid HTTP/0.9 request: "GET /\r\n" -> 7 bytes
#define MIN_REQ_LEN   7

#define CRLF "\r\n"
#define HTTP11_CONNECTIONKEEPALIVE "Connection: keep-alive"

#if LWIP_HTTPD_SSI
#define LWIP_HTTPD_IS_SSI(hs) ((hs)->ssi)
#else // LWIP_HTTPD_SSI
#define LWIP_HTTPD_IS_SSI(hs) 0
#endif // LWIP_HTTPD_SSI

// These defines check whether tcp_write has to copy data or not

// This was TI's check whether to let TCP copy data or not
// #define HTTP_IS_DATA_VOLATILE(hs) ((hs->file < (char *)0x20000000) ? 0 : TCP_WRITE_FLAG_COPY)
#ifndef HTTP_IS_DATA_VOLATILE
#if LWIP_HTTPD_SSI
// Copy for SSI files, no copy for non-SSI files
#define HTTP_IS_DATA_VOLATILE(hs)   ((hs)->ssi ? TCP_WRITE_FLAG_COPY : 0)
#else // LWIP_HTTPD_SSI
// Default: don't copy if the data is sent from file-system directly
#define HTTP_IS_DATA_VOLATILE(hs) (((hs->file != NULL) && (hs->handle != NULL) && (hs->file == \
    (char*)hs->handle->data + hs->handle->len - hs->left)) \
    ? 0 : TCP_WRITE_FLAG_COPY)
#endif /* LWIP_HTTPD_SSI */
#endif

// Default: headers are sent from ROM
#ifndef HTTP_IS_HDR_VOLATILE
#define HTTP_IS_HDR_VOLATILE(hs, ptr) 0
#endif

#if LWIP_HTTPD_SSI
// Default: Tags are sent from struct http_state and are therefore volatile
#ifndef HTTP_IS_TAG_VOLATILE
#define HTTP_IS_TAG_VOLATILE(ptr) TCP_WRITE_FLAG_COPY
#endif
#endif // LWIP_HTTPD_SSI

// Return values for http_send_*()
#define HTTP_DATA_TO_SEND_BREAK    2
#define HTTP_DATA_TO_SEND_CONTINUE 1
#define HTTP_NO_DATA_TO_SEND       0

#if HTTPD_USE_MEM_POOL
#define HTTP_ALLOC_SSI_STATE()  (struct http_ssi_state *)memp_malloc(MEMP_HTTPD_SSI_STATE)
#define HTTP_ALLOC_HTTP_STATE() (struct http_state *)memp_malloc(MEMP_HTTPD_STATE)
#else // HTTPD_USE_MEM_POOL
#define HTTP_ALLOC_SSI_STATE()  (struct http_ssi_state *)mem_malloc(sizeof(struct http_ssi_state))
#define HTTP_ALLOC_HTTP_STATE() (struct http_state *)mem_malloc(sizeof(struct http_state))
#endif // HTTPD_USE_MEM_POOL

typedef struct
{
    const char *name;
    u8_t shtml;
} default_filename;

const default_filename g_psDefaultFilenames[] = {
    {"/index.shtml", 1 },
    {"/index.ssi",   1 },
    {"/index.shtm",  1 },
    {"/index.html",  0 },
    {"/index.htm",   0 }
};

#define NUM_DEFAULT_FILENAMES (sizeof(g_psDefaultFilenames) /   \
    sizeof(default_filename))

#if LWIP_HTTPD_SUPPORT_REQUESTLIST
// HTTP request is copied here from pbufs for simple parsing
static char httpd_req_buf[LWIP_HTTPD_MAX_REQ_LENGTH+1];
char *evitaAvisoDeNaoUtilizacao = httpd_req_buf;
#endif // LWIP_HTTPD_SUPPORT_REQUESTLIST

#if LWIP_HTTPD_SUPPORT_POST
// Filename for response file to send when POST is finished
static char http_post_response_filename[LWIP_HTTPD_POST_MAX_RESPONSE_URI_LEN+1];
#endif // LWIP_HTTPD_SUPPORT_POST

#if LWIP_HTTPD_DYNAMIC_HEADERS
// The number of individual strings that comprise the headers sent before each requested file.
#define NUM_FILE_HDR_STRINGS 3
#endif // LWIP_HTTPD_DYNAMIC_HEADERS

#if LWIP_HTTPD_SSI

#define HTTPD_LAST_TAG_PART 0xFFFF

enum tag_check_state {
    TAG_NONE,       // Not processing an SSI tag
    TAG_LEADIN,     // Tag lead in "<!--#" being processed
    TAG_FOUND,      // Tag name being read, looking for lead-out start
    TAG_LEADOUT,    // Tag lead out "-->" being processed
    TAG_SENDING     // Sending tag replacement string
};

struct http_ssi_state {
    const char *parsed;     // Pointer to the first unparsed byte in buf.
#if !LWIP_HTTPD_SSI_INCLUDE_TAG
    const char *tag_started; // Pointer to the first opening '<' of the tag.
#endif // !LWIP_HTTPD_SSI_INCLUDE_TAG
    const char *tag_end;    // Pointer to char after the closing '>' of the tag.
    u32_t parse_left; // Number of unparsed bytes in buf.
    u16_t tag_index;   // Counter used by tag parsing state machine
    u16_t tag_insert_len; // Length of insert in string tag_insert
#if LWIP_HTTPD_SSI_MULTIPART
    u16_t tag_part; // Counter passed to and changed by tag insertion function to insert multiple times
#endif // LWIP_HTTPD_SSI_MULTIPART
    u8_t tag_name_len; // Length of the tag name in string tag_name
    char tag_name[LWIP_HTTPD_MAX_TAG_NAME_LEN + 1]; // Last tag name extracted
    char tag_insert[LWIP_HTTPD_MAX_TAG_INSERT_LEN + 1]; // Insert string for tag_name
    enum tag_check_state tag_state; // State of the tag processor
};
#endif // LWIP_HTTPD_SSI

struct http_state {
#if LWIP_HTTPD_KILL_OLD_ON_CONNECTIONS_EXCEEDED
    struct http_state *next;
#endif // LWIP_HTTPD_KILL_OLD_ON_CONNECTIONS_EXCEEDED
    struct fs_file file_handle;
    struct fs_file *handle;
    char *file;       // Pointer to first unsent byte in buf.

    struct tcp_pcb *pcb;
#if LWIP_HTTPD_SUPPORT_REQUESTLIST
    struct pbuf *req;
#endif // LWIP_HTTPD_SUPPORT_REQUESTLIST

#if LWIP_HTTPD_DYNAMIC_FILE_READ
    char *buf;        // File read buffer.
    int buf_len;      // Size of file read buffer, buf.
#endif // LWIP_HTTPD_DYNAMIC_FILE_READ
    u32_t left;       // Number of unsent bytes in buf.
    u8_t retries;
#if LWIP_HTTPD_SUPPORT_11_KEEPALIVE
    u8_t keepalive;
#endif // LWIP_HTTPD_SUPPORT_11_KEEPALIVE
#if LWIP_HTTPD_SSI
    struct http_ssi_state *ssi;
#endif // LWIP_HTTPD_SSI
#if LWIP_HTTPD_CGI
    char *params[LWIP_HTTPD_MAX_CGI_PARAMETERS]; // Params extracted from the request URI
    char *param_vals[LWIP_HTTPD_MAX_CGI_PARAMETERS]; // Values for each extracted param
#endif // LWIP_HTTPD_CGI
#if LWIP_HTTPD_DYNAMIC_HEADERS
    const char *hdrs[NUM_FILE_HDR_STRINGS]; // HTTP headers to be sent.
    u16_t hdr_pos;     // The position of the first unsent header byte in the current string
    u16_t hdr_index;   // The index of the hdr string currently being sent.
#endif // LWIP_HTTPD_DYNAMIC_HEADERS
#if LWIP_HTTPD_TIMING
    u32_t time_started;
#endif // LWIP_HTTPD_TIMING
#if LWIP_HTTPD_SUPPORT_POST
    u32_t post_content_len_left;
#if LWIP_HTTPD_POST_MANUAL_WND
    u32_t unrecved_bytes;
    u8_t no_auto_wnd;
    u8_t post_finished;
#endif // LWIP_HTTPD_POST_MANUAL_WND
#endif // LWIP_HTTPD_SUPPORT_POST
};

void httpd_init(void);
void httpd_server_init(void);

#endif // __HTTPD_H__
