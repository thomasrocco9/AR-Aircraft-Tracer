#include "json.h"

static void init_lexer(Lexer *lex, String str) {
	memset(lex, 0, sizeof(*lex));

	lex->str = str.buf;
	lex->len = str.len;

	lex->row = 1;
}

static char *advance(Lexer *lex, int count) {
	char *result = lex->str;
	if (lex->len > count) {
		lex->str += count;
		lex->len -= count;
		lex->col += count;
	}
	return result;
}

static char *peek(Lexer *lex) {
	char *result = lex->str;
	return result;
}

static bool is_whitespace(char c) {
	bool result = c == ' ' || c == '\n' || c == '\r' || c == '\t';
	return result;
}

static Token get_token(Lexer *lex) {
	Token result = {};

	if (!lex->error) {
		char *at = advance(lex, 1); 
		while (is_whitespace(*at)) {
			if (*at == '\n') {
				at = advance(lex, 1);
				++lex->row;
				lex->col = 1;
			} else {
				at = advance(lex, 1);
			}
		}

		char c = *at;
		switch (c) {
			case '{': 
				result.type = Token_Type_Left_Curly; 
				break;
			case '}': 
				result.type = Token_Type_Right_Curly; 
				break;
			case '[': 
				result.type = Token_Type_Left_Bracket; 
				break;
			case ']': 
				result.type = Token_Type_Right_Bracket; 
				break;
			case ',': 
				result.type = Token_Type_Comma; 
				break;
			case ':': 
				result.type = Token_Type_Colon; 
				break;

			case '"':
				{
					result.type = Token_Type_String;
					result.string.buf = advance(lex, 1);
					for (at = result.string.buf; *at != '"'; at = advance(lex, 1)) {
						++result.string.len;
					}
				} break;

			default:
				{
					if (strings_are_equal({at, 4}, "true")) {
						result.type = Token_Type_True;
						advance(lex, 3);
					} else if (strings_are_equal({at, 5}, "false")) {
						result.type = Token_Type_False;
						advance(lex, 4);
					} else if (strings_are_equal({at, 4}, "null")) {
						result.type = Token_Type_Null;
						advance(lex, 3);
					} else {
						int negate = 0;
						if (c == '-') {
							negate = -1;
							at = advance(lex, 1);
						} else if (c == '+') {
							negate = 1;
							at = advance(lex, 1);
						}

						if (is_digit(*at)) {
							result.type = Token_Type_Number;
							int whole = *at - '0';
							for (at = peek(lex); is_digit(*at); at = peek(lex)) {
								advance(lex, 1);
								whole = whole * 10 + (*at - '0');
							}

							result.int_lit = whole;

							float frac = 0;
							if (*at == '.') {
								advance(lex, 1);
								at = peek(lex);
								if (is_digit(*at)) {
									advance(lex, 1);

									frac = 0.1 * (*at - '0');
									float coefficient = 0.01;
									for (at = peek(lex); is_digit(*at); at = peek(lex)) {
										advance(lex, 1);
										frac = frac + coefficient * (*at - '0');
										coefficient *= 0.1;
									}
								}
							}

							result.float_lit = whole + frac;
						} 
					}
				} break;
		}
	}

	return result;
}

static Token peek_token(Lexer *lex) {
	Token tok = {};

	if (!lex->error) {
		Lexer tmp = *lex;
		tok       = get_token(&tmp);
	}

	return tok;
}

static String get_first_json_key(Lexer *lex) {
	String str = {};

	Token key_tok = get_token(lex);
	if (key_tok.type == Token_Type_String) {
		Token tok = get_token(lex);
		if (tok.type == Token_Type_Colon) {
			str = key_tok.string;
		} else {
			lex->error = true;
		}
	} else {
		lex->error = true;
	}

	return str;
}

static String get_next_json_key(Lexer *lex) {
	String result = {};

	Token tok = get_token(lex);
	if (tok.type == Token_Type_Comma) {
		Token key_tok = get_token(lex);
		if (key_tok.type == Token_Type_String) {
			tok = get_token(lex);
			if (tok.type == Token_Type_Colon) {
				result = key_tok.string;
			} else {
				lex->error = true;
			}
		} else {
			lex->error = true;
		}
	} else if (tok.type != Token_Type_Right_Curly) {
		lex->error = true;
	}

	return result;
}

static void find_matching_tok(Lexer *lex, Token_Type left, Token_Type right) {
	int num_left_matches  = 1;
	int num_right_matches = 0;

	while (num_left_matches != num_right_matches) {
		Token tok = get_token(lex);
		if (tok.type == left) {
			++num_left_matches;
		} else if (tok.type == right) {
			++num_right_matches;
		}
	}
}

static void skip_json_value(Lexer *lex) {
	Token tok = get_token(lex);
	if (tok.type == Token_Type_Left_Curly) {
		find_matching_tok(lex, Token_Type_Left_Curly, Token_Type_Right_Curly);
	} else if (tok.type == Token_Type_Left_Bracket) {
		find_matching_tok(lex, Token_Type_Left_Bracket, Token_Type_Right_Bracket);
	} else if (tok.type != Token_Type_Number && tok.type != Token_Type_String && tok.type != Token_Type_False && tok.type != Token_Type_True && tok.type != Token_Type_Null) {
		lex->error = true;
	}
}

static bool get_json_null(Lexer *lex) {
	bool result = false;

	Lexer tmp = *lex;
	Token tok = get_token(&tmp);
	if (tok.type == Token_Type_Null) {
		get_token(lex);
		result = true;
	}

	return result;
}

static int get_json_int(Lexer *lex) {
	int result = 0;

	Token tok = get_token(lex);
	if (tok.type == Token_Type_Number) {
		result = tok.int_lit;
	} else {
		lex->error = true;
	}

	return result;
}

static float get_json_float(Lexer *lex) {
	float result = 0;

	Token tok = get_token(lex);
	if (tok.type == Token_Type_Number) {
		result = tok.float_lit;
	} else {
		lex->error = true;
	}

	return result;
}

static String get_json_string(Lexer *lex) {
	String result = {};

	Token tok = get_token(lex);
	if (tok.type == Token_Type_String) {
		result = tok.string;
	} else {
		lex->error = true;
	}

	return result;
}

static void begin_json_obj(Lexer *lex) {
	Token tok = get_token(lex);
	if (tok.type != Token_Type_Left_Curly) {
		lex->error = true;
	}

	tok = peek_token(lex);
	if (tok.type != Token_Type_String && tok.type != Token_Type_Right_Curly) {
		lex->error = true;
	}
}

static void begin_json_array(Lexer *lex) {
	Token tok = get_token(lex);
	if (tok.type != Token_Type_Left_Bracket) {
		lex->error = true;
	}

	tok = peek_token(lex);
	if (tok.type != Token_Type_String && tok.type != Token_Type_Number && tok.type != Token_Type_Left_Curly && tok.type != Token_Type_Left_Bracket && tok.type != Token_Type_Right_Bracket) {
		lex->error = true;
	}
}

static bool next_json_array_entry(Lexer *lex) {
	bool result = false;

	Token tok = get_token(lex);
	if (tok.type == Token_Type_Comma) {
		result = true;
	} else if (tok.type != Token_Type_Right_Bracket) {
		lex->error = true;
	}

	return result;
}

static int get_json_array_count(Lexer *lex) {
	int result = 0;

	Lexer tmp = *lex;

	begin_json_array(&tmp);
	for (;;) {
		skip_json_value(&tmp);

		++result;

		if (!next_json_array_entry(&tmp)) {
			break;
		}
	}

	return result;
}

