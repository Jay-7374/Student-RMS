/**
 * @file    protocol.c
 * @brief   Shared protocol utility implementations.
 *
 * @author  Student Record System
 * @version 2.0.0
 */

#include "../common/protocol.h"

const char *opcode_to_string(int op)
{
    switch (op) {
        case OP_ADD:      return "OP_ADD";
        case OP_VIEW_ALL: return "OP_VIEW_ALL";
        case OP_SEARCH:   return "OP_SEARCH";
        case OP_UPDATE:   return "OP_UPDATE";
        case OP_DELETE:   return "OP_DELETE";
        case OP_REPORT:   return "OP_REPORT";
        case OP_PING:     return "OP_PING";
        case OP_QUIT:     return "OP_QUIT";
        default:          return "OP_UNKNOWN";
    }
}
