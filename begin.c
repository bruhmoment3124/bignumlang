#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

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
	float rel_addr;
	int num_of_args;
};

struct node
{
	struct entry *entry;
	int num_of_entries;
	struct node *back; /* to implement cactus stack */
};

struct entry * search_entry(struct node *current, char *name, enum type type)
{
    struct entry *entry = NULL;
    
    while(current != NULL)
    {
        int i;
        for(i = current->num_of_entries; i>0; i--)
        {
            if(strcmp(current->entry[i-1].name, name) == 0 && current->entry[i-1].type == type)
            {
				entry = &current->entry[i-1];
				goto found; /* break */
            }
        }
        
        current = current->back;
    }
	
	found:
    
    if(entry == NULL)
    {
        printf("ERROR! '%s' hasn't been declared!\n", name);
    }
	
	return entry;
}

struct node * create_entry(struct node *current, int rel_addr, char *name, enum type type)
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
    current->entry[current->num_of_entries-1].rel_addr = rel_addr;
    
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

enum inst_type
{
    label,
    decl,
    push_adr,
	push_loc,
    push_val,
    pop,
	print,
    jmpf,
    jmp,
	call,
    param,
    ret_val,
    ret_none,
	set_equal,
	less_than,
	more_than,
	plus,
	minus,
	multiply,
	divide,
	and,
	or
};

struct inst
{
	enum inst_type type;
	float *args;
	int num_of_args;
};

struct opstack
{
    float *stack;
    int top;
};

struct opstack * create_opstack(void)
{
    struct opstack *temp = malloc(sizeof(struct opstack));
    
    temp->stack = NULL;
    temp->top = -1;
    
    return temp;
}

struct opstack * push_op(struct opstack *op, float n)
{
    op->top++;
    op->stack = realloc(op->stack, (op->top + 1) * sizeof(float));
    op->stack[op->top] = n;
    
    return op;
}

struct opstack * pop_op(struct opstack *op)
{
    if(op->stack != NULL)
    {
        op->top--;
        
        if(op->top == -1)
        {
            op->stack = NULL;
        } else
        {
            op->stack = realloc(op->stack, (op->top + 1) * sizeof(float));
        }
    } else
    {
        printf("RUNTIME ERROR: unable to pop!\n");
        exit(-1);
    }
    
    return op;
}

struct frame
{
    float *locals;
    int num_of_locals;
	struct opstack *op;
	float *ret_val;
};

struct frstack
{
    struct frame *frame;
    int top;
};

struct frstack * declare_local(struct frstack *stack, float val)
{
    stack->frame[stack->top].num_of_locals++;
    stack->frame[stack->top].locals = realloc(stack->frame[stack->top].locals, stack->frame[stack->top].num_of_locals * sizeof(int));
    stack->frame[stack->top].locals[stack->frame[stack->top].num_of_locals-1] = val;
    
    return stack;
}

struct frstack * push_frame(struct frstack *stack)
{
    stack->top++;
    stack->frame = realloc(stack->frame, (stack->top + 1) * sizeof(struct frame));
    stack->frame[stack->top].locals = NULL;
    stack->frame[stack->top].num_of_locals = 0;
	stack->frame[stack->top].op = create_opstack();
    stack->frame[stack->top].ret_val = NULL;
    
    return stack;
}

struct frstack * pop_frame(struct frstack *stack)
{
    if(stack->top > 0)
    {
        stack->top--;
        stack->frame = realloc(stack->frame, (stack->top + 1) * sizeof(struct frame));
    } else if(stack->top == 0)
    {
        stack->top--;
        stack->frame = NULL;
    } else if(stack->top == -1)
    {
        printf("ERROR: unable to pop, nothing on the stack!\n");
        exit(-1);
    }
	
	return stack;
}

void print_top(struct frstack *stack)
{
    printf("(");
	
	if(stack->frame[stack->top].locals != NULL)
	{
		int i;
		for(i = 0; i<stack->frame[stack->top].num_of_locals; i++)
		{
			if(i == 0) printf("%f", stack->frame[stack->top].locals[i]);
			if(i > 0)  printf(", %f", stack->frame[stack->top].locals[i]);
		}
    }
	
    printf(")\n");
    
    printf("%d\n", stack->frame[stack->top].num_of_locals);
	
	if(stack->frame[stack->top].op->stack == NULL)
	{
		printf("NULL\n");
    } else
	{
		printf("%f\n", stack->frame[stack->top].op->stack[stack->frame[stack->top].op->top]);
	}
	
    if(stack->frame[stack->top].ret_val == NULL)
    {
        printf("NULL\n");
    } else
    {
        printf("%f\n", *(stack->frame[stack->top].ret_val));
    }
}

struct frstack * create_frstack(void)
{
    struct frstack *temp_st = malloc(sizeof(struct frstack));
    temp_st->frame = NULL;
    temp_st->top = -1;
    
    return temp_st;
}

struct vm
{
	struct inst *code;
	int num_of_insts;
	float *label_list;
	int num_of_labels;
	
	struct frstack *stack;
};

void run_vm(struct vm *vm)
{
	int i;
	for(i = 0; i<vm->num_of_insts; i++)
	{
		struct frame * current_frame = &vm->stack->frame[vm->stack->top];
		
		float top = current_frame->op->stack[current_frame->op->top];
		float topminus1 = current_frame->op->stack[current_frame->op->top-1];
		float inter;
		
		switch(vm->code[i].type)
		{
			case decl: vm->stack = declare_local(vm->stack, vm->code[i].args[0]); break;
			case label: break;
			
			case push_adr: current_frame->op = push_op(current_frame->op, vm->code[i].args[0]); break;
			case push_loc: current_frame->op = push_op(current_frame->op, current_frame->locals[(int)vm->code[i].args[0]]); break;
			case push_val: current_frame->op = push_op(current_frame->op, vm->code[i].args[0]); break;
			
			case set_equal:
				current_frame->locals[(int)current_frame->op->stack[current_frame->op->top]] = current_frame->op->stack[current_frame->op->top-1];
				current_frame->op = pop_op(current_frame->op);
				current_frame->op = pop_op(current_frame->op);
			break;
			
			case less_than:
				inter = top < topminus1;
				current_frame->op = pop_op(current_frame->op);
				current_frame->op = pop_op(current_frame->op);
				current_frame->op = push_op(current_frame->op, inter);
			break;
			
			case more_than:
				inter = top > topminus1;
				current_frame->op = pop_op(current_frame->op);
				current_frame->op = pop_op(current_frame->op);
				current_frame->op = push_op(current_frame->op, inter);
			break;
			
			case plus:
				inter = top + topminus1;
				current_frame->op = pop_op(current_frame->op);
				current_frame->op = pop_op(current_frame->op);
				current_frame->op = push_op(current_frame->op, inter);
			break;
			
			case minus:
				inter = top - topminus1;
				current_frame->op = pop_op(current_frame->op);
				current_frame->op = pop_op(current_frame->op);
				current_frame->op = push_op(current_frame->op, inter);
			break;
			
			case multiply:
				inter = top * topminus1;
				current_frame->op = pop_op(current_frame->op);
				current_frame->op = pop_op(current_frame->op);
				current_frame->op = push_op(current_frame->op, inter);
			break;
			
			case divide:
				inter = top / topminus1;
				current_frame->op = pop_op(current_frame->op);
				current_frame->op = pop_op(current_frame->op);
				current_frame->op = push_op(current_frame->op, inter);
			break;
			
			case and:
				inter = (unsigned int)top && (unsigned int)topminus1;
				current_frame->op = pop_op(current_frame->op);
				current_frame->op = pop_op(current_frame->op);
				current_frame->op = push_op(current_frame->op, (float)inter);
			break;
			
			case or:
				inter = (unsigned int)floor(top) || (unsigned int)floor(topminus1);
				current_frame->op = pop_op(current_frame->op);
				current_frame->op = pop_op(current_frame->op);
				current_frame->op = push_op(current_frame->op, (float)inter);
			break;
		}
		
		print_top(vm->stack);
		getchar();
	}
}

