/*  BuGLe: an OpenGL debugging tool
 *  Copyright (C) 2004  Bruce Merry
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <GL/gl.h>

#define MAX_TOKEN_LEN 100
#define MAX_TOKEN_LEN_STR "100"

typedef struct
{
    char name[MAX_TOKEN_LEN + 1];
    unsigned int value;
} token;

static int compare_name(const void *a, const void *b)
{
    const token *A = (const token *) a;
    const token *B = (const token *) b;
    return strcmp(A->name, B->name);
}

static int compare_value(const void *a, const void *b)
{
    const token *A = (const token *) a;
    const token *B = (const token *) b;
    if (A->value < B->value) return -1;
    else if (A->value > B->value) return 1;
    else return 0;
}

static void dump_tokens(token *tokens, int count, const char *name)
{
    int i;
    printf("const gl_token %s[] =\n"
           "{\n", name);
    for (i = 0; i < count; i++)
    {
        if (i) fputs(",\n", stdout);
        printf("    { \"%s\", 0x%.4x }", tokens[i].name, tokens[i].value);
    }
    fputs("\n};\n\n", stdout);
}

int main()
{
    token *tokens = NULL;
    int num_tokens = 0;
    int max_tokens = 0;
    int r;

    while (!feof(stdin))
    {
        if (num_tokens == max_tokens)
        {
            if (!max_tokens) max_tokens = 1024;
            else max_tokens *= 2;
            tokens = realloc(tokens, sizeof(token) * max_tokens);
        }
        r = scanf("%" MAX_TOKEN_LEN_STR "s %x",
                  tokens[num_tokens].name,
                  &tokens[num_tokens].value);
        if (r != 2) break;
        assert(strlen(tokens[num_tokens].name) < MAX_TOKEN_LEN);
        num_tokens++;
    }
    fputs("#if HAVE_CONFIG_H\n"
          "# include <config.h>\n"
          "#endif\n"
          "#include <GL/gl.h>\n"
          "#include \"src/gltokens.h\"\n"
          "\n", stdout);
    qsort(tokens, num_tokens, sizeof(token), &compare_name);
    dump_tokens(tokens, num_tokens, "gl_tokens_name");
    qsort(tokens, num_tokens, sizeof(token), &compare_value);
    dump_tokens(tokens, num_tokens, "gl_tokens_value");
    printf("int gl_token_count = %d;\n", num_tokens);
    return 0;
}
