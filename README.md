# DIO port C + libbpf/CO-RE

**Em desenvolvimento.** Este é um port experimental do DIO, a partir da versão Go/BCC. O objetivo é passar o userspace para C e o BPF para libbpf/CO-RE.
Ainda não substitui o DIO original nem está integrado com a sua pipeline.

## O que funciona neste momento

- Captura openat, read, write e close, com entrada/saída e duração.
- Passa os eventos do kernel para userspace através de um ring buffer.
- Tem um consumidor em C.
- Filtra por PID ou TID e aceita configuração por CLI e YAML.
- Escreve texto no terminal ou num ficheiro; os caminhos aparecem em linhas PATH separadas.

Ainda não há JSON compatível com o DIO, estatísticas ou integração com Elasticsearch/Kibana. A saída é provisória: nomes com espaços, aspas ou quebras de linha não têm o formato adequado.
Também podem faltar eventos ou caminhos, sem contadores que permitam medir essas perdas.

A implementação está em dio-tracer/. A pipeline, os scripts e os workloads antigos não fazem parte ainda até conseguir dar sync ao output e integrar na pipeline do DIO.

## Dependências e build

Usado numa VM Ubuntu x86_64 com kernel 7.0.0-31-generic. É necessário BTF do kernel em /sys/kernel/btf/vmlinux mas costuma já vir compilado na distro.

```bash
sudo apt install build-essential clang bpftool libbpf-dev libyaml-dev
make -C dio-tracer
```

O Makefile gera vmlinux.h a partir do kernel local, compila o BPF, gera o skeleton e cria dio-tracer/build/dio.
```bash
make -C dio-tracer clean
```
apaga-os.

## Executar

A partir da raiz do repo, substituindo "x" pelo PID real:

```bash
sudo ./dio-tracer/build/dio --config dio-tracer/config.yaml --pid x
```

O YAML de exemplo deixa os alvos vazios. É obrigatório indicar pelo menos um PID ou TID. --tid x seleciona uma thread; se houver PIDs e TIDs, os TIDs têm prioridade.

A prioridade é defaults < YAML < CLI. O YAML só é lido com --config. --events openat,close seleciona eventos, --duration 20 limita a duração e --output /tmp/dio-trace.txt escreve texto num ficheiro, substituindo o
conteúdo anterior. Sem essa opção, a saída vai para stdout. As restantes opções estão em ./dio-tracer/build/dio --help. Ctrl+C termina o tracer.

## Teste manual com SQLite

Este percurso foi verificado na VM indicada com SQLite 3. É um teste pequeno para observar eventos de um programa real.

Em um terminal abrir o SQLite sem base de dados em disco:

```bash
cd /tmp
sqlite3
```

No terminal 2, procurar o PID com pgrep -a -x sqlite3 e iniciar o tracer com
esse PID, como no exemplo anterior. Esperar pela mensagem DIO ready.

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

O tracer deve mostrar eventos de abertura/fecho de ficheiros e leitura/escrita do terminal e do ficheiro de saída. Isto não captura todo o I/O do SQLite:
Por exemplo as operações pread64 e pwrite64, usadas no acesso à base de dados, ainda não estão implementadas.
O teste deixa /tmp/dio-demo.db e /tmp/dio-demo.txt