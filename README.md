# TCCelho
Implementação de um [servidor Echo](https://medium.com/@himalee.tailor/what-is-an-echoserver-b2bfd3b8deeb) utilizando estratégias de Kernel-Bypass

## Estrutura de diretórios

```
├── client
│   ├── Makefile
│   └── client.c
├── common
│   └── common.h
├── mtcp_client
│   ├── Makefile
│   ├── client.c
│   └── client.conf
├── mtcp_server
│   ├── Makefile
│   ├── server.c
│   └── server.conf
├── results
│   ├── mtcp_results_latency
│   │   ├── latency_S<n_servers>_C<n_clients>_P<payload_size>.txt
│   ├── mtcp_results_requests
│   │   ├── requests_S<n_servers>_C<n_clients>_P<payload_size>.txt
│   ├── results_latency
│   │   ├── latency_S<n_servers>_C<n_clients>_P<payload_size>.txt
│   └── results_requests
│       └── requests_S<n_servers>_C<n_clients>_P<payload_size>.txt
├── server
│   ├── Makefile
│   └── server.c
└── sockets
    ├── sockets.c
    └── sockets.h
```

`client` e `server`: Possuem o código fonte para a versão do protótipo que utiliza as funcionalidades disponibilizadas pelo Kernel

`mtcp_client` e `mtcp_server`: Possuem a versão do protótipo que utiliza a biblioteca mTCP

`common` e `sockets`: Possuem código que pode ser reutilizado pelos dois protótipos

`results`: Guarda os arquivos de resultados após os testes feitos. O formato dos arquivos é o que segue; `(latency│requests)_S<n_servers>_C<n_clients>_P<payload_size>.txt`. Onde `n_servers` é o número de threads utilizadas por servidor, `n_clients` é o número de threads utilizadas pelo cliente e `payload_size` é o tamanho do payload utilizado para os testes.

`results/mtcp_results*`: Resultados referentes a execução do protótipo mTCP

`results/results*`: Resultados referentes a execução do protótipo que utiliza estratégias convencionais

## Executando
Por enquanto só é possível executar a versão que não utiliza mTCP.

Entre no diretório `server` e execute `make run-server`
Entre no diretório `client` e execute `make run-client`

Para para o processo aperte CTRL+C no terminal, ou então espere o Runtime do servidor terminar. Atualmente o Runtime está setado para 60 segundos.

Serão criados dois arquivos: `latency.txt`, referente a medida de latência pelo cliente, e `requests_per_second.txt` referente ao número de requisições servidas por segundo pelo servidor.

## Anotações
Para os primeiros testes, apenas uma thread foi utilizada para o servidor. A variação ocorreu apenas nas threads clientes
