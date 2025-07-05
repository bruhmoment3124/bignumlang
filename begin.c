#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

enum tk_type
{
	keyword,
	id,
	uint,
	num,
	delim,
	arrow,
	eoi /* end of input */
};

struct token
{
	char *lex; /* lexeme */
	enum tk_type type;
};

int start_panic(char *str, int forward, int *panic, char *errMsg)
{
	printf(errMsg, str[forward]);
	forward++; /* skip past erroneous char  */
    (*panic) = 1; /* panicking is handled in next_token */
	
	return forward;
}

int isdelim(char c)
{
    if(strchr(";()+-*/<>,=.", c) != NULL || isspace(c)) return 1;
    
    return 0;
}

int get_delim(char *str, int *begin, struct token *tk)
{
	int forward = *begin;
	
	tk->type = delim;
	
	if(str[forward] == '\0') tk->type = eoi;
	
	if(str[forward] == '-')
	{
		forward++;
		if(str[forward] == '>') forward++; /* 2 chars for arrow */
	} else
	{
		forward++; /* single char */
	}
	
	return forward;
}

int elim_whitespace(char *str, int *begin)
{
    while(isspace(str[*begin])) (*begin)++;
    
    return *begin;
}

int get_id(char *str, int *begin, int *panic, struct token *tk)
{
	tk->type = id;
	
	int forward = *begin;
	
    do
	{
		forward++;
	} while(isalpha(str[forward]) || isdigit(str[forward]));
	
	if(!isdelim(str[forward])) goto PANIC;
	
    goto SUCCESS; /* if not panic */
    
    PANIC:
	(*begin) = start_panic(str, forward, panic, "expected a delimiter, got '%c'\n");
	
    SUCCESS:
	
    return forward;
}

int get_num(char *str, int *begin, int *panic, struct token *tk)
{
	char *errMsg;
	int forward = *begin;
	
    do
    {
        forward++;
    } while(isdigit(str[forward]));
    
    if(str[forward] == '.') 
    {
		tk->type = num;
		
		forward++;
		if(isdigit(str[forward]))
		{
			do
			{
				forward++;
			} while(isdigit(str[forward]));
		} else
		{
			errMsg = "expected a digit, got '%c'\n";
			goto PANIC;
		}
    } else /* fall through if unsigned */
	{
		tk->type = uint;
	}
    
    if(!isdelim(str[forward])) 
	{
		errMsg = "expected a delimiter, got '%c'\n";
		goto PANIC;
	}
	
    goto SUCCESS; /* if not panic */
    
    PANIC:
	(*begin) = start_panic(str, forward, panic, errMsg);
	
    SUCCESS:
    
    return forward;
}


/* begin is the start of the token */
struct token next_token(char *str, int *begin)
{
    int panic = 0; /* panic mode enabled? */
    int forward = *begin;
	struct token temp_tk;
    
    do
    {
        forward = elim_whitespace(str, begin);
	
        panic = 0;
        if(isdigit(str[forward]))
        {
            forward = get_num(str, begin, &panic, &temp_tk);
        } else if(isalpha(str[forward])) 
        {
            forward = get_id(str, begin, &panic, &temp_tk);
        } else if(isdelim(str[forward])) /* single char tokens */
        {
			forward = get_delim(str, begin, &temp_tk);
        } else
        {
			(*begin) = start_panic(str, forward, &panic, "unexpected char, got '%c'\n");
        }
    } while(panic == 1); /* keep searching until no longer panicking */
	
    /* find length of string, allocate space for it and copy it from str */
    temp_tk.lex = calloc(forward-(*begin)+1, 1);
    memcpy(temp_tk.lex, str+(*begin), forward-(*begin));
	
	if(temp_tk.type == id)
	{
		char *keywords[8] = {"decl", "ret", "print", "if", "while", "for", "and", "or"};
		
		int i;
		for(i = 0; i<8; i++) 
		{
			if(strcmp(temp_tk.lex, keywords[i]) == 0) temp_tk.type = keyword;
		}
	}

    (*begin) = forward; /* set begin to forward for later use */
    
    return temp_tk;
}

enum type
{
    var_type,
    func_type
};

struct entry
{
	char *name;
	enum type type;
};

struct node
{
	struct entry *entry;
	int num_of_entries;
	struct node *back; /* to implement cactus stack */
};

int search_entry(struct node *current, char *name, enum type type)
{
    int found = 0;
    
    while(current != NULL)
    {
        int i;
        for(i = current->num_of_entries; i>0; i--)
        {
            if(strcmp(current->entry[i-1].name, name) == 0 && current->entry[i-1].type == type)
            {
                found = 1;
            }
        }
        
        current = current->back;
    }
    
    if(!found)
    {
        printf("ERROR! '%s' hasn't been declared!\n", name);
    }
	
	return found;
}

struct node * create_entry(struct node *current, char *name, enum type type)
{
    if(current == NULL)
    {
        printf("ERROR! cannot create entry: no table!\n");
        exit(-1);
    }
    
    int i;
    for(i = current->num_of_entries; i>0; i--)
    {
        if(strcmp(current->entry[i-1].name, name) == 0 && current->entry[i-1].type == type)
        {
            printf("ERROR! redeclaration of '%s'!\n", name);
        }
    }
    
    current->num_of_entries++;
    current->entry = realloc(current->entry, current->num_of_entries * sizeof(struct entry));
    
    /* temporary so it can be freed later by pop_tb */
    char *temp_name = calloc(strlen(name)+1, 1);
    strcat(temp_name, name);
    
    current->entry[current->num_of_entries-1].name = temp_name;
    current->entry[current->num_of_entries-1].type = type;
    
    return current;
}

struct node * create_st(struct node *current)
{
    current = realloc(current, sizeof(struct node));
    
    current->entry = NULL;
    current->num_of_entries = 0;
    current->back = NULL;
    
    return current;
}

struct node * push_tb(struct node *current)
{
    struct node *temp;
    
    if(current == NULL)
    {
        current = create_st(current);
    } else
    {
        temp = malloc(sizeof(struct node));
        temp->entry = NULL;
        temp->num_of_entries = 0;
        temp->back = current;
        
        current = temp;
    }
    
    return current;
}

struct node * pop_tb(struct node *current)
{
    if(current == NULL)
    {
        printf("ERROR! unable to pop: no table on the stack!\n");
        exit(-1);
    }
    
    if(current->entry != NULL)
    {
        int i;
        for(i = 0; i<current->num_of_entries; i++)
        {
            free(current->entry[i].name);
        }
        
        free(current->entry);
    }
    
    struct node *temp = current->back;
    free(current);
    
    return temp;
}

struct parser
{
	char *code;
	int begin;
	struct token *tk_list;
	struct token *current_tk;
	int list_size;
	
	int panic;
	int syntax_error; /* used to supress semantic errors if a syntax error occurs */
	int had_error;
	
	struct node *current_tb;
};

void error(struct parser *parser, char *msg)
{
	if(parser->panic) return;
	
	printf("ERROR: %s\n", msg);
	parser->panic = 1;
	parser->syntax_error = 1;
	parser->had_error = 1;
}

void expect_lex(struct parser *parser, char *str)
{
	if(strcmp(parser->current_tk->lex, str) != 0)
	{
		error(parser, "expected different lexeme!");
	}
	
	if(!parser->panic) parser->current_tk++; /* advance input if not panicking */
}

void expect_type(struct parser *parser, enum tk_type type)
{
	if(parser->current_tk->type != type)
	{
		error(parser, "expected different token type!");
	}
	
	if(!parser->panic) parser->current_tk++; /* advance input if not panicking */
}

void parser_and(struct parser *parser); /* forward declaration for funcparens and val */

void funcparens(struct parser *parser)
{
	expect_lex(parser, "(");
	
	if(parser->current_tk->type == id  || 
	   parser->current_tk->type == num ||
	   parser->current_tk->type == uint)
	{
		parser_and(parser);
		
		while(strcmp(parser->current_tk->lex, ",") == 0)
		{
			if(parser->panic) break;
			
			expect_lex(parser, ",");
			parser_and(parser);
		}
	}
	
	expect_lex(parser, ")");
}

void val(struct parser *parser)
{	
	if(parser->current_tk->type == id)
	{
		//printf("push %s\n", parser->current_tk->lex);
		
		char *temp_lex = parser->current_tk->lex;
		enum type temp_type = var_type;
		
		expect_type(parser, id);
		
		if(strcmp(parser->current_tk->lex, "(") == 0)
		{
			temp_type = func_type;
			funcparens(parser);
		}
		
		if(!parser->syntax_error)
		{
			int found = search_entry(parser->current_tb, temp_lex, temp_type);
			
			if(!found) parser->had_error = 1;
		}
	} else if(parser->current_tk->type == num ||
			  parser->current_tk->type == uint)
	{
		//printf("push %s\n", parser->current_tk->lex);
		expect_type(parser, parser->current_tk->type);
	} else if(strcmp(parser->current_tk->lex, "(") == 0)
	{
		expect_lex(parser, "(");
		parser_and(parser);
		expect_lex(parser, ")");
	} else
	{
		error(parser, "bad value.");
	}
}

