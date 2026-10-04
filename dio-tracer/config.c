#define _POSIX_C_SOURCE 200809L
#include "config.h"

#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yaml.h>

static int fail(const char *name, const char *message)
{
    fprintf(stderr, "Configuração: %s: %s\n", name, message);
    return -1;
}

static int number(const char *name, const char *text, unsigned long maximum, unsigned *result)
{
    if (*text == '\0') {
        return fail(name, "falta um inteiro sem sinal");
    }
    for (const char *p = text; *p != '\0'; p++) {
        if (!isdigit((unsigned char)*p)) {
            return fail(name, "esperado um inteiro sem sinal");
        }
    }
    errno = 0;
    unsigned long value = strtoul(text, NULL, 10);
    if (errno != 0 || value > maximum) {
        return fail(name, "inteiro fora dos limites");
    }
    *result = (unsigned)value;
    return 0;
}

static int boolean(const char *name, const char *text, bool *result)
{
    if (strcmp(text, "true") == 0) {
        *result = true;
    } else if (strcmp(text, "false") == 0) {
        *result = false;
    } else {
        return fail(name, "usa true ou false");
    }
    return 0;
}

static int list(struct config *cfg, const char *name, const char *text)
{
    bool events = strcmp(name, "tracer.events") == 0;
    bool tids = strcmp(name, "tracer.target_tids") == 0;
    uint32_t *ids = tids ? cfg->tids : cfg->pids;
    unsigned *count = tids ? &cfg->nr_tids : &cfg->nr_pids;
    char buffer[4096];

    if (strlen(text) >= sizeof(buffer)) {
        return fail(name, "lista demasiado longa");
    }
    strcpy(buffer, text);
    if (events) {
        cfg->openat = cfg->read = cfg->write = cfg->close = false;
    } else {
        *count = 0;
        if (*text == '\0') {
            return 0;
        }
    }

    char *next = buffer;
    while (next != NULL) {
        char *item = next;
        next = strchr(item, ',');
        if (next != NULL) {
            *next = '\0';
            next++;
        }
        while (isspace((unsigned char)*item)) {
            item++;
        }
        char *end = item + strlen(item);
        while (end > item && isspace((unsigned char)end[-1])) {
            end--;
            *end = '\0';
        }
        if (events) {
            if (strcmp(item, "all") == 0) {
                cfg->openat = cfg->read = cfg->write = cfg->close = true;
            } else if (strcmp(item, "openat") == 0) {
                cfg->openat = true;
            } else if (strcmp(item, "read") == 0) {
                cfg->read = true;
            } else if (strcmp(item, "write") == 0) {
                cfg->write = true;
            } else if (strcmp(item, "close") == 0) {
                cfg->close = true;
            } else {
                return fail(name, "usa openat, read, write, close ou all; não deixes elementos vazios");
            }
        } else {
            unsigned id;
            if (number(name, item, INT_MAX, &id) != 0) {
                return -1;
            }
            if (id == 0) {
                return fail(name, "PID/TID deve ser um inteiro positivo");
            }
            unsigned i = 0;
            while (i < *count && ids[i] != id) {
                i++;
            }
            if (i == *count) {
                if (*count == MAX_TARGETS) {
                    return fail(name, "limite de 256 alvos excedido");
                }
                ids[*count] = id;
                (*count)++;
            }
        }
    }
    return 0;
}

static int apply(struct config *cfg, const char *name, const char *text)
{
    if (strcmp(name, "tracer.events") == 0 || strcmp(name, "tracer.target_pids") == 0 ||
        strcmp(name, "tracer.target_tids") == 0) {
        return list(cfg, name, text);
    }
    if (strcmp(name, "tracer.discard_errors") == 0) {
        return boolean(name, text, &cfg->discard_errors);
    }
    if (strcmp(name, "tracer.discard_directories") == 0) {
        return boolean(name, text, &cfg->discard_directories);
    }
    if (strcmp(name, "tracer.duration") == 0) {
        return number(name, text, UINT_MAX, &cfg->duration);
    }
    if (strcmp(name, "tracer.session_name") == 0) {
        if (strlen(text) >= sizeof(cfg->session_name)) {
            return fail(name, "nome de sessão demasiado longo");
        }
        strcpy(cfg->session_name, text);
        return 0;
    }
    if (strcmp(name, "output.file_writer.enabled") == 0) {
        if (strcmp(text, "true") != 0) {
            return fail(name, "só true é suportado");
        }
        return 0;
    }
    if (strcmp(name, "output.file_writer.filename") == 0) {
        if (strlen(text) >= sizeof(cfg->output)) {
            return fail(name, "caminho demasiado longo");
        }
        strcpy(cfg->output, strcmp(text, "-") == 0 ? "" : text);
        return 0;
    }
    return fail(name, "opção desconhecida");
}

