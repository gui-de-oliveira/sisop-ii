
# Relatório do Trabalho I de Sistemas Operationais II

por  Gui de Oliveira (00278301)

## Sobre o Ambiente de desenvolvimento

O trabalho foi desenvolvido utilizando o sistema operacional Windows 10 Home e acessando um Linux Ubuntu atráves da WSL (Windows Subsystem for Linux) versão 5.10.16. O processador da máquina utilizada é um AMD Ryzen 7 5700U 1.80 GHz e possui 20 GB de memória ram. O compilador utilizado foi o GCC na versão 11.3.0.

## Como foi implementada a concorrência no servidor para atender múltiplos clientes?

A concorrência no servidor foi implementada utilizando a biblioteca Future do C++. Essa biblioteca permite que uma função rode de forma assíncrona, podendo rodar em outra thread. Para atender múltiplos clientes ao mesmo tempo, uma nova função assíncrona é gerada a cada conexão do cliente com o servidor, possibilitando que se comuniquem ao mesmo tempo.

## Em quais áreas do código foi necessário garantir sincronização no acesso a dados?

Foi necessário garantir a sincronização em todas alterações e leituras a arquivos, tomando cuidado especial com situações como tentativa de escrita em um arquivo que ainda está sendo lido, tentativa de remoção de um arquivo sendo lido, tentativa de escrita em um arquivo que já está sendo escrito e outros tipos de colisão.

## Descreva as principais estruturas e funções que você implementou

### Fila Thread Safe

```c++
template <typename T>
class ThreadSafeQueue
{
private:
    std::queue<T> _queue;
    std::mutex _mutex;
    std::condition_variable _condition;

public:
    void queue(T value)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _queue.push(value);
        _condition.notify_one();
    }

    std::optional<T> pop()
    {
        std::unique_lock<std::mutex> lock(_mutex);

        if (_queue.empty())
        {
            return std::nullopt;
        }

        T value = _queue.front();
        _queue.pop();

        return value;
    }
};
```
O coração dessa implementação é a fila thread safe. Ela é o ÚNICO mecanismo de sincronização utilizado, todo o resto da aplicação gira em torno dela e isso não foi por acaso. Meu objetivo com essa implementação era modelar esse problema como um sistema de troca de mensagens. Esse tipo de arquitetura permite uma modelagem do problema de uma maneira mais simples. Essa arquitetura é fortemente inspirada no capítulo 5 do livro Pragmatic Programmer e em como implementamos microsserviços na empresa onde trabalho. O foco dessa arquitetura é modelar um processo assíncronos através de múltiplos agentes assíncronos rodando em paralelo.

### Processadores de fila

A partir da fila thread safe são implementados os processadores de fila. Eles são os agentes síncronos que permitirão o acesso único às seções críticas. Esses processadores consomem os itens adicionados a fila, dado que garantimos que a fila é thread safe, podemos garantir que esse processador síncrono também vai ser. Mesmo ele sendo síncrono, isso não impede que faça o disparo de funções assíncronas. Isso permite que o processador paralelize processos demorados sem colocar em seções críticas em risco. Entraremos em mais detalhes sobre isso em seguida.

### Gerenciador de estado do arquivo

Para garantir que o arquivo sempre estará em um estado válido, foi implementado o gerenciador de estado do arquivo. Ele é um processador de fila onde cada elemento da fila é uma ação aplicada ao arquivo. O estado do arquivo é atualizado a partir de seu estado atual e da ação aplicada sobre ele e então o gerenciador dispara funções assíncronas que aplicarão os efeitos da ação sobre o arquivo e, quando completas, adicionam na fila a mensagem de que finalizaram a ação. 

Por exemplo: Todo arquivo começa no estado "não existente". Se a ação "criar arquivo" for adicionada  na fila, o gerenciador irá alterar o estado do arquivo para "criando arquivo" e dispará a função assíncrona que realizará a criação do arquivo. Essa função, ao completar, adicionará na fila o evento "criação completa" que quando processada, mudará o estado do arquivo de "criando arquivo" para "arquivo pronto".

Ao garantirmos que a lista de ações vai ser processada de forma síncrona e que dado  um estado do arquivo, a ação aplicada a levará para outro estado válido, podemos garantir que o estado do arquivo sempre estará em um estado válido.

### Gerenciador de arquivos remoto

Simplesmente o gerenciador de estado dos arquivos do servidor. Os clientes conectados adicionarão ações a serem aplicadas em cada arquivo e o gerenciador realizará a atualização dos estados de arquivo resultantes e o disparo de funções assíncronas.

### Gerenciador de arquivos local