void term(struct parser *parser)
{
	val(parser);
	
	while(strcmp(parser->current_tk->lex, "*") == 0 ||
	      strcmp(parser->current_tk->lex, "/") == 0)
	{
		if(parser->panic) break;
		
		//char *temp = parser->current_tk->lex;
		
		expect_lex(parser, parser->current_tk->lex);
		val(parser);
		
		//printf("%s\n", temp);
	}
}

void expr(struct parser *parser)
{
	term(parser);
	
	while(strcmp(parser->current_tk->lex, "+") == 0 ||
	      strcmp(parser->current_tk->lex, "-") == 0)
	{
		if(parser->panic) break;
		
		//char *temp = parser->current_tk->lex;
		
		expect_lex(parser, parser->current_tk->lex);
		term(parser);
		
		//printf("%s\n", temp);
	}
}

void rel(struct parser *parser)
{
	expr(parser);
	
	while(strcmp(parser->current_tk->lex, "<") == 0 ||
	      strcmp(parser->current_tk->lex, ">") == 0)
	{
		if(parser->panic) break;
		
		//char *temp = parser->current_tk->lex;
		
		expect_lex(parser, parser->current_tk->lex);
		expr(parser);
		
		//printf("%s\n", temp);
	}
}

void parser_or(struct parser *parser)
{
	rel(parser);
	
	while(strcmp(parser->current_tk->lex, "or") == 0)
	{
		if(parser->panic) break;
		
		//char *temp = parser->current_tk->lex;
		
		expect_lex(parser, "or");
		rel(parser);
		
		//printf("%s\n", temp);
	}
} 

void parser_and(struct parser *parser)
{
	parser_or(parser);
	
	while(strcmp(parser->current_tk->lex, "and") == 0)
	{
		if(parser->panic) break;
		
		//char *temp = parser->current_tk->lex;
		
		expect_lex(parser, "and");
		parser_or(parser);
		
		//printf("%s\n", temp);
	}
} 

void body(struct parser *parser); /* forward declaration of body for flow */

void flow(struct parser *parser)
{	
	expect_lex(parser, parser->current_tk->lex); /* previous function says it has to be "if" or "while" */
	
	if(!parser->syntax_error) parser->current_tb = push_tb(parser->current_tb);
	
	expect_lex(parser, "(");
	parser_and(parser);
	expect_lex(parser, "->");
	body(parser);
	expect_lex(parser, ")");
	
	if(!parser->syntax_error) parser->current_tb = pop_tb(parser->current_tb);
}

void parser_return(struct parser *parser)
{
	expect_lex(parser, "ret");
	parser_and(parser);
	expect_lex(parser, ";");
}

void parser_print(struct parser *parser)
{
	expect_lex(parser, "print");
	
	if(!parser->syntax_error)
	{
		int found = search_entry(parser->current_tb, parser->current_tk->lex, var_type);
		
		if(!found) parser->had_error = 1;
	}
	
	expect_type(parser, id);
	expect_lex(parser, ".");
	expect_type(parser, uint);
	expect_lex(parser, ";");
}

void next(struct parser *parser)
{
	if(strcmp(parser->current_tk->lex, "=") == 0)
	{
		if(!parser->syntax_error)
		{
			int found = search_entry(parser->current_tb, (parser->current_tk-1)->lex, var_type);
			
			if(!found) parser->had_error = 1;
		}
		
		expect_lex(parser, "=");
		parser_and(parser);
		expect_lex(parser, ";");
	} else if(strcmp(parser->current_tk->lex, "(") == 0)
	{
		if(!parser->syntax_error)
		{
			int found = search_entry(parser->current_tb, (parser->current_tk-1)->lex, func_type);
			
			if(!found) parser->had_error = 1;
		}
		
		funcparens(parser);	
		expect_lex(parser, ";");
	} else
	{
		error(parser, "bad operator on id.");
	}
}

void idstart(struct parser *parser)
{
	expect_type(parser, id);
	next(parser);
}

void declaration(struct parser *parser)
{
	expect_lex(parser, "decl");
	
	if(!parser->syntax_error) parser->current_tb = create_entry(parser->current_tb, parser->current_tk->lex, var_type);
	
	expect_type(parser, id);
	expect_lex(parser, "=");
	parser_and(parser);
	expect_lex(parser, ";");
}

