#include <stdio.h>
#include <string.h>

#include "common.h"
#include "scanner.h"

// scanner to run through the source code
typedef struct
{
	const char* start;		// marks the beginning of the current lexeme('word', you can say_
	const char* current;	// points to the character being looked at
	int line;				// int to tell the current line being looked at
} Scanner;

Scanner scanner;

void initScanner(const char* source)
{
	scanner.start = source;			// again, pointing to a string array means pointing to the beginning
	scanner.current = source;		
	scanner.line = 1;
}

// to check for identifiers(eg. for, while, print)
static bool isAplha(char c)
{
	return (c >= 'a' && c <= 'z') ||
		(c >= 'A' && c <= 'Z') ||
		c == '_';
} 

static bool isDigit(char c)
{
	return c >= '0' && c <= '9';		// let string comparison handle it
}

// to get EOF symbol -> '\0'
static bool isAtEnd()
{
	return *scanner.current == '\0';
}



// goes to next char
static char advance()
{
	scanner.current++;				// advance to next 
	return scanner.current[-1];		// return previous one
}

// logical conditioning to check if 2nd character is embedded to first(e.g two char token)
static bool match(char expected)
{
	if (isAtEnd()) return false;			// if already at end, error
	if (*scanner.current != expected)
	{
		//printf("no match");
		return false;				// if current char does not equal expected char, it is false
	}
	//printf("match");
	scanner.current++;		// if yes, advance to next
	return true;
}

// make a token, uses the scanner's start and current to capture the lexeme and its size
static Token makeToken(TokenType type)
{
	Token token;
	token.type = type;
	token.start = scanner.start;
	token.length = (int)(scanner.current - scanner.start);
	token.line = scanner.line;

	return token;
}

// similar to makeToken, but the "lexeme" points to the error message string instead of the scanner's current and start pointers
static Token errorToken(const char* message)
{
	Token token;
	token.type = TOKEN_ERROR;
	token.start = message;
	token.length = (int)strlen(message);	// get string length and turn to int
	token.line = scanner.line;

	return token;
}

// returns current character
static char peek()
{
	return *scanner.current;
}

// returns next character
static char peekNext()
{
	if (isAtEnd()) return '\0';
	return scanner.current[1];		// C syntax, basically return index 1 (or second) from the array/pointer
}

// skipping white spaces, tabs, etc.
static void skipWhiteSpace()
{
	for (;;)
	{
		char c = peek();		// peek is a function to check the next code character
		switch (c)
		{
		case ' ':
		case '\r':
		case '\t':
			advance();
			break;

		case '\n':				// if a new line is found, also add line number
			scanner.line++;		
			advance();
			break;

		// for comments
		case '/':
			if (peekNext == '/')
			{
				// comment goes until end of line
				while (peek() != '\n' && !isAtEnd()) advance();		// if not new line or not end, treat as whitespace and advance
			}
			else {
				return;
			}

		default:
			return;
		}
	}
}


// to check for identifiers, if they are keyword or not. rest means the rest of the letter
static TokenType checkKeyword(int start, int length, const char* rest, TokenType type)
{
	/* hard expression here
	bascially if they are exactly the same, and compares their memory(memcmp)
	int memcmp(const void *str1, const void *str2, size_t n) -> if it is exactly the same, then it is 0
	*/
	if (scanner.current - scanner.start == start + length && memcmp(scanner.start + start, rest, length) == 0)
	{
		return type;
	}

	return TOKEN_IDENTIFIER;
}
// the 'trie' to store the set of strings
static TokenType identifierType()
{
	switch (scanner.start[0])		// start of the lexeme
	{
	//case 'a': return checkKeyword(1, 2, "nd", TOKEN_AND);
	case 'a':
	{
		if (scanner.current - scanner.start > 1)
		{
			switch (scanner.start[1])
			{
			case 'n': return checkKeyword(2, 1, "d", TOKEN_AND);
			case 's': return checkKeyword(2, 6, "signed", TOKEN_EQUAL);
			}
		}
	}
	case 'b': return checkKeyword(1, 4, "reak", TOKEN_BREAK);
	case 'c':
	{
		if (scanner.current - scanner.start > 1)
		{
			switch (scanner.start[1])
			{
			case 'a': return checkKeyword(2, 2, "se", TOKEN_CASE);
			case 'l': return checkKeyword(2, 3, "ass", TOKEN_CLASS);
			case 'o': return checkKeyword(2, 6, "ntinue", TOKEN_CONTINUE);
			}
		}
	}
	case 'd':
	{
		if (scanner.current - scanner.start > 1)
		{
			switch (scanner.start[1])
			{
			case 'e': return checkKeyword(2, 5, "fault", TOKEN_DEFAULT);
			case 'o': return checkKeyword(2, 0, "", TOKEN_DO);
			}
		}
	}
	case 'e': 
		if (scanner.current - scanner.start > 1)
		{
			switch (scanner.start[1])	// check if there is a second letter
			{
			case 'l':
				{
					if (scanner.current - scanner.start > 2)	// check if there is a third letter
					{
						switch (scanner.start[2])
						{
						case 's': return checkKeyword(3, 1, "e", TOKEN_ELSE);
						case 'f': return checkKeyword(3, 0, "", TOKEN_ELF);			// already matched
						}
					}
				}
			case 'q': return checkKeyword(2, 4, "uals", TOKEN_EQUAL_EQUAL);
			}
		}
	case 'f':
		if (scanner.current - scanner.start > 1)	// check if there is a second letter
		{
			switch (scanner.start[1])
			{
			case 'a': return checkKeyword(2, 3, "lse", TOKEN_FALSE);	// starts from 2 not 3, as first letter is already an f
			case 'o': return checkKeyword(2, 1, "r", TOKEN_FOR);
			case 'r': return checkKeyword(2, 2, "om", TOKEN_FROM);
			case 'n': return checkKeyword(2, 0, "", TOKEN_FUN);
			case 'u': return checkKeyword(2, 6, "nction", TOKEN_FUN);
			}
		}
	case 'i':
		if (scanner.current - scanner.start > 1)
		{
			switch (scanner.start[1])
			{
			case 'f': return checkKeyword(2, 0, "", TOKEN_IF);
			case 's': return checkKeyword(2, 0, "", TOKEN_EQUAL_EQUAL);
			}
		}
	case 'n': return checkKeyword(1, 3, "ull", TOKEN_NULL);
	case 'o': return checkKeyword(1, 1, "r", TOKEN_OR);
	case 'p': return checkKeyword(1, 4, "rint", TOKEN_PRINT);
	//case 'r': return checkKeyword(1, 5, "eturn", TOKEN_RETURN);
	case 'r':
		if (scanner.current - scanner.start > 1)
		{
			switch (scanner.start[1])
			{
			case 'e':
				if (scanner.current - scanner.start > 2)
				{
					switch (scanner.start[2])
					{
					case 't': return checkKeyword(3, 3, "urn", TOKEN_RETURN);
					case 'p': return checkKeyword(3, 3, "eat", TOKEN_REPEAT);
					}
				}
			}
		}
	case 's': 
		if (scanner.current - scanner.start > 1)			// if there is a second letter
		{
			switch (scanner.start[1])
			{
			case 'u': return checkKeyword(2, 3, "per", TOKEN_SUPER);
			case 'w': return checkKeyword(2, 4, "itch", TOKEN_SWITCH);
			}
		}
	case 't':
		if (scanner.current - scanner.start > 1)
		{
			switch (scanner.start[1])
			{
			//case 'h': return checkKeyword(2, 2, "is", TOKEN_THIS);
			case 'h':
				if (scanner.current - scanner.start > 2)	// check if there is a third letter
				{
					switch (scanner.start[2])
					{
					case 'e': return checkKeyword(3, 1, "n", TOKEN_THEN);
					case 'i': return checkKeyword(3, 1, "s", TOKEN_THIS);			// already matched
					}
				}
			case 'r': return checkKeyword(2, 2, "ue", TOKEN_TRUE);
			}
		}
	case 'u': return checkKeyword(1, 4, "ntil", TOKEN_UNTIL);
	case 'v': return checkKeyword(1, 2, "ar", TOKEN_VAR);
	case 'w': return checkKeyword(1, 4, "hile", TOKEN_WHILE);

	}


	return TOKEN_IDENTIFIER;
}

static Token identifier()
{
	while (isAplha(peek()) || isDigit(peek())) advance();		// skip if still letters or digits 
	return makeToken(identifierType());
}

static Token number()
{
	while (isDigit(peek())) advance();		// while next is still a digit advance

	// look for fractional part	
	if (peek() == '.' && isDigit(peekNext()))		// if there is a . and next is still digit
	{
		// consume '.'
		advance();

		while (isDigit(peek())) advance();
	}

	return makeToken(TOKEN_NUMBER);
}

// for string tokens
static Token string()
{
	while (peek() != '"' && !isAtEnd())
	{
		if (peek() == '\n') scanner.line++;		// allow strings to go until next line
		advance();		// consume characters until the closing quote is reached
	}

	if (isAtEnd()) return errorToken("Unterminated string.");

	// closing quote
	advance();
	return makeToken(TOKEN_STRING);

	// convert lexeme to runtime value later
}

// reading the char, and return a token
Token scanToken()
{
	skipWhiteSpace();

	scanner.start = scanner.current;		// reset the scanner to current

	if (isAtEnd()) return makeToken(TOKEN_EOF);		// check if at end

	// if not end of file
	char c = advance();

	if (isAplha(c)) return identifier();
	if (isDigit(c)) return number();		// number() is a TOKEN_NUMBER


	// lexical grammar for the language
	switch (c)
	{
		// for single characters
	case '(': return makeToken(TOKEN_LEFT_PAREN);
	case ')': return makeToken(TOKEN_RIGHT_PAREN);
	case '{': return makeToken(TOKEN_LEFT_BRACE);
	case '}': return makeToken(TOKEN_RIGHT_BRACE);
	case ';': return makeToken(TOKEN_SEMICOLON);
	case ':': return makeToken(TOKEN_COLON);
	case ',': return makeToken(TOKEN_COMMA);
	case '.': return makeToken(TOKEN_DOT);
	case '-': return makeToken(TOKEN_MINUS);
	case '+': return makeToken(TOKEN_PLUS);
	case '*': return makeToken(TOKEN_STAR);
	case '/': return makeToken(TOKEN_SLASH);

		// for two characters
	case '!':
		return makeToken(match('=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
	case '=':
		return makeToken(match('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
	case '>':
		return makeToken(match('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
	case '<':
		return makeToken(match('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);

		// literal tokens
	case '"': return string();			// string token
	}



	return errorToken("Unexpected character.");
}

