struct generic_function_call
{
    budgie_function id;
    int num_args;
    const void **args;
    void *ret;
};

struct function_call_glPolygonMode
{
    const GLenum *arg1;
    const GLenum *arg2;
};

struct function_call
{
    generic_function_call generic;
    const void *args[MAX_ARGS];
    union
    {
        function_call_glPolygonMode glPolygonMode;
        ...
    } typed;
};
