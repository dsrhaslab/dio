# DIO port C + libbpf/CO-RE + JSON Compatível

**Em desenvolvimento.** Este é um port experimental do DIO, a partir da versão Go/BCC. O objetivo é passar o userspace para C e o BPF para libbpf/CO-RE.
Ainda não substitui o DIO original nem está integrado com a sua pipeline.

## O que funciona neste momento

- Captura openat, read, write, close, pread64, pwrite64, readv, writev, fsync e fdatasync, com entrada/saída e duração.
- Passa os eventos do kernel para userspace através de um ring buffer.
- Tem um consumidor em C, na mesma thread do main.
- Filtra por PID ou TID e aceita configuração por CLI e YAML.
- Escreve JSON no terminal ou num ficheiro, usando yyjson.
- Os caminhos aparecem em eventos EventPath separados, ligados aos eventos das syscalls pelo file_tag.

Esta versão acrescenta readv e writev, aproveitando o caminho de read/write, e fsync e fdatasync para observar a sincronização dos ficheiros. Em readv/writev, bytes_requested é a soma dos tamanhos dos vetores na entrada e return_value é o que a syscall realmente transferiu. Não são copiados os conteúdos dos buffers.

Em pread64 e pwrite64, args.offset continua a ser o offset explícito passado pelo programa. Em read/write/readv/writev é a posição observada no ficheiro à entrada.
O output continua a ser um array JSON com os campos do DIO legacy. As strings são escapadas pela biblioteca, os números ficam como números e o array é fechado quando o tracer termina normalmente. Mantém o nome da sessão, os timestamps e os erros com return_value igual a -1 e error_message.

Agora são dez syscalls. Há contadores opcionais de eventos e perdas, mas ainda não há integração com Elasticsearch/Kibana.
O formato foi comparado com o legacy, mas falta ligar e testar a pipeline completa.

A implementação está em dio-tracer/. A pipeline, os scripts e os workloads antigos ainda não fazem parte desta cópia.

## Dependências e build

Usado numa VM Ubuntu x86_64 com kernel 7.0.0-38-generic. É necessário BTF do kernel em /sys/kernel/btf/vmlinux mas costuma já vir compilado na distro.

```bash
sudo apt install build-essential clang bpftool libbpf-dev libyaml-dev libyyjson-dev
make -C dio-tracer
```

libyyjson-dev é usado para escrever o JSON. O Makefile gera vmlinux.h a partir do kernel local, compila o BPF, gera o skeleton e cria dio-tracer/build/dio.

```bash
make -C dio-tracer clean
```

apaga os ficheiros gerados pelo build.

## Executar

A partir da raiz do repo, substituindo "x" pelo PID real:

```bash
sudo ./dio-tracer/build/dio --config dio-tracer/config.yaml --pid x --output /tmp/dio-trace.json
```

O YAML de exemplo deixa os alvos vazios. É obrigatório indicar pelo menos um PID ou TID. --tid x seleciona uma thread; se houver PIDs e TIDs, os TIDs têm prioridade. Os processos filhos não são incluídos automaticamente.

A prioridade é defaults < YAML < CLI. O YAML só é lido com --config. As dez syscalls estão ativas por defeito. --events openat,close seleciona eventos, --duration 20 limita a duração e --output /tmp/dio-trace.json escreve JSON num ficheiro, substituindo o conteúdo anterior.
Sem essa opção, a saída vai para stdout. --session-name teste dá um nome à sessão; se não for indicado, é gerado um nome. Também pode ser definido no YAML em tracer.session_name.
As restantes opções estão em ./dio-tracer/build/dio --help. Ctrl+C termina o tracer e fecha o array JSON. Também termina quando todos os alvos desaparecem ou ficam zombies. Se estiver a observar um PID e ainda houver outras threads desse processo a correr, continua a captura.

As mensagens como DIO ready vão para stderr, para não ficarem misturadas com o JSON. O ficheiro só tem o array completo depois de o tracer terminar.

Na VM com kernel 7.0.0-38 foi encontrada uma regressão nos tracepoints de entrada de openat/write/pwrite64: só eram observados na CPU 0. Para estas três entradas é usado raw_syscalls/sys_enter, filtrado pelo número da syscall. As saídas e o resto do percurso continuam iguais. Foi confirmado nas quatro CPUs da VM.

## Contadores

Para os ativar, acrescentar --show-stats true ao comando ou usar tracer.show_stats: true no YAML. Estão desligados por defeito. O resumo aparece no stderr quando o tracer termina, separado do JSON.

- entered: entradas que passaram o filtro de PID/TID.
- matched: saídas para as quais havia uma entrada guardada.
- filtered: chamadas descartadas pelas opções de erros/diretórios; não são perdas.
- emitted / ring_lost: eventos enviados ao ring / tentativas de envio que falharam.
- received / written: eventos recebidos no callback / documentos escritos com sucesso.
- pending_failed, pending_replaced e pending_delete_failed: problemas ao guardar ou remover o estado entre entrada e saída.
- abandoned e pending_at_stop: entradas limpas quando a thread termina e chamadas ainda pendentes quando a captura para.
- exit_without_entry: saídas sem entrada guardada. Pode acontecer ao começar a observar uma chamada que já estava a correr.
- args_unavailable: não foi possível obter um total válido para os vetores; o evento segue sem bytes_requested.

EventPath tem uma linha separada: attempted, prepare_failed, emitted, ring_lost, received e written. stale significa que o slot com o caminho já tinha sido reutilizado. rejected e failed distinguem registos inválidos de falhas de processamento/output.

accounting=ok significa que as contas fecharam para os pontos que contamos. Não garante que observámos todas as chamadas do programa ou que todos os caminhos chegaram. drain_complete indica se conseguimos consumir o ring até ao fim; output_ok indica se o output terminou sem erro. Um documento escrito ainda não foi importado para Elasticsearch.

Depois de drenar, eligible = matched - filtered = emitted + ring_lost e emitted = received. As entradas pendentes e as falhas dos mapas são contabilizadas à parte. Não contamos enter e exit como dois eventos.

## Teste manual com SQLite

Foi testado na VM com SQLite 3. É um teste pequeno para observar eventos de um programa real. É preciso ter sqlite3 instalado para o fazer.

Num terminal abrir o SQLite sem base de dados em disco:

```bash
cd /tmp
sqlite3
```

No terminal 2, procurar o PID com pgrep -a -x sqlite3 e iniciar o tracer com esse PID, como no exemplo anterior. Esperar pela mensagem DIO ready.

De volta ao terminal 1:

```sql
.open dio-demo.db
CREATE TABLE IF NOT EXISTS teste (valor TEXT);
INSERT INTO teste VALUES ('ola DIO');
SELECT * FROM teste;
.output dio-demo.txt
SELECT * FROM teste;
.output stdout
.quit
```

O tracer escreve eventos de abertura/fecho de ficheiros e leitura/escrita do terminal e do ficheiro de saída. Também aparecem pread64 e pwrite64 no acesso à base de dados e fdatasync nas sincronizações. Continua a capturar só as dez syscalls implementadas, não todo o I/O do SQLite.
O teste deixa /tmp/dio-demo.db, /tmp/dio-demo.txt e /tmp/dio-trace.json. Depois de o tracer terminar, pode abrir-se o JSON num editor para ver os eventos.

Foram feitos testes individuais, com threads, pressão no ring, SQLite e o test_syscalls do DIO. O JSON foi comparado com capturas do legacy. Isto ainda não testa a integração completa com Elasticsearch/Kibana.

Continua a ser para processos nativos x86_64. Em readv/writev são lidos no máximo 1024 descritores. Se não forem legíveis ou o total não couber num inteiro de 64 bits com sinal, bytes_requested é omitido. Outra thread pode mudar os descritores enquanto os estamos a observar.

Podem faltar caminhos: o armazenamento é limitado e reutiliza slots. Os contadores distinguem isso de perder um evento da syscall, mas ainda não cobrem todas as falhas ao obter a identidade do ficheiro. A posição observada também pode mudar com outras threads a usar o mesmo ficheiro.

Ao parar o tracer podem ficar chamadas por registar. Se terminar à força ou houver um erro de escrita, o JSON pode ficar incompleto.