void statement(struct parser *parser)
{
	if(strcmp(parser->current_tk->lex, "decl") == 0)
	{
		declaration(parser);
	} else if(parser->current_tk->type == id)
	{
		idstart(parser); /* statements that start with an id */
	} else if(strcmp(parser->current_tk->lex, "print") == 0)
	{
		parser_print(parser);
	} else if(strcmp(parser->current_tk->lex, "ret") == 0)
	{
		parser_return(parser);
	} else if(strcmp(parser->current_tk->lex, "if") == 0 ||
			  strcmp(parser->current_tk->lex, "while") == 0)
	{
		flow(parser);
	} else
	{
		error(parser, "expected different start of statement.");
	}
}

void sync(struct parser *parser)
{
	while(parser->current_tk->type != eoi)
	{
		if(strcmp(parser->current_tk->lex, "if")      == 0 ||
		   strcmp(parser->current_tk->lex, "while")   == 0) break;
		
		if(strcmp(parser->current_tk->lex, ";") == 0)
		{
			parser->current_tk++;
			break;
		}
		
		parser->current_tk++;
	}
	
	parser->panic = 0;
}

void body(struct parser *parser)
{
	if(parser->panic) sync(parser);
	
	while(strcmp(parser->current_tk->lex, ")") != 0)
	{
		if(parser->current_tk->type == eoi) break;
		
		statement(parser);
		if(parser->panic) sync(parser);
	}
}

void funcdecl(struct parser *parser)
{	
	expect_lex(parser, "(");

	if(!parser->syntax_error) parser->current_tb = create_entry(parser->current_tb, parser->current_tk->lex, func_type);

	expect_type(parser, id);
	
	if(!parser->syntax_error) parser->current_tb = push_tb(parser->current_tb); /* new scope for inside of function */
	
	if(parser->current_tk->type == id)
	{
		
		/*
			THESE ARE FOR FUNCTION ARGUMENTS, ***MIGHT*** NEED TO FIX SYMBOL TABLES LATER FOR THIS
		*/
		
		if(!parser->syntax_error) parser->current_tb = create_entry(parser->current_tb, parser->current_tk->lex, var_type);
		
		expect_type(parser, id);
		
		while(strcmp(parser->current_tk->lex, ",") == 0)
		{
			if(parser->panic) break;
			
			expect_lex(parser, ",");
			
			if(!parser->syntax_error) parser->current_tb = create_entry(parser->current_tb, parser->current_tk->lex, var_type);
			
			expect_type(parser, id);
		}
	}
	
	expect_lex(parser, "->");
	
	body(parser);
	
	expect_lex(parser, ")");
	
	if(!parser->syntax_error) parser->current_tb = pop_tb(parser->current_tb);
}

void funclist(struct parser *parser)
{
	while(parser->current_tk->type != eoi)
	{
		funcdecl(parser);
	}

	expect_lex(parser, "\0");
}

int main(void)
{
	struct parser *parser = malloc(sizeof(struct parser));
	parser->tk_list = malloc(sizeof(struct token));
	
	parser->code = "(f -> decl r = x+y+z; decl l = x+y+z;)";
	parser->begin = 0;
	parser->panic = 0;
	parser->syntax_error = 0;
	parser->had_error = 0;
	parser->list_size = 0;
	
	parser->current_tb = NULL;
	parser->current_tb = push_tb(parser->current_tb); /* global scope for function declarations */
	
	/* put all of the tokens into a list (tk_list) */
	do
	{
		parser->list_size++;
		parser->tk_list = realloc(parser->tk_list, parser->list_size * sizeof(struct token));
		parser->tk_list[parser->list_size-1] = next_token(parser->code, &parser->begin);
		//printf("(%s, %d)\n", parser->tk_list[parser->list_size-1].lex, parser->tk_list[parser->list_size-1].type);
	} while(parser->tk_list[parser->list_size-1].type != eoi);

	parser->current_tk = parser->tk_list;

	funclist(parser);
	
	getchar();
	
	/* clears out symbol table stack if an error occurs; this is won't do anything if the program
	   executes properly */
	while(parser->current_tb != NULL)
	{
		parser->current_tb = pop_tb(parser->current_tb);
	}
	
	int i;
	for(i = 0; i<parser->list_size; i++) free(parser->tk_list[i].lex);
	
	free(parser->tk_list);
	
	free(parser);

    return 0;
}