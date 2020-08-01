#include "args.h"
#include <stdio.h>
#include <string.h>


void args_print_usage(const struct arg_option options[], int numopts)
{
    printf("\nUsage: ");

    for (int i = 0; i < numopts; i++)
    {
        printf("\n   -%c", options[i].option);
        if (options[i].alias)
            printf(",%s", options[i].alias);
        if (options[i].arguments_text && options[i].num_arguments)
            printf("\t[%s]", options[i].arguments_text);

        printf("\t%s", options[i].description);
    }
}

int args_parse(int argc, char* argv[], const struct arg_option options[], int numopts, void* args_state, int (*parseCb)(int index, char opt, char**args, void* state))
{
    // iterate all arguments
    for (int arg_index = 1; arg_index < argc; arg_index++)
    {
        int found_option_index = -1;

        if (strlen(argv[arg_index]) == 2 && argv[arg_index][0] == '-') // check if using -%c format
        {
            for (int option_index = 0; option_index < numopts; option_index++)
            {
                if (options[option_index].option == argv[arg_index][1])
                {
                    found_option_index = option_index;
                    break;
                }
            }

            if (found_option_index < 0)
            {
                fprintf(stderr, "\nUnknown option '-%c'!", argv[arg_index][1]);
                return ARGS_UNKNOWN;
            }
        }
        else // check if using alias format
        {
            for (int option_index = 0; option_index < numopts; option_index++)
            {
                if (!options[option_index].alias)
                    continue;

                if (strcmp(options[option_index].alias, argv[arg_index]) == 0)
                {
                    found_option_index = option_index;
                    break;
                }
            }

            if (found_option_index < 0)
            {
                fprintf(stderr, "\nUnknown option '%s'!", argv[arg_index]);
                return ARGS_UNKNOWN;
            }
        }

        if (argc - arg_index -1 < options[found_option_index].num_arguments)
        {
            fprintf(stderr, "\nToo few parameters for option -%c, expected [%s]", options[found_option_index].option, options[found_option_index].arguments_text);
            return ARGS_WRONG_COUNT;
        }

        int ret;

        if ((ret = parseCb(arg_index, options[found_option_index].option, &argv[arg_index+1], args_state)) != ARGS_OK)
            return ret;

        arg_index += options[found_option_index].num_arguments;
    }

    return ARGS_OK;
}
