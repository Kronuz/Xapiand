#include "logger.h"

#define _ L_NOTHING

#ifdef NDEBUG
#undef L_DEBUG
#define L_DEBUG _
#define L_TEST _
#define L_OBJ_BEGIN _
#define L_OBJ_END _
#define L_DATABASE_BEGIN _
#define L_DATABASE_END _
#define L_EV_BEGIN _
#define L_EV_END _
#else
#define L_TEST _
#define L_OBJ_BEGIN L_DELAYED_1000
#define L_OBJ_END L_DELAYED_N_UNLOG
#define L_DATABASE_BEGIN L_DELAYED_200
#define L_DATABASE_END L_DELAYED_N_UNLOG
#define L_EV_BEGIN L_DELAYED_600
#define L_EV_END L_DELAYED_N_UNLOG
#endif

#define L_MARK _LOG(false, LOG_DEBUG, "🔥  " DEBUG_COL, args)

////////////////////////////////////////////////////////////////////////////////
// Enable the following when needed. Use L_* or L_STACKED_* or L_UNINDENTED_*
// ex. L_STACKED_DARK_GREY, L_CYAN, L_STACKED_LOG or L_MAGENTA

#define L_ERRNO _
#define L_TRACEBACK _
#define L_CALL _
#define L_TIME _
#define L_CONN _
#define L_RAFT _
#define L_RAFT_PROTO _
#define L_DISCOVERY _
#define L_DISCOVERY_PROTO _
#define L_REPLICATION _
#define L_OBJ _
#define L_THREADPOOL _
#define L_DATABASE _
#define L_DATABASE_WAL _
#define L_HTTP _
#define L_BINARY _
#define L_HTTP_PROTO_PARSER _
#define L_EV _//_
#define L_HTTP_WIRE _
#define L_BINARY_WIRE _
#define L_TCP_WIRE _
#define L_UDP_WIRE _
#define L_HTTP_PROTO _
#define L_BINARY_PROTO _
#define L_DATABASE_WRAP_INIT _
#define L_DATABASE_WRAP _
#define L_INDEX _
#define L_SEARCH _