Similar ao gerenciador de arquivos remoto com a diferença de possuir apenas dois agentes que adicionam itens em sua fila: o gerenciador de alterações na pasta e o gerenciador de mudanças no servidor. O gerenciador de alterações na pasta notificará o gerenciador de arquivos com todas alterações que ocorrerem na pasta. O gerenciador de mudanças no servidor é disparado toda vez que uma nova mudança é recebida em um arquivo do usuário no servidor. Ele é implementado com um socket próprio para ser notificado sempre que uma alteração ocorrer.

## Explique o uso das diferentes primitivas de comunicação

Toda comunicação entre cliente-servidor foi implementada a partir de sockets. Uma camada de abstração foi implementada para facilitar a comunicação.

``` c++
enum MessageType
{
    Empty,
    InvalidMessage,
    Login,
    UploadCommand,
    RemoteFileUpdate,
    RemoteFileDelete,
    DownloadCommand,
    DeleteCommand,
    EndCommand,
    ListServerCommand,
    SubscribeUpdates,
    FileInfo,
    DataMessage,
    Response,
    Start,
};

enum ResponseType
{
    Invalid,
    Ok,
    FileNotFound,
};

class Message
{
public:
    MessageType type;
    time_t timestamp;

    // utilizada na mensagem de login
    std::string username;

    // utilizada em mensagens do tipo response
    ResponseType responseType;
    
    // utilizada em mensagens de dados
    std::string data;

    // utilizada em mensagens de descrição de arquivos
    std::string filename;
    time_t mtime;
    time_t atime;
    time_t ctime;

    static Message Parse(char *_buffer);
};
```
Essa abstração facilitou a implementação da comunicação entre os processos. Com os possíveis tipos de mensagem esperados explicitados, ficou muito mais fácil de modelar requisições e as diferentes respostas. Inicialmente essa abstração servia apenas como um encapsulamento das mensagens e então evoluiu para uma abstração da comunicação em si, fazendo as chamadas de socket.

```c++
void downloadFile(Session session, string path)
{
    std::fstream file;
    file.open(path, ios::out);

    // Espera a mensagem de início
    Message message = Message::Listen(session.socket);

    if (message.type != MessageType::Start)
    {
        // Se a mensagem recebida não é a esperada, encerra a conexão
        message.panic();
        return;
    }

    // Confirma que recebeu a mensagem de início e espera as mensagens de dados
    message = message.Reply(Message::Response(ResponseType::Ok));

    while (true)
    {
        // mensagem de dado
        if (message.type == MessageType::DataMessage)
        {
            // salva os dados da mensagem no arquivo
            file << message.data;

            // confirma que recebeu e espera a próxima mensagem
            message = message.Reply(Message::Response(ResponseType::Ok));
            continue;
        }

        // mensagem de encerramento
        if (message.type == MessageType::EndCommand)
        {
            // confirma que recebeu os dados corretamente
            message.Reply(Message::Response(ResponseType::Ok), false);
            break;
        }

        message.panic();
    }

    file.close();
}
```
## Também inclua no relatório uma descrição dos problemas que você encontrou durante a implementação e como estes foram resolvidos (ou não)

### Problemas de compatibilidade com o Windows

Enfrentei algumas dificuldades tentando desenvolver a aplicação em Windows para ambientes Linux. Felizmente, a WSL (Windows Subsystem for Linux) ajudou bastante e me permitiu desenvolver tudo sem uma máquina virtual ou dual boot. Tive algumas dores de cabeça ao longo do caminho, como por exemplo pelo fato do inotify não funcionar em pastas Windows, mas no final foi mais tranquilo do que achei que seria.

### Entendendo o async no C++

Tenho pouca experiência em C++ e menos ainda em programar paralelo nele. O JavaScript tem um conceito similar ao future do C++, mas ainda tem diferenças consideráveis. Por exemplo, um objeto future pode receber um await apenas uma vez, o próximo objeto que tentar utilizar await fará com que uma exceção seja lançada. Isso fez com que eu tivesse que mudar bastante a arquitetura que tinha em mente. Inicialmente, gostaria que todas as ações de leitura de um arquivo esperassem a mesma ação de escrita, por exemplo. Perdi bastante tempo batendo a cabeça tentando entender o porquê de mesmo usando a biblioteca de async, a thread ainda estava esperando aquela função terminar para continuar. A resposta: O desconstrutor do future faz com que a função seja esperada, então quando eu colocava um laço while com asyncs dentro, a iteração do while ainda esperava a função terminar.

### Debugging de processos distribuídos

Precisei dedicar bastante esforço nos logs para conseguir visualizar através dos logs o que estava ocorrendo para pegar estados inconsistentes. E ainda assim é difícil entender o fluxo de mensagens e estados mesmo com uma boa visualização das mensagens.