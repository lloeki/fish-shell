#include "parse_productions.h"

using namespace parse_productions;
#define NO_PRODUCTION ((production_option_idx_t)(-1))

static bool production_is_empty(const production_t production)
{
    return production[0] == token_type_invalid;
}

// Empty productions are allowed but must be first. Validate that the given production is in the valid range, i.e. it is either not empty or there is a non-empty production after it
static bool production_is_valid(const production_options_t production_list, production_option_idx_t which)
{
    if (which < 0 || which >= MAX_PRODUCTIONS)
        return false;

    bool nonempty_found = false;
    for (int i=which; i < MAX_PRODUCTIONS; i++)
    {
        if (! production_is_empty(production_list[i]))
        {
            nonempty_found = true;
            break;
        }
    }
    return nonempty_found;
}

#define PRODUCTIONS(sym) static const production_options_t productions_##sym
#define RESOLVE(sym) static production_option_idx_t resolve_##sym (parse_token_type_t token_type, parse_keyword_t token_keyword)
#define RESOLVE_ONLY(sym) static production_option_idx_t resolve_##sym (parse_token_type_t token_type, parse_keyword_t token_keyword) { return 0; }

#define KEYWORD(x) ((x) + LAST_TOKEN_OR_SYMBOL + 1)


/* A job_list is a list of jobs, separated by semicolons or newlines */
PRODUCTIONS(job_list) =
{
    {},
    {symbol_job, symbol_job_list},
    {parse_token_type_end, symbol_job_list}
};

RESOLVE(job_list)
{
    switch (token_type)
    {
        case parse_token_type_string:
            // 'end' is special
            switch (token_keyword)
            {
                case parse_keyword_end:
                case parse_keyword_else:
                    // End this job list
                    return 0;

                default:
                    // Normal string
                    return 1;
            }

        case parse_token_type_pipe:
        case parse_token_type_redirection:
        case parse_token_type_background:
            return 1;

        case parse_token_type_end:
            // Empty line
            return 2;

        case parse_token_type_terminate:
            // no more commands, just transition to empty
            return 0;

        default:
            return NO_PRODUCTION;
    }
}

/* A job is a non-empty list of statements, separated by pipes. (Non-empty is useful for cases like if statements, where we require a command). To represent "non-empty", we require a statement, followed by a possibly empty job_continuation */

PRODUCTIONS(job) =
{
    {symbol_statement, symbol_job_continuation}
};
RESOLVE_ONLY(job)

PRODUCTIONS(job_continuation) =
{
    {},
    {parse_token_type_pipe, symbol_statement, symbol_job_continuation}
};
RESOLVE(job_continuation)
{
    switch (token_type)
    {
        case parse_token_type_pipe:
            // Pipe, continuation
            return 1;

        default:
            // Not a pipe, no job continuation
            return 0;
    }
}

/* A statement is a normal command, or an if / while / and etc */
PRODUCTIONS(statement) =
{
    {symbol_boolean_statement},
    {symbol_block_statement},
    {symbol_if_statement},
    {symbol_switch_statement},
    {symbol_decorated_statement}
};
RESOLVE(statement)
{
    switch (token_type)
    {
        case parse_token_type_string:
            switch (token_keyword)
            {
                case parse_keyword_and:
                case parse_keyword_or:
                case parse_keyword_not:
                    return 0;

                case parse_keyword_for:
                case parse_keyword_while:
                case parse_keyword_function:
                case parse_keyword_begin:
                    return 1;

                case parse_keyword_if:
                    return 2;

                case parse_keyword_else:
                    return NO_PRODUCTION;

                case parse_keyword_switch:
                    return 3;

                case parse_keyword_end:
                    return NO_PRODUCTION;

                    // 'in' is only special within a for_header
                case parse_keyword_in:
                case parse_keyword_none:
                case parse_keyword_command:
                case parse_keyword_builtin:
                case parse_keyword_case:
                    return 4;
            }
            break;

        case parse_token_type_pipe:
        case parse_token_type_redirection:
        case parse_token_type_background:
        case parse_token_type_terminate:
            return NO_PRODUCTION;
            //parse_error(L"statement", token);

        default:
            return NO_PRODUCTION;
    }
}

PRODUCTIONS(if_statement) =
{
    {symbol_if_clause, symbol_else_clause, KEYWORD(parse_keyword_end), symbol_arguments_or_redirections_list}
};
RESOLVE_ONLY(if_statement)

PRODUCTIONS(if_clause) =
{
    { KEYWORD(parse_keyword_if), symbol_job, parse_token_type_end, symbol_job_list }
};
RESOLVE_ONLY(if_clause)

PRODUCTIONS(else_clause) =
{
    { },
    { KEYWORD(parse_keyword_else), symbol_else_continuation }
};
RESOLVE(else_clause)
{
    switch (token_keyword)
    {
        case parse_keyword_else:
            return 1;
        default:
            return 0;
    }
}

PRODUCTIONS(else_continuation) =
{
    {symbol_if_clause, symbol_else_clause},
    {parse_token_type_end, symbol_job_list}
};
RESOLVE(else_continuation)
{
    switch (token_keyword)
    {
        case parse_keyword_if:
            return 0;
        default:
            return 1;
    }
}

PRODUCTIONS(switch_statement) =
{
    { KEYWORD(parse_keyword_switch), parse_token_type_string, parse_token_type_end, symbol_case_item_list, KEYWORD(parse_keyword_end)}
};
RESOLVE_ONLY(switch_statement)

PRODUCTIONS(case_item_list) =
{
    {},
    {symbol_case_item, symbol_case_item_list},
    {parse_token_type_end, symbol_case_item_list}
};
RESOLVE(case_item_list)
{
    if (token_keyword == parse_keyword_case) return 1;
    else if (token_type == parse_token_type_end) return 2; //empty line
    else return 0;
}

PRODUCTIONS(case_item) =
{
    {KEYWORD(parse_keyword_case), symbol_argument_list, parse_token_type_end, symbol_job_list}
};
RESOLVE_ONLY(case_item)

PRODUCTIONS(argument_list) =
{
    {},
    {symbol_argument, symbol_argument_list}
};
RESOLVE(argument_list)
{
    switch (token_type)
    {
        case parse_token_type_string:
            return 1;
        default:
            return 0;
    }
}

PRODUCTIONS(block_statement) =
{
    {symbol_block_header, parse_token_type_end, symbol_job_list, KEYWORD(parse_keyword_end), symbol_arguments_or_redirections_list}
};
RESOLVE_ONLY(block_statement)

PRODUCTIONS(block_header) =
{
    {symbol_for_header},
    {symbol_while_header},
    {symbol_function_header},
    {symbol_begin_header}
};
RESOLVE(block_header)
{
    switch (token_keyword)
    {
        case parse_keyword_else:
            return NO_PRODUCTION;
        case parse_keyword_for:
            return 0;
        case parse_keyword_while:
            return 1;
        case parse_keyword_function:
            return 2;
        case parse_keyword_begin:
            return 3;
        default:
            return NO_PRODUCTION;
    }
}

PRODUCTIONS(for_header) =
{
    {KEYWORD(parse_keyword_for), parse_token_type_string, KEYWORD(parse_keyword_in), symbol_arguments_or_redirections_list}
};
RESOLVE_ONLY(for_header)

PRODUCTIONS(while_header) =
{
    {KEYWORD(parse_keyword_while), symbol_statement}
};
RESOLVE_ONLY(while_header)

PRODUCTIONS(begin_header) =
{
    {KEYWORD(parse_keyword_begin)}
};
RESOLVE_ONLY(begin_header)

PRODUCTIONS(function_header) =
{
    {KEYWORD(parse_keyword_function), parse_token_type_string, symbol_argument_list}
};
RESOLVE_ONLY(function_header)

/* A boolean statement is AND or OR or NOT */
PRODUCTIONS(boolean_statement) =
{
    {KEYWORD(parse_keyword_and), symbol_statement},
    {KEYWORD(parse_keyword_or), symbol_statement},
    {KEYWORD(parse_keyword_not), symbol_statement}
};
RESOLVE(boolean_statement)
{
    switch (token_keyword)
    {
        case parse_keyword_and:
            return 0;
        case parse_keyword_or:
            return 1;
        case parse_keyword_not:
            return 2;
        default:
            return NO_PRODUCTION;
    }
}

PRODUCTIONS(decorated_statement) =
{
    {symbol_plain_statement},
    {KEYWORD(parse_keyword_command), symbol_plain_statement},
    {KEYWORD(parse_keyword_builtin), symbol_plain_statement},
};
RESOLVE(decorated_statement)
{
    switch (token_keyword)
    {
        default:
            return 0;
        case parse_keyword_command:
            return 1;
        case parse_keyword_builtin:
            return 2;
    }
}

PRODUCTIONS(plain_statement) =
{
    {parse_token_type_string, symbol_arguments_or_redirections_list, symbol_optional_background}
};
RESOLVE_ONLY(plain_statement)

