#include "leptjson.h"
#include <assert.h> /* assert() */
#include <stdlib.h> /* NULL, strtod() */
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <errno.h>

#define ISDIGIT(ch) ((ch) >= '0' && (ch) <= '9')
#define ISDIGIT1TO9(ch) ((ch) >= '1' && (ch) <= '9')

#define EXPECT(c, ch)             \
    do                            \
    {                             \
        assert(*c->json == (ch)); \
        c->json++;                \
    } while (0)

typedef struct
{
    const char *json;
} lept_context;

static void lept_parse_whitespace(lept_context *c)
{
    const char *p = c->json;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;
    c->json = p;
}

#if 0
static int lept_parse_true(lept_context *c, lept_value *v)
{
    EXPECT(c, 't');
    if (c->json[0] != 'r' || c->json[1] != 'u' || c->json[2] != 'e')
        return LEPT_PARSE_INVALID_VALUE;
    c->json += 3;
    v->type = LEPT_TRUE;
    return LEPT_PARSE_OK;
}

static int lept_parse_false(lept_context *c, lept_value *v)
{
    EXPECT(c, 'f');
    if (c->json[0] != 'a' || c->json[1] != 'l' || c->json[2] != 's' || c->json[3] != 'e')
        return LEPT_PARSE_INVALID_VALUE;
    c->json += 4;
    v->type = LEPT_FALSE;
    return LEPT_PARSE_OK;
}

static int lept_parse_null(lept_context *c, lept_value *v)
{
    EXPECT(c, 'n');
    if (c->json[0] != 'u' || c->json[1] != 'l' || c->json[2] != 'l')
        return LEPT_PARSE_INVALID_VALUE;
    c->json += 3;
    v->type = LEPT_NULL;
    return LEPT_PARSE_OK;
}
#endif

static int lept_parse_literal(lept_context *c, lept_value *v, const char *lept_str, lept_type type)
{
    size_t i = 0;
    EXPECT(c, *lept_str);
    while (lept_str[i + 1])
    {
        if (c->json[i] != lept_str[i + 1])
            return LEPT_PARSE_INVALID_VALUE;
        i++;
    }
    c->json += i;
    v->type = type;
    return LEPT_PARSE_OK;
}

enum LEPT_VALIDATE_STATE
{
    VALIDATE_START,
    VALIDATE_POST_SIGN,
    VALIDATE_POST_INT,
    VALIDATE_POST_FRAC,
    VALIDATE_END
};

#define BEGIN_WITH_CHAR(s, c) (*(s) == (c)) ? true : false

static bool lept_validate_number(const char *num, size_t *size)
{
    const char *s = num;
    enum LEPT_VALIDATE_STATE state = VALIDATE_START;
    while (true)
    {
        switch (state)
        {
        case VALIDATE_START:
        {
            if (BEGIN_WITH_CHAR(s, '-'))
                s++;
            state = VALIDATE_POST_SIGN;
            break;
        }
        case VALIDATE_POST_SIGN: /* int part is essential */
        {
            if (BEGIN_WITH_CHAR(s, '0'))
            {
                s++;
                state = VALIDATE_POST_INT;
                break;
            }
            else if (ISDIGIT1TO9(*s))
            {
                s++;
                while (ISDIGIT(*s))
                    s++;
                state = VALIDATE_POST_INT;
                break;
            }
            else
                return false;
        }
        case VALIDATE_POST_INT:
        {
            if (BEGIN_WITH_CHAR(s, '.'))
            {
                s++;
                if (!ISDIGIT(*s))
                    return false;
                while (ISDIGIT(*s))
                    s++;
            }
            state = VALIDATE_POST_FRAC;
            break;
        }
        case VALIDATE_POST_FRAC:
        {
            if (BEGIN_WITH_CHAR(s, 'e') || BEGIN_WITH_CHAR(s, 'E'))
            {
                s++;
                if (BEGIN_WITH_CHAR(s, '+') || BEGIN_WITH_CHAR(s, '-'))
                    s++;
                if (!ISDIGIT(*s))
                    return false;
                while (ISDIGIT(*s))
                    s++;
            }
            state = VALIDATE_END;
            break;
        }
        case VALIDATE_END:
        {
            *size = s - num;
            return true;
        }
        default:
            return false;
        }
    }
}

static int lept_parse_number(lept_context *c, lept_value *v)
{
    size_t size;
    if (!lept_validate_number(c->json, &size))
        return LEPT_PARSE_INVALID_VALUE;
    errno = 0;
    v->n = strtod(c->json, NULL);
    if (errno == ERANGE && (v->n == HUGE_VAL || v->n == -HUGE_VAL))
        return LEPT_PARSE_NUMBER_TOO_BIG;
    c->json += size;
    v->type = LEPT_NUMBER;
    return LEPT_PARSE_OK;
}

static int lept_parse_value(lept_context *c, lept_value *v)
{
    switch (*c->json)
    {
    case 't':
        return lept_parse_literal(c, v, "true", LEPT_TRUE);
    case 'f':
        return lept_parse_literal(c, v, "false", LEPT_FALSE);
    case 'n':
        return lept_parse_literal(c, v, "null", LEPT_NULL);
    default:
        return lept_parse_number(c, v);
    case '\0':
        return LEPT_PARSE_EXPECT_VALUE;
    }
}

int lept_parse(lept_value *v, const char *json)
{
    lept_context c;
    int ret;
    assert(v != NULL);
    c.json = json;
    v->type = LEPT_NULL;
    lept_parse_whitespace(&c);
    if ((ret = lept_parse_value(&c, v)) == LEPT_PARSE_OK)
    {
        lept_parse_whitespace(&c);
        if (*c.json != '\0')
        {
            v->type = LEPT_NULL;
            ret = LEPT_PARSE_ROOT_NOT_SINGULAR;
        }
    }
    return ret;
}

lept_type lept_get_type(const lept_value *v)
{
    assert(v != NULL);
    return v->type;
}

double lept_get_number(const lept_value *v)
{
    assert(v != NULL && v->type == LEPT_NUMBER);
    return v->n;
}
