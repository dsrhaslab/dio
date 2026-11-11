# DIO port C + libbpf/CO-RE

Em desenvolvimento. Port do DIO de Go/BCC para userspace em C e BPF com libbpf/CO-RE.

- Captura open, creat, openat, read, write, close, pread64, pwrite64, readv, writev, fsync, fdatasync e lseek.
- Guarda argumentos, retorno, duração e informação do processo.
- Filtra por PID ou TID, com configuração por CLI e YAML.
- Usa um ring buffer e escreve JSON com yyjson.
- Associa os caminhos aos eventos através da file_tag.
- Tem contadores de eventos e perdas.
- Reutiliza o exporter, a correlação de caminhos e os dashboards do DIO original para Elasticsearch/Kibana.

O userspace do tracer para já single-threaded. A importação para Elasticsearch é manual, depois de terminar a captura.