static const char *scalar(yaml_node_t *node)
{
    if (node == NULL || node->type != YAML_SCALAR_NODE ||
        strlen((char *)node->data.scalar.value) != node->data.scalar.length) {
        return NULL;
    }
    return (char *)node->data.scalar.value;
}

static int read_node(struct config *cfg, yaml_document_t *doc, yaml_node_t *node,
                     const char *name, unsigned depth)
{
    if (node == NULL || depth > 3) {
        return fail(name, "estrutura YAML inválida");
    }
    if (node->type == YAML_MAPPING_NODE) {
        if (*name != '\0' && strcmp(name, "tracer") != 0 && strcmp(name, "output") != 0 &&
            strcmp(name, "output.file_writer") != 0) {
            return fail(name, "grupo desconhecido ou valor esperado");
        }
        for (yaml_node_pair_t *p = node->data.mapping.pairs.start;
             p < node->data.mapping.pairs.top; p++) {
            const char *key = scalar(yaml_document_get_node(doc, p->key));
            if (key == NULL || *key == '\0' || strchr(key, '.') != NULL) {
                return fail(name, "chave YAML inválida");
            }
            for (yaml_node_pair_t *q = node->data.mapping.pairs.start; q < p; q++) {
                const char *previous = scalar(yaml_document_get_node(doc, q->key));
                if (previous != NULL && strcmp(key, previous) == 0) {
                    return fail(key, "chave YAML repetida");
                }
            }
            char child[128];
            int length = snprintf(child, sizeof(child), "%s%s%s", name, *name != '\0' ? "." : "", key);
            if (length < 0 || (size_t)length >= sizeof(child)) {
                return fail(name, "chave YAML demasiado longa");
            }
            if (read_node(cfg, doc, yaml_document_get_node(doc, p->value), child, depth + 1) != 0) {
                return -1;
            }
        }
        return 0;
    }
    if (node->type == YAML_SEQUENCE_NODE) {
        if (strcmp(name, "tracer.events") != 0 && strcmp(name, "tracer.target_pids") != 0 &&
            strcmp(name, "tracer.target_tids") != 0) {
            return fail(name, "lista inesperada");
        }
        char text[4096] = "";
        size_t used = 0;
        for (yaml_node_item_t *p = node->data.sequence.items.start;
             p < node->data.sequence.items.top; p++) {
            const char *item = scalar(yaml_document_get_node(doc, *p));
            if (item == NULL || *item == '\0' || strchr(item, ',') != NULL) {
                return fail(name, "elemento da lista inválido");
            }
            size_t length = strlen(item);
            if (used + length + 2 > sizeof(text)) {
                return fail(name, "lista demasiado longa");
            }
            if (used > 0) {
                text[used++] = ',';
            }
            memcpy(text + used, item, length + 1);
            used += length;
        }
        return apply(cfg, name, text);
    }
    const char *text = scalar(node);
    if (text == NULL) {
        return fail(name, "valor YAML inválido");
    }
    return apply(cfg, name, text);
}

static int read_yaml(struct config *cfg, const char *filename)
{
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        return fail(filename, strerror(errno));
    }
    yaml_parser_t parser;
    yaml_document_t doc;
    if (!yaml_parser_initialize(&parser)) {
        fclose(file);
        return fail(filename, "não foi possível criar o parser YAML");
    }
    yaml_parser_set_input_file(&parser, file);
    int result = -1;
    if (!yaml_parser_load(&parser, &doc)) {
        fail(filename, parser.problem ? parser.problem : "YAML inválido");
    } else {
        result = read_node(cfg, &doc, yaml_document_get_root_node(&doc), "", 0);
        yaml_document_delete(&doc);
        if (result == 0) {
            if (!yaml_parser_load(&parser, &doc)) {
                result = fail(filename, "YAML inválido depois do documento");
            } else {
                if (yaml_document_get_root_node(&doc) != NULL) {
                    result = fail(filename, "usa apenas um documento YAML");
                }
                yaml_document_delete(&doc);
            }
        }
    }
    yaml_parser_delete(&parser);
    fclose(file);
    return result;
}

static void usage(const char *program)
{
    printf("Uso: %s [--config FICHEIRO] [--pid PID[,PID...]] [--tid TID[,TID...]] [opções]\n", program);
    puts("  --config FICHEIRO              configuração YAML\n"
         "  --pid PID[,PID...]              PIDs a observar\n"
         "  --tid TID[,TID...]              TIDs têm prioridade sobre PIDs\n"
         "  --events LISTA                 openat,read,write,close ou all (default: all)\n"
         "  --duration SEGUNDOS            zero não impõe limite de duração\n"
         "  --discard-errors true|false    default: false\n"
         "  --discard-directories true|false  default: false\n"
         "  --output FICHEIRO              JSON; substitui o ficheiro; '-' usa stdout\n"
         "  --session-name NOME           nome da sessão no JSON\n"
         "  --help                         mostra esta ajuda\n"
         "Precedência: defaults < YAML < CLI. Sem --output, usa stdout. Ctrl+C termina.");
}

int config_read(struct config *cfg, int argc, char **argv)
{
    static const struct option options[] = {
        {"config", required_argument, NULL, 'c'},
        {"pid", required_argument, NULL, 'p'},
        {"tid", required_argument, NULL, 't'},
        {"events", required_argument, NULL, 'e'},
        {"duration", required_argument, NULL, 'd'},
        {"discard-errors", required_argument, NULL, 'E'},
        {"discard-directories", required_argument, NULL, 'D'},
        {"output", required_argument, NULL, 'o'},
        {"session-name", required_argument, NULL, 's'},
        {"help", no_argument, NULL, 'h'},
        {NULL, 0, NULL, 0}
    };
    memset(cfg, 0, sizeof(*cfg));
    cfg->openat = cfg->read = cfg->write = cfg->close = true;
    const char *filename = NULL;
    int option;
    opterr = 0;
    optind = 0;

    // Encontrar o YAML antes de aplicar qualquer opção da CLI.
    while ((option = getopt_long(argc, argv, "", options, NULL)) != -1) {
        if (option == 'h') {
            usage(argv[0]);
            return 1;
        }
        if (option == '?') {
            return fail("CLI", "opção desconhecida ou argumento em falta; usa --help");
        }
        if (option == 'c') {
            filename = optarg;
        }
    }
    if (optind != argc) {
        return fail("CLI", "argumentos posicionais não são suportados");
    }
    if (filename != NULL && read_yaml(cfg, filename) != 0) {
        return -1;
    }

    optind = 0;
    while ((option = getopt_long(argc, argv, "", options, NULL)) != -1) {
        const char *name = NULL;
        switch (option) {
        case 'p': name = "tracer.target_pids"; break;
        case 't': name = "tracer.target_tids"; break;
        case 'e': name = "tracer.events"; break;
        case 'd': name = "tracer.duration"; break;
        case 'E': name = "tracer.discard_errors"; break;
        case 'D': name = "tracer.discard_directories"; break;
        case 'o': name = "output.file_writer.filename"; break;
        case 's': name = "tracer.session_name"; break;
        default: break;
        }
        if (name != NULL && apply(cfg, name, optarg) != 0) {
            return -1;
        }
    }
    if (cfg->nr_pids == 0 && cfg->nr_tids == 0) {
        return fail("alvos", "indica pelo menos um PID ou TID");
    }
    return 0;
}
