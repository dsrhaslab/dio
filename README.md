# DIO port C + libbpf/CO-RE + JSON Compatível

**Em desenvolvimento.** Este é um port experimental do DIO, a partir da versão Go/BCC. O objetivo é passar o userspace para C e o BPF para libbpf/CO-RE.
Ainda não substitui o DIO original nem está integrado com a sua pipeline.

## O que funciona neste momento

- Captura openat, read, write e close, com entrada/saída e duração.
- Passa os eventos do kernel para userspace através de um ring buffer.
- Tem um consumidor em C, na mesma thread do main.
- Filtra por PID ou TID e aceita configuração por CLI e YAML.
- Escreve JSON no terminal ou num ficheiro, usando yyjson.
- Os caminhos aparecem em eventos EventPath separados, ligados aos eventos das syscalls pelo file_tag.

A principal mudança face à versão anterior é o output. Antes escrevia texto provisório; agora escreve um array JSON com os campos do DIO legacy. As strings são escapadas pela biblioteca, os números ficam como números e o array é fechado quando o tracer termina normalmente.
Também foi acrescentado o nome da sessão, a conversão dos timestamps para tempo de calendário e a representação dos erros com return_value igual a -1 e error_message. O timestamp de saída passou a ser recolhido logo ao entrar no handler de saída, antes do trabalho de procurar o caminho.

Continuam a ser só as mesmas quatro syscalls. Ainda não há pread64/pwrite64, estatísticas ou integração com Elasticsearch/Kibana. Não é capturado o conteúdo dos buffers de read/write.
O formato foi comparado com o legacy, mas falta ligar e testar a pipeline completa.

A implementação está em dio-tracer/. A pipeline, os scripts e os workloads antigos ainda não fazem parte desta cópia.

## Dependências e build

Usado numa VM Ubuntu x86_64 com kernel 7.0.0-31-generic. É necessário BTF do kernel em /sys/kernel/btf/vmlinux mas costuma já vir compilado na distro.

```bash
sudo apt install build-essential clang bpftool libbpf-dev libyaml-dev libyyjson-dev
make -C dio-tracer
```

A dependência nova é libyyjson-dev, para escrever o JSON. O Makefile gera vmlinux.h a partir do kernel local, compila o BPF, gera o skeleton e cria dio-tracer/build/dio.

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

A prioridade é defaults < YAML < CLI. O YAML só é lido com --config. --events openat,close seleciona eventos, --duration 20 limita a duração e --output /tmp/dio-trace.json escreve JSON num ficheiro, substituindo o conteúdo anterior.
Sem essa opção, a saída vai para stdout. --session-name teste dá um nome à sessão; se não for indicado, é gerado um nome. Também pode ser definido no YAML em tracer.session_name.
As restantes opções estão em ./dio-tracer/build/dio --help. Ctrl+C termina o tracer e fecha o array JSON. Também termina quando os alvos desaparecem.

As mensagens como DIO ready vão para stderr, para não ficarem misturadas com o JSON. O ficheiro só tem o array completo depois de o tracer terminar.

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

O tracer escreve eventos de abertura/fecho de ficheiros e leitura/escrita do terminal e do ficheiro de saída. Isto não captura todo o I/O do SQLite: por exemplo pread64 e pwrite64, usadas no acesso à base de dados, ainda não estão implementadas.
O teste deixa /tmp/dio-demo.db, /tmp/dio-demo.txt e /tmp/dio-trace.json. Depois de o tracer terminar, pode abrir-se o JSON num editor para ver os eventos.

Tem funcionado nos testes feitos até agora, mas ainda falta testar melhor com mais eventos e situações diferentes. Podem faltar eventos ou caminhos e ainda não há contadores para perceber essas perdas. A parte de obter os caminhos e identificar os ficheiros também tem limites e precisa de mais testes.

Ao parar o tracer podem ficar chamadas por registar. Se terminar à força ou houver um erro de escrita, o JSON pode ficar incompleto. Também pode continuar à espera mesmo depois de o processo observado terminar. Nesse caso usa-se Ctrl+C, ou define-se --duration ao iniciar o tracer.