float * create_args(int num_of_args, ...)
{
    va_list list;
    va_start(list, num_of_args);
    
    float *temp = malloc(num_of_args * sizeof(float));
    
    int i;
    for(i = 0; i<num_of_args; i++) temp[i] = va_arg(list, double);
    
    va_end(list);
    
    return temp;
}

struct vm * emit_code(struct vm *vm, enum inst_type type, float *args, int num_of_args)
{
    vm->num_of_insts++;
    vm->code = realloc(vm->code, vm->num_of_insts * sizeof(struct inst));
    vm->code[vm->num_of_insts-1].type = type;
    vm->code[vm->num_of_insts-1].args = args;
    vm->code[vm->num_of_insts-1].num_of_args = num_of_args;
    
    if(type == label)
    {
        vm->label_list = realloc(vm->label_list, vm->num_of_labels * sizeof(int));
        vm->label_list[vm->num_of_labels-1] = vm->num_of_insts-1;
    }
    
    return vm;
}

struct vm * create_vm(void)
{
    struct vm *temp_vm = malloc(sizeof(struct vm));
    
    temp_vm->code = NULL;
    temp_vm->num_of_insts = 0;
    temp_vm->label_list = NULL;
    
    temp_vm->stack = create_frstack();
    
    return temp_vm;
}

void print_code(struct vm *vm)
{
    int i;
    for(i = 0; i<vm->num_of_insts; i++)
    {
        printf("(");
        
        switch(vm->code[i].type)
        {
            case label:     printf("label");     break;
            case decl:      printf("decl");      break;
            case push_adr:  printf("push_adr");  break;
            case push_loc:  printf("push_loc");  break;
            case push_val:  printf("push_val");  break;
            case pop:       printf("pop");       break;
            case print:     printf("print");     break;
            case jmpf:      printf("jmpf");      break;
            case jmp:       printf("jmp");       break;
            case call:      printf("call");      break;
            case param:     printf("param");     break;
            case ret_val:   printf("ret_val");   break;
            case ret_none:  printf("ret_none");  break;
            case set_equal: printf("set_equal"); break;
            case less_than: printf("less_than"); break;
            case more_than: printf("more_than"); break;
            case plus:      printf("plus");      break;
            case minus:     printf("minus");     break;
            case multiply:  printf("multiply");  break;
            case divide:    printf("divide");    break;
            case and:       printf("and");       break;
            case or:        printf("or");        break;
        }
        
        int j;
        for(j = 0; j<vm->code[i].num_of_args; j++)
        {
            if(j == 0) printf(" %f", vm->code[i].args[j]);
            if(j > 0)  printf(", %f", vm->code[i].args[j]);
        }
        
        printf(")\n");
    }
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
	float rel_addr;
	
	struct vm *vm;
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

int funcparens(struct parser *parser)
{
	int args = 0;
	
	expect_lex(parser, "(");
	
	if(parser->current_tk->type == id  || 
	   parser->current_tk->type == num ||
	   parser->current_tk->type == uint)
	{
		parser_and(parser);
		
		/* change later !!!  */
		if(!parser->had_error) parser->vm = emit_code(parser->vm, param, NULL, 0);
		/* change later !!!  */
		
		args++;
		
		while(strcmp(parser->current_tk->lex, ",") == 0)
		{
			if(parser->panic) break;
			
			expect_lex(parser, ",");
			parser_and(parser);
				
			if(!parser->had_error) parser->vm = emit_code(parser->vm, param, NULL, 0);
			
			args++;
		}
	}
	
	expect_lex(parser, ")");
	
	return args;
}

void val(struct parser *parser)
{	
	struct entry *entry;

	if(parser->current_tk->type == id)
	{
		char *temp_lex = parser->current_tk->lex;
		enum type temp_type = var_type;
		
		expect_type(parser, id);
		
		if(strcmp(parser->current_tk->lex, "(") == 0)
		{
			if(!parser->syntax_error)
			{
				entry = search_entry(parser->current_tb, temp_lex, func_type);
				
				int args = funcparens(parser);
				
				if(entry == NULL) parser->had_error = 1;
				
				if(entry->num_of_args != args)
				{
					printf("ERROR: incorrect number of arguments!\n");
					parser->had_error = 1;
				}
			}
			
			if(!parser->had_error)
			{
				float *args = create_args(1, entry->rel_addr);
				parser->vm = emit_code(parser->vm, call, args, 1);
			}
		} else
		{
			if(!parser->syntax_error)
			{
				entry = search_entry(parser->current_tb, temp_lex, temp_type);
				
				if(entry == NULL) parser->had_error = 1;
			}
			
			if(!parser->had_error)
			{
				float *args = create_args(1, entry->rel_addr);
				parser->vm = emit_code(parser->vm, push_loc, args, 1);
			}
		}
	} else if(parser->current_tk->type == num ||
			  parser->current_tk->type == uint)
	{
		if(!parser->had_error)
		{
			float *args = create_args(1, atof(parser->current_tk->lex));
			parser->vm = emit_code(parser->vm, push_val, args, 1);
		}
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
		
		char *temp = parser->current_tk->lex;
		
		expect_lex(parser, parser->current_tk->lex);
		val(parser);
		
		if(!parser->had_error)
		{
			if(strcmp(temp, "*") == 0) parser->vm = emit_code(parser->vm, multiply, NULL, 0);
			if(strcmp(temp, "/") == 0) parser->vm = emit_code(parser->vm, divide, NULL, 0);
		}
	}
}

void expr(struct parser *parser)
{
	term(parser);
	
	while(strcmp(parser->current_tk->lex, "+") == 0 ||
	      strcmp(parser->current_tk->lex, "-") == 0)
	{
		if(parser->panic) break;
		
		char *temp = parser->current_tk->lex;
		
		expect_lex(parser, parser->current_tk->lex);
		term(parser);
		
		if(!parser->had_error)
		{
			if(strcmp(temp, "+") == 0) parser->vm = emit_code(parser->vm, plus, NULL, 0);
			if(strcmp(temp, "-") == 0) parser->vm = emit_code(parser->vm, minus, NULL, 0);
		}
	}
}

void rel(struct parser *parser)
{
	expr(parser);
	
	while(strcmp(parser->current_tk->lex, "<") == 0 ||
	      strcmp(parser->current_tk->lex, ">") == 0)
	{
		if(parser->panic) break;
		
		char *temp = parser->current_tk->lex;
		
		expect_lex(parser, parser->current_tk->lex);
		expr(parser);
		
		if(!parser->had_error)
		{
			if(strcmp(temp, "<") == 0) parser->vm = emit_code(parser->vm, less_than, NULL, 0);
			if(strcmp(temp, ">") == 0) parser->vm = emit_code(parser->vm, more_than, NULL, 0);
		}
	}
}

void parser_or(struct parser *parser)
{
	rel(parser);
	
	while(strcmp(parser->current_tk->lex, "or") == 0)
	{
		if(parser->panic) break;
		
		expect_lex(parser, "or");
		rel(parser);
		
		if(!parser->had_error) parser->vm = emit_code(parser->vm, or, NULL, 0);
	}
} 

void parser_and(struct parser *parser)
{
	parser_or(parser);
	
	while(strcmp(parser->current_tk->lex, "and") == 0)
	{
		if(parser->panic) break;
		
		expect_lex(parser, "and");
		parser_or(parser);
		
		if(!parser->had_error) parser->vm = emit_code(parser->vm, and, NULL, 0);
	}
} 

void body(struct parser *parser); /* forward declaration of body for flow */

void flow(struct parser *parser)
{	
	char *temp = parser->current_tk->lex;

	expect_lex(parser, parser->current_tk->lex); /* previous function says it has to be "if" or "while" */
	
	if(!parser->syntax_error) parser->current_tb = push_tb(parser->current_tb);
	
	expect_lex(parser, "(");
	
	float temp_label = parser->vm->num_of_labels;
	
	if(strcmp("if", temp) == 0)
	{
		parser_and(parser);
		if(!parser->had_error)
		{
			float *args = create_args(1, temp_label);
			parser->vm = emit_code(parser->vm, jmpf, args, 1);
			parser->vm->num_of_labels++;
		}
	} else if(strcmp("while", temp) == 0)
	{
		if(!parser->had_error)
		{
			float *args = create_args(1, temp_label);
			parser->vm = emit_code(parser->vm, label, args, 1);
		}
		
		parser_and(parser);
		
		if(!parser->had_error)
		{
			float *args = create_args(1, temp_label+1);
			parser->vm = emit_code(parser->vm, jmpf, args, 1);
			parser->vm->num_of_labels += 2;
		}
	}
	
	expect_lex(parser, "->");
	
	body(parser);
	
	if(strcmp("if", temp) == 0)
	{
		if(!parser->had_error)
		{
			float *args = create_args(1, temp_label);
			parser->vm = emit_code(parser->vm, label, args, 1);
		}
	} else if(strcmp("while", temp) == 0)
	{
		if(!parser->had_error)
		{
			float *args1 = create_args(1, temp_label);
			parser->vm = emit_code(parser->vm, jmp, args1, 1);
			
			float *args2 = create_args(1, temp_label+1);
			parser->vm = emit_code(parser->vm, label, args2, 1);
		}
	}
	
	expect_lex(parser, ")");
	
	if(!parser->syntax_error) parser->current_tb = pop_tb(parser->current_tb);
}

void parser_return(struct parser *parser)
{
	expect_lex(parser, "ret");
	parser_and(parser);
	expect_lex(parser, ";");
	
	if(!parser->had_error) parser->vm = emit_code(parser->vm, ret_val, NULL, 0);
}

void parser_print(struct parser *parser)
{
	struct entry *entry;
	
	expect_lex(parser, "print");
	
	if(!parser->syntax_error)
	{
		entry = search_entry(parser->current_tb, parser->current_tk->lex, var_type);
		
		if(entry == NULL) parser->had_error = 1;
	}
	
	if(!parser->had_error)
	{
		float *args = create_args(1, entry->rel_addr);
		parser->vm = emit_code(parser->vm, push_val, args, 1);
	}
	
	expect_type(parser, id);
	
	if(!parser->had_error) parser->vm = emit_code(parser->vm, print, NULL, 0);
	
	expect_lex(parser, ";");
}

void next(struct parser *parser)
{
	struct entry *entry;
	
	if(strcmp(parser->current_tk->lex, "=") == 0)
	{
		if(!parser->syntax_error)
		{
			entry = search_entry(parser->current_tb, (parser->current_tk-1)->lex, var_type);
			
			if(entry == NULL) parser->had_error = 1;
		}
		
		if(!parser->had_error)
		{
			float *args = create_args(1, entry->rel_addr);
			parser->vm = emit_code(parser->vm, push_adr, args, 1);
		}
		
		expect_lex(parser, "=");
		parser_and(parser);
		
		if(!parser->had_error) parser->vm = emit_code(parser->vm, set_equal, NULL, 0);
		
		expect_lex(parser, ";");
	} else if(strcmp(parser->current_tk->lex, "(") == 0)
	{
		if(!parser->syntax_error)
		{
			entry = search_entry(parser->current_tb, (parser->current_tk-1)->lex, func_type);
			
			int args = funcparens(parser);
			
			if(entry == NULL) parser->had_error = 1;
			
			if(entry->num_of_args != args)
			{
				printf("ERROR: incorrect number of arguments!\n");
				parser->had_error = 1;
			}
			
			if(!parser->had_error)
			{
				float *args = create_args(1, entry->rel_addr);
				parser->vm = emit_code(parser->vm, call, args, 1);
			}
		}
		
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
	
	if(!parser->syntax_error)
	{
		parser->current_tb = create_entry(parser->current_tb, parser->rel_addr, parser->current_tk->lex, var_type);
		parser->rel_addr++;
	}
	
	if(!parser->had_error)
	{
		parser->vm = emit_code(parser->vm, decl, NULL, 0);
		
		float *args = create_args(1, parser->rel_addr-1);
		parser->vm = emit_code(parser->vm, push_adr, args, 1);
	}
	
	expect_type(parser, id);
	expect_lex(parser, "=");
	parser_and(parser);
	
	if(!parser->had_error) parser->vm = emit_code(parser->vm, set_equal, NULL, 0);
	
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

	/* rel_addr is not incremented for function declarations */
	if(!parser->syntax_error)
	{
		parser->current_tb = create_entry(parser->current_tb, parser->vm->num_of_labels, parser->current_tk->lex, func_type);
		parser->vm->num_of_labels++;
	}
	
	if(!parser->had_error)
	{
		float *args = create_args(1, (float)parser->vm->num_of_labels-1);
		parser->vm = emit_code(parser->vm, label, args, 1);
	}
	
	expect_type(parser, id);
	
	if(!parser->syntax_error) parser->current_tb = push_tb(parser->current_tb); /* new scope for inside of function */
	
	
	parser->current_tb->back->entry[parser->current_tb->back->num_of_entries-1].num_of_args = 0;
	
	
	if(parser->current_tk->type == id)
	{
		
		/*
			THESE ARE FOR FUNCTION ARGUMENTS, ***MIGHT*** NEED TO FIX SYMBOL TABLES LATER FOR THIS
		*/
		
		if(!parser->syntax_error)
		{
			parser->current_tb = create_entry(parser->current_tb, parser->rel_addr, parser->current_tk->lex, var_type);
			parser->current_tb->back->entry[parser->current_tb->back->num_of_entries-1].num_of_args++;
			parser->rel_addr++;
		}
		
		expect_type(parser, id);
		
		while(strcmp(parser->current_tk->lex, ",") == 0)
		{
			if(parser->panic) break;
			
			expect_lex(parser, ",");
			
			if(!parser->syntax_error)
			{
				parser->current_tb = create_entry(parser->current_tb, parser->rel_addr, parser->current_tk->lex, var_type);
				parser->current_tb->back->entry[parser->current_tb->back->num_of_entries-1].num_of_args++;
				parser->rel_addr++;
			}
			
			expect_type(parser, id);
		}
	}
	
	expect_lex(parser, "->");
	
	body(parser);
	
	expect_lex(parser, ")");
	
	if(!parser->had_error) parser->vm = emit_code(parser->vm, ret_none, NULL, 0);
	
	if(!parser->syntax_error) parser->current_tb = pop_tb(parser->current_tb);
}

void funclist(struct parser *parser)
{
	parser->current_tb = push_tb(parser->current_tb); /* global scope for function declarations */
	
	while(parser->current_tk->type != eoi)
	{
		parser->rel_addr = 0;
		
		funcdecl(parser);
	}
	
	if(!parser->syntax_error) parser->current_tb = pop_tb(parser->current_tb);

	expect_lex(parser, "\0");
}

int main(void)
{
	struct parser *parser = malloc(sizeof(struct parser));
	parser->tk_list = malloc(sizeof(struct token));
	
	parser->code = "(f a, b -> decl c = a + b; ret c;) (g -> decl x = 5; f(x, 2); g();)";
	parser->begin = 0;
	parser->panic = 0;
	parser->syntax_error = 0;
	parser->had_error = 0;
	parser->list_size = 0;
	
	parser->current_tb = NULL;
	
	parser->vm = create_vm();
	
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
	
	print_code(parser->vm);
	
	run_vm(parser->vm);
	
	//printf("%d\n", parser->vm->code->args[0]);
	
	//int j;
	//for(j = 0; j<(int)parser->vm->num_of_labels; j++) printf("%d\n", (int)parser->vm->label_list[j]);
	
	getchar();
	
	/* clears out symbol table stack if an error occurs; this won't do anything if the program
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