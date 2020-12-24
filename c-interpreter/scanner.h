#ifndef scanner_h
#define scanner_h

typedef enum
{
	// single character
	TOKEN_LEFT_PAREN, TOKEN_RIGHT_PAREN,		// ( )
	TOKEN_LEFT_BRACE, TOKEN_RIGHT_BRACE,		// { }
	TOKEN_COMMA, TOKEN_DOT, TOKEN_MINUS, TOKEN_PLUS,
	TOKEN_SEMICOLON, TOKEN_COLON, TOKEN_SLASH, TOKEN_STAR,
	TOKEN_MODULO,

	// one or two compare operators
	TOKEN_BANG, TOKEN_BANG_EQUAL,		// !, !=
	TOKEN_EQUAL, TOKEN_EQUAL_EQUAL,
	TOKEN_GREATER, TOKEN_GREATER_EQUAL,
	TOKEN_LESS, TOKEN_LESS_EQUAL,

	// literals
	TOKEN_IDENTIFIER, TOKEN_STRING, TOKEN_NUMBER,

	// keywords
	TOKEN_AND, TOKEN_CLASS, TOKEN_ELF, TOKEN_ELSE, TOKEN_FALSE,
	TOKEN_FOR, TOKEN_FUN, TOKEN_IF, TOKEN_NULL, TOKEN_OR,
	TOKEN_PRINT, TOKEN_RETURN, TOKEN_SUPER, TOKEN_SWITCH,
	TOKEN_DEFAULT, TOKEN_CASE, TOKEN_THIS, TOKEN_TRUE, 
	TOKEN_VAR, TOKEN_WHILE, TOKEN_BREAK, TOKEN_CONTINUE,
	TOKEN_THEN,

	// do while, repeat until
	TOKEN_DO, TOKEN_REPEAT, TOKEN_UNTIL,

	// class inheritance
	TOKEN_FROM,

	TOKEN_ERROR,
	TOKEN_EOF
} TokenType;

typedef struct
{
	TokenType type;			// identifier to type of token, eg. number, + operator, identifier
	const char* start;
	int length;
	int line;
} Token;

void initScanner(const char* source);
Token scanToken();

#endif