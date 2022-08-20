/*
 * httpd.c
 *
 *  Created on: 11/01/2022
 *      Author: 89247469
 */

#include "gpio1.h"
#include "httpd_structs.h"
#include "lwip/api.h"
#include "lwip/tcpip.h"
#include "lwip/udp.h"
#include "lwip/dhcp.h"
#include "lwip/opt.h"
#include "lwip/tcp.h"
#include "lwip/mem.h"
#include "lwip/raw.h"
#include "lwip/icmp.h"
#include "lwip/netif.h"
#include "lwip/sys.h"
#include "lwip/timers.h"
#include "lwip/inet_chksum.h"
#include "lwip/init.h"
#include "lwip/debug.h"
#include "lwip/stats.h"
#include <string.h>
#include <stdio.h>
#include "ff.h"
#include "Events.h"
#include "httpd.h"
#include "sdcard.h"

volatile bool cardInserted = false;
volatile bool cartaoSSI = false;
volatile bool cartaoCGI = false;
volatile bool cartaoRELAY = false;

static err_t http_close_conn(struct tcp_pcb *pcb, struct http_state *hs);
static err_t http_close_or_abort_conn(struct tcp_pcb *pcb, struct http_state *hs, u8_t abort_conn);
static err_t http_find_file(struct http_state *hs, const char *uri, int is_09);
static err_t http_init_file(struct http_state *hs, struct fs_file *file, int is_09, const char *uri, u8_t tag_check);
static err_t http_poll(void *arg, struct tcp_pcb *pcb);

#if LWIP_HTTPD_FS_ASYNC_READ
static void http_continue(void *connection);
#endif // LWIP_HTTPD_FS_ASYNC_READ

#if LWIP_HTTPD_SSI
// SSI insert handler function pointer.
tSSIHandler g_pfnSSIHandler = NULL;
int g_iNumTags = 0;
const char **g_ppcTags = NULL;

#define LEN_TAG_LEAD_IN 5
const char * const g_pcTagLeadIn = "<!--#";

#define LEN_TAG_LEAD_OUT 3
const char * const g_pcTagLeadOut = "-->";
#endif // LWIP_HTTPD_SSI

#if LWIP_HTTPD_CGI
// CGI handler information
const tCGI *g_pCGIs;
int g_iNumCGIs;
#endif // LWIP_HTTPD_CGI

#if LWIP_HTTPD_KILL_OLD_ON_CONNECTIONS_EXCEEDED
// global list of active HTTP connections, use to kill the oldest when running out of memory
static struct http_state *http_connections;
#endif // LWIP_HTTPD_KILL_OLD_ON_CONNECTIONS_EXCEEDED

#if LWIP_HTTPD_STRNSTR_PRIVATE
/* Like strstr but does not need 'buffer' to be NULL-terminated */
static char* strnstr(const char* buffer, const char* token, size_t n)
{
    const char* p;
    int tokenlen = (int)strlen(token);
    if (tokenlen == 0)
    {
        return (char *)buffer;
    }
    for (p = buffer; *p && (p + tokenlen <= buffer + n); p++)
    {
        if ((*p == *token) && (strncmp(p, token, tokenlen) == 0))
        {
            return (char *)p;
        }
    }
    return NULL;
}
#endif // LWIP_HTTPD_STRNSTR_PRIVATE

#if LWIP_HTTPD_KILL_OLD_ON_CONNECTIONS_EXCEEDED
static void http_kill_oldest_connection(u8_t ssi_required)
{
    struct http_state *hs = http_connections;
    struct http_state *hs_free_next = NULL;
    while(hs && hs->next)
    {
        if (ssi_required)
        {
            if (hs->next->ssi != NULL)
            {
                hs_free_next = hs;
            }
        }
        else
        {
            hs_free_next = hs;
        }
        hs = hs->next;
    }
    if (hs_free_next != NULL)
    {
        LWIP_ASSERT("hs_free_next->next != NULL", hs_free_next->next != NULL);
        LWIP_ASSERT("hs_free_next->next->pcb != NULL", hs_free_next->next->pcb != NULL);
        // send RST when killing a connection because of memory shortage
        // this also unlinks the http_state from the list
        http_close_or_abort_conn(hs_free_next->next->pcb, hs_free_next->next, 1);
    }
}
#endif // LWIP_HTTPD_KILL_OLD_ON_CONNECTIONS_EXCEEDED

#if LWIP_HTTPD_SSI
/* Allocate as struct http_ssi_state. */
static struct http_ssi_state* http_ssi_state_alloc(void)
{
    struct http_ssi_state *ret = HTTP_ALLOC_SSI_STATE();
#if LWIP_HTTPD_KILL_OLD_ON_CONNECTIONS_EXCEEDED
    if (ret == NULL)
    {
        http_kill_oldest_connection(1);
        ret = HTTP_ALLOC_SSI_STATE();
    }
#endif // LWIP_HTTPD_KILL_OLD_ON_CONNECTIONS_EXCEEDED
    if (ret != NULL)
    {
        memset(ret, 0, sizeof(struct http_ssi_state));
    }
    return ret;
}

/* Free a struct http_ssi_state. */
static void http_ssi_state_free(struct http_ssi_state *ssi)
{
    if (ssi != NULL)
    {
#if HTTPD_USE_MEM_POOL
        memp_free(MEMP_HTTPD_SSI_STATE, ssi);
#else // HTTPD_USE_MEM_POOL
        mem_free(ssi);
#endif // HTTPD_USE_MEM_POOL
    }
}
#endif // LWIP_HTTPD_SSI

/* Initialize a struct http_state.*/
static void http_state_init(struct http_state* hs)
{
    // Initialize the structure.
    memset(hs, 0, sizeof(struct http_state));

#if LWIP_HTTPD_DYNAMIC_HEADERS
    // Indicate that the headers are not yet valid
    hs->hdr_index = NUM_FILE_HDR_STRINGS;
#endif // LWIP_HTTPD_DYNAMIC_HEADERS
}

/* Allocate a struct http_state. */
static struct http_state* http_state_alloc(void)
{
    struct http_state *ret = HTTP_ALLOC_HTTP_STATE();
#if LWIP_HTTPD_KILL_OLD_ON_CONNECTIONS_EXCEEDED
    if (ret == NULL)
    {
        http_kill_oldest_connection(0);
        ret = HTTP_ALLOC_HTTP_STATE();
    }
#endif // LWIP_HTTPD_KILL_OLD_ON_CONNECTIONS_EXCEEDED
    if (ret != NULL)
    {
        http_state_init(ret);
#if LWIP_HTTPD_KILL_OLD_ON_CONNECTIONS_EXCEEDED
        // add the connection to the list
        if (http_connections == NULL)
        {
            http_connections = ret;
        }
        else
        {
            struct http_state *last;
            for(last = http_connections; last->next != NULL; last = last->next);
            LWIP_ASSERT("last != NULL", last != NULL);
            last->next = ret;
        }
#endif // LWIP_HTTPD_KILL_OLD_ON_CONNECTIONS_EXCEEDED
    }
    return ret;
}

/* Free a struct http_state. Also frees the file data if dynamic. */
static void http_state_eof(struct http_state *hs)
{
    if(hs->handle)
    {
#if LWIP_HTTPD_TIMING
        u32_t ms_needed = sys_now() - hs->time_started;
        u32_t needed = LWIP_MAX(1, (ms_needed/100));
        LWIP_DEBUGF(HTTPD_DEBUG_TIMING, ("httpd: needed %"U32_F" ms to send file of %d bytes -> %"U32_F" bytes/sec\r\n",
        ms_needed, hs->handle->len, ((((u32_t)hs->handle->len) * 10) / needed)));
#endif // LWIP_HTTPD_TIMING
        fs_close(hs->handle);
        hs->handle = NULL;
    }
#if LWIP_HTTPD_DYNAMIC_FILE_READ
    if (hs->buf != NULL)
    {
        mem_free(hs->buf);
        hs->buf = NULL;
    }
#endif // LWIP_HTTPD_DYNAMIC_FILE_READ

#if LWIP_HTTPD_SSI
    if (hs->ssi)
    {
        http_ssi_state_free(hs->ssi);
        hs->ssi = NULL;
    }
#endif // LWIP_HTTPD_SSI
}

/* Free a struct http_state. Also frees the file data if dynamic. */
static void http_state_free(struct http_state *hs)
{
    if (hs != NULL)
    {
        http_state_eof(hs);
#if LWIP_HTTPD_KILL_OLD_ON_CONNECTIONS_EXCEEDED
        // take the connection off the list
        if (http_connections)
        {
            if (http_connections == hs)
            {
                http_connections = hs->next;
            }
            else
            {
                struct http_state *last;
                for(last = http_connections; last->next != NULL; last = last->next)
                {
                    if (last->next == hs)
                    {
                        last->next = hs->next;
                        break;
                    }
                }
            }
        }
#endif // LWIP_HTTPD_KILL_OLD_ON_CONNECTIONS_EXCEEDED
#if HTTPD_USE_MEM_POOL
        memp_free(MEMP_HTTPD_STATE, hs);
#else // HTTPD_USE_MEM_POOL
        mem_free(hs);
#endif // HTTPD_USE_MEM_POOL
    }
}

/*!
 * @brief Call tcp_write() in a loop trying smaller and smaller length
 *
 * @param pcb tcp_pcb to send
 * @param ptr Data to send
 * @param length Length of data to send (in/out: on return, contains the
 *        amount of data sent)
 * @param apiflags directly passed to tcp_write
 * @return the return value of tcp_write
 */
static err_t http_write(struct tcp_pcb *pcb, const void* ptr, u16_t *length, u8_t apiflags)
{
    u16_t len;
    err_t err;
    LWIP_ASSERT("length != NULL", length != NULL);
    len = *length;
    if (len == 0)
    {
        return ERR_OK;
    }
    do {
        LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("Trying go send %d bytes\r\n", len));
        err = tcp_write(pcb, ptr, len, apiflags);
        if (err == ERR_MEM)
        {
            if ((tcp_sndbuf(pcb) == 0) ||
                    (tcp_sndqueuelen(pcb) >= TCP_SND_QUEUELEN))
            {
                // no need to try smaller sizes
                len = 1;
            }
            else
            {
                len /= 2;
            }
            LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE,
            ("Send failed, trying less (%d bytes)\r\n", len));
        }
    } while ((err == ERR_MEM) && (len > 1));

    if (err == ERR_OK)
    {
        LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("Sent %d bytes\r\n", len));
    }
    else
    {
        LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("Send failed with err %d (\"%s\")\r\n", err, lwip_strerr(err)));
    }

    *length = len;
    return err;
}

/*!
 * @brief The connection shall be actively closed (using RST to close from fault states).
 * Reset the sent- and recv-callbacks.
 *
 * @param pcb the tcp pcb to reset callbacks
 * @param hs connection state to free
 */