PRODUCTIONS(arguments_or_redirections_list) =
{
    {},
    {symbol_argument_or_redirection, symbol_arguments_or_redirections_list}
};
RESOLVE(arguments_or_redirections_list)
{
    switch (token_type)
    {
        case parse_token_type_string:
        case parse_token_type_redirection:
            return 1;
        default:
            return 0;
    }
}

PRODUCTIONS(argument_or_redirection) =
{
    {symbol_argument},
    {parse_token_type_redirection}
};
RESOLVE(argument_or_redirection)
{
    switch (token_type)
    {
        case parse_token_type_string:
            return 0;
        case parse_token_type_redirection:
            return 1;
        default:
            return NO_PRODUCTION;
    }
}

PRODUCTIONS(argument) =
{
    {parse_token_type_string}
};
RESOLVE_ONLY(argument)

PRODUCTIONS(redirection) =
{
    {parse_token_type_redirection}
};
RESOLVE_ONLY(redirection)

PRODUCTIONS(optional_background) =
{
    {},
    { parse_token_type_background }
};

RESOLVE(optional_background)
{
    switch (token_type)
    {
        case parse_token_type_background:
            return 1;
        default:
            return 0;
    }
}

#define TEST(sym) case (symbol_##sym): production_list = & productions_ ## sym ; resolver = resolve_ ## sym ; break;
const production_t *parse_productions::production_for_token(parse_token_type_t node_type, parse_token_type_t input_type, parse_keyword_t input_keyword, production_option_idx_t *out_which_production, wcstring *out_error_text)
{
    bool log_it = false;
    if (log_it)
    {
        fprintf(stderr, "Resolving production for %ls with input type %ls <%ls>\n", token_type_description(node_type).c_str(), token_type_description(input_type).c_str(), keyword_description(input_keyword).c_str());
    }

    /* Fetch the list of productions and the function to resolve them */
    const production_options_t *production_list = NULL;
    production_option_idx_t (*resolver)(parse_token_type_t token_type, parse_keyword_t token_keyword) = NULL;
    switch (node_type)
    {
            TEST(job_list)
            TEST(job)
            TEST(statement)
            TEST(job_continuation)
            TEST(boolean_statement)
            TEST(block_statement)
            TEST(if_statement)
            TEST(if_clause)
            TEST(else_clause)
            TEST(else_continuation)
            TEST(switch_statement)
            TEST(decorated_statement)
            TEST(case_item_list)
            TEST(case_item)
            TEST(argument_list)
            TEST(block_header)
            TEST(for_header)
            TEST(while_header)
            TEST(begin_header)
            TEST(function_header)
            TEST(plain_statement)
            TEST(arguments_or_redirections_list)
            TEST(argument_or_redirection)
            TEST(argument)
            TEST(redirection)
            TEST(optional_background)

        case parse_token_type_string:
        case parse_token_type_pipe:
        case parse_token_type_redirection:
        case parse_token_type_background:
        case parse_token_type_end:
        case parse_token_type_terminate:
            fprintf(stderr, "Terminal token type %ls passed to %s\n", token_type_description(node_type).c_str(), __FUNCTION__);
            PARSER_DIE();
            break;

        case parse_special_type_parse_error:
        case parse_special_type_tokenizer_error:
        case parse_special_type_comment:
            fprintf(stderr, "Special type %ls passed to %s\n", token_type_description(node_type).c_str(), __FUNCTION__);
            PARSER_DIE();
            break;


        case token_type_invalid:
            fprintf(stderr, "token_type_invalid passed to %s\n", __FUNCTION__);
            PARSER_DIE();
            break;

    }
    PARSE_ASSERT(production_list != NULL);
    PARSE_ASSERT(resolver != NULL);

    const production_t *result = NULL;
    production_option_idx_t which = resolver(input_type, input_keyword);

    if (log_it)
    {
        fprintf(stderr, "\tresolved to %u\n", (unsigned)which);
    }


    if (which == NO_PRODUCTION)
    {
        if (log_it)
        {
            fprintf(stderr, "Token type '%ls' has no production for input type '%ls', keyword '%ls' (in %s)\n", token_type_description(node_type).c_str(), token_type_description(input_type).c_str(), keyword_description(input_keyword).c_str(), __FUNCTION__);
        }
        result = NULL;
    }
    else
    {
        PARSE_ASSERT(production_is_valid(*production_list, which));
        result = &((*production_list)[which]);
    }
    *out_which_production = which;
    return result;
}