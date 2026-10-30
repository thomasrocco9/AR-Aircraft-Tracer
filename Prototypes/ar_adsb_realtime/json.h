#ifndef JSON_H
#define JSON_H

#include "stringlib.h"

enum Token_Type {
	Token_Type_None,

	Token_Type_Left_Curly,
	Token_Type_Right_Curly,
	Token_Type_Left_Bracket,
	Token_Type_Right_Bracket,
	Token_Type_Comma,
	Token_Type_Period,
	Token_Type_Colon,

	Token_Type_String,

	Token_Type_Number,
	Token_Type_True,
	Token_Type_False,
	Token_Type_Null,
};

struct Token {
	Token_Type type;

	// TODO: This shouldn't be here move this out we just wanna store the string representation of the token
	String string;
	int    int_lit;
	float  float_lit;
};

struct Lexer {
	char *str;
	int   len;

	bool error;
	int  col, row;
};

static void init_lexer(Lexer *lex, String str);

static String get_first_json_key(Lexer *lex);
static String get_next_json_key(Lexer *lex);
static void   skip_json_value(Lexer *lex);
static bool   get_json_null(Lexer *lex);
static int    get_json_int(Lexer *lex);
static float  get_json_float(Lexer *lex);
static String get_json_string(Lexer *lex);
static void   begin_json_obj(Lexer *lex);
static void   begin_json_array(Lexer *lex);
static bool   next_json_array_entry(Lexer *lex);
static int    get_json_array_count(Lexer *lex);

#endif
