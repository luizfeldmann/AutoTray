
enum arg_error
{
    ARGS_OK = 0,
    ARGS_UNKNOWN = -1,
    ARGS_WRONG_VALUE = -2,
    ARGS_WRONG_COUNT = -3,
};

struct arg_option {
    char option;
    char* alias;
    int num_arguments;
    char* arguments_text;
    char* description;
};

void args_print_usage(const struct arg_option options[], int numopts);
int args_parse(int argc, char* argv[], const struct arg_option options[], int numopts, void* args_state, int (*parseCb)(int index, char opt, char**args, void* state));