static err_t http_close_or_abort_conn(struct tcp_pcb *pcb, struct http_state *hs, u8_t abort_conn)
{
    err_t err;
    LWIP_DEBUGF(HTTPD_DEBUG, ("Closing connection %p\r\n", (void*)pcb));

#if LWIP_HTTPD_SUPPORT_POST
    if (hs != NULL)
    {
        if ((hs->post_content_len_left != 0)
#if LWIP_HTTPD_POST_MANUAL_WND
                || ((hs->no_auto_wnd != 0) && (hs->unrecved_bytes != 0))
#endif // LWIP_HTTPD_POST_MANUAL_WND
                )
        {
            // make sure the post code knows that the connection is closed
            http_post_response_filename[0] = 0;
            httpd_post_finished(hs, http_post_response_filename, LWIP_HTTPD_POST_MAX_RESPONSE_URI_LEN);
        }
    }
#endif // LWIP_HTTPD_SUPPORT_POST


    tcp_arg(pcb, NULL);
    tcp_recv(pcb, NULL);
    tcp_err(pcb, NULL);
    tcp_poll(pcb, NULL, 0);
    tcp_sent(pcb, NULL);
    if (hs != NULL)
    {
        http_state_free(hs);
    }

    if (abort_conn)
    {
        tcp_abort(pcb);
        return ERR_OK;
    }
    err = tcp_close(pcb);
    if (err != ERR_OK)
    {
        LWIP_DEBUGF(HTTPD_DEBUG, ("Error %d closing %p\r\n", err, (void*)pcb));
        // error closing, try again later in poll
        tcp_poll(pcb, http_poll, HTTPD_POLL_INTERVAL);
    }
    return err;
}

/*!
 * @brief The connection shall be actively closed.
 * Reset the sent- and recv-callbacks.
 *
 * @param pcb the tcp pcb to reset callbacks
 * @param hs connection state to free
 */
static err_t http_close_conn(struct tcp_pcb *pcb, struct http_state *hs)
{
    return http_close_or_abort_conn(pcb, hs, 0);
}

/* End of file: either close the connection (Connection: close)
 * or close the file (Connection: keep-alive)
 */
static void http_eof(struct tcp_pcb *pcb, struct http_state *hs)
{
    // HTTP/1.1 persistent connection? (Not supported for SSI)
#if LWIP_HTTPD_SUPPORT_11_KEEPALIVE
    if (hs->keepalive && !LWIP_HTTPD_IS_SSI(hs))
    {
        http_state_eof(hs);
        http_state_init(hs);
        hs->keepalive = 1;
    }
    else
#endif // LWIP_HTTPD_SUPPORT_11_KEEPALIVE
    {
        http_close_conn(pcb, hs);
    }
}

#if LWIP_HTTPD_CGI
/*!
 * @brief Extract URI parameters from the parameter-part of an URI in the form
 * "test.cgi?x=y" @todo: better explanation!
 * Pointers to the parameters are stored in hs->param_vals.
 *
 * @param hs http connection state
 * @param params pointer to the NULL-terminated parameter string from the URI
 * @return number of parameters extracted
 */
static int extract_uri_parameters(struct http_state *hs, char *params)
{
    char *pair;
    char *equals;
    int loop;

    // If we have no parameters at all, return immediately.
    if(!params || (params[0] == '\0'))
    {
        return(0);
    }

    // Get a pointer to our first parameter
    pair = params;

    // Parse up to LWIP_HTTPD_MAX_CGI_PARAMETERS from the passed string
    // and ignore the remainder (if any)
    for(loop = 0; (loop < LWIP_HTTPD_MAX_CGI_PARAMETERS) && pair; loop++)
    {
        // Save the name of the parameter
        hs->params[loop] = pair;

        // Remember the start of this name=value pair
        equals = pair;

        // Find the start of the next name=value pair and replace the delimiter
        // with a 0 to terminate the previous pair string.
        pair = strchr(pair, '&');
        if(pair)
        {
            *pair = '\0';
            pair++;
        }
        else
        {
            // We didn't find a new parameter so find the end of the URI
            // and replace the space with a '\0'
            pair = strchr(equals, ' ');
            if(pair)
            {
                *pair = '\0';
            }

            // Revert to NULL so that we exit the loop as expected.
            pair = NULL;
        }

        // Now find the '=' in the previous pair, replace it with '\0'
        // and save the parameter value string.
        equals = strchr(equals, '=');
        if(equals)
        {
            *equals = '\0';
            hs->param_vals[loop] = equals + 1;
        }
        else
        {
            hs->param_vals[loop] = NULL;
        }
    }

    return loop;
}
#endif // LWIP_HTTPD_CGI

#if LWIP_HTTPD_SSI
/*!
 * @brief Insert a tag (found in an shtml in the form of "<!--#tagname-->" into the file.
 * The tag's name is stored in ssi->tag_name (NULL-terminated), the replacement
 * should be written to hs->tag_insert (up to a length of LWIP_HTTPD_MAX_TAG_INSERT_LEN).
 * The amount of data written is stored to ssi->tag_insert_len.
 *
 * @todo: return tag_insert_len - maybe it can be removed from struct http_state?
 *
 * @param hs http connection state
 */
static void get_tag_insert(struct http_state *hs)
{
    int loop;
    size_t len;
    struct http_ssi_state *ssi;
    LWIP_ASSERT("hs != NULL", hs != NULL);
    ssi = hs->ssi;
    LWIP_ASSERT("ssi != NULL", ssi != NULL);

#if LWIP_HTTPD_SSI_MULTIPART
    u16_t current_tag_part = ssi->tag_part;
    ssi->tag_part = HTTPD_LAST_TAG_PART;
#endif // LWIP_HTTPD_SSI_MULTIPART

    if(g_pfnSSIHandler && g_ppcTags && g_iNumTags)
    {
        // Find this tag in the list we have been provided.
        for(loop = 0; loop < g_iNumTags; loop++)
        {
            if(strcmp(ssi->tag_name, g_ppcTags[loop]) == 0)
            {
                ssi->tag_insert_len = g_pfnSSIHandler(loop, ssi->tag_insert,
                LWIP_HTTPD_MAX_TAG_INSERT_LEN

#if LWIP_HTTPD_SSI_MULTIPART
                , current_tag_part, &ssi->tag_part
#endif // LWIP_HTTPD_SSI_MULTIPART
#if LWIP_HTTPD_FILE_STATE
                , hs->handle->state
#endif // LWIP_HTTPD_FILE_STATE
                );
                return;
            }
        }
    }

    // If we drop out, we were asked to serve a page which contains tags that
    // we don't have a handler for. Merely echo back the tags with an error marker.
#define UNKNOWN_TAG1_TEXT "<b>***UNKNOWN TAG "
#define UNKNOWN_TAG1_LEN  18
#define UNKNOWN_TAG2_TEXT "***</b>"
#define UNKNOWN_TAG2_LEN  7
    len = LWIP_MIN(strlen(ssi->tag_name),
    LWIP_HTTPD_MAX_TAG_INSERT_LEN - (UNKNOWN_TAG1_LEN + UNKNOWN_TAG2_LEN));
    MEMCPY(ssi->tag_insert, UNKNOWN_TAG1_TEXT, UNKNOWN_TAG1_LEN);
    MEMCPY(&ssi->tag_insert[UNKNOWN_TAG1_LEN], ssi->tag_name, len);
    MEMCPY(&ssi->tag_insert[UNKNOWN_TAG1_LEN + len], UNKNOWN_TAG2_TEXT, UNKNOWN_TAG2_LEN);
    ssi->tag_insert[UNKNOWN_TAG1_LEN + len + UNKNOWN_TAG2_LEN] = 0;

    len = strlen(ssi->tag_insert);
    LWIP_ASSERT("len <= 0xffff", len <= 0xffff);
    ssi->tag_insert_len = (u16_t)len;
}
#endif // LWIP_HTTPD_SSI

#if LWIP_HTTPD_DYNAMIC_HEADERS
/*
 * Generate the relevant HTTP headers for the given filename
 * and write them into the supplied buffer.
 */
static void get_http_headers(struct http_state *pState, char *pszURI)
{
    unsigned int iLoop;
    char *pszWork;
    char *pszExt;
    char *pszVars;

    // Ensure that we initialize the loop counter. */
    iLoop = 0;

    // In all cases, the second header we send is the server identification so set it here.
    pState->hdrs[1] = g_psHTTPHeaderStrings[HTTP_HDR_SERVER];

    // Is this a normal file or the special case we use to send back the
    // default "404: Page not found" response?
    if (pszURI == NULL)
    {
        pState->hdrs[0] = g_psHTTPHeaderStrings[HTTP_HDR_NOT_FOUND];
        pState->hdrs[2] = g_psHTTPHeaderStrings[DEFAULT_404_HTML];

        // Set up to send the first header string.
        pState->hdr_index = 0;
        pState->hdr_pos = 0;
        return;
    }
    else
    {
        //  We are dealing with a particular filename. Look for one other
        // special case.  We assume that any filename with "404" in it must be
        // indicative of a 404 server error whereas all other files require
        // the 200 OK header.
        if (strstr(pszURI, "404"))
        {
            pState->hdrs[0] = g_psHTTPHeaderStrings[HTTP_HDR_NOT_FOUND];
        }
        else if (strstr(pszURI, "400"))
        {
            pState->hdrs[0] = g_psHTTPHeaderStrings[HTTP_HDR_BAD_REQUEST];
        }
        else if (strstr(pszURI, "501"))
        {
            pState->hdrs[0] = g_psHTTPHeaderStrings[HTTP_HDR_NOT_IMPL];
        }
        else
        {
            pState->hdrs[0] = g_psHTTPHeaderStrings[HTTP_HDR_OK];
        }

        // Determine if the URI has any variables and, if so, temporarily remove them.
        pszVars = strchr(pszURI, '?');
        if(pszVars)
        {
            *pszVars = '\0';
        }

        // Get a pointer to the file extension.
        // We find this by looking for the last occurrence of "." in the filename passed.
        pszExt = NULL;
        pszWork = strchr(pszURI, '.');
        while(pszWork)
        {
            pszExt = pszWork + 1;
            pszWork = strchr(pszExt, '.');
        }

        // Now determine the content type and add the relevant header for that.
        for(iLoop = 0; (iLoop < NUM_HTTP_HEADERS) && pszExt; iLoop++)
        {
            // Have we found a matching extension?
            if(!strcmp(g_psHTTPHeaders[iLoop].extension, pszExt))
            {
                pState->hdrs[2] =
                g_psHTTPHeaderStrings[g_psHTTPHeaders[iLoop].headerIndex];
                break;
            }
        }

        // Reinstate the parameter marker if there was one in the original URI.
        if(pszVars)
        {
            *pszVars = '?';
        }
    }

    // Does the URL passed have any file extension?  If not, we assume it
    // is a special-case URL used for control state notification and we do
    // not send any HTTP headers with the response.
    if(!pszExt)
    {
        // Force the header index to a value indicating that all headers have already been sent.
        pState->hdr_index = NUM_FILE_HDR_STRINGS;
    }
    else
    {
        // Did we find a matching extension?
        if(iLoop == NUM_HTTP_HEADERS)
        {
            // No - use the default, plain text file type.
            pState->hdrs[2] = g_psHTTPHeaderStrings[HTTP_HDR_DEFAULT_TYPE];
        }

        // Set up to send the first header string.
        pState->hdr_index = 0;
        pState->hdr_pos = 0;
    }
}

/*!
 * @brief Sub-function of http_send(): send dynamic headers
 *
 * @returns: - HTTP_NO_DATA_TO_SEND: no new data has been enqueued
 *           - HTTP_DATA_TO_SEND_CONTINUE: continue with sending HTTP body
 *           - HTTP_DATA_TO_SEND_BREAK: data has been enqueued, headers pending,
 *                                      so don't send HTTP body yet
 */
static u8_t http_send_headers(struct tcp_pcb *pcb, struct http_state *hs)
{
    err_t err;
    u16_t len;
    u8_t data_to_send = HTTP_NO_DATA_TO_SEND;
    u16_t hdrlen, sendlen;

    // How much data can we send?
    len = tcp_sndbuf(pcb);
    sendlen = len;

    while(len && (hs->hdr_index < NUM_FILE_HDR_STRINGS) && sendlen)
    {
        const void *ptr;
        u16_t old_sendlen;
        // How much do we have to send from the current header?
        hdrlen = (u16_t)strlen(hs->hdrs[hs->hdr_index]);

        // How much of this can we send?
        sendlen = (len < (hdrlen - hs->hdr_pos)) ? len : (hdrlen - hs->hdr_pos);

        // Send this amount of data or as much as we can given memory constraints.
        ptr = (const void *)(hs->hdrs[hs->hdr_index] + hs->hdr_pos);
        old_sendlen = sendlen;
        err = http_write(pcb, ptr, &sendlen, HTTP_IS_HDR_VOLATILE(hs, ptr));
        if ((err == ERR_OK) && (old_sendlen != sendlen))
        {
            // Remember that we added some more data to be transmitted.
            data_to_send = HTTP_DATA_TO_SEND_CONTINUE;
        }
        else if (err != ERR_OK)
        {
            // special case: http_write does not try to send 1 byte
            sendlen = 0;
        }

        // Fix up the header position for the next time round.
        hs->hdr_pos += sendlen;
        len -= sendlen;

        // Have we finished sending this string?
        if(hs->hdr_pos == hdrlen)
        {
            // Yes - move on to the next one
            hs->hdr_index++;
            hs->hdr_pos = 0;
        }
    }
    // If we get here and there are still header bytes to send, we send
    // the header information we just wrote immediately. If there are no
    // more headers to send, but we do have file data to send, drop through
    // to try to send some file data too.
    if((hs->hdr_index < NUM_FILE_HDR_STRINGS) || !hs->file)
    {
        LWIP_DEBUGF(HTTPD_DEBUG, ("tcp_output\r\n"));
        return HTTP_DATA_TO_SEND_BREAK;
    }
    return data_to_send;
}
#endif // LWIP_HTTPD_DYNAMIC_HEADERS

/*!
 * @brief Sub-function of http_send(): end-of-file (or block) is reached,
 * either close the file or read the next block (if supported).
 *
 * @returns: 0 if the file is finished or no data has been read
 *           1 if the file is not finished and data has been read
 */
static u8_t http_check_eof(struct tcp_pcb *pcb, struct http_state *hs)
{
#if LWIP_HTTPD_DYNAMIC_FILE_READ
    int count;
#endif // LWIP_HTTPD_DYNAMIC_FILE_READ

    // Do we have a valid file handle?
    if (hs->handle == NULL)
    {
        // No - close the connection.
        http_eof(pcb, hs);
        return 0;
    }
    if (fs_bytes_left(hs->handle) <= 0)
    {
        // We reached the end of the file so this request is done.
        LWIP_DEBUGF(HTTPD_DEBUG, ("End of file.\r\n"));
        http_eof(pcb, hs);
        return 0;
    }
#if LWIP_HTTPD_DYNAMIC_FILE_READ
    // Do we already have a send buffer allocated?
    if(hs->buf)
    {
        // Yes - get the length of the buffer
        count = hs->buf_len;
    }
    else
    {
        // We don't have a send buffer so allocate one up to 2mss bytes long.
        count = 2 * tcp_mss(pcb);
        do {
            hs->buf = (char*)mem_malloc((mem_size_t)count);
            if (hs->buf != NULL)
            {
                hs->buf_len = count;
                break;
            }
            count = count / 2;
        } while (count > 100);

        // Did we get a send buffer? If not, return immediately.
        if (hs->buf == NULL)
        {
            LWIP_DEBUGF(HTTPD_DEBUG, ("No buff\r\n"));
            return 0;
        }
    }

    // Read a block of data from the file.
    LWIP_DEBUGF(HTTPD_DEBUG, ("Trying to read %d bytes.\r\n", count));

#if LWIP_HTTPD_FS_ASYNC_READ
    count = fs_read_async(hs->handle, hs->buf, count, http_continue, hs);
#else // LWIP_HTTPD_FS_ASYNC_READ
    count = fs_read(hs->handle, hs->buf, count);
#endif // LWIP_HTTPD_FS_ASYNC_READ
    if (count < 0
    {
        if (count == FS_READ_DELAYED)
        {
            // Delayed read, wait for FS to unblock us
            return 0;
        }
        // We reached the end of the file so this request is done.
        // @todo: don't close here for HTTP/1.1? */
        LWIP_DEBUGF(HTTPD_DEBUG, ("End of file.\r\n"));
        http_eof(pcb, hs);
        return 0;
    }

    // Set up to send the block of data we just read
    LWIP_DEBUGF(HTTPD_DEBUG, ("Read %d bytes.\r\n", count));
    hs->left = count;
    hs->file = hs->buf;
#if LWIP_HTTPD_SSI
    if (hs->ssi)
    {
        hs->ssi->parse_left = count;
        hs->ssi->parsed = hs->buf;
    }
#endif // LWIP_HTTPD_SSI
#else // LWIP_HTTPD_DYNAMIC_FILE_READ
    LWIP_ASSERT("SSI and DYNAMIC_HEADERS turned off but eof not reached", 0);
#endif // LWIP_HTTPD_SSI || LWIP_HTTPD_DYNAMIC_HEADERS
    return 1;
}

/*!
 * @brief Sub-function of http_send(): This is the normal send-routine for non-ssi files
 *
 * @returns: - 1: data has been written (so call tcp_ouput)
 *           - 0: no data has been written (no need to call tcp_output)
 */
static u8_t http_send_data_nonssi(struct tcp_pcb *pcb, struct http_state *hs)
{
    err_t err;
    u16_t len;
    u16_t mss;
    u8_t data_to_send = 0;

    // We are not processing an SHTML file so no tag checking is necessary.
    // Just send the data as we received it from the file.

    // We cannot send more data than space available in the send buffer.
    if (tcp_sndbuf(pcb) < hs->left)
    {
        len = tcp_sndbuf(pcb);
    }
    else
    {
        len = (u16_t)hs->left;
        LWIP_ASSERT("hs->left did not fit into u16_t!", (len == hs->left));
    }
    mss = tcp_mss(pcb);
    if (len > (2 * mss))
    {
        len = 2 * mss;
    }

    err = http_write(pcb, hs->file, &len, HTTP_IS_DATA_VOLATILE(hs));
    if (err == ERR_OK)
    {
        data_to_send = 1;
        hs->file += len;
        hs->left -= len;
    }

    return data_to_send;
}

#if LWIP_HTTPD_SSI
/*!
 * @brief Sub-function of http_send(): This is the send-routine for ssi files
 *
 * @returns: - 1: data has been written (so call tcp_ouput)
 *           - 0: no data has been written (no need to call tcp_output)
 */
static u8_t http_send_data_ssi(struct tcp_pcb *pcb, struct http_state *hs)
{
    err_t err = ERR_OK;
    u16_t len;
    u16_t mss;
    u8_t data_to_send = 0;

    struct http_ssi_state *ssi = hs->ssi;
    LWIP_ASSERT("ssi != NULL", ssi != NULL);
    // We are processing an SHTML file so need to scan for tags and replace
    // them with insert strings. We need to be careful here since a tag may
    // straddle the boundary of two blocks read from the file and we may also
    // have to split the insert string between two tcp_write operations.

    // How much data could we send?
    len = tcp_sndbuf(pcb);

    // Do we have remaining data to send before parsing more?
    if(ssi->parsed > hs->file) {
        // We cannot send more data than space available in the send buffer.
        if (tcp_sndbuf(pcb) < (ssi->parsed - hs->file))
        {
            len = tcp_sndbuf(pcb);
        }
        else
        {
            LWIP_ASSERT("Data size does not fit into u16_t!",
            (ssi->parsed - hs->file) <= 0xffff);
            len = (u16_t)(ssi->parsed - hs->file);
        }
        mss = tcp_mss(pcb);
        if(len > (2 * mss))
        {
            len = 2 * mss;
        }

        err = http_write(pcb, hs->file, &len, HTTP_IS_DATA_VOLATILE(hs));
        if (err == ERR_OK)
        {
            data_to_send = 1;
            hs->file += len;
            hs->left -= len;
        }

        // If the send buffer is full, return now.
        if(tcp_sndbuf(pcb) == 0)
        {
            return data_to_send;
        }
    }

    LWIP_DEBUGF(HTTPD_DEBUG, ("State %d, %d left\r\n", ssi->tag_state, (int)ssi->parse_left));

    // We have sent all the data that was already parsed so continue parsing
    // the buffer contents looking for SSI tags.
    while((ssi->parse_left) && (err == ERR_OK))
    {
        // @todo: somewhere in this loop, 'len' should grow again...
        if (len == 0)
        {
            return data_to_send;
        }
        switch(ssi->tag_state)
        {
            case TAG_NONE:
                // We are not currently processing an SSI tag so scan for the
                // start of the lead-in marker.
                if(*ssi->parsed == g_pcTagLeadIn[0])
                {
                    // We found what could be the lead-in for a new tag
                    // so change state appropriately.
                    ssi->tag_state = TAG_LEADIN;
                    ssi->tag_index = 1;
#if !LWIP_HTTPD_SSI_INCLUDE_TAG
                    ssi->tag_started = ssi->parsed;
#endif // !LWIP_HTTPD_SSI_INCLUDE_TAG
                }

                // Move on to the next character in the buffer
                ssi->parse_left--;
                ssi->parsed++;
                break;

            case TAG_LEADIN:
                // We are processing the lead-in marker, looking for the start of
                // the tag name.

                // Have we reached the end of the leadin?
                if(ssi->tag_index == LEN_TAG_LEAD_IN)
                {
                    ssi->tag_index = 0;
                    ssi->tag_state = TAG_FOUND;
                }
                else
                {
                    // Have we found the next character we expect for the tag leadin?
                    if(*ssi->parsed == g_pcTagLeadIn[ssi->tag_index])
                    {
                        // Yes - move to the next one unless we have found the complete
                        // leadin, in which case we start looking for the tag itself
                        ssi->tag_index++;
                    }
                    else
                    {
                        // We found an unexpected character so this is not a tag. Move
                        // back to idle state.
                        ssi->tag_state = TAG_NONE;
                    }

                    // Move on to the next character in the buffer
                    ssi->parse_left--;
                    ssi->parsed++;
                }
                break;

            case TAG_FOUND:
                // We are reading the tag name, looking for the start of the
                // lead-out marker and removing any whitespace found.

                // Remove leading whitespace between the tag leading and the first
                // tag name character.
                if((ssi->tag_index == 0) && ((*ssi->parsed == ' ') ||
                            (*ssi->parsed == '\t') || (*ssi->parsed == '\n') ||
                            (*ssi->parsed == '\r')))
                {
                    // Move on to the next character in the buffer
                    ssi->parse_left--;
                    ssi->parsed++;
                    break;
                }

                // Have we found the end of the tag name? This is signalled by
                // us finding the first leadout character or whitespace
                if((*ssi->parsed == g_pcTagLeadOut[0]) ||
                        (*ssi->parsed == ' ')  || (*ssi->parsed == '\t') ||
                        (*ssi->parsed == '\n') || (*ssi->parsed == '\r'))
                {
                    if(ssi->tag_index == 0)
                    {
                        // We read a zero length tag so ignore it.
                        ssi->tag_state = TAG_NONE;
                    }
                    else
                    {
                        // We read a non-empty tag so go ahead and look for the leadout string.
                        ssi->tag_state = TAG_LEADOUT;
                        LWIP_ASSERT("ssi->tag_index <= 0xff", ssi->tag_index <= 0xff);
                        ssi->tag_name_len = (u8_t)ssi->tag_index;
                        ssi->tag_name[ssi->tag_index] = '\0';
                        if(*ssi->parsed == g_pcTagLeadOut[0])
                        {
                            ssi->tag_index = 1;
                        }
                        else
                        {
                            ssi->tag_index = 0;
                        }
                    }
                }
                else
                {
                    // This character is part of the tag name so save it
                    if(ssi->tag_index < LWIP_HTTPD_MAX_TAG_NAME_LEN)
                    {
                        ssi->tag_name[ssi->tag_index++] = *ssi->parsed;
                    }
                    else
                    {
                        // The tag was too long so ignore it.
                        ssi->tag_state = TAG_NONE;
                    }
                }

                // Move on to the next character in the buffer
                ssi->parse_left--;
                ssi->parsed++;

                break;

                // We are looking for the end of the lead-out marker.
            case TAG_LEADOUT:
                // Remove leading whitespace between the tag leading and the first
                // tag leadout character.
                if((ssi->tag_index == 0) && ((*ssi->parsed == ' ') ||
                            (*ssi->parsed == '\t') || (*ssi->parsed == '\n') ||
                            (*ssi->parsed == '\r')))
                {
                    // Move on to the next character in the buffer
                    ssi->parse_left--;
                    ssi->parsed++;
                    break;
                }

                // Have we found the next character we expect for the tag leadout?
                if(*ssi->parsed == g_pcTagLeadOut[ssi->tag_index])
                {
                    // Yes - move to the next one unless we have found the complete
                    // leadout, in which case we need to call the client to process the tag.

                    // Move on to the next character in the buffer
                    ssi->parse_left--;
                    ssi->parsed++;

                    if(ssi->tag_index == (LEN_TAG_LEAD_OUT - 1))
                    {
                        // Call the client to ask for the insert string for the ag we just found.
#if LWIP_HTTPD_SSI_MULTIPART
                        ssi->tag_part = 0; // start with tag part 0
#endif // LWIP_HTTPD_SSI_MULTIPART
                        get_tag_insert(hs);

                        // Next time through, we are going to be sending data
                        // immediately, either the end of the block we start
                        // sending here or the insert string.
                        ssi->tag_index = 0;
                        ssi->tag_state = TAG_SENDING;
                        ssi->tag_end = ssi->parsed;
#if !LWIP_HTTPD_SSI_INCLUDE_TAG
                        ssi->parsed = ssi->tag_started;
#endif // !LWIP_HTTPD_SSI_INCLUDE_TAG

                        // If there is any unsent data in the buffer prior to the
                        // tag, we need to send it now.
                        if (ssi->tag_end > hs->file)
                        {
                            // How much of the data can we send?
#if LWIP_HTTPD_SSI_INCLUDE_TAG
                            if(len > ssi->tag_end - hs->file)
                            {
                                len = (u16_t)(ssi->tag_end - hs->file);
                            }
#else // LWIP_HTTPD_SSI_INCLUDE_TAG
                            if(len > ssi->tag_started - hs->file)
                            {
                                // we would include the tag in sending
                                len = (u16_t)(ssi->tag_started - hs->file);
                            }
#endif // LWIP_HTTPD_SSI_INCLUDE_TAG

                            err = http_write(pcb, hs->file, &len, HTTP_IS_DATA_VOLATILE(hs));
                            if (err == ERR_OK)
                            {
                                data_to_send = 1;
#if !LWIP_HTTPD_SSI_INCLUDE_TAG
                                if(ssi->tag_started <= hs->file)
                                {
                                    // pretend to have sent the tag, too
                                    len += ssi->tag_end - ssi->tag_started;
                                }
#endif // !LWIP_HTTPD_SSI_INCLUDE_TAG
                                hs->file += len;
                                hs->left -= len;
                            }
                        }
                    }
                    else
                    {
                        ssi->tag_index++;
                    }
                }
                else
                {
                    // We found an unexpected character so this is not a tag.
                    // Move back to idle state.
                    ssi->parse_left--;
                    ssi->parsed++;
                    ssi->tag_state = TAG_NONE;
                }
                break;

            // We have found a valid tag and are in the process of sending
            // data as a result of that discovery. We send either remaining data
            // from the file prior to the insert point or the insert string itself.
            case TAG_SENDING:
                // Do we have any remaining file data to send from the buffer prior to the tag?
                if(ssi->tag_end > hs->file)
                {
                    // How much of the data can we send?
#if LWIP_HTTPD_SSI_INCLUDE_TAG
                    if(len > ssi->tag_end - hs->file)
                    {
                        len = (u16_t)(ssi->tag_end - hs->file);
                    }
#else // LWIP_HTTPD_SSI_INCLUDE_TAG
                    LWIP_ASSERT("hs->started >= hs->file", ssi->tag_started >= hs->file);
                    if (len > ssi->tag_started - hs->file)
                    {
                        // we would include the tag in sending
                        len = (u16_t)(ssi->tag_started - hs->file);
                    }
#endif // LWIP_HTTPD_SSI_INCLUDE_TAG
                    if (len != 0)
                    {
                        err = http_write(pcb, hs->file, &len, HTTP_IS_DATA_VOLATILE(hs));
                    }
                    else
                    {
                        err = ERR_OK;
                    }
                    if (err == ERR_OK)
                    {
                        data_to_send = 1;
#if !LWIP_HTTPD_SSI_INCLUDE_TAG
                        if(ssi->tag_started <= hs->file)
                        {
                            // pretend to have sent the tag, too
                            len += ssi->tag_end - ssi->tag_started;
                        }
#endif // !LWIP_HTTPD_SSI_INCLUDE_TAG
                        hs->file += len;
                        hs->left -= len;
                    }
                }
                else
                {
#if LWIP_HTTPD_SSI_MULTIPART
                    if(ssi->tag_index >= ssi->tag_insert_len)
                    {
                        // Did the last SSIHandler have more to send?
                        if (ssi->tag_part != HTTPD_LAST_TAG_PART)
                        {
                            // If so, call it again
                            ssi->tag_index = 0;
                            get_tag_insert(hs);
                        }
                    }
#endif // LWIP_HTTPD_SSI_MULTIPART

                    // Do we still have insert data left to send?
                    if(ssi->tag_index < ssi->tag_insert_len)
                    {
                        // We are sending the insert string itself. How much of the
                        // insert can we send?
                        if(len > (ssi->tag_insert_len - ssi->tag_index))
                        {
                            len = (ssi->tag_insert_len - ssi->tag_index);
                        }

                        // Note that we set the copy flag here since we only have a
                        // single tag insert buffer per connection. If we don't do
                        // this, insert corruption can occur if more than one insert
                        // is processed before we call tcp_output.
                        err = http_write(pcb, &(ssi->tag_insert[ssi->tag_index]), &len,
                        HTTP_IS_TAG_VOLATILE(hs));
                        if (err == ERR_OK)
                        {
                            data_to_send = 1;
                            ssi->tag_index += len;
                            // Don't return here: keep on sending data
                        }
                    }
                    else
                    {
#if LWIP_HTTPD_SSI_MULTIPART
                        if (ssi->tag_part == HTTPD_LAST_TAG_PART)
#endif // LWIP_HTTPD_SSI_MULTIPART
                        {
                            // We have sent all the insert data so go back
                            // to looking for a new tag.
                            LWIP_DEBUGF(HTTPD_DEBUG, ("Everything sent.\r\n"));
                            ssi->tag_index = 0;
                            ssi->tag_state = TAG_NONE;
#if !LWIP_HTTPD_SSI_INCLUDE_TAG
                            ssi->parsed = ssi->tag_end;
#endif // !LWIP_HTTPD_SSI_INCLUDE_TAG
                        }
                    }
                    break;
                }
        }
    }

    // If we drop out of the end of the for loop, this implies we must have
    // file data to send so send it now. In TAG_SENDING state, we've already
    // handled this so skip the send if that's the case.
    if((ssi->tag_state != TAG_SENDING) && (ssi->parsed > hs->file))
    {
        // We cannot send more data than space available in the send buffer.
        if (tcp_sndbuf(pcb) < (ssi->parsed - hs->file))
        {
            len = tcp_sndbuf(pcb);
        }
        else
        {
            LWIP_ASSERT("Data size does not fit into u16_t!",
            (ssi->parsed - hs->file) <= 0xffff);
            len = (u16_t)(ssi->parsed - hs->file);
        }
        if(len > (2 * tcp_mss(pcb)))
        {
            len = 2 * tcp_mss(pcb);
        }

        err = http_write(pcb, hs->file, &len, HTTP_IS_DATA_VOLATILE(hs));
        if (err == ERR_OK)
        {
            data_to_send = 1;
            hs->file += len;
            hs->left -= len;
        }
    }
    return data_to_send;
}
#endif // LWIP_HTTPD_SSI

/*!
 * @brief Try to send more data on this pcb.
 *
 * @param pcb the pcb to send data
 * @param hs connection state
 */
static u8_t http_send(struct tcp_pcb *pcb, struct http_state *hs)
{
    u8_t data_to_send = HTTP_NO_DATA_TO_SEND;

    LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("http_send: pcb=%p hs=%p left=%d\r\n", (void*)pcb,
    (void*)hs, hs != NULL ? (int)hs->left : 0));

#if LWIP_HTTPD_SUPPORT_POST && LWIP_HTTPD_POST_MANUAL_WND
    if (hs->unrecved_bytes != 0)
    {
        return 0;
    }
#endif // LWIP_HTTPD_SUPPORT_POST && LWIP_HTTPD_POST_MANUAL_WND

    // If we were passed a NULL state structure pointer, ignore the call.
    if (hs == NULL)
    {
        return 0;
    }

#if LWIP_HTTPD_FS_ASYNC_READ
    // Check if we are allowed to read from this file.
    // (e.g. SSI might want to delay sending until data is available)
    if (!fs_is_file_ready(hs->handle, http_continue, hs))
    {
        return 0;
    }
#endif // LWIP_HTTPD_FS_ASYNC_READ

#if LWIP_HTTPD_DYNAMIC_HEADERS
    // Do we have any more header data to send for this file?
    if(hs->hdr_index < NUM_FILE_HDR_STRINGS)
    {
        data_to_send = http_send_headers(pcb, hs);
        if (data_to_send != HTTP_DATA_TO_SEND_CONTINUE)
        {
            return data_to_send;
        }
    }
#endif // LWIP_HTTPD_DYNAMIC_HEADERS

    // Have we run out of file data to send? If so, we need to read the next block from the file.
    if (hs->left == 0)
    {
        if (!http_check_eof(pcb, hs))
        {
            return 0;
        }
    }

#if LWIP_HTTPD_SSI
    if(hs->ssi)
    {
        data_to_send = http_send_data_ssi(pcb, hs);
    }
    else
#endif // LWIP_HTTPD_SSI
    {
        data_to_send = http_send_data_nonssi(pcb, hs);
    }

    if((hs->left == 0) && (fs_bytes_left(hs->handle) <= 0))
    {
        // We reached the end of the file so this request is done.
        // This adds the FIN flag right into the last data segment.
        LWIP_DEBUGF(HTTPD_DEBUG, ("End of file.\r\n"));
        http_eof(pcb, hs);
        return 0;
    }
    LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("send_data end.\r\n"));
    return data_to_send;
}

#if LWIP_HTTPD_SUPPORT_EXTSTATUS
/*!
 * @brief Initialize a http connection with a file to send for an error message
 *
 * @param hs http connection state
 * @param error_nr HTTP error number
 * @return ERR_OK if file was found and hs has been initialized correctly
 *         another err_t otherwise
 */
static err_t http_find_error_file(struct http_state *hs, u16_t error_nr)
{
    const char *uri1, *uri2, *uri3;
    err_t err;

    if (error_nr == 501)
    {
        uri1 = "/501.html";
        uri2 = "/501.htm";
        uri3 = "/501.shtml";
    }
    else
    {
        // 400 (bad request is the default)
        uri1 = "/400.html";
        uri2 = "/400.htm";
        uri3 = "/400.shtml";
    }
    err = fs_open(&hs->file_handle, uri1);
    if (err != ERR_OK)
    {
        err = fs_open(&hs->file_handle, uri2);
        if (err != ERR_OK)
        {
            err = fs_open(&hs->file_handle, uri3);
            if (err != ERR_OK)
            {
                LWIP_DEBUGF(HTTPD_DEBUG, ("Error page for error %"U16_F" not found\r\n",
                error_nr));
                return ERR_ARG;
            }
        }
    }
    return http_init_file(hs, &hs->file_handle, 0, NULL, 0);
}
#else // LWIP_HTTPD_SUPPORT_EXTSTATUS
#define http_find_error_file(hs, error_nr) ERR_ARG
#endif // LWIP_HTTPD_SUPPORT_EXTSTATUS

/*!
 * @brief Get the file struct for a 404 error page.
 * Tries some file names and returns NULL if none found.
 *
 * @param uri pointer that receives the actual file name URI
 * @return file struct for the error page or NULL no matching file was found
 */
static struct fs_file * http_get_404_file(struct http_state *hs, const char **uri)
{
    err_t err;

    *uri = "/404.html";
    err = fs_open(&hs->file_handle, *uri);
    if (err != ERR_OK)
    {
        // 404.html doesn't exist. Try 404.htm instead.
        *uri = "/404.htm";
        err = fs_open(&hs->file_handle, *uri);
        if (err != ERR_OK)
        {
            // 404.htm doesn't exist either. Try 404.shtml instead.
            *uri = "/404.shtml";
            err = fs_open(&hs->file_handle, *uri);
            if (err != ERR_OK)
            {
                // 404.htm doesn't exist either. Indicate to the caller that it should
                // send back a default 404 page.
                *uri = NULL;
                return NULL;
            }
        }
    }

    return &hs->file_handle;
}

#if LWIP_HTTPD_SUPPORT_POST
static err_t http_handle_post_finished(struct http_state *hs)
{
#if LWIP_HTTPD_POST_MANUAL_WND
    // Prevent multiple calls to httpd_post_finished, since it might have already
    // been called before from httpd_post_data_recved().
    if (hs->post_finished)
    {
        return ERR_OK;
    }
    hs->post_finished = 1;
#endif // LWIP_HTTPD_POST_MANUAL_WND
    // application error or POST finished
    // NULL-terminate the buffer
    http_post_response_filename[0] = 0;
    httpd_post_finished(hs, http_post_response_filename, LWIP_HTTPD_POST_MAX_RESPONSE_URI_LEN);
    return http_find_file(hs, http_post_response_filename, 0);
}

/*!
 * @brief Pass received POST body data to the application and correctly handle
 * returning a response document or closing the connection.
 * ATTENTION: The application is responsible for the pbuf now, so don't free it!
 *
 * @param hs http connection state
 * @param p pbuf to pass to the application
 * @return ERR_OK if passed successfully, another err_t if the response file
 *         hasn't been found (after POST finished)
 */
static err_t http_post_rxpbuf(struct http_state *hs, struct pbuf *p)
{
    err_t err;

    // adjust remaining Content-Length
    if (hs->post_content_len_left < p->tot_len)
    {
        hs->post_content_len_left = 0;
    }
    else
    {
        hs->post_content_len_left -= p->tot_len;
    }
    err = httpd_post_receive_data(hs, p);
    if ((err != ERR_OK) || (hs->post_content_len_left == 0))
    {
#if LWIP_HTTPD_SUPPORT_POST && LWIP_HTTPD_POST_MANUAL_WND
        if (hs->unrecved_bytes != 0)
        {
            return ERR_OK;
        }
#endif // LWIP_HTTPD_SUPPORT_POST && LWIP_HTTPD_POST_MANUAL_WND
        // application error or POST finished
        return http_handle_post_finished(hs);
    }

    return ERR_OK;
}

/*!
 * @brief Handle a post request. Called from http_parse_request when method 'POST'
 * is found.
 *
 * @param p The input pbuf (containing the POST header and body).
 * @param hs The http connection state.
 * @param data HTTP request (header and part of body) from input pbuf(s).
 * @param data_len Size of 'data'.
 * @param uri The HTTP URI parsed from input pbuf(s).
 * @param uri_end Pointer to the end of 'uri' (here, the rest of the HTTP
 *                header starts).
 * @return ERR_OK: POST correctly parsed and accepted by the application.
 *         ERR_INPROGRESS: POST not completely parsed (no error yet)
 *         another err_t: Error parsing POST or denied by the application
 */
static err_t http_post_request(struct pbuf **inp, struct http_state *hs,
char *data, u16_t data_len, char *uri, char *uri_end)
{
    err_t err;
    // search for end-of-header (first double-CRLF)
    char* crlfcrlf = strnstr(uri_end + 1, CRLF CRLF, data_len - (uri_end + 1 - data));

    if (crlfcrlf != NULL) {
        // search for "Content-Length: "
#define HTTP_HDR_CONTENT_LEN                "Content-Length: "
#define HTTP_HDR_CONTENT_LEN_LEN            16
#define HTTP_HDR_CONTENT_LEN_DIGIT_MAX_LEN  10
        char *scontent_len = strnstr(uri_end + 1, HTTP_HDR_CONTENT_LEN, crlfcrlf - (uri_end + 1));
        if (scontent_len != NULL)
        {
            char *scontent_len_end = strnstr(scontent_len + HTTP_HDR_CONTENT_LEN_LEN, CRLF, HTTP_HDR_CONTENT_LEN_DIGIT_MAX_LEN);
            if (scontent_len_end != NULL)
            {
                int content_len;
                char *conten_len_num = scontent_len + HTTP_HDR_CONTENT_LEN_LEN;
                *scontent_len_end = 0;
                content_len = atoi(conten_len_num);
                if (content_len > 0)
                {
                    // adjust length of HTTP header passed to application
                    const char *hdr_start_after_uri = uri_end + 1;
                    u16_t hdr_len = LWIP_MIN(data_len, crlfcrlf + 4 - data);
                    u16_t hdr_data_len = LWIP_MIN(data_len, crlfcrlf + 4 - hdr_start_after_uri);
                    u8_t post_auto_wnd = 1;
                    http_post_response_filename[0] = 0;
                    err = httpd_post_begin(hs, uri, hdr_start_after_uri, hdr_data_len, content_len,
                    http_post_response_filename, LWIP_HTTPD_POST_MAX_RESPONSE_URI_LEN, &post_auto_wnd);
                    if (err == ERR_OK)
                    {
                        // try to pass in data of the first pbuf(s)
                        struct pbuf *q = *inp;
                        u16_t start_offset = hdr_len;
#if LWIP_HTTPD_POST_MANUAL_WND
                        hs->no_auto_wnd = !post_auto_wnd;
#endif // LWIP_HTTPD_POST_MANUAL_WND
                        // set the Content-Length to be received for this POST
                        hs->post_content_len_left = (u32_t)content_len;

                        // get to the pbuf where the body starts
                        while((q != NULL) && (q->len <= start_offset))
                        {
                            struct pbuf *head = q;
                            start_offset -= q->len;
                            q = q->next;
                            // free the head pbuf
                            head->next = NULL;
                            pbuf_free(head);
                        }
                        *inp = NULL;
                        if (q != NULL)
                        {
                            // hide the remaining HTTP header
                            pbuf_header(q, -(s16_t)start_offset);
#if LWIP_HTTPD_POST_MANUAL_WND
                            if (!post_auto_wnd)
                            {
                                // already tcp_recved() this data...
                                hs->unrecved_bytes = q->tot_len;
                            }
#endif // LWIP_HTTPD_POST_MANUAL_WND
                            return http_post_rxpbuf(hs, q);
                        }
                        else
                        {
                            return ERR_OK;
                        }
                    }
                    else
                    {
                        // return file passed from application
                        return http_find_file(hs, http_post_response_filename, 0);
                    }
                }
                else
                {
                    LWIP_DEBUGF(HTTPD_DEBUG, ("POST received invalid Content-Length: %s\r\n",
                    conten_len_num));
                    return ERR_ARG;
                }
            }
        }
    }
    // if we come here, the POST is incomplete
#if LWIP_HTTPD_SUPPORT_REQUESTLIST
    return ERR_INPROGRESS;
#else // LWIP_HTTPD_SUPPORT_REQUESTLIST
    return ERR_ARG;
#endif // LWIP_HTTPD_SUPPORT_REQUESTLIST
}

#if LWIP_HTTPD_POST_MANUAL_WND
/*!
 * @brief A POST implementation can call this function to update the TCP window.
 * This can be used to throttle data reception (e.g. when received data is
 * programmed to flash and data is received faster than programmed).
 *
 * @param connection A connection handle passed to httpd_post_begin for which
 *        httpd_post_finished has *NOT* been called yet!
 * @param recved_len Length of data received (for window update)
 */
void httpd_post_data_recved(void *connection, u16_t recved_len)
{
    struct http_state *hs = (struct http_state*)connection;
    if (hs != NULL)
    {
        if (hs->no_auto_wnd)
        {
            u16_t len = recved_len;
            if (hs->unrecved_bytes >= recved_len)
            {
                hs->unrecved_bytes -= recved_len;
            }
            else
            {
                LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_LEVEL_WARNING, ("httpd_post_data_recved: recved_len too big\r\n"));
                len = (u16_t)hs->unrecved_bytes;
                hs->unrecved_bytes = 0;
            }
            if (hs->pcb != NULL)
            {
                if (len != 0)
                {
                    tcp_recved(hs->pcb, len);
                }
                if ((hs->post_content_len_left == 0) && (hs->unrecved_bytes == 0))
                {
                    // finished handling POST
                    http_handle_post_finished(hs);
                    http_send(hs->pcb, hs);
                }
            }
        }
    }
}
#endif // LWIP_HTTPD_POST_MANUAL_WND

#endif // LWIP_HTTPD_SUPPORT_POST

#if LWIP_HTTPD_FS_ASYNC_READ
/* Try to send more data if file has been blocked before
 * This is a callback function passed to fs_read_async().
 */
static void http_continue(void *connection)
{
    struct http_state *hs = (struct http_state*)connection;
    if (hs && (hs->pcb) && (hs->handle))
    {
        LWIP_ASSERT("hs->pcb != NULL", hs->pcb != NULL);
        LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("httpd_continue: try to send more data\r\n"));
        if (http_send(hs->pcb, hs))
        {
            // If we wrote anything to be sent, go ahead and send it now.
            LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("tcp_output\r\n"));
            tcp_output(hs->pcb);
        }
    }
}
#endif // LWIP_HTTPD_FS_ASYNC_READ

/*!
 * When data has been received in the correct state, try to parse it
 * as a HTTP request.
 *
 * @param p the received pbuf
 * @param hs the connection state
 * @param pcb the tcp_pcb which received this packet
 * @return ERR_OK if request was OK and hs has been initialized correctly
 *         ERR_INPROGRESS if request was OK so far but not fully received
 *         another err_t otherwise
 */
static err_t http_parse_request(struct pbuf **inp, struct http_state *hs, struct tcp_pcb *pcb)
{
    char *data;
    char *crlf;
    u16_t data_len;
    struct pbuf *p = *inp;
#if LWIP_HTTPD_SUPPORT_REQUESTLIST
    u16_t clen;
#endif // LWIP_HTTPD_SUPPORT_REQUESTLIST
#if LWIP_HTTPD_SUPPORT_POST
    err_t err;
#endif // LWIP_HTTPD_SUPPORT_POST

    LWIP_UNUSED_ARG(pcb); // only used for post
    LWIP_ASSERT("p != NULL", p != NULL);
    LWIP_ASSERT("hs != NULL", hs != NULL);

    if ((hs->handle != NULL) || (hs->file != NULL))
    {
        LWIP_DEBUGF(HTTPD_DEBUG, ("Received data while sending a file\r\n"));
        // already sending a file
        // @todo: abort?
        return ERR_USE;
    }

#if LWIP_HTTPD_SUPPORT_REQUESTLIST

    LWIP_DEBUGF(HTTPD_DEBUG, ("Received %"U16_F" bytes\r\n", p->tot_len));

    // first check allowed characters in this pbuf?

    // enqueue the pbuf
    if (hs->req == NULL)
    {
        LWIP_DEBUGF(HTTPD_DEBUG, ("First pbuf\r\n"));
        hs->req = p;
    }
    else
    {
        LWIP_DEBUGF(HTTPD_DEBUG, ("pbuf enqueued\r\n"));
        pbuf_cat(hs->req, p);
    }

    if (hs->req->next != NULL)
    {
        data_len = LWIP_MIN(hs->req->tot_len, LWIP_HTTPD_MAX_REQ_LENGTH);
        pbuf_copy_partial(hs->req, httpd_req_buf, data_len, 0);
        data = httpd_req_buf;
    }
    else
#endif // LWIP_HTTPD_SUPPORT_REQUESTLIST
    {
        data = (char *)p->payload;
        data_len = p->len;
        if (p->len != p->tot_len)
        {
            LWIP_DEBUGF(HTTPD_DEBUG, ("Warning: incomplete header due to chained pbufs\r\n"));
        }
    }

    // received enough data for minimal request?
    if (data_len >= MIN_REQ_LEN)
    {
        // wait for CRLF before parsing anything
        crlf = strnstr(data, CRLF, data_len);
        if (crlf != NULL)
        {
#if LWIP_HTTPD_SUPPORT_POST
            int is_post = 0;
#endif // LWIP_HTTPD_SUPPORT_POST
            int is_09 = 0;
            char *sp1, *sp2;
            u16_t left_len, uri_len;
            LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("CRLF received, parsing request\r\n"));
            // parse method
            if (!strncmp(data, "GET ", 4))
            {
                sp1 = data + 3;
                // received GET request
                LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("Received GET request\"\r\n"));
#if LWIP_HTTPD_SUPPORT_POST
            }
            else if (!strncmp(data, "POST ", 5))
            {
                // store request type
                is_post = 1;
                sp1 = data + 4;
                // received GET request
                LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("Received POST request\r\n"));
#endif // LWIP_HTTPD_SUPPORT_POST
            }
            else
            {
                // null-terminate the METHOD (pbuf is freed anyway wen returning)
                data[4] = 0;
                // unsupported method!
                LWIP_DEBUGF(HTTPD_DEBUG, ("Unsupported request method (not implemented): \"%s\"\r\n",
                data));
                return http_find_error_file(hs, 501);
            }
            // if we come here, method is OK, parse URI
            left_len = data_len - ((sp1 +1) - data);
            sp2 = strnstr(sp1 + 1, " ", left_len);
#if LWIP_HTTPD_SUPPORT_V09
            if (sp2 == NULL)
            {
                // HTTP 0.9: respond with correct protocol version
                sp2 = strnstr(sp1 + 1, CRLF, left_len);
                is_09 = 1;
#if LWIP_HTTPD_SUPPORT_POST
                if (is_post)
                {
                    // HTTP/0.9 does not support POST
                    goto badrequest;
                }
#endif // LWIP_HTTPD_SUPPORT_POST
            }
#endif // LWIP_HTTPD_SUPPORT_V09
            uri_len = sp2 - (sp1 + 1);
            if ((sp2 != 0) && (sp2 > sp1))
            {
                // wait for CRLFCRLF (indicating end of HTTP headers) before parsing anything
                if (strnstr(data, CRLF CRLF, data_len) != NULL)
                {
                    char *uri = sp1 + 1;
#if LWIP_HTTPD_SUPPORT_11_KEEPALIVE
                    if (!is_09 && strnstr(data, HTTP11_CONNECTIONKEEPALIVE, data_len))
                    {
                        hs->keepalive = 1;
                    }
#endif // LWIP_HTTPD_SUPPORT_11_KEEPALIVE
                    // null-terminate the METHOD (pbuf is freed anyway wen returning)
                    *sp1 = 0;
                    uri[uri_len] = 0;
                    LWIP_DEBUGF(HTTPD_DEBUG, ("Received \"%s\" request for URI: \"%s\"\r\n",
                    data, uri));
#if LWIP_HTTPD_SUPPORT_POST
                    if (is_post)
                    {
#if LWIP_HTTPD_SUPPORT_REQUESTLIST
                        struct pbuf **q = &hs->req;
#else // LWIP_HTTPD_SUPPORT_REQUESTLIST
                        struct pbuf **q = inp;
#endif // LWIP_HTTPD_SUPPORT_REQUESTLIST
                        err = http_post_request(q, hs, data, data_len, uri, sp2);
                        if (err != ERR_OK)
                        {
                            /* restore header for next try */
                            *sp1 = ' ';
                            *sp2 = ' ';
                            uri[uri_len] = ' ';
                        }
                        if (err == ERR_ARG)
                        {
                            goto badrequest;
                        }
                        return err;
                    }
                    else
#endif // LWIP_HTTPD_SUPPORT_POST
                    {
                        return http_find_file(hs, uri, is_09);
                    }
                }
            }
            else
            {
                LWIP_DEBUGF(HTTPD_DEBUG, ("invalid URI\r\n"));
            }
        }
    }

#if LWIP_HTTPD_SUPPORT_REQUESTLIST
    clen = pbuf_clen(hs->req);
    if ((hs->req->tot_len <= LWIP_HTTPD_REQ_BUFSIZE) &&
            (clen <= LWIP_HTTPD_REQ_QUEUELEN))
    {
        // request not fully received (too short or CRLF is missing)
        return ERR_INPROGRESS;
    }
    else
#endif // LWIP_HTTPD_SUPPORT_REQUESTLIST
    {
#if LWIP_HTTPD_SUPPORT_POST
badrequest:
#endif // LWIP_HTTPD_SUPPORT_POST
        LWIP_DEBUGF(HTTPD_DEBUG, ("bad request\r\n"));
        // could not parse request
        return http_find_error_file(hs, 400);
    }
}

/*!
 * @brief Try to find the file specified by uri and, if found, initialize hs
 * accordingly.
 *
 * @param hs the connection state
 * @param uri the HTTP header URI
 * @param is_09 1 if the request is HTTP/0.9 (no HTTP headers in response)
 * @return ERR_OK if file was found and hs has been initialized correctly
 *         another err_t otherwise
 */
static err_t http_find_file(struct http_state *hs, const char *uri, int is_09)
{
    size_t loop;
    struct fs_file *file = NULL;
    char *params;
    err_t err;
#if LWIP_HTTPD_CGI
    int i;
    int count;
#endif // LWIP_HTTPD_CGI
#if !LWIP_HTTPD_SSI
    const
#endif // !LWIP_HTTPD_SSI
    // By default, assume we will not be processing server-side-includes tags
    u8_t tag_check = 0;

    // Have we been asked for the default root file?
    if((uri[0] == '/') &&  (uri[1] == 0))
    {
        // Try each of the configured default filenames until we find one that exists.
        for (loop = 0; loop < NUM_DEFAULT_FILENAMES; loop++)
        {
            LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("Looking for %s...\r\n", g_psDefaultFilenames[loop].name));
            err = fs_open(&hs->file_handle, (char *)g_psDefaultFilenames[loop].name);
            uri = (char *)g_psDefaultFilenames[loop].name;
            if(err == ERR_OK)
            {
                file = &hs->file_handle;
                LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("Opened.\r\n"));
#if LWIP_HTTPD_SSI
                tag_check = g_psDefaultFilenames[loop].shtml;
#endif /* LWIP_HTTPD_SSI */
                break;
            }
        }
        if (file == NULL)
        {
            // None of the default filenames exist so send back a 404 page
            file = http_get_404_file(hs, &uri);
#if LWIP_HTTPD_SSI
            tag_check = 0;
#endif // LWIP_HTTPD_SSI
        }
    }
    else
    {
        // No - we've been asked for a specific file.
        // First, isolate the base URI (without any parameters)
        params = (char *)strchr(uri, '?');
        if (params != NULL)
        {
            // URI contains parameters. NULL-terminate the base URI
            *params = '\0';
            params++;
        }

#if LWIP_HTTPD_CGI
        // Does the base URI we have isolated correspond to a CGI handler?
        if (g_iNumCGIs && g_pCGIs)
        {
            for (i = 0; i < g_iNumCGIs; i++)
            {
                if (strcmp(uri, g_pCGIs[i].pcCGIName) == 0)
                {
                    // We found a CGI that handles this URI so extract the
                    // parameters and call the handler.
                    count = extract_uri_parameters(hs, params);
                    uri = g_pCGIs[i].pfnCGIHandler(i, count, hs->params, hs->param_vals);
                    break;
                }
            }
        }
#endif // LWIP_HTTPD_CGI

        LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("Opening %s\r\n", uri));

        err = fs_open(&hs->file_handle, uri);
        if (err == ERR_OK)
        {
            file = &hs->file_handle;
        }
        else
        {
            file = http_get_404_file(hs, &uri);
        }
#if LWIP_HTTPD_SSI
        if (file != NULL)
        {
            // See if we have been asked for an shtml file and, if so, enable tag checking.
            tag_check = 0;
            for (loop = 0; loop < NUM_SHTML_EXTENSIONS; loop++)
            {
                if (strstr(uri, g_pcSSIExtensions[loop]))
                {
                    tag_check = 1;
                    break;
                }
            }
        }
#endif // LWIP_HTTPD_SSI
    }
    return http_init_file(hs, file, is_09, uri, tag_check);
}

/*!
 * @brief Initialize a http connection with a file to send (if found).
 * Called by http_find_file and http_find_error_file.
 *
 * @param hs http connection state
 * @param file file structure to send (or NULL if not found)
 * @param is_09 1 if the request is HTTP/0.9 (no HTTP headers in response)
 * @param uri the HTTP header URI
 * @param tag_check enable SSI tag checking
 * @return ERR_OK if file was found and hs has been initialized correctly
 *         another err_t otherwise
 */
static err_t http_init_file(struct http_state *hs, struct fs_file *file, int is_09, const char *uri, u8_t tag_check)
{
    if (file != NULL)
    {
        // file opened, initialise struct http_state
#if LWIP_HTTPD_SSI
        if (tag_check)
        {
            struct http_ssi_state *ssi = http_ssi_state_alloc();
            if (ssi != NULL)
            {
                ssi->tag_index = 0;
                ssi->tag_state = TAG_NONE;
                ssi->parsed = file->data;
                ssi->parse_left = file->len;
                ssi->tag_end = file->data;
                hs->ssi = ssi;
            }
        }
#else // LWIP_HTTPD_SSI
        LWIP_UNUSED_ARG(tag_check);
#endif // LWIP_HTTPD_SSI
        hs->handle = file;
        hs->file = (char*)file->data;
        LWIP_ASSERT("File length must be positive!", (file->len >= 0));
        hs->left = file->len;
        hs->retries = 0;
#if LWIP_HTTPD_TIMING
        hs->time_started = sys_now();
#endif // LWIP_HTTPD_TIMING
#if !LWIP_HTTPD_DYNAMIC_HEADERS
        LWIP_ASSERT("HTTP headers not included in file system", hs->handle->http_header_included);
#endif // !LWIP_HTTPD_DYNAMIC_HEADERS
#if LWIP_HTTPD_SUPPORT_V09
        if (hs->handle->http_header_included && is_09)
        {
            // HTTP/0.9 responses are sent without HTTP header, search for the end of the header.
            char *file_start = strnstr(hs->file, CRLF CRLF, hs->left);
            if (file_start != NULL)
            {
                size_t diff = file_start + 4 - hs->file;
                hs->file += diff;
                hs->left -= (u32_t)diff;
            }
        }
#endif // LWIP_HTTPD_SUPPORT_V09
    }
    else
    {
        hs->handle = NULL;
        hs->file = NULL;
        hs->left = 0;
        hs->retries = 0;
    }
#if LWIP_HTTPD_DYNAMIC_HEADERS
    // Determine the HTTP headers to send based on the file extension of the requested URI.
    if ((hs->handle == NULL) || !hs->handle->http_header_included)
    {
        get_http_headers(hs, (char*)uri);
    }
#else // LWIP_HTTPD_DYNAMIC_HEADERS
    LWIP_UNUSED_ARG(uri);
#endif // LWIP_HTTPD_DYNAMIC_HEADERS
    return ERR_OK;
}

/*
 * The pcb had an error and is already deallocated.
 * The argument might still be valid (if != NULL).
 */
static void http_err(void *arg, err_t err)
{
    struct http_state *hs = (struct http_state *)arg;
    LWIP_UNUSED_ARG(err);

    LWIP_DEBUGF(HTTPD_DEBUG, ("http_err: %s", lwip_strerr(err)));

    if (hs != NULL)
    {
        http_state_free(hs);
    }
}

/*
 * Data has been sent and acknowledged by the remote host.
 * This means that more data can be sent.
 */
static err_t http_sent(void *arg, struct tcp_pcb *pcb, u16_t len)
{
    struct http_state *hs = (struct http_state *)arg;

    LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("http_sent %p\r\n", (void*)pcb));

    LWIP_UNUSED_ARG(len);

    if (hs == NULL)
    {
        return ERR_OK;
    }

    hs->retries = 0;

    http_send(pcb, hs);

    return ERR_OK;
}

/*
 * The poll function is called every 2nd second.
 * If there has been no data sent (which resets the retries) in 8 seconds, close.
 * If the last portion of a file has not been sent in 2 seconds, close.
 *
 * This could be increased, but we don't want to waste resources for bad connections.
 */
static err_t http_poll(void *arg, struct tcp_pcb *pcb)
{
    struct http_state *hs = (struct http_state *)arg;
    LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("http_poll: pcb=%p hs=%p pcb_state=%s\r\n",
    (void*)pcb, (void*)hs, tcp_debug_state_str(pcb->state)));

    if (hs == NULL)
    {
        err_t closed;
        // arg is null, close.
        LWIP_DEBUGF(HTTPD_DEBUG, ("http_poll: arg is NULL, close\r\n"));
        closed = http_close_conn(pcb, NULL);
        LWIP_UNUSED_ARG(closed);
#if LWIP_HTTPD_ABORT_ON_CLOSE_MEM_ERROR
        if (closed == ERR_MEM)
        {
            tcp_abort(pcb);
            return ERR_ABRT;
        }
#endif // LWIP_HTTPD_ABORT_ON_CLOSE_MEM_ERROR
        return ERR_OK;
    }
    else
    {
        hs->retries++;
        if (hs->retries == HTTPD_MAX_RETRIES)
        {
            LWIP_DEBUGF(HTTPD_DEBUG, ("http_poll: too many retries, close\r\n"));
            http_close_conn(pcb, hs);
            return ERR_OK;
        }

        // If this connection has a file open, try to send some more data. If
        // it has not yet received a GET request, don't do this since it will
        // cause the connection to close immediately.
        if(hs && (hs->handle))
        {
            LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("http_poll: try to send more data\r\n"));
            if(http_send(pcb, hs))
            {
                // If we wrote anything to be sent, go ahead and send it now.
                LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("tcp_output\r\n"));
                tcp_output(pcb);
            }
        }
    }

    return ERR_OK;
}

/*
 * Data has been received on this pcb.
 * For HTTP 1.0, this should normally only happen once (if the request fits in one packet).
 */
static err_t http_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
{
    err_t parsed = ERR_ABRT;
    struct http_state *hs = (struct http_state *)arg;
    LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("http_recv: pcb=%p pbuf=%p err=%s\r\n", (void*)pcb,
    (void*)p, lwip_strerr(err)));

    if ((err != ERR_OK) || (p == NULL) || (hs == NULL))
    {
        // error or closed by other side?
        if (p != NULL)
        {
            // Inform TCP that we have taken the data.
            tcp_recved(pcb, p->tot_len);
            pbuf_free(p);
        }
        if (hs == NULL)
        {
            // this should not happen, only to be robust
            LWIP_DEBUGF(HTTPD_DEBUG, ("Error, http_recv: hs is NULL, close\r\n"));
        }
        http_close_conn(pcb, hs);
        return ERR_OK;
    }

#if LWIP_HTTPD_SUPPORT_POST && LWIP_HTTPD_POST_MANUAL_WND
    if (hs->no_auto_wnd)
    {
        hs->unrecved_bytes += p->tot_len;
    }
    else
#endif // LWIP_HTTPD_SUPPORT_POST && LWIP_HTTPD_POST_MANUAL_WND
    {
        // Inform TCP that we have taken the data.
        tcp_recved(pcb, p->tot_len);
    }

#if LWIP_HTTPD_SUPPORT_POST
    if (hs->post_content_len_left > 0)
    {
        // reset idle counter when POST data is received
        hs->retries = 0;
        // this is data for a POST, pass the complete pbuf to the application
        http_post_rxpbuf(hs, p);
        // pbuf is passed to the application, don't free it!
        if (hs->post_content_len_left == 0)
        {
            // all data received, send response or close connection
            http_send(pcb, hs);
        }
        return ERR_OK;
    }
    else
#endif // LWIP_HTTPD_SUPPORT_POST
    {
        if (hs->handle == NULL)
        {
            parsed = http_parse_request(&p, hs, pcb);
            LWIP_ASSERT("http_parse_request: unexpected return value", parsed == ERR_OK
            || parsed == ERR_INPROGRESS ||parsed == ERR_ARG || parsed == ERR_USE);
        }
        else
        {
            LWIP_DEBUGF(HTTPD_DEBUG, ("http_recv: already sending data\r\n"));
        }
#if LWIP_HTTPD_SUPPORT_REQUESTLIST
        if (parsed != ERR_INPROGRESS)
        {
            // request fully parsed or error
            if (hs->req != NULL)
            {
                pbuf_free(hs->req);
                hs->req = NULL;
            }
        }
#else // LWIP_HTTPD_SUPPORT_REQUESTLIST
        if (p != NULL)
        {
            // pbuf not passed to application, free it now
            pbuf_free(p);
        }
#endif // LWIP_HTTPD_SUPPORT_REQUESTLIST
        if (parsed == ERR_OK)
        {
#if LWIP_HTTPD_SUPPORT_POST
            if (hs->post_content_len_left == 0)
#endif // LWIP_HTTPD_SUPPORT_POST
            {
                LWIP_DEBUGF(HTTPD_DEBUG | LWIP_DBG_TRACE, ("http_recv: data %p len %"S32_F"\r\n", hs->file, hs->left));
                http_send(pcb, hs);
            }
        }
        else if (parsed == ERR_ARG)
        {
            // @todo: close on ERR_USE?
            http_close_conn(pcb, hs);
        }
    }
    return ERR_OK;
}

/*
 * A new incoming connection has been accepted.
 */
static err_t http_accept(void *arg, struct tcp_pcb *pcb, err_t err)
{
    struct http_state *hs;
    struct tcp_pcb_listen *lpcb = (struct tcp_pcb_listen*)arg;
    LWIP_UNUSED_ARG(err);
    LWIP_DEBUGF(HTTPD_DEBUG, ("http_accept %p / %p\r\n", (void*)pcb, arg));

    // Decrease the listen backlog counter
    tcp_accepted(lpcb);
    // Set priority
    tcp_setprio(pcb, HTTPD_TCP_PRIO);

    // Allocate memory for the structure that holds the state of the
    // connection - initialized by that function.
    hs = http_state_alloc();
    if (hs == NULL)
    {
        LWIP_DEBUGF(HTTPD_DEBUG, ("http_accept: Out of memory, RST\r\n"));
        return ERR_MEM;
    }
    hs->pcb = pcb;

    // Tell TCP that this is the structure we wish to be passed for our callbacks.
    tcp_arg(pcb, hs);

    // Set up the various callback functions
    tcp_recv(pcb, http_recv);
    tcp_err(pcb, http_err);
    tcp_poll(pcb, http_poll, HTTPD_POLL_INTERVAL);
    tcp_sent(pcb, http_sent);

    return ERR_OK;
}

/*
 * Initialize the httpd with the specified local address.
 */
static void httpd_init_addr(ip_addr_t *local_addr)
{
    struct tcp_pcb *pcb;
    err_t err;

    pcb = tcp_new();
    LWIP_ASSERT("httpd_init: tcp_new failed", pcb != NULL);
    tcp_setprio(pcb, HTTPD_TCP_PRIO);
    // set SOF_REUSEADDR here to explicitly bind httpd to multiple interfaces
    err = tcp_bind(pcb, local_addr, HTTPD_SERVER_PORT);
    LWIP_ASSERT("httpd_init: tcp_bind failed", err == ERR_OK);
    pcb = tcp_listen(pcb);
    LWIP_ASSERT("httpd_init: tcp_listen failed", pcb != NULL);
    // initialize callback arg and accept callback
    tcp_arg(pcb, pcb);
    tcp_accept(pcb, http_accept);
}

/*
 * Initialize the httpd: set up a listening PCB and bind it to the defined port
 */
void httpd_init(void)
{
#if HTTPD_USE_MEM_POOL
    LWIP_ASSERT("memp_sizes[MEMP_HTTPD_STATE] >= sizeof(http_state)",
    memp_sizes[MEMP_HTTPD_STATE] >= sizeof(http_state));
    LWIP_ASSERT("memp_sizes[MEMP_HTTPD_SSI_STATE] >= sizeof(http_ssi_state)",
    memp_sizes[MEMP_HTTPD_SSI_STATE] >= sizeof(http_ssi_state));
#endif
    LWIP_DEBUGF(HTTPD_DEBUG, ("httpd_init\r\n"));

    httpd_init_addr(IP_ADDR_ANY);
}

#if LWIP_HTTPD_SSI
/*!
 * @brief Set the SSI handler function.
 *
 * @param ssi_handler the SSI handler function
 * @param tags an array of SSI tag strings to search for in SSI-enabled files
 * @param num_tags number of tags in the 'tags' array
 */
void http_set_ssi_handler(tSSIHandler ssi_handler, const char **tags, int num_tags)
{
    LWIP_DEBUGF(HTTPD_DEBUG, ("http_set_ssi_handler\r\n"));

    LWIP_ASSERT("no ssi_handler given", ssi_handler != NULL);
    LWIP_ASSERT("no tags given", tags != NULL);
    LWIP_ASSERT("invalid number of tags", num_tags > 0);

    g_pfnSSIHandler = ssi_handler;
    g_ppcTags = tags;
    g_iNumTags = num_tags;
}
#endif // LWIP_HTTPD_SSI

#if LWIP_HTTPD_CGI
/*!
 * @brief Set an array of CGI filenames/handler functions
 *
 * @param cgis an array of CGI filenames/handler functions
 * @param num_handlers number of elements in the 'cgis' array
 */
void http_set_cgi_handlers(const tCGI *cgis, int num_handlers)
{
    LWIP_ASSERT("no cgis given", cgis != NULL);
    LWIP_ASSERT("invalid number of handlers", num_handlers > 0);

    g_pCGIs = cgis;
    g_iNumCGIs = num_handlers;
}
#endif // LWIP_HTTPD_CGI

adc16_converter_config_t adcUserConfig;
rtc_datetime_t date;
volatile int valorADC11P = 0;
volatile bool buzzer = false;

/**** CGI handler ****/
// the function pointer for a CGI script handler is defined in httpd.h as tCGIHandler
const char * processaCGI(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
	RTC_HAL_GetDatetime(RTC_BASE_PTR, &date);
	if (iIndex == 0)
	{
		GPIO_DRV_WritePinOutput(SHIELD_RELAY1, 0);
		GPIO_DRV_WritePinOutput(SHIELD_RELAY2, 0);
		GPIO_DRV_WritePinOutput(SHIELD_RELAY3, 0);
		GPIO_DRV_WritePinOutput(SHIELD_RELAY4, 0);
		GPIO_DRV_WritePinOutput(ON_BOARD_RELAY, 0);
		for (uint8_t i = 0; i < iNumParams; i++)
		{
			if (strcmp(pcParam[i], "rele") == 0)
			{
				if(strcmp(pcValue[i], "1") == 0)
				{
					GPIO_DRV_WritePinOutput(SHIELD_RELAY1, 1);
				}
				else
				if(strcmp(pcValue[i], "2") == 0)
				{
					GPIO_DRV_WritePinOutput(SHIELD_RELAY2, 1);
				}
				else
				if(strcmp(pcValue[i], "3") == 0)
				{
					GPIO_DRV_WritePinOutput(SHIELD_RELAY3, 1);
				}
				else
				if(strcmp(pcValue[i], "4") == 0)
				{
					GPIO_DRV_WritePinOutput(SHIELD_RELAY4, 1);
				}
				else
				if(strcmp(pcValue[i], "5") == 0)
				{
					GPIO_DRV_WritePinOutput(ON_BOARD_RELAY, 1);
				}
			}
		}

		if (cardInserted == true)
		{
			cartaoRELAY = true;
		}
	}
	else if (iIndex == 1)
	{
		// ex.: 2021-05-18
		char *data = malloc(10);
		char *tmp = malloc(10);
		strcpy(data, pcValue[0]);
		date.year = atoi(data);
		tmp[0] = data[5];
		tmp[1] = data[6];
		date.month = atoi(tmp);
		tmp[0] = data[8];
		tmp[1] = data[9];
		date.day = atoi(tmp);
	}
	else if (iIndex == 2)
	{
		// ex.: 14%3A42 sem os segundos
		char *relogio = malloc(10);
		char *tmp = malloc(10);
		strcpy(relogio, pcValue[0]);
		date.hour = atoi(relogio);
		tmp[0] = relogio[5];
		tmp[1] = relogio[6];
		date.minute = atoi(tmp);
	}
	RTC_HAL_SetDatetime(RTC_BASE_PTR, &date);
	if (cardInserted == true)
	{
		cartaoCGI = true;
	}
	return "/index.shtml";
}

// prototype CGI handler
const char * processaCGI(int iIndex, int iNumParams, char *pcParam[], char *pcValue[]);
// this structure contains the name of the CGI handlers
const tCGI releCGI={"/rele.cgi", processaCGI};
const tCGI dataCGI={"/data.cgi", processaCGI};
const tCGI horaCGI={"/hora.cgi", processaCGI};
//table of the CGI names and handlers
tCGI tabelaCGI[3];

// Initialize the CGI handlers
void cgi_init(void)
{
	tabelaCGI[0] = releCGI;
	tabelaCGI[1] = dataCGI;
	tabelaCGI[2] = horaCGI;
	http_set_cgi_handlers(tabelaCGI, 3);
}

//array of tags for the SSI handler
//these are the tags <!--#tag1--> contained in the shtml file
#define numEtiquetasSSI 3
char const *etiquetasSSI[numEtiquetasSSI] = {"tag1","tag2","tag3"};
/**** SSI handler ****/
u16_t atualizadorSSI(int iIndex, char *pcInsert, int iInsertLen)
{
	RTC_HAL_GetDatetime(RTC_BASE_PTR, &date);
	char tmpStr[] = "                  ";
	if (iIndex == 0)
	 {
		 sprintf(tmpStr, "%02hd/%02hd/%04hd ", date.day, date.month, date.year);
		 strcpy(pcInsert, tmpStr);
		 return strlen(tmpStr);
	 }
	 else if (iIndex == 1)
	 {
		 //sprintf(tmpStr, "%02hd:%02hd:%02hd ", date.hour, date.minute, date.second);
		 sprintf(tmpStr, "%02hd:%02hd ", date.hour, date.minute);
		 strcpy(pcInsert, tmpStr);
		 return strlen(tmpStr);
	 }
	 else if (iIndex == 2)
	 {
		 sprintf(tmpStr, "%03d", valorADC11P);
		 strcpy(pcInsert, tmpStr);
		 return strlen(tmpStr);
	 }
	RTC_HAL_SetDatetime(RTC_BASE_PTR, &date);
	if (cardInserted == true)
	{
		cartaoSSI = true;
	}
	return 0;
}

/**** Initialize SSI handlers ****/
void ssi_init(void)
{
	http_set_ssi_handler(atualizadorSSI, (char const **)etiquetasSSI, numEtiquetasSSI);
}

void httpd_server_init(void)
{
	struct netif fsl_netif0;
	ip_addr_t fsl_netif0_ipaddr, fsl_netif0_netmask, fsl_netif0_gw;

	lwip_init();
	IP4_ADDR(&fsl_netif0_ipaddr, 10,192,79,100);
	IP4_ADDR(&fsl_netif0_netmask, 255,255,255,0);
	IP4_ADDR(&fsl_netif0_gw, 10,191,79,1);

	netif_add(&fsl_netif0, &fsl_netif0_ipaddr, &fsl_netif0_netmask, &fsl_netif0_gw, NULL, ethernetif_init, ethernet_input);
	netif_set_default(&fsl_netif0);
	netif_set_up(&fsl_netif0);

	httpd_init();
	cgi_init();
	ssi_init();

	#if !ENET_RECEIVE_ALL_INTERRUPT
	uint32_t devNumber = 0;
	enet_dev_if_t * enetIfPtr;
	#if LWIP_HAVE_LOOPIF
	devNumber = fsl_netif0.num - 1;
	#else
	devNumber = fsl_netif0.num;
	#endif
	enetIfPtr = (enet_dev_if_t *)&enetDevIf[devNumber];
	#endif

	if (GPIO_DRV_ReadPinInput(SD_CARD_DETECT))
	{
		cardInserted = false;
	}
	else
	{
		cardInserted = true;
		RTC_HAL_GetDatetime(RTC_BASE_PTR, &date);
		char tmpStr[] = "                                                                                                                                     ";
		sprintf(tmpStr, "\r\n%02hd:%02hd:%02hd servidor httpd inicializado com sucesso", date.hour, date.minute, date.second);
		append("/httpd.log",tmpStr);
	}

	while(1)
	{
	#if !ENET_RECEIVE_ALL_INTERRUPT
		  ENET_receive(enetIfPtr);
	#endif
		  sys_check_timeouts();

		  if (cartaoCGI == true)
		  {
			  cartaoCGI = false;
			  RTC_HAL_GetDatetime(RTC_BASE_PTR, &date);
			  char tmpStr[] = "                                                                                                                                     ";
			  sprintf(tmpStr, "\r\n%02hd:%02hd:%02hd CGI acessado", date.hour, date.minute, date.second);
			  append("/httpd.log",tmpStr);
		  }

		  if (cartaoSSI == true)
		  {
			  cartaoSSI = false;
			  RTC_HAL_GetDatetime(RTC_BASE_PTR, &date);
			  char tmpStr[] = "                                                                                                                                     ";
			  sprintf(tmpStr, "\r\n%02hd:%02hd:%02hd SSI acessado", date.hour, date.minute, date.second);
			  append("/httpd.log",tmpStr);
		  }

		  if (cartaoRELAY == true)
		  {
			  cartaoRELAY = false;
			  RTC_HAL_GetDatetime(RTC_BASE_PTR, &date);
			  char tmpStr[] = "                                                                                                                                     ";
			  sprintf(tmpStr, "\r\n%02hd:%02hd:%02hd Relay acessado", date.hour, date.minute, date.second);
			  append("/httpd.log",tmpStr);
		  }
}
}
