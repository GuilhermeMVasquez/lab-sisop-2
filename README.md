# Trabalho 2 - Laboratório de Sistemas Operacionais

### Instalação

O processo de instalação ocorre através da execução do script `t2_install`, responsável 
colocar os arquivos nos diretórios corretos, compilar e instalar o módulo e o programa
que possibilita testar o funcionamento do driver na distribuição Linux. Além disso,
ele adiciona ao seu script `pre-build.sh` a linha necessária para compilar e instalar o
driver durante a compilação do buildroot.

Após rodar o script e antes de emular o qemu, certifique-se que as variáveis de ambiente:
`LINUX_OVERRIDE_SRCDIR` aponta para o caminho da pasta `linux-4.13.9/`. Além disso, garanta
que o caminho para o bin foi adicionado ao `PATH` para possibilitar o uso do comando
`qemu-system-i386`.

O driver possibilita o envio e leitura de mensagens entre n aplicações. As operações definidas
são:

    1. /reg <name>

O comando 1 registra o processo atual no driver com o nome passado em name. O nome deve
ser único dentre os processos registrados.

    2. /<name> <msg>

O comando 2 envia uma mensagem msg para o processo registrado com name. O processo
identificado por name deve estar registrado no driver para ser possível o envio da 
mensagem. Além disso o processo qual está enviando a mensagem também deve estar 
registrado no driver.

    3. /unreg <name>

O comando 3 retira o processo identificado por name do driver. Só é possível fazer
a retirada do processo identificado por name se esse nome está atrelado ao processo
que fez a requisição.

A última operação possível é a leitura das mensagens endereçadas ao próprio processo.
A cada leitura apenas uma mensagem será retornada ao processo. Sendo assim, para ler
várias mensagens será necessário executar um read no driver várias vezes.

## Créditos

### Trabalho realizado por:

    1. Guilherme Malgarizi Vásquez
    2. Henrique Zapella Rocha
    3. Pedro Mezacasa Muller

### Professor: 
    Sérgio Johann Filho

### Instituição:
    PUCRS - Escola Politécnica

### Data:
    7 de maio de 2025