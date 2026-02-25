#ifndef JSMN_H
#define JSMN_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* JSON token types */
typedef enum {
    JSMN_UNDEFINED = 0,
    JSMN_OBJECT = 1,
    JSMN_ARRAY = 2,
    JSMN_STRING = 3,
    JSMN_PRIMITIVE = 4
} jsmntype_t;

enum jsmnerr {
    JSMN_ERROR_NOMEM = -1,
    JSMN_ERROR_INVAL = -2,
    JSMN_ERROR_PART = -3
};

/* JSON token description */
typedef struct {
    jsmntype_t type;
    int start;
    int end;
    int size;
} jsmntok_t;

/* JSON parser state */
typedef struct {
    unsigned int pos;
    unsigned int toknext;
    int toksuper;
} jsmn_parser;

/* Initialize parser */
void jsmn_init(jsmn_parser *parser);

/* Parse JSON string */
int jsmn_parse(jsmn_parser *parser, const char *js, size_t len,
               jsmntok_t *tokens, size_t num_tokens);

#ifdef __cplusplus
}
#endif

#endif /* JSMN_H */
