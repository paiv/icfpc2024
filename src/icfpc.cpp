#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>


static const char
_usage[] = R"(usage: icfpc [-a] [-t] [-v] [file...]

ICFP document compiler

Options:
  -a,--asserts  generate asserts
  -t,--text     generate ICFP code
  -v,--verbose  set verbose logging
)";


static const char
_usageq[] = "usage: icfpc [-a] [-t] [-v] [file...]";


static constexpr const size_t _SymbolBufSizeMax = 0x100000;
static constexpr const size_t _TokenSizeMax = 0x1000;
static constexpr const size_t _ExprTreeSizeMax = 0x10000;
static constexpr const size_t _NameTableSizeMax = 0x1000;
static constexpr const size_t _NameTableListSizeMax = 0x1000;
static constexpr const size_t _NamesSizeMax = 0x10000;


struct _Number {
    int64_t value;
};


static void
number_init(struct _Number* num) {
    num->value = 0;
}


static int
number_is_neg(struct _Number* num) {
    return num->value < 0;
}


static void
number_add(struct _Number* num, int64_t arg) {
    num->value += arg;
}


static void
number_mul(struct _Number* num, int64_t arg) {
    num->value *= arg;
}


struct _SymbolList {
    char* buf;
    size_t bufsize;
    size_t used;
};


static void
symbol_list_init(struct _SymbolList* list, size_t bufsize, char* buf) {
    list->buf = buf;
    list->bufsize = bufsize;
    list->used = 0;
}


static void
symbol_list_reset(struct _SymbolList* list) {
    list->used = 0;
}


static char*
symbol_list_push(struct _SymbolList* list, const char* value) {
    int n = strlen(value);
    if (list->used + n >= list->bufsize) {
        fprintf(stderr, "! out of symbol storage at %zu bytes\n", list->used + n);
        abort();
    }
    char* res = strcpy(&list->buf[list->used], value);
    list->used += n + 1;
    return res;
}


static int
symbol_list_pop(struct _SymbolList* list, char* value) {
    int n = strlen(value);
    size_t pos = list->used - (n + 1);
    if (&list->buf[pos] != value) {
        return 1;
    }
    list->used = pos;
    return 0;
}


static char*
symbol_list_concat(struct _SymbolList* list, char* dest, char* value) {
    int n = strlen(dest);
    if (&list->buf[list->used] != dest + (n + 1)) {
        abort();
    }
    list->used -= 1;
    return symbol_list_push(list, value);
}


enum _TokenType {
    _TokenType_invalid,
    _TokenType_eof,
    _TokenType_open_paren,
    _TokenType_close_paren,
    _TokenType_identifier,
    _TokenType_bool_false,
    _TokenType_bool_true,
    _TokenType_number,
    _TokenType_str,
    _TokenType_str_start,
    _TokenType_str_data,
    _TokenType_str_end,
};


struct _Token {
    enum _TokenType type;
    int len;
    size_t value_size;
    char* value;
    int lineno;
    int colno;
};


static void
token_init(struct _Token* token, size_t bufsize, char* buf) {
    token->type = _TokenType_invalid;
    token->value = buf;
    token->value_size = bufsize;
    token->value[0] = '\0';
    token->len = 0;
    token->lineno = 0;
    token->colno = 0;
}


enum _ExprType {
    _ExprType_invalid,
    _ExprType_identifier,
    _ExprType_literal,
    _ExprType_apply1,
    _ExprType_apply2,
    _ExprType_apply3,
    _ExprType_lambda,
    _ExprType_define,
    _ExprType_assert,
};


struct _Expr {
    enum _ExprType type;
    struct _Token token;
    struct _Expr* expr0;
    struct _Expr* expr1;
    struct _Expr* expr2;
    struct _Expr* expr3;
    int lineno;
    int colno;
};


static void
expr_init(struct _Expr* expr) {
    *expr = {};
    expr->type = _ExprType_invalid;
}


struct _ExprTree {
    struct _Expr* exprs;
    size_t exprs_size;
    size_t used;
};


static void
expr_tree_init(struct _ExprTree* tree, size_t bufsize, _Expr* buf) {
    tree->exprs = buf;
    tree->exprs_size = bufsize;
    tree->used = 0;
}


static void
expr_tree_reset(struct _ExprTree* tree) {
    tree->used = 0;
}


static _Expr*
expr_tree_push(struct _ExprTree* tree) {
    if (tree->used >= tree->exprs_size) {
        fprintf(stderr, "! out of expr storage at %zu used\n", tree->used);
        abort();
    }
    _Expr* expr = &tree->exprs[tree->used++];
    expr_init(expr);
    return expr;
}


struct _Name {
    const char* name;
    int seqno;
    const char* filename;
    int lineno;
    int colno;
    struct _Expr* expr;
};


static void
name_init(struct _Name* name, const char* s) {
    *name = {};
    name->name = s;
}


struct _NameList {
    struct _Name* names;
    size_t names_size;
    size_t used;
};


struct _NameTableList;

struct _NameTable {
    struct _NameTable* parent;
    struct _Name* names[_NameTableSizeMax];
    size_t names_size;
    size_t used;
    struct _NameTableList* table_storage;
    struct _NameList* name_storage;
};


struct _NameTableList {
    struct _NameTable* tables;
    size_t tables_size;
    size_t used;
};


static void
name_list_init(struct _NameList* list, size_t bufsize, struct _Name* buf) {
    list->names = buf;
    list->names_size = bufsize;
    list->used = 0;
}


static struct _Name*
name_list_push(struct _NameList* list) {
    if (list->used + 1 >= list->names_size) {
        fprintf(stderr, "! out of name storage at %zu\n", list->used);
        abort();
    }
    struct _Name* name = &list->names[list->used++];
    return name;
}


static void
name_table_init(struct _NameTable* table, struct _NameTableList* table_storage, struct _NameList* name_storage) {
    table->names_size = sizeof(table->names) / sizeof(table->names[0]);
    table->used = 0;
    table->table_storage = table_storage;
    table->name_storage = name_storage;
}


static struct _NameTable*
name_table_list_init(struct _NameTableList* list, size_t bufsize, struct _NameTable* buf) {
    list->tables = buf;
    list->tables_size = bufsize;
    list->used = 0;
    struct _NameTable* table = &list->tables[list->used++];
    name_table_init(table, list, NULL);
    return table;
}


static struct _NameTable*
name_table_list_push(struct _NameTableList* list) {
    if (list->used + 1 >= list->tables_size) {
        fprintf(stderr, "! out of name table storage at %zu\n", list->used);
        abort();
    }
    struct _NameTable* table = &list->tables[list->used++];
    name_table_init(table, list, NULL);
    return table;
}


static struct _NameTable*
name_table_add_child(struct _NameTable* table) {
    struct _NameTable* child = name_table_list_push(table->table_storage);
    name_table_init(child, table->table_storage, table->name_storage);
    child->parent = table;
    return child;
}


static struct _Name*
name_table_put(struct _NameTable* table, const char* name, struct _Expr* expr) {
    if (table->used + 1 >= table->names_size) {
        fprintf(stderr, "! out of name storage in the table at %zu\n", table->used);
        abort();
    }
    struct _Name* s = name_list_push(table->name_storage);
    table->names[table->used++] = s;
    name_init(s, name);
    s->expr = expr;
    return s;
}


static int
name_table_resolve(struct _NameTable* table, struct _Token* token, struct _Name** resolved) {
    struct _Name** p = table->names;
    for (size_t i = 0; i < table->used; ++i, ++p) {
        struct _Name* s = *p;
        if (strcmp(s->name, token->value) == 0) {
            if (s->expr == NULL) {
                *resolved = s;
                return 0;
            }
            if (s->expr->type == _ExprType_identifier) {
                int res = name_table_resolve(table, &s->expr->token, resolved);
                if (res == 0) { return 0; }
            }
            *resolved = s;
            return 0;
        }
    }
    table = table->parent;
    if (table != NULL) {
        return name_table_resolve(table, token, resolved);
    }
    fprintf(stderr, ":%d:%d: use of undeclared name %s\n", token->lineno, token->colno, token->value);
    return 1;
}


struct _ParserState {
    int out_asserts;
    int verbose;
    const char* filename;
    int lineno;
    int colno;
    struct _SymbolList symbols;
    size_t token_bufsize;
    char* token_buf;
    struct _ExprTree expr_tree;
    struct _NameList name_list;
};


struct _WriterState {
    FILE* file;
    int out_format;
    const char* filename;
    int verbose;
    struct _NameTableList nametable_list;
    struct _NameTable* nametable;
};


static int
icfp_writer_init(struct _WriterState* context, FILE* file, int oformat) {
    context->file = file;
    context->out_format = oformat;
    return 0;
}

static int
_icfp_parser_skip_comment(struct _ParserState* context, FILE* file, struct _Token* token) {
    int state = 1;
    int end_char = 0;
    for (;;) {
        int c = fgetc(file);
        if (c == EOF) {
            fprintf(stderr, "%s:%d:%d: unterminated comment\n", context->filename, token->lineno, token->colno);
            return 1;
        }
        switch (c) {
            case '\n':
                context->lineno += 1;
                context->colno = 1;
                break;
            default:
                context->colno += 1;
                break;
        }
        switch (state) {
            case 1:
                switch (c) {
                    case '(':
                        end_char = ')';
                        state = 5;
                        break;
                    case '{':
                        end_char = '}';
                        state = 5;
                        break;
                    case '<':
                        end_char = '>';
                        state = 5;
                        break;
                    case '[':
                        end_char = ']';
                        state = 5;
                        break;
                    case '!': case '"': case '#': case '$': case '%': case '&': case '\'':
                    case '*': case '+': case '-': case '/': case ':': case ';': case '=':
                    case '?': case '@': case '^': case '`': case '|': case '~':
                        end_char = c;
                        state = 5;
                        break;
                    default:
                        state = 2;
                        break;
                }
                break;
            case 2:
                if (c == '}') {
                    return 0;
                }
                break;
            case 5:
                if (c == end_char) {
                    state = 6;
                }
                break;
            case 6:
                if (c == '}') {
                    return 0;
                }
                else if (c != end_char) {
                    state = 5;
                }
                break;
        }
    }
}


static int
_icfp_parser_read_str_data(struct _ParserState* context, FILE* file, struct _Token* token) {
    int state = 0;
    for (;;) {
        int c = fgetc(file);
        if (c == EOF) {
            fprintf(stderr, "%s:%d:%d: unexpected EOF\n", context->filename, context->lineno, context->colno);
            return -1;
        }
        switch (state) {
            case 0:
                switch (c) {
                    case '"':
                        if (token->len >= token->value_size) {
                            abort();
                        }
                        token->value[token->len] = '\0';
                        token->type = _TokenType_str_end;
                        context->colno += 1;
                        return 0;
                    case '\\':
                        if (token->len + 1 >= token->value_size) {
                            ungetc(c, file);
                            token->value[token->len] = '\0';
                            token->type = _TokenType_str_data;
                            return 0;
                        }
                        state = 10;
                        break;
                    case 0x20:
                    case 0x21:
                    case 0x23 ... 0x5b:
                    case 0x5d ... 0x7a:
                    case 0x7c:
                    case 0x7e:
                        if (token->len + 1 >= token->value_size) {
                            ungetc(c, file);
                            token->value[token->len] = '\0';
                            return 0;
                        }
                        token->value[token->len++] = c;
                        token->type = _TokenType_str_data;
                        context->colno += 1;
                        break;
                    default:
                        fprintf(stderr, "%s:%d:%d: invalid string %c\n", context->filename, context->lineno, context->colno, c);
                        return -1;
                }
                break;

            case 10:
                switch (c) {
                    case '\\':
                    case '"':
                        token->value[token->len++] = c;
                        token->type = _TokenType_str_data;
                        context->colno += 2;
                        state = 0;
                        break;
                    case 'n':
                        token->value[token->len++] = '\n';
                        token->type = _TokenType_str_data;
                        context->colno += 2;
                        state = 0;
                        break;
                    default:
                        fprintf(stderr, "%s:%d:%d: invalid escape %c\n", context->filename, context->lineno, context->colno, c);
                        return -1;
                }
                break;
        }
    }
    abort();
}


static int
_icfp_parser_read_identifier(struct _ParserState* context, FILE* file, struct _Token* token) {
    for (;;) {
        int c = fgetc(file);
        if (c == EOF) {
            return 0;
        }
        switch (c) {
            case ' ':
            case '\t':
            case '\r':
                context->colno += 1;
                return 0;
            case '\n':
                context->lineno += 1;
                context->colno = 1;
                return 0;
            case '(':
            case ')':
                ungetc(c, file);
                return 0;
            case 0x21 ... 0x27:
            case 0x2a ... 0x7e:
                if (token->len + 1 >= token->value_size) {
                    fprintf(stderr, "%s:%d:%d: token is too long\n", context->filename, token->lineno, token->colno);
                    return 1;
                }
                token->value[token->len++] = c;
                token->value[token->len] = '\0';
                context->colno += 1;
                break;
            default:
                fprintf(stderr, "%s:%d:%d: invalid char %c\n", context->filename, context->lineno, context->colno, c);
                return 1;
        }
    }
}


static int
_icfp_parser_parse_number(const char* p, struct _Number* value) {
    int state = 0;
    number_init(value);
    int sign = 1;
    char c = *p;
    for (; c != '\0'; c = *++p) {
        switch (state) {
            case 0:
                switch (c) {
                    case '0':
                        state = 5;
                        break;
                    case '-':
                        sign = -1;
                        state = 1;
                        break;
                    case '1' ... '9':
                        switch (c) {
                            case '1': number_add(value, 1); break;
                            case '2': number_add(value, 2); break;
                            case '3': number_add(value, 3); break;
                            case '4': number_add(value, 4); break;
                            case '5': number_add(value, 5); break;
                            case '6': number_add(value, 6); break;
                            case '7': number_add(value, 7); break;
                            case '8': number_add(value, 8); break;
                            case '9': number_add(value, 9); break;
                        }
                        state = 10;
                        break;
                    default:
                        return 1;
                }
                break;
            case 1:
                switch (c) {
                    case '0':
                        state = 5;
                        break;
                    case '1' ... '9':
                        switch (c) {
                            case '1': number_add(value, 1); break;
                            case '2': number_add(value, 2); break;
                            case '3': number_add(value, 3); break;
                            case '4': number_add(value, 4); break;
                            case '5': number_add(value, 5); break;
                            case '6': number_add(value, 6); break;
                            case '7': number_add(value, 7); break;
                            case '8': number_add(value, 8); break;
                            case '9': number_add(value, 9); break;
                        }
                        state = 10;
                        break;
                    default:
                        return 1;
                }
                break;
            case 5:
                switch (c) {
                    case 'x':
                        state = 20;
                        break;
                    default:
                        return 1;
                }
                break;
            case 10:
                switch (c) {
                    case '0': number_mul(value, 10); number_add(value, 0); break;
                    case '1': number_mul(value, 10); number_add(value, 1); break;
                    case '2': number_mul(value, 10); number_add(value, 2); break;
                    case '3': number_mul(value, 10); number_add(value, 3); break;
                    case '4': number_mul(value, 10); number_add(value, 4); break;
                    case '5': number_mul(value, 10); number_add(value, 5); break;
                    case '6': number_mul(value, 10); number_add(value, 6); break;
                    case '7': number_mul(value, 10); number_add(value, 7); break;
                    case '8': number_mul(value, 10); number_add(value, 8); break;
                    case '9': number_mul(value, 10); number_add(value, 9); break;
                    default:
                        return 1;
                }
                break;
            case 16:
                switch (c) {
                    case '0': number_mul(value, 16); number_add(value, 0); break;
                    case '1': number_mul(value, 16); number_add(value, 1); break;
                    case '2': number_mul(value, 16); number_add(value, 2); break;
                    case '3': number_mul(value, 16); number_add(value, 3); break;
                    case '4': number_mul(value, 16); number_add(value, 4); break;
                    case '5': number_mul(value, 16); number_add(value, 5); break;
                    case '6': number_mul(value, 16); number_add(value, 6); break;
                    case '7': number_mul(value, 16); number_add(value, 7); break;
                    case '8': number_mul(value, 16); number_add(value, 8); break;
                    case '9': number_mul(value, 16); number_add(value, 9); break;
                    case 'A': number_mul(value, 16); number_add(value, 10); break;
                    case 'B': number_mul(value, 16); number_add(value, 11); break;
                    case 'C': number_mul(value, 16); number_add(value, 12); break;
                    case 'D': number_mul(value, 16); number_add(value, 13); break;
                    case 'E': number_mul(value, 16); number_add(value, 14); break;
                    case 'F': number_mul(value, 16); number_add(value, 15); break;
                    case 'a': number_mul(value, 16); number_add(value, 10); break;
                    case 'b': number_mul(value, 16); number_add(value, 11); break;
                    case 'c': number_mul(value, 16); number_add(value, 12); break;
                    case 'd': number_mul(value, 16); number_add(value, 13); break;
                    case 'e': number_mul(value, 16); number_add(value, 14); break;
                    case 'f': number_mul(value, 16); number_add(value, 15); break;
                    default:
                        return 1;
                }
                break;
            case 20:
                switch (c) {
                    case '0':
                        state = 25;
                        break;
                    case '1' ... '9':
                    case 'A' ... 'F':
                    case 'a' ... 'f':
                        switch (c) {
                            case '0': number_add(value, 0); break;
                            case '1': number_add(value, 1); break;
                            case '2': number_add(value, 2); break;
                            case '3': number_add(value, 3); break;
                            case '4': number_add(value, 4); break;
                            case '5': number_add(value, 5); break;
                            case '6': number_add(value, 6); break;
                            case '7': number_add(value, 7); break;
                            case '8': number_add(value, 8); break;
                            case '9': number_add(value, 9); break;
                            case 'A': number_add(value, 10); break;
                            case 'B': number_add(value, 11); break;
                            case 'C': number_add(value, 12); break;
                            case 'D': number_add(value, 13); break;
                            case 'E': number_add(value, 14); break;
                            case 'F': number_add(value, 15); break;
                            case 'a': number_add(value, 10); break;
                            case 'b': number_add(value, 11); break;
                            case 'c': number_add(value, 12); break;
                            case 'd': number_add(value, 13); break;
                            case 'e': number_add(value, 14); break;
                            case 'f': number_add(value, 15); break;
                        }
                        state = 16;
                        break;
                }
                break;
        }
    }
    switch (state) {
        case 5:
        case 25:
            return 0;
        case 10:
        case 16:
            number_mul(value, sign);
            return 0;
        default:
            // fprintf(stderr, "%s:%d:%d: invalid number literal %s\n", context->filename, token->lineno, token->colno, token->value);
            return 1;
    }
}


static const char* _symbols128[128];
static const char* _symbols_tok[32];

static void
_icfp_parser_init_symbols(struct _ParserState* context) {
    struct _SymbolList* list = &context->symbols;
    _symbols128[0] = NULL;
    char s[2] = "_";
    for (int c = 1; c < 128; ++c) {
        s[0] = c;
        char* p = symbol_list_push(list, s);
        _symbols128[c] = p;
    }
    _symbols_tok[_TokenType_bool_false] = symbol_list_push(list, "false");
    _symbols_tok[_TokenType_bool_true] = symbol_list_push(list, "true");
}


static void
symbolicate_token(struct _ParserState* context, struct _Token* token) {
    switch (token->type) {
        case _TokenType_bool_false:
        case _TokenType_bool_true:
            token->value = (char*) _symbols_tok[token->type];
            break;
        case _TokenType_invalid:
        case _TokenType_eof:
            break;
        case _TokenType_open_paren:
        case _TokenType_close_paren:
        case _TokenType_identifier:
        case _TokenType_number:
        case _TokenType_str:
        case _TokenType_str_start:
        case _TokenType_str_data:
        case _TokenType_str_end:
            switch (token->len) {
                case 0:
                    break;
                case 1: {
                    int c = token->value[0];
                    if (c > 127) {
                        fprintf(stderr, "%s:%d:%d: invalid token %c\n", context->filename, token->lineno, token->colno, c);
                        abort();
                    }
                    token->value = (char*) _symbols128[c];
                    break;
                }
                default:
                    token->value = symbol_list_push(&context->symbols, token->value);
                    break;
            }
    }
}


static int
_icfp_parser_tokenize(struct _ParserState* context, FILE* file, struct _Token* token) {
    token_init(token, context->token_bufsize, context->token_buf);
    for (;;) {
        token->lineno = context->lineno;
        token->colno = context->colno;
        int c = fgetc(file);
        if (c == EOF) {
            token->type = _TokenType_eof;
            return 0;
        }
        switch (c) {
            case ' ':
            case '\t':
            case '\r':
                context->colno += 1;
                break;
            case '\n':
                context->lineno += 1;
                context->colno = 1;
                break;
            case '(':
                token->type = _TokenType_open_paren;
                token->value = (char*) _symbols128[c];
                token->len = 1;
                context->colno += 1;
                return 0;
            case ')':
                token->type = _TokenType_close_paren;
                token->value = (char*) _symbols128[c];
                token->len = 1;
                context->colno += 1;
                return 0;
            case '"': {
                context->colno += 1;
                int res = _icfp_parser_read_str_data(context, file, token);
                if (res != 0) { return res; }
                symbolicate_token(context, token);
                if (token->type == _TokenType_str_end) {
                    token->type = _TokenType_str;
                    return 0;
                }
                struct _Token temp;
                token_init(&temp, context->token_bufsize, context->token_buf);
                for (;;) {
                    temp.lineno = context->lineno;
                    temp.colno = context->colno;
                    int res = _icfp_parser_read_str_data(context, file, &temp);
                    if (res != 0) { return res; }
                    token->value = symbol_list_concat(&context->symbols, token->value, temp.value);
                    token->len += temp.len;
                    if (temp.type == _TokenType_str_end) {
                        break;
                    }
                }
                token->type = _TokenType_str;
                return 0;
            }
            case '{': {
                context->colno += 1;
                int res = _icfp_parser_skip_comment(context, file, token);
                if (res != 0) { return res; }
                break;
            }
            case 0x21:
            case 0x23 ... 0x27:
            case 0x2a ... 0x7a:
            case 0x7c:
            case 0x7e: {
                token->value[token->len++] = c;
                token->value[token->len] = '\0';
                token->type = _TokenType_identifier;
                context->colno += 1;
                int res = _icfp_parser_read_identifier(context, file, token);
                if (res != 0) { return res; }

                if (strcmp(token->value, "false") == 0) {
                    token->type = _TokenType_bool_false;
                }
                else if (strcmp(token->value, "true") == 0) {
                    token->type = _TokenType_bool_true;
                }
                else {
                    struct _Number num;
                    res = _icfp_parser_parse_number(token->value, &num);
                    if (res == 0) {
                        token->type = _TokenType_number;
                    }
                }
                symbolicate_token(context, token);
                return 0;
            }
            default:
                fprintf(stderr, "%s:%d:%d: invalid char %c\n", context->filename, context->lineno, context->colno, c);
                return 1;
        }
    }
}


static int
_icfp_write_str_data(const char* s, FILE* file) {
    char c = *s;
    int res = 0;
    for (; c != '\0'; c = *++s) {
        switch (c) {
            case 'a': res = fputc('!', file); break;
            case 'b': res = fputc('"', file); break;
            case 'c': res = fputc('#', file); break;
            case 'd': res = fputc('$', file); break;
            case 'e': res = fputc('%', file); break;
            case 'f': res = fputc('&', file); break;
            case 'g': res = fputc('\'', file); break;
            case 'h': res = fputc('(', file); break;
            case 'i': res = fputc(')', file); break;
            case 'j': res = fputc('*', file); break;
            case 'k': res = fputc('+', file); break;
            case 'l': res = fputc(',', file); break;
            case 'm': res = fputc('-', file); break;
            case 'n': res = fputc('.', file); break;
            case 'o': res = fputc('/', file); break;
            case 'p': res = fputc('0', file); break;
            case 'q': res = fputc('1', file); break;
            case 'r': res = fputc('2', file); break;
            case 's': res = fputc('3', file); break;
            case 't': res = fputc('4', file); break;
            case 'u': res = fputc('5', file); break;
            case 'v': res = fputc('6', file); break;
            case 'w': res = fputc('7', file); break;
            case 'x': res = fputc('8', file); break;
            case 'y': res = fputc('9', file); break;
            case 'z': res = fputc(':', file); break;
            case 'A': res = fputc(';', file); break;
            case 'B': res = fputc('<', file); break;
            case 'C': res = fputc('=', file); break;
            case 'D': res = fputc('>', file); break;
            case 'E': res = fputc('?', file); break;
            case 'F': res = fputc('@', file); break;
            case 'G': res = fputc('A', file); break;
            case 'H': res = fputc('B', file); break;
            case 'I': res = fputc('C', file); break;
            case 'J': res = fputc('D', file); break;
            case 'K': res = fputc('E', file); break;
            case 'L': res = fputc('F', file); break;
            case 'M': res = fputc('G', file); break;
            case 'N': res = fputc('H', file); break;
            case 'O': res = fputc('I', file); break;
            case 'P': res = fputc('J', file); break;
            case 'Q': res = fputc('K', file); break;
            case 'R': res = fputc('L', file); break;
            case 'S': res = fputc('M', file); break;
            case 'T': res = fputc('N', file); break;
            case 'U': res = fputc('O', file); break;
            case 'V': res = fputc('P', file); break;
            case 'W': res = fputc('Q', file); break;
            case 'X': res = fputc('R', file); break;
            case 'Y': res = fputc('S', file); break;
            case 'Z': res = fputc('T', file); break;
            case '0': res = fputc('U', file); break;
            case '1': res = fputc('V', file); break;
            case '2': res = fputc('W', file); break;
            case '3': res = fputc('X', file); break;
            case '4': res = fputc('Y', file); break;
            case '5': res = fputc('Z', file); break;
            case '6': res = fputc('[', file); break;
            case '7': res = fputc('\\', file); break;
            case '8': res = fputc(']', file); break;
            case '9': res = fputc('^', file); break;
            case '!': res = fputc('_', file); break;
            case '"': res = fputc('`', file); break;
            case '#': res = fputc('a', file); break;
            case '$': res = fputc('b', file); break;
            case '%': res = fputc('c', file); break;
            case '&': res = fputc('d', file); break;
            case '\'': res = fputc('e', file); break;
            case '(': res = fputc('f', file); break;
            case ')': res = fputc('g', file); break;
            case '*': res = fputc('h', file); break;
            case '+': res = fputc('i', file); break;
            case ',': res = fputc('j', file); break;
            case '-': res = fputc('k', file); break;
            case '.': res = fputc('l', file); break;
            case '/': res = fputc('m', file); break;
            case ':': res = fputc('n', file); break;
            case ';': res = fputc('o', file); break;
            case '<': res = fputc('p', file); break;
            case '=': res = fputc('q', file); break;
            case '>': res = fputc('r', file); break;
            case '?': res = fputc('s', file); break;
            case '@': res = fputc('t', file); break;
            case '[': res = fputc('u', file); break;
            case '\\': res = fputc('v', file); break;
            case ']': res = fputc('w', file); break;
            case '^': res = fputc('x', file); break;
            case '_': res = fputc('y', file); break;
            case '`': res = fputc('z', file); break;
            case '|': res = fputc('{', file); break;
            case '~': res = fputc('|', file); break;
            case ' ': res = fputc('}', file); break;
            case '\n': res = fputc('~', file); break;
            default:
                fprintf(stderr, "! invalid char in string literal %c\n", c);
                abort();
        }
        if (res == EOF) {
            perror(NULL);
            return 1;
        }
    }
    return 0;
}


static int
_icfp_write_str_start(const char* s, FILE* file) {
    int res = fputc('S', file);
    if (res == EOF) {
        perror(NULL);
        return 1;
    }
    return _icfp_write_str_data(s, file);
}


static int
_icfp_write_str_end(const char* s, FILE* file) {
    return _icfp_write_str_data(s, file);
}


static int
_icfp_write_str(const char* s, FILE* file) {
    return _icfp_write_str_start(s, file);
}


static int
_icfp_write_bool(enum _TokenType value, FILE* file) {
    int res;
    switch (value) {
        case _TokenType_bool_false:
            res = fputc('F', file);
            return res == EOF;
        case _TokenType_bool_true:
            res = fputc('T', file);
            return res == EOF;
        default:
            abort();
    }
}


static int
_icfp_write_number(struct _Number* num, FILE* file) {
    int64_t x = num->value;
    if (x == 0) {
        int res = fputs("I!", file);
        if (res == EOF) { perror(NULL); return 1; }
        return 0;
    }

    char buf[32];
    size_t bufsize = sizeof(buf);
    char* p = &buf[bufsize-1];
    *p = '\0';

    if (number_is_neg(num)) {
        int res = fputs("U- ", file);
        if (res == EOF) { perror(NULL); return 1; }
        x = -x;
    }
    for (; x != 0; x /= 94) {
        int v = x % 94 + 33;
        *--p = v;
    }
    *--p = 'I';
    int res = fputs(p, file);
    if (res == EOF) { perror(NULL); return 1; }
    return 0;
}


static int
_icfp_write_expression(struct _WriterState* context, struct _Expr* expr, struct _NameTable* nametable);


static int
_icfp_write_resolved_name(struct _WriterState* context, struct _Name* name, struct _NameTable* nametable) {
    struct _Expr* expr = name->expr;
    if (expr == NULL) {
        int res = fputc('v', context->file);
        if (res == EOF) { perror(NULL); return 1; }
        res = fputs(name->name, context->file);
        if (res == EOF) { perror(NULL); return 1; }
        return 0;
    }
    if (expr->type == _ExprType_identifier) {
        int res = fputc('v', context->file);
        if (res == EOF) { perror(NULL); return 1; }
        res = fputs(expr->token.value, context->file);
        if (res == EOF) { perror(NULL); return 1; }
        return 0;
    }
    int res = _icfp_write_expression(context, expr, nametable);
    return res;
}


static int
_icfp_write_expr_apply1(struct _WriterState* context, struct _Expr* expr, struct _NameTable* nametable) {
    switch (expr->expr0->type) {
        case _ExprType_identifier: {
            struct _Token* token = &expr->expr0->token;
            if (token->len == 1) {
                char c = token->value[0];
                switch (c) {
                    case '-':
                    case '!':
                    case '#':
                    case '$': {
                        char s[4] = "U_ ";
                        s[1] = c;
                        int res = fputs(s, context->file);
                        if (res == EOF) { perror(NULL); return 1; }
                        return _icfp_write_expression(context, expr->expr1, nametable);
                    }
                    default:
                        break;
                }
            }
            struct _Name* resolved_name;
            int res = name_table_resolve(nametable, token, &resolved_name);
            if (res != 0) { return res; }
            res = fputs("B$ ", context->file);
            if (res == EOF) { perror(NULL); return 1; }
            res = _icfp_write_resolved_name(context, resolved_name, nametable);
            if (res != 0) { return res; }
            res = fputc(' ', context->file);
            if (res == EOF) { perror(NULL); return 1; }
            return _icfp_write_expression(context, expr->expr1, nametable);
        }
        case _ExprType_apply1:
        case _ExprType_apply2:
        case _ExprType_apply3: {
            int res = fputs("B$ ", context->file);
            if (res == EOF) { perror(NULL); return 1; }
            res = _icfp_write_expression(context, expr->expr0, nametable);
            if (res != 0) { return res; }
            res = fputc(' ', context->file);
            if (res == EOF) { perror(NULL); return 1; }
            return _icfp_write_expression(context, expr->expr1, nametable);
        }
        case _ExprType_lambda: {
            int res;
            struct _Expr* args = expr->expr0->expr1;
            enum _ExprType arity = args->type;
            switch (arity) {
                case _ExprType_apply3:
                    res = fputs("B$ ", context->file);
                    if (res == EOF) { perror(NULL); return 1; }
                    // fallthrough
                case _ExprType_apply2:
                    res = fputs("B$ ", context->file);
                    if (res == EOF) { perror(NULL); return 1; }
                    // fallthrough
                case _ExprType_apply1:
                    res = fputs("B$ ", context->file);
                    if (res == EOF) { perror(NULL); return 1; }
                    break;
                default:
                    fprintf(stderr, "! invalid lambda arity %d\n", arity);
                    abort();
            }
            res = _icfp_write_expression(context, expr->expr0, nametable);
            if (res != 0) { return res; }
            res = fputc(' ', context->file);
            if (res == EOF) { perror(NULL); return 1; }
            return _icfp_write_expression(context, expr->expr1, nametable);
        }
        default:
            fprintf(stderr, "! invalid expression to apply %d\n", expr->expr0->type);
            abort();
    }
}


static int
_icfp_write_expression(struct _WriterState* context, struct _Expr* expr, struct _NameTable* nametable) {
    FILE* file = context->file;
    switch (expr->type) {
        case _ExprType_identifier: {
            struct _Name* resolved_name;
            int res = name_table_resolve(nametable, &expr->token, &resolved_name);
            if (res != 0) { return res; }
            res = _icfp_write_resolved_name(context, resolved_name, nametable);
            if (res != 0) { return res; }
            return 0;
        }
        case _ExprType_apply1:
            return _icfp_write_expr_apply1(context, expr, nametable);
        case _ExprType_apply2: {
            struct _Token* token = &expr->expr0->token;
            if (token->len == 1) {
                char c = token->value[0];
                switch (c) {
                    case '+':
                    case '-':
                    case '*':
                    case '/':
                    case '%':
                    case '<':
                    case '>':
                    case '=':
                    case '|':
                    case '&':
                    case '.':
                    case 'T':
                    case 'D':
                    case '$': {
                        char s[4] = "B_ ";
                        s[1] = c;
                        int res = fputs(s, file);
                        if (res == EOF) { perror(NULL); return 1; }
                        res = _icfp_write_expression(context, expr->expr1, nametable);
                        if (res != 0) { return res; }
                        res = fputc(' ', file);
                        if (res == EOF) { perror(NULL); return 1; }
                        return _icfp_write_expression(context, expr->expr2, nametable);
                    }
                    default:
                        break;
                }
            }
            struct _Name* resolved_name;
            int res = name_table_resolve(nametable, token, &resolved_name);
            if (res != 0) { return res; }
            res = fputs("B$ ", file);
            if (res == EOF) { perror(NULL); return 1; }
            res = fputs("B$ ", file);
            if (res == EOF) { perror(NULL); return 1; }
            res = _icfp_write_resolved_name(context, resolved_name, nametable);
            if (res != 0) { return res; }
            res = fputc(' ', file);
            if (res == EOF) { perror(NULL); return 1; }
            res = _icfp_write_expression(context, expr->expr1, nametable);
            if (res != 0) { return res; }
            res = fputc(' ', file);
            if (res == EOF) { perror(NULL); return 1; }
            return _icfp_write_expression(context, expr->expr2, nametable);
        }
        case _ExprType_apply3: {
            struct _Token* token = &expr->expr0->token;
            if (token->len == 1) {
                char c = token->value[0];
                switch (c) {
                    case '?': {
                        int res = fputc('?', file);
                        if (res == EOF) { perror(NULL); return 1; }
                        res = fputc(' ', file);
                        if (res == EOF) { perror(NULL); return 1; }
                        res = _icfp_write_expression(context, expr->expr1, nametable);
                        if (res != 0) { return res; }
                        res = fputc(' ', file);
                        if (res == EOF) { perror(NULL); return 1; }
                        res = _icfp_write_expression(context, expr->expr2, nametable);
                        if (res != 0) { return res; }
                        res = fputc(' ', file);
                        if (res == EOF) { perror(NULL); return 1; }
                        return _icfp_write_expression(context, expr->expr3, nametable);
                    }
                    default:
                        break;
                }
            }
            fprintf(stderr, "! apply3 not implemented\n");
            abort();
        }
        case _ExprType_lambda: {
            struct _Expr* args = expr->expr1;
            enum _ExprType arity = args->type;
            struct _NameTable* body_nametable = name_table_add_child(nametable);
            switch (arity) {
                case _ExprType_apply1: {
                    int res = fputc('L', file);
                    if (res == EOF) { perror(NULL); return 1; }
                    res = fputs(args->expr1->token.value, file);
                    if (res == EOF) { perror(NULL); return 1; }
                    res = fputc(' ', file);
                    if (res == EOF) { perror(NULL); return 1; }
                    name_table_put(body_nametable, args->expr1->token.value, NULL);
                    break;
                }
                case _ExprType_apply2: {
                    int res = fputc('L', file);
                    if (res == EOF) { perror(NULL); return 1; }
                    res = fputs(args->expr1->token.value, file);
                    if (res == EOF) { perror(NULL); return 1; }
                    res = fputc(' ', file);
                    if (res == EOF) { perror(NULL); return 1; }
                    name_table_put(body_nametable, args->expr1->token.value, NULL);

                    res = fputc('L', file);
                    if (res == EOF) { perror(NULL); return 1; }
                    res = fputs(args->expr2->token.value, file);
                    if (res == EOF) { perror(NULL); return 1; }
                    res = fputc(' ', file);
                    if (res == EOF) { perror(NULL); return 1; }
                    name_table_put(body_nametable, args->expr2->token.value, NULL);
                    break;
                }
                default:
                    fprintf(stderr, "unhandled lambda arity %d\n", arity);
                    abort();
            }
            return _icfp_write_expression(context, expr->expr2, body_nametable);
        }
        case _ExprType_literal:
            switch (expr->token.type) {
                case _TokenType_str:
                    return _icfp_write_str(expr->token.value, file);
                case _TokenType_number: {
                    struct _Number num;
                    int res = _icfp_parser_parse_number(expr->token.value, &num);
                    if (res != 0) { return res; }
                    res = _icfp_write_number(&num, file);
                    return res;
                }
                case _TokenType_bool_false:
                case _TokenType_bool_true:
                    return _icfp_write_bool(expr->token.type, file);
                case _TokenType_invalid:
                case _TokenType_eof:
                case _TokenType_open_paren:
                case _TokenType_close_paren:
                case _TokenType_identifier:
                case _TokenType_str_start:
                case _TokenType_str_data:
                case _TokenType_str_end:
                    abort();
            }
            break;
        case _ExprType_assert: {
            int res = fprintf(file, "AT ");
            if (res < 0) {
                perror(NULL);
                return 1;
            }
            return _icfp_write_expression(context, expr->expr1, nametable);
        }
        case _ExprType_define:
        case _ExprType_invalid:
            fprintf(stderr, "! write of invalid expression\n");
            return 1;
    }
    return 1;
}


struct _Nesting {
    int level;
    int popped;
};


static void
dump_arg_list(struct _Expr* expr, FILE* file) {
    switch (expr->type) {
        case _ExprType_apply1:
            fputc('(', file);
            if (expr->expr0->type == _ExprType_identifier) {
                fputs(expr->expr0->token.value, file);
                fputc(' ', file);
            }
            fputs(expr->expr1->token.value, file);
            fputc(')', file);
            break;
        case _ExprType_apply2:
            fputc('(', file);
            if (expr->expr0->type == _ExprType_identifier) {
                fputs(expr->expr0->token.value, file);
                fputc(' ', file);
            }
            fputs(expr->expr1->token.value, file);
            fputc(' ', file);
            fputs(expr->expr2->token.value, file);
            fputc(')', file);
            break;
        case _ExprType_apply3:
            fputc('(', file);
            if (expr->expr0->type == _ExprType_identifier) {
                fputs(expr->expr0->token.value, file);
                fputc(' ', file);
            }
            fputs(expr->expr1->token.value, file);
            fputc(' ', file);
            fputs(expr->expr2->token.value, file);
            fputc(' ', file);
            fputs(expr->expr3->token.value, file);
            fputc(')', file);
            break;
        case _ExprType_identifier:
        case _ExprType_literal:
        case _ExprType_lambda:
        case _ExprType_define:
        case _ExprType_assert:
        case _ExprType_invalid:
            fprintf(file, "expr:err%d", expr->type);
            break;
    }
}


static void
dump_expr(struct _Expr* expr, FILE* file) {
    switch (expr->type) {
        case _ExprType_invalid:
            fprintf(file, "expr:err%d", expr->type);
            break;
        case _ExprType_identifier:
            fprintf(file, "expr:id %s", expr->token.value);
            break;
        case _ExprType_literal:
            fprintf(file, "expr:literal %s", expr->token.value);
            break;
        case _ExprType_apply1:
            fprintf(file, "expr:apply (");
            dump_expr(expr->expr0, file);
            fputc(')', file);
            fputc(' ', file);
            fputc('(', file);
            dump_expr(expr->expr1, file);
            fputc(')', file);
            break;
        case _ExprType_apply2:
            fprintf(file, "expr:apply (");
            dump_expr(expr->expr0, file);
            fputc(')', file);
            fputc(' ', file);
            fputc('(', file);
            dump_expr(expr->expr1, file);
            fputc(')', file);
            fputc(' ', file);
            fputc('(', file);
            dump_expr(expr->expr2, file);
            fputc(')', file);
            break;
        case _ExprType_apply3:
            fprintf(file, "expr:apply (");
            dump_expr(expr->expr0, file);
            fputc(')', file);
            fputc(' ', file);
            fputc('(', file);
            dump_expr(expr->expr1, file);
            fputc(')', file);
            fputc(' ', file);
            fputc('(', file);
            dump_expr(expr->expr2, file);
            fputc(')', file);
            fputc(' ', file);
            fputc('(', file);
            dump_expr(expr->expr3, file);
            fputc(')', file);
            break;
        case _ExprType_lambda:
            fprintf(file, "expr:lambda ");
            dump_arg_list(expr->expr1, file);
            fputc(' ', file);
            fputc('(', file);
            dump_expr(expr->expr2, file);
            fputc(')', file);
            break;
        case _ExprType_define:
            fprintf(file, "expr:define ");
            dump_arg_list(expr->expr1, file);
            fputc(' ', file);
            fputc('(', file);
            dump_expr(expr->expr2, file);
            fputc(')', file);
            break;
        case _ExprType_assert:
            fprintf(file, "expr:assert (");
            dump_expr(expr->expr0, file);
            fputc(')', file);
            fputc(' ', file);
            fputc('(', file);
            dump_expr(expr->expr1, file);
            fputc(')', file);
            break;
    }
}


static int
_icfp_parser_parse_expression(struct _ParserState* context, FILE* file,
    struct _Nesting* nesting, struct _Expr** parsed_expr);


static int
_icfp_parser_parse_arg_list(struct _ParserState* context, FILE* file,
    struct _Nesting* nesting, int minargs, struct _Expr** expr) {

    struct _Token token;
    struct _Expr* args;
    struct _Expr* nested;
    int state = 0;
    for (;;) {
        int res = _icfp_parser_tokenize(context, file, &token);
        if (res != 0) { return -1; }
        if (context->verbose) {
            fprintf(stderr, "%s:%d:%d: token %d %s\n", context->filename, token.lineno, token.colno, token.type, token.value);
        }
        switch (state) {
            case 0:
                switch (token.type) {
                    case _TokenType_open_paren:
                        args = expr_tree_push(&context->expr_tree);
                        args->token = token;
                        args->lineno = token.lineno;
                        args->colno = token.colno;
                        args->expr0 = expr_tree_push(&context->expr_tree);
                        state = 1;
                        break;
                    default:
                        fprintf(stderr, "%s:%d:%d: expecting an argument list\n", context->filename, token.lineno, token.colno);
                        return 1;
                }
                break;
            case 1:
                switch (token.type) {
                    case _TokenType_identifier:
                        nested = expr_tree_push(&context->expr_tree);
                        nested->type = _ExprType_identifier;
                        nested->token = token;
                        nested->lineno = token.lineno;
                        nested->colno = token.colno;
                        args->expr1 = nested;
                        state = 2;
                        break;
                    case _TokenType_close_paren:
                        if (minargs > 0) {
                            fprintf(stderr, "%s:%d:%d: expecting an identifier\n", context->filename, token.lineno, token.colno);
                            return 1;
                        }
                        *expr = args;
                        fprintf(stderr, "%s:%d:%d: empty arg list\n", context->filename, token.lineno, token.colno);
                        abort();
                        return 0;
                    default:
                        fprintf(stderr, "%s:%d:%d: expecting an identifier\n", context->filename, token.lineno, token.colno);
                        return 1;
                }
                break;
            case 2:
                switch (token.type) {
                    case _TokenType_identifier:
                        nested = expr_tree_push(&context->expr_tree);
                        nested->type = _ExprType_identifier;
                        nested->token = token;
                        nested->lineno = token.lineno;
                        nested->colno = token.colno;
                        args->type = _ExprType_apply1;
                        args->expr2 = nested;
                        state = 3;
                        break;
                    case _TokenType_close_paren:
                        if (minargs > 1) {
                            fprintf(stderr, "%s:%d:%d: expecting an identifier\n", context->filename, token.lineno, token.colno);
                            return 1;
                        }
                        args->type = _ExprType_apply1;
                        *expr = args;
                        return 0;
                    default:
                        fprintf(stderr, "%s:%d:%d: expecting an identifier\n", context->filename, token.lineno, token.colno);
                        return 1;
                }
                break;
            case 3:
                switch (token.type) {
                    case _TokenType_identifier:
                        nested = expr_tree_push(&context->expr_tree);
                        nested->type = _ExprType_identifier;
                        nested->token = token;
                        nested->lineno = token.lineno;
                        nested->colno = token.colno;
                        args->type = _ExprType_apply2;
                        args->expr3 = nested;
                        state = 3;
                        break;
                    case _TokenType_close_paren:
                        args->type = _ExprType_apply2;
                        *expr = args;
                        return 0;
                    default:
                        fprintf(stderr, "%s:%d:%d: expecting an identifier\n", context->filename, token.lineno, token.colno);
                        return 1;
                }
                break;
            case 4:
                switch (token.type) {
                    case _TokenType_close_paren:
                        args->type = _ExprType_apply3;
                        *expr = args;
                        return 0;
                    default:
                        fprintf(stderr, "%s:%d:%d: expecting a closing paren\n", context->filename, token.lineno, token.colno);
                        return 1;
                }
                break;
        }
    }
    abort();
}


static int
_icfp_parser_parse_lambda(struct _ParserState* context, FILE* file,
    struct _Nesting* nesting, struct _Expr* expr) {

    struct _Expr* nested;
    struct _Expr* args;
    int minargs = 1;
    int res = _icfp_parser_parse_arg_list(context, file, nesting, minargs, &args);
    if (res != 0) { return res; }

    expr->expr1 = args;

    struct _Nesting deeper = {};
    res = _icfp_parser_parse_expression(context, file, &deeper, &nested);
    if (res == 0) {
        fprintf(stderr, "%s:%d:%d: expecting expression\n", context->filename, context->lineno, context->colno);
        return 1;
    }
    if (res != 1) { return 1; }
    expr->expr2 = nested;

    struct _Token token;
    res = _icfp_parser_tokenize(context, file, &token);
    if (res != 0) { return -1; }
    if (context->verbose) {
        fprintf(stderr, "%s:%d:%d: token %d %s\n", context->filename, token.lineno, token.colno, token.type, token.value);
    }
    switch (token.type) {
        case _TokenType_close_paren:
            expr->type = _ExprType_lambda;
            return 0;
        default:
            fprintf(stderr, "%s:%d:%d: expecting a closing paren\n", context->filename, token.lineno, token.colno);
            return 1;
    }
}


static int
_icfp_parser_parse_define(struct _ParserState* context, FILE* file,
    struct _Nesting* nesting, struct _Expr* expr) {

    struct _Expr* nested;
    struct _Expr* args;
    int minargs = 2;
    int res = _icfp_parser_parse_arg_list(context, file, nesting, minargs, &args);
    if (res != 0) { return res; }

    args->expr0 = args->expr1;
    args->expr1 = args->expr2;
    args->expr2 = args->expr3;
    args->expr3 = NULL;
    switch (args->type) {
        case _ExprType_apply2:
            args->type = _ExprType_apply1;
            break;
        case _ExprType_apply3:
            args->type = _ExprType_apply2;
            break;
        default:
            abort();
    }
    expr->expr1 = args;

    struct _Nesting deeper = {};
    res = _icfp_parser_parse_expression(context, file, &deeper, &nested);
    if (res == 0) {
        fprintf(stderr, "%s:%d:%d: expecting expression\n", context->filename, context->lineno, context->colno);
        return 1;
    }
    if (res != 1) { return 1; }
    expr->expr2 = nested;

    struct _Token token;
    res = _icfp_parser_tokenize(context, file, &token);
    if (res != 0) { return -1; }
    if (context->verbose) {
        fprintf(stderr, "%s:%d:%d: token %d %s\n", context->filename, token.lineno, token.colno, token.type, token.value);
    }
    switch (token.type) {
        case _TokenType_close_paren:
            expr->type = _ExprType_define;
            return 0;
        default:
            fprintf(stderr, "%s:%d:%d: expecting a closing paren\n", context->filename, token.lineno, token.colno);
            return 1;
    }
}


static int
_icfp_parser_parse_expression_body(struct _ParserState* context, FILE* file,
    struct _Nesting* nesting, struct _Expr* expr) {

    struct _Expr* nested;
    nesting->popped = 0;
    int res = _icfp_parser_parse_expression(context, file, nesting, &nested);
    if (res == 0) {
        fprintf(stderr, "%s:%d:%d: expecting expression\n", context->filename, context->lineno, context->colno);
        return 1;
    }
    if (res != 1) { return 1; }
    switch (nested->type) {
        case _ExprType_identifier:
            expr->expr0 = nested;
            if (strcmp(nested->token.value, "\\") == 0) {
                return _icfp_parser_parse_lambda(context, file, nesting, expr);
            }
            else if (strcmp(nested->token.value, "define") == 0) {
                return _icfp_parser_parse_define(context, file, nesting, expr);
            }
            break;
        case _ExprType_apply1:
        case _ExprType_apply2:
        case _ExprType_apply3:
        case _ExprType_lambda:
            expr->expr0 = nested;
            break;
        case _ExprType_literal:
        case _ExprType_define:
        case _ExprType_assert:
        case _ExprType_invalid:
            fprintf(stderr, "%s:%d:%d: expecting identifier\n", context->filename, nested->lineno, nested->colno);
            return 1;
    }

    nesting->popped = 0;
    res = _icfp_parser_parse_expression(context, file, nesting, &nested);
    if (res == 0) {
        fprintf(stderr, "%s:%d:%d: expecting expression\n", context->filename, context->lineno, context->colno);
        return 1;
    }
    if (res != 1) { return 1; }
    switch (nested->type) {
        case _ExprType_identifier:
        case _ExprType_apply1:
        case _ExprType_apply2:
        case _ExprType_apply3:
        case _ExprType_literal:
        case _ExprType_lambda:
            expr->expr1 = nested;
            break;
        case _ExprType_define:
        case _ExprType_assert:
        case _ExprType_invalid:
            fprintf(stderr, "%s:%d:%d: expecting identifier\n", context->filename, nested->lineno, nested->colno);
            return 1;
    }

    nesting->popped = 0;
    res = _icfp_parser_parse_expression(context, file, nesting, &nested);
    if (res == 0 && nesting->popped != 0) {
        expr->type = _ExprType_apply1;
        if (strcmp(expr->expr0->token.value, "assert") == 0) {
            expr->type = _ExprType_assert;
        }
        return 0;
    }
    if (res != 1) { return 1; }
    switch (nested->type) {
        case _ExprType_identifier:
        case _ExprType_apply1:
        case _ExprType_apply2:
        case _ExprType_apply3:
        case _ExprType_literal:
            expr->expr2 = nested;
            break;
        case _ExprType_lambda:
        case _ExprType_define:
        case _ExprType_assert:
        case _ExprType_invalid:
            fprintf(stderr, "%s:%d:%d: expecting identifier\n", context->filename, nested->lineno, nested->colno);
            return 1;
    }

    nesting->popped = 0;
    res = _icfp_parser_parse_expression(context, file, nesting, &nested);
    if (res == 0 && nesting->popped != 0) {
        expr->type = _ExprType_apply2;
        if (strcmp(expr->expr0->token.value, "define") == 0) {
            expr->type = _ExprType_define;
        }
        return 0;
    }
    if (res != 1) { return 1; }
    switch (nested->type) {
        case _ExprType_identifier:
        case _ExprType_apply1:
        case _ExprType_apply2:
        case _ExprType_apply3:
        case _ExprType_literal:
            expr->expr3 = nested;
            break;
        case _ExprType_lambda:
        case _ExprType_define:
        case _ExprType_assert:
        case _ExprType_invalid:
            fprintf(stderr, "%s:%d:%d: expecting identifier\n", context->filename, nested->lineno, nested->colno);
            return 1;
    }

    nesting->popped = 0;
    res = _icfp_parser_parse_expression(context, file, nesting, &nested);
    if (res == 0 && nesting->popped != 0) {
        expr->type = _ExprType_apply3;
        return 0;
    }
    if (res != 1) { return 1; }
    fprintf(stderr, "%s:%d:%d: expecting close paren\n", context->filename, nested->lineno, nested->colno);
    return 1;
}


static void
_icfp_parser_log_expr(struct _ParserState* context, struct _Expr* expr, FILE* file) {
    fprintf(stderr, "%s:%d:%d: ", context->filename, expr->lineno, expr->colno);
    dump_expr(expr, stderr);
    fprintf(stderr, "\n");
}


static int
_icfp_parser_parse_expression(struct _ParserState* context, FILE* file,
    struct _Nesting* nesting, struct _Expr** parsed_expr) {

    struct _Token token;
    int res = _icfp_parser_tokenize(context, file, &token);
    if (res != 0) { return -1; }
    if (context->verbose) {
        fprintf(stderr, "%s:%d:%d: token %d %s\n", context->filename, token.lineno, token.colno, token.type, token.value);
    }
    switch (token.type) {
        case _TokenType_open_paren: {
            struct _Nesting deeper = {};
            deeper.level = nesting->level + 1;
            struct _Expr* expr = expr_tree_push(&context->expr_tree);
            expr->token = token;
            expr->lineno = token.lineno;
            expr->colno = token.colno;
            int res = _icfp_parser_parse_expression_body(context, file, &deeper, expr);
            if (res != 0) { return -1; }
            *parsed_expr = expr;
            if (context->verbose) {
                _icfp_parser_log_expr(context, expr, stderr);
            }
            return 1;
        }
        case _TokenType_close_paren:
            if (nesting->level > 0) {
                nesting->popped = 1;
                return 0;
            }
            else {
                fprintf(stderr, "%s:%d:%d: expecting expression %s\n", context->filename, token.lineno, token.colno, token.value);
                return -1;
            }
        case _TokenType_number:
        case _TokenType_bool_false:
        case _TokenType_bool_true:
        case _TokenType_str: {
            struct _Expr* expr = expr_tree_push(&context->expr_tree);
            expr->type = _ExprType_literal;
            expr->token = token;
            expr->lineno = token.lineno;
            expr->colno = token.colno;
            *parsed_expr = expr;
            if (context->verbose) {
                _icfp_parser_log_expr(context, expr, stderr);
            }
            return 1;
        }
        case _TokenType_identifier: {
            // fprintf(stderr, "%s:%d:%d: expecting expression %s\n", context->filename, token.lineno, token.colno, token.value);
            struct _Expr* expr = expr_tree_push(&context->expr_tree);
            expr->type = _ExprType_identifier;
            expr->token = token;
            expr->lineno = token.lineno;
            expr->colno = token.colno;
            *parsed_expr = expr;
            if (context->verbose) {
                _icfp_parser_log_expr(context, expr, stderr);
            }
            return 1;
        }
        case _TokenType_eof:
            if (nesting->level > 0) {
                fprintf(stderr, "%s:%d:%d: unexpected EOF\n", context->filename, token.lineno, token.colno);
                return -1;
            }
            return 0;
        case _TokenType_invalid:
        case _TokenType_str_start:
        case _TokenType_str_data:
        case _TokenType_str_end:
            fprintf(stderr, "%s:%d:%d: invalid token %c\n", context->filename, token.lineno, token.colno, token.value[0]);
            return -1;
    }
}


static int
icfp_parser_process(struct _ParserState* context, struct _WriterState* wstate, const char* filename, FILE* file) {
    context->filename = filename;
    context->lineno = 1;
    context->colno = 1;
    switch (wstate->out_format) {
        case 1:
            break;
        default:
            fprintf(stderr, "! unhandled output format %d\n", wstate->out_format);
            return 1;
    }

    struct _Expr* expr;
    int count = 0;
    for (;;) {
        if (count > 0) {
            fputc('\n', wstate->file);
        }
        struct _Nesting nesting = {};
        int res = _icfp_parser_parse_expression(context, file, &nesting, &expr);
        if (res != 1) { return res; }
        switch (expr->type) {
            case _ExprType_literal:
            case _ExprType_apply1:
            case _ExprType_apply2:
            case _ExprType_apply3:
            case _ExprType_lambda: {
                int res = _icfp_write_expression(wstate, expr, wstate->nametable);
                if (res != 0) { return res; }
                ++count;
                break;
            }
            case _ExprType_define: {
                struct _Expr* args = expr->expr1;
                struct _Expr* body = expr->expr2;
                const char* name = args->expr0->token.value;
                struct _Expr* lamb = expr_tree_push(&context->expr_tree);
                lamb->type = _ExprType_lambda;
                lamb->token = expr->token;
                lamb->lineno = expr->token.lineno;
                lamb->colno = expr->token.colno;
                lamb->expr0 = args->expr0;
                lamb->expr1 = args;
                lamb->expr2 = body;
                name_table_put(wstate->nametable, name, lamb);
                break;
            }
            case _ExprType_assert:
                if (context->out_asserts != 0) {
                    int res = _icfp_write_expression(wstate, expr, wstate->nametable);
                    if (res != 0) { return res; }
                    ++count;
                }
                break;
            case _ExprType_invalid:
            case _ExprType_identifier:
                fprintf(stderr, "%s:%d:%d: expecting expression\n", context->filename, expr->lineno, expr->colno);
                return 1;
        }
    }
    return 0;
}


struct _Config {
    int filename_count;
    const char* filenames[100];
    int verbose;
    int out_text;
    int out_asserts;
};


static int
_parse_args(int argc, const char* argv[], struct _Config* config) {
    config->filename_count = 0;
    config->verbose = 0;
    config->out_text = 1;
    config->out_asserts = 0;
    size_t fncap = sizeof(config->filenames) / sizeof(config->filenames[0]);
    int state = 0;
    for (size_t argi = 1; argi < argc; ++argi) {
        const char* arg = argv[argi];
        int narg = strlen(arg);
        switch (state) {
            case 0: {
                if (narg == 1 && arg[0] == '-') {
                    if (config->filename_count >= fncap) {
                        fprintf(stderr, "! filename limit reached, skipping %s\n", arg);
                    }
                    else {
                        config->filenames[config->filename_count++] = arg;
                    }
                }
                else if (narg > 0 && arg[0] == '-') {
                    if (
                        strcmp(arg, "-h") == 0 ||
                        strcmp(arg, "-help") == 0 ||
                        strcmp(arg, "--help") == 0
                    ) {
                        printf("%s", _usage);
                        exit(0);
                    }
                    else if (
                        strcmp(arg, "-v") == 0 ||
                        strcmp(arg, "--verbose") == 0
                    ) {
                        config->verbose = 1;
                    }
                    else if (
                        strcmp(arg, "-t") == 0 ||
                        strcmp(arg, "--text") == 0
                    ) {
                        config->out_text = 1;
                    }
                    else if (
                        strcmp(arg, "-a") == 0 ||
                        strcmp(arg, "--asserts") == 0
                    ) {
                        config->out_asserts = 1;
                    }
                    else {
                        fprintf(stderr, "! invalid option %s\n", arg);
                        fprintf(stderr, "%s\n", _usageq);
                        return 1;
                    }
                }
                else {
                    if (config->filename_count >= fncap) {
                        fprintf(stderr, "! filename limit reached, skipping %s\n", arg);
                    }
                    else {
                        config->filenames[config->filename_count++] = arg;
                    }
                }
                break;
            }
        }
    }
    if (config->filename_count == 0) {
        config->filenames[config->filename_count++] = "-";
    }
    return 0;
}


int main(int argc, const char* argv[]) {
    struct _Config config;

    int res = _parse_args(argc, argv, &config);
    if (res != 0) { return res; }

    FILE* out_file = stdout;

    char* symbs = (char*) calloc(_SymbolBufSizeMax, sizeof(char));
    size_t symb_size = _SymbolBufSizeMax;

    struct _Expr* exprs = (struct _Expr*) calloc(_ExprTreeSizeMax, sizeof(_Expr));
    size_t expr_size = _ExprTreeSizeMax;

    char* token_buf = (char*) calloc(_TokenSizeMax, sizeof(char));
    size_t token_bufsize = _TokenSizeMax;

    struct _Name* names_buf = (struct _Name*) calloc(_NamesSizeMax, sizeof(_Name));
    size_t names_bufsize = _NamesSizeMax;

    struct _NameTable* name_tables = (struct _NameTable*) calloc(_NameTableListSizeMax, sizeof(_NameTable));
    size_t name_tables_size = _NameTableListSizeMax;
    struct _NameTable* root_nametable;

    struct _ParserState pstate;
    pstate.token_buf = token_buf;
    pstate.token_bufsize = token_bufsize;
    symbol_list_init(&pstate.symbols, symb_size, symbs);
    expr_tree_init(&pstate.expr_tree, expr_size, exprs);
    name_list_init(&pstate.name_list, names_bufsize, names_buf);
    pstate.verbose = config.verbose;
    pstate.out_asserts = config.out_asserts;

    _icfp_parser_init_symbols(&pstate);

    struct _WriterState wstate;
    res = icfp_writer_init(&wstate, out_file, config.out_text);
    if (res != 0) { return res; }
    wstate.filename = NULL;
    wstate.verbose = config.verbose;
    root_nametable = name_table_list_init(&wstate.nametable_list, name_tables_size, name_tables);
    root_nametable->name_storage = &pstate.name_list;
    wstate.nametable = root_nametable;

    for (int fni = 0; fni < config.filename_count; ++fni) {
        const char* filename = config.filenames[fni];

        FILE* fp;
        if (strcmp(filename, "-") == 0) {
            filename = "<stdin>";
            fp = stdin;
        }
        else {
            fp = fopen(filename, "r");
            if (fp == NULL) {
                perror(filename);
                return 1;
            }
        }

        if (config.verbose) {
            fprintf(stderr, "procesing %s\n", filename);
        }

        res = icfp_parser_process(&pstate, &wstate, filename, fp);
        if (res != 0) { return res; }

        if (fp != stdin) {
            fclose(fp);
        }
    }

    return 0;
}

