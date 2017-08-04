#ifdef STDC_HEADERS
# include <stdio.h>
#endif
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#ifdef _WIN32
#include <winsock2.h>
#endif

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <stdio.h>
#include <stdlib.h>

#include "sb_options.h"
#include "db_driver.h"

#include <tarantool/tarantool.h>
#include <tarantool/tnt_net.h>
#include <tarantool/tnt_opt.h>

/* Tarantool driver arguments */

static sb_arg_t tarantool_drv_args[] =
{
  SB_OPT("tarantool-port", "Tarantool server port", "3306", LIST),
  SB_OPT("tarantool-db", "Tarantool database name", "sbtest", STRING),

  SB_OPT_END
};

typedef struct
{
  sb_list_t          *ports;
  char               *db;
} tarantool_drv_args_t;

typedef struct
{
  // TARANTOOL    *tarantool;
  char         *db;
  unsigned int port;
} db_tarantool_conn_t;

/* Structure used for DB-to-Tarantool bind types map */
//....

/* DB-to-Tarantool bind types map */
//...

/* Tarantool driver capabilities */

static drv_caps_t tarantool_drv_caps =
{
  .multi_rows_insert = 1,
  .prepared_statements = 0,
  .auto_increment = 0,
  .needs_commit = 0,
  .serial = 0,
  .unsigned_int = 0,
};

static tarantool_drv_args_t args;          /* driver args */

static char use_ps; /* whether server-side prepared statemens should be used */

/* Positions in the list of hosts/ports/sockets. Protected by pos_mutex */
//...

/* Tarantool driver operations */

static int tarantool_drv_init(void);
static int tarantool_drv_describe(drv_caps_t *);
static int tarantool_drv_connect(db_conn_t *);
static int tarantool_drv_disconnect(db_conn_t *);
static db_error_t tarantool_drv_query(db_conn_t *, const char *, size_t, db_result_t *);
static int tarantool_drv_done(void);


/* Tarantool driver definition */

static db_driver_t tarantool_driver =
{
  .sname = "tarantool",
  .lname = "Tarantool driver",
  .args = tarantool_drv_args,
  .ops = {
    .init = tarantool_drv_init,
    .connect = tarantool_drv_connect,
    .query = tarantool_drv_query,
    .disconnect = tarantool_drv_disconnect,
    .done = tarantool_drv_done,
    .describe = tarantool_drv_describe,
  }
};

/* Local functions */
//...

/* Register Tarantool driver */
int register_driver_tarantool(sb_list_t *drivers)
{
  SB_LIST_ADD_TAIL(&tarantool_driver.listitem, drivers);

  return 0;
}


/* Tarantool driver initialization */
int tarantool_drv_init(void)
{
  	log_text(LOG_DEBUG, "tarantool_drv_init\n");
    
    args.db = sb_get_value_string("tarantool-db");
    args.ports = sb_get_value_list("tarantool-port");

    return 0;
}

/* Connect to database */
int tarantool_drv_connect(db_conn_t *sb_conn)
{
  log_text(LOG_DEBUG, "tarantool_drv_connect\n");

  const char * uri = "localhost:3301";
  struct tnt_stream * con = tnt_net(NULL); // Allocating stream
  tnt_set(con, TNT_OPT_URI, uri); // Setting URI
  tnt_set(con, TNT_OPT_SEND_BUF, 0); // Disable buffering for send
  tnt_set(con, TNT_OPT_RECV_BUF, 0); // Disable buffering for recv

  // Initialize stream and connect to Tarantool
  if (tnt_connect(con) < 0) {
       log_text(LOG_ALERT, "Connection refused\n");
       return -1;
   }

  sb_conn->ptr = con;

  return 0;
}

/* Disconnect from database */
int tarantool_drv_disconnect(db_conn_t *sb_conn)
{
  log_text(LOG_DEBUG, "tarantool_drv_disconnect\n");

  struct tnt_stream * con = sb_conn->ptr;

  if (con != NULL) {
    // Close connection and free stream object
    tnt_close(con); 
    tnt_stream_free(con); 
  }

  return 0;
}

/* Execute SQL query */
db_error_t tarantool_drv_query(db_conn_t *sb_conn, const char *query, size_t len,
                           db_result_t *rs)
{
  struct tnt_stream * con = sb_conn->ptr;

  log_text(LOG_DEBUG, "tarantool_drv_query(%p, \"%s\", %u)\n",
        con,
        query,
        len);

  struct tnt_stream *tuple = tnt_object(NULL);
  tnt_object_format(tuple, "[%s]", query);

  char proc[] = "box.sql.execute";
  tnt_call(con, proc, sizeof(proc) - 1, tuple);
  tnt_flush(con);

  struct tnt_reply reply;
  tnt_reply_init(&reply);
  con->read_reply(con, &reply);

  if (reply.code != 0) {
    log_text(LOG_FATAL, "Failed %lu (%s).\n", reply.code, reply.error);
    return DB_ERROR_FATAL;
  }

  rs->counter = SB_CNT_WRITE;

  tnt_stream_free(tuple);

  return DB_ERROR_NONE;
}

/* Uninitialize driver */
int tarantool_drv_done(void)
{
  log_text(LOG_DEBUG, "tarantool_drv_done\n");
  return 0;
}

/* Describe database capabilities */

int tarantool_drv_describe(drv_caps_t *caps)
{
  *caps = tarantool_drv_caps;

  return 0;
}
