# Fonctions réseau / système (C++98)

Mémo des fonctions autorisées, avec exemples compatibles **C++98**.

- Compilation : `c++ -Wall -Wextra -Werror -std=c++98 fichier.cpp`
- Pas de `nullptr` (utiliser `NULL`), pas de `auto`, pas de `std::to_string`, pas de `override`, pas de lambdas.
- Ces fonctions viennent de l'API C : on les appelle en incluant les headers C (`<sys/socket.h>`, `<unistd.h>`, ...) ou leurs versions C++ (`<cstring>`, `<cerrno>`, ...).
- Presque toutes renvoient `-1` en cas d'erreur et remplissent `errno`. Utilise `std::strerror(errno)` (`<cstring>`) pour obtenir un message lisible.

## Sommaire

1. [Sockets : création et cycle de vie](#1-sockets--création-et-cycle-de-vie) : `socket`, `close`, `setsockopt`, `getsockname`
2. [Serveur : bind, listen, accept](#2-serveur--bind-listen-accept) : `bind`, `listen`, `accept`
3. [Client : connect](#3-client--connect) : `connect`
4. [Résolution de noms et protocoles](#4-résolution-de-noms-et-protocoles) : `getprotobyname`, `gethostbyname`, `getaddrinfo`, `freeaddrinfo`
5. [Ordre des octets](#5-ordre-des-octets) : `htons`, `htonl`, `ntohs`, `ntohl`
6. [Conversion d'adresses IP](#6-conversion-dadresses-ip) : `inet_addr`, `inet_ntoa`, `inet_ntop`
7. [Envoi / réception](#7-envoi--réception) : `send`, `recv`
8. [Signaux](#8-signaux) : `signal`, `sigaction`, `sigemptyset`, `sigfillset`, `sigaddset`, `sigdelset`, `sigismember`
9. [Fichiers et descripteurs](#9-fichiers-et-descripteurs) : `lseek`, `fstat`, `fcntl`
10. [Multiplexage](#10-multiplexage) : `poll`

---

## 1. Sockets : création et cycle de vie

### `socket`

**Résumé** : crée un point de communication (une "prise") et renvoie un descripteur de fichier pour l'utiliser.

```cpp
#include <sys/socket.h>
int socket(int domain, int type, int protocol);
```

**Paramètres**

| Paramètre | Explication |
|---|---|
| `domain` | Famille d'adresses. `AF_INET` = IPv4, `AF_INET6` = IPv6, `AF_UNIX` = communication locale via un fichier. |
| `type` | Type de communication. `SOCK_STREAM` = flux fiable orienté connexion (TCP). `SOCK_DGRAM` = datagrammes (UDP). |
| `protocol` | Protocole précis. `0` = choix automatique selon `type` (TCP pour `SOCK_STREAM`, UDP pour `SOCK_DGRAM`). Sinon, valeur issue de `getprotobyname`. |

**Retour** : descripteur (>= 0) ou `-1` en cas d'erreur.

**Flags / valeurs utiles**
- `domain` : `AF_INET`, `AF_INET6`, `AF_UNIX`
- `type` : `SOCK_STREAM`, `SOCK_DGRAM`, `SOCK_RAW`
- (Linux uniquement) on peut combiner `type` avec `SOCK_NONBLOCK` et `SOCK_CLOEXEC` : `SOCK_STREAM | SOCK_NONBLOCK`. Non portable (absent de macOS).

**Exemple concret** : socket UDP pour envoyer une mesure de température à un collecteur.

```cpp
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <iostream>

int main()
{
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == -1)
    {
        std::cerr << "socket: " << std::strerror(errno) << std::endl;
        return 1;
    }

    struct sockaddr_in dest;
    std::memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(9000);
    dest.sin_addr.s_addr = inet_addr("127.0.0.1");

    const char *msg = "temp=21.5";
    sendto(fd, msg, std::strlen(msg), 0,
           (struct sockaddr *)&dest, sizeof(dest));

    close(fd);
    return 0;
}
```

---

### `close`

**Résumé** : ferme un descripteur de fichier (socket, fichier, pipe...). Libère la ressource côté noyau.

```cpp
#include <unistd.h>
int close(int fd);
```

**Paramètres**

| Paramètre | Explication |
|---|---|
| `fd` | Descripteur à fermer. |

**Retour** : `0` si OK, `-1` en cas d'erreur.

**Notes**
- Fermer un descripteur déjà fermé est une erreur (`EBADF`) : après `close`, remets ta variable à `-1`.
- Pour une socket TCP, `close` démarre la fermeture de la connexion (le pair verra `recv` renvoyer 0).
- Chaque `socket()` / `accept()` / `open()` réussi doit avoir son `close()`, sinon fuite de descripteurs.

**Exemple concret** : ouvrir un fichier, lire, puis le fermer proprement.

```cpp
#include <fcntl.h>
#include <unistd.h>
#include <iostream>

int main()
{
    int fd = open("/etc/hostname", O_RDONLY);
    if (fd == -1)
        return 1;

    char buf[64];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    if (n > 0)
    {
        buf[n] = '\0';
        std::cout << buf;
    }

    close(fd);
    fd = -1;
    return 0;
}
```

---

### `setsockopt`

**Résumé** : modifie une option d'une socket (comportement du noyau pour cette socket).

```cpp
#include <sys/socket.h>
int setsockopt(int sockfd, int level, int optname,
               const void *optval, socklen_t optlen);
```

**Paramètres**

| Paramètre | Explication |
|---|---|
| `sockfd` | La socket à configurer. |
| `level` | Niveau de l'option. `SOL_SOCKET` pour les options génériques de socket, `IPPROTO_TCP` pour les options TCP, `IPPROTO_IP` pour IP. |
| `optname` | Nom de l'option (voir flags ci-dessous). |
| `optval` | Pointeur vers la valeur à appliquer (souvent un `int`). |
| `optlen` | Taille de la valeur pointée (`sizeof(int)` en général). |

**Retour** : `0` si OK, `-1` sinon.

**Options utiles (niveau `SOL_SOCKET`)**
- `SO_REUSEADDR` : autorise à réutiliser une adresse/port en `TIME_WAIT` (évite "Address already in use" au redémarrage).
- `SO_REUSEPORT` : plusieurs sockets peuvent écouter sur le même port (Linux >= 3.9, BSD).
- `SO_KEEPALIVE` : envoie des sondes périodiques pour détecter une connexion morte.
- `SO_RCVBUF` / `SO_SNDBUF` : taille des tampons de réception / d'envoi.
- `SO_RCVTIMEO` / `SO_SNDTIMEO` : délai max (struct `timeval`) pour `recv` / `send` bloquants.
- `SO_LINGER` : comportement de `close` s'il reste des données à envoyer (struct `linger`).
- `SO_BROADCAST` : autorise l'envoi en broadcast (UDP).

**Options utiles (niveau `IPPROTO_TCP`)**
- `TCP_NODELAY` : désactive l'algorithme de Nagle (envoi immédiat des petits paquets).

**Exemple concret** : serveur de "citations du jour" qui peut redémarrer sans attendre, avec un timeout de lecture.

```cpp
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <cstring>
#include <cerrno>
#include <iostream>

int main()
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1)
        return 1;

    int yes = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1)
        std::cerr << "SO_REUSEADDR: " << std::strerror(errno) << std::endl;

    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == -1)
        std::cerr << "SO_RCVTIMEO: " << std::strerror(errno) << std::endl;

    // ... bind / listen ...
    return 0;
}
```

---

### `getsockname`

**Résumé** : récupère l'adresse locale (IP + port) à laquelle une socket est liée.

```cpp
#include <sys/socket.h>
int getsockname(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
```

**Paramètres**

| Paramètre | Explication |
|---|---|
| `sockfd` | La socket à interroger. |
| `addr` | Structure à remplir avec l'adresse locale (cast depuis `sockaddr_in`). |
| `addrlen` | **Entrée** : taille de la structure fournie. **Sortie** : taille réellement écrite. Doit être initialisé avant l'appel. |

**Retour** : `0` si OK, `-1` sinon.

**Cas d'usage typiques**
- Savoir quel port a été attribué quand on a fait `bind` avec le port `0` (le noyau choisit un port libre).
- Connaître l'IP locale utilisée par une socket connectée.

(Pour l'adresse du pair distant, la fonction sœur est `getpeername`, non listée ici.)

**Exemple concret** : demander un port libre au système et l'afficher.

```cpp
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

int main()
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(0);               // 0 = "choisis pour moi"
    bind(fd, (struct sockaddr *)&addr, sizeof(addr));

    struct sockaddr_in real;
    socklen_t len = sizeof(real);           // à initialiser !
    if (getsockname(fd, (struct sockaddr *)&real, &len) == 0)
        std::cout << "Port attribué : " << ntohs(real.sin_port) << std::endl;

    close(fd);
    return 0;
}
```

---

## 2. Serveur : bind, listen, accept

### `bind`

**Résumé** : associe une socket à une adresse locale (IP + port). Obligatoire côté serveur pour dire "j'écoute ici".

```cpp
#include <sys/socket.h>
int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
```

**Paramètres**

| Paramètre | Explication |
|---|---|
| `sockfd` | La socket créée avec `socket()`. |
| `addr` | Adresse à lier (cast depuis `sockaddr_in` pour IPv4). |
| `addrlen` | Taille de la structure d'adresse (`sizeof(struct sockaddr_in)`). |

**Retour** : `0` si OK, `-1` sinon (erreurs fréquentes : `EADDRINUSE` port déjà pris, `EACCES` port < 1024 sans droits).

**Valeurs utiles pour `sin_addr.s_addr`**
- `htonl(INADDR_ANY)` : toutes les interfaces (`0.0.0.0`).
- `htonl(INADDR_LOOPBACK)` : uniquement `127.0.0.1`.

**Exemple concret** : serveur qui sert l'heure sur le port 7000 (liaison seulement locale).

```cpp
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <iostream>

int main()
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    int yes = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(7000);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    {
        std::cerr << "bind: " << std::strerror(errno) << std::endl;
        close(fd);
        return 1;
    }
    std::cout << "Lié sur 127.0.0.1:7000" << std::endl;
    close(fd);
    return 0;
}
```

---

### `listen`

**Résumé** : met une socket liée en mode "écoute" : elle peut maintenant recevoir des demandes de connexion entrantes.

```cpp
#include <sys/socket.h>
int listen(int sockfd, int backlog);
```

**Paramètres**

| Paramètre | Explication |
|---|---|
| `sockfd` | Socket `SOCK_STREAM` déjà passée par `bind`. |
| `backlog` | Nombre max de connexions en attente d'être `accept`ées. Au-delà, les nouveaux clients sont retardés ou refusés. |

**Valeurs utiles pour `backlog`**
- `SOMAXCONN` : maximum autorisé par le système.
- Une petite valeur (`5`, `10`) suffit pour des tests.

**Retour** : `0` si OK, `-1` sinon.

**Exemple concret** : serveur qui écoute avec une file d'attente de 10.

```cpp
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

int main()
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    int yes = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(7000);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
        return 1;
    if (listen(fd, 10) == -1)
        return 1;

    std::cout << "En écoute..." << std::endl;
    close(fd);
    return 0;
}
```

---

### `accept`

**Résumé** : extrait la première connexion en attente de la file d'écoute et renvoie une **nouvelle socket** dédiée à ce client. La socket d'écoute reste disponible pour les suivants.

```cpp
#include <sys/socket.h>
int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
```

**Paramètres**

| Paramètre | Explication |
|---|---|
| `sockfd` | Socket d'écoute (après `listen`). |
| `addr` | (peut être `NULL`) structure remplie avec l'adresse du client. |
| `addrlen` | (peut être `NULL` si `addr` l'est) **Entrée** : taille de `addr`. **Sortie** : taille écrite. À initialiser avant l'appel. |

**Retour** : nouveau descripteur (>= 0) pour parler au client, ou `-1`.

**Comportement**
- Sur une socket **bloquante**, `accept` attend qu'un client arrive.
- Sur une socket **non bloquante**, elle renvoie `-1` avec `errno` = `EAGAIN` / `EWOULDBLOCK` s'il n'y a personne.
- (Linux) `accept4` accepte en plus des flags comme `SOCK_NONBLOCK`, mais n'est pas dans ta liste.

**Exemple concret** : serveur qui accepte un client, affiche son IP et lui renvoie l'heure.

```cpp
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctime>
#include <cstring>
#include <iostream>

int main()
{
    int srv = socket(AF_INET, SOCK_STREAM, 0);
    int yes = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(7000);
    bind(srv, (struct sockaddr *)&addr, sizeof(addr));
    listen(srv, 5);

    struct sockaddr_in cli;
    socklen_t len = sizeof(cli);
    int cfd = accept(srv, (struct sockaddr *)&cli, &len);
    if (cfd == -1)
        return 1;

    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &cli.sin_addr, ip, sizeof(ip));
    std::cout << "Client : " << ip << ":" << ntohs(cli.sin_port) << std::endl;

    time_t now = time(NULL);
    const char *txt = ctime(&now);
    send(cfd, txt, std::strlen(txt), 0);

    close(cfd);
    close(srv);
    return 0;
}
```

---

## 3. Client : connect

### `connect`

**Résumé** : établit une connexion vers une adresse distante. Pour TCP, déclenche la poignée de main (3-way handshake).

```cpp
#include <sys/socket.h>
int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
```

**Paramètres**

| Paramètre | Explication |
|---|---|
| `sockfd` | Socket créée avec `socket()`. |
| `addr` | Adresse (IP + port) du serveur à joindre. |
| `addrlen` | Taille de la structure d'adresse. |

**Retour** : `0` si connecté, `-1` sinon.

**Erreurs fréquentes** : `ECONNREFUSED` (personne n'écoute), `ETIMEDOUT` (pas de réponse), `ENETUNREACH` (réseau injoignable).

**Comportement non bloquant** : sur une socket non bloquante, `connect` renvoie `-1` avec `errno == EINPROGRESS` ; on attend ensuite que la socket soit "prête en écriture" (`POLLOUT`) puis on lit `SO_ERROR` via `getsockopt` pour savoir si ça a réussi.

**Exemple concret** : mini-client qui récupère l'heure auprès du serveur du chapitre précédent.

```cpp
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <iostream>

int main()
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1)
        return 1;

    struct sockaddr_in srv;
    std::memset(&srv, 0, sizeof(srv));
    srv.sin_family = AF_INET;
    srv.sin_port = htons(7000);
    inet_pton(AF_INET, "127.0.0.1", &srv.sin_addr);

    if (connect(fd, (struct sockaddr *)&srv, sizeof(srv)) == -1)
    {
        std::cerr << "connect: " << std::strerror(errno) << std::endl;
        close(fd);
        return 1;
    }

    char buf[128];
    ssize_t n = recv(fd, buf, sizeof(buf) - 1, 0);
    if (n > 0)
    {
        buf[n] = '\0';
        std::cout << "Heure du serveur : " << buf;
    }
    close(fd);
    return 0;
}
```

---

## 4. Résolution de noms et protocoles

### `getprotobyname`

**Résumé** : cherche un protocole réseau par son nom (dans `/etc/protocols`) et renvoie ses infos, notamment son numéro.

```cpp
#include <netdb.h>
struct protoent *getprotobyname(const char *name);
```

**Paramètres**

| Paramètre | Explication |
|---|---|
| `name` | Nom du protocole : `"tcp"`, `"udp"`, `"icmp"`, `"ip"`... |

**Retour** : pointeur vers une `struct protoent` (statique, ne pas `free`) ou `NULL` si inconnu.

```cpp
struct protoent {
    char  *p_name;     // nom officiel
    char **p_aliases;  // alias
    int    p_proto;    // numéro de protocole <-- le champ qui sert
};
```

**Exemple concret** : afficher les numéros de quelques protocoles et créer une socket avec.

```cpp
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>

int main()
{
    const char *names[] = { "tcp", "udp", "icmp", NULL };
    for (int i = 0; names[i] != NULL; ++i)
    {
        struct protoent *p = getprotobyname(names[i]);
        if (p)
            std::cout << p->p_name << " = " << p->p_proto << std::endl;
        else
            std::cout << names[i] << " : inconnu" << std::endl;
    }

    struct protoent *tcp = getprotobyname("tcp");
    if (tcp)
    {
        int fd = socket(AF_INET, SOCK_STREAM, tcp->p_proto);
        if (fd != -1)
            close(fd);
    }
    return 0;
}
```

---

### `gethostbyname`

**Résumé** : résout un nom d'hôte (ex. `"localhost"`) en adresse(s) IPv4. **Fonction obsolète** (IPv4 uniquement, non réentrante) : `getaddrinfo` est le remplaçant moderne.

```cpp
#include <netdb.h>
struct hostent *gethostbyname(const char *name);
```

**Paramètres**

| Paramètre | Explication |
|---|---|
| `name` | Nom d'hôte ou adresse IPv4 en notation pointée. |

**Retour** : pointeur vers une `struct hostent` **statique** (écrasée à chaque appel, ne pas `free`) ou `NULL` en cas d'échec (voir `h_errno`).

```cpp
struct hostent {
    char  *h_name;       // nom officiel
    char **h_aliases;    // alias
    int    h_addrtype;   // AF_INET
    int    h_length;     // taille d'une adresse (4)
    char **h_addr_list;  // tableau d'adresses terminé par NULL
};
```

**Exemple concret** : lister les IPv4 d'un nom d'hôte.

```cpp
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <iostream>

int main()
{
    struct hostent *h = gethostbyname("localhost");
    if (h == NULL)
    {
        std::cerr << "résolution échouée" << std::endl;
        return 1;
    }

    std::cout << "Nom officiel : " << h->h_name << std::endl;
    for (int i = 0; h->h_addr_list[i] != NULL; ++i)
    {
        struct in_addr a = *(struct in_addr *)h->h_addr_list[i];
        std::cout << "  IP : " << inet_ntoa(a) << std::endl;
    }
    return 0;
}
```

---

### `getaddrinfo`

**Résumé** : résolution moderne nom d'hôte + service → liste de structures `addrinfo` prêtes à être passées à `socket`, `bind` ou `connect`. Gère IPv4 et IPv6.

```cpp
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
int getaddrinfo(const char *node, const char *service,
                const struct addrinfo *hints, struct addrinfo **res);
```

**Paramètres**

| Paramètre | Explication |
|---|---|
| `node` | Nom d'hôte ou IP en texte (`"example.com"`, `"127.0.0.1"`). `NULL` possible avec `AI_PASSIVE`. |
| `service` | Port en texte (`"8080"`) ou nom de service (`"http"`). Peut être `NULL`. |
| `hints` | Filtre/critères de recherche (peut être `NULL`). Mettre à zéro puis remplir `ai_family`, `ai_socktype`, `ai_flags`. |
| `res` | **Sortie** : pointeur vers le début d'une liste chaînée de résultats (`ai_next`). À libérer avec `freeaddrinfo`. |

**Retour** : `0` si OK, sinon un code d'erreur à décoder avec `gai_strerror(code)` (ce n'est **pas** `errno`).

**Champs de `struct addrinfo`** : `ai_flags`, `ai_family`, `ai_socktype`, `ai_protocol`, `ai_addrlen`, `ai_addr`, `ai_canonname`, `ai_next`.

**Flags utiles (`ai_flags`)**
- `AI_PASSIVE` : adresse destinée à un `bind` (avec `node == NULL` → adresse "wildcard" `0.0.0.0`).
- `AI_CANONNAME` : remplit `ai_canonname` avec le nom canonique.
- `AI_NUMERICHOST` : `node` est forcément une IP numérique (pas de requête DNS).
- `AI_NUMERICSERV` : `service` est forcément un numéro de port.
- `AI_ADDRCONFIG` : ne renvoie que les familles réellement configurées sur la machine.

**Valeurs utiles pour `ai_family` / `ai_socktype`** : `AF_UNSPEC`, `AF_INET`, `AF_INET6` / `SOCK_STREAM`, `SOCK_DGRAM`.

**Exemple concret** : résoudre un nom et lister toutes les IP trouvées.

```cpp
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <cstring>
#include <iostream>

int main()
{
    struct addrinfo hints;
    struct addrinfo *res = NULL;

    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;        // IPv4 ou IPv6
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_CANONNAME;

    int rc = getaddrinfo("localhost", "80", &hints, &res);
    if (rc != 0)
    {
        std::cerr << "getaddrinfo: " << gai_strerror(rc) << std::endl;
        return 1;
    }

    for (struct addrinfo *p = res; p != NULL; p = p->ai_next)
    {
        char ip[INET6_ADDRSTRLEN];
        void *src;
        if (p->ai_family == AF_INET)
            src = &((struct sockaddr_in *)p->ai_addr)->sin_addr;
        else
            src = &((struct sockaddr_in6 *)p->ai_addr)->sin6_addr;
        inet_ntop(p->ai_family, src, ip, sizeof(ip));
        std::cout << (p->ai_family == AF_INET ? "IPv4 " : "IPv6 ")
                  << ip << std::endl;
    }

    freeaddrinfo(res);
    return 0;
}
```

---

### `freeaddrinfo`

**Résumé** : libère toute la liste chaînée allouée par `getaddrinfo`.

```cpp
#include <netdb.h>
void freeaddrinfo(struct addrinfo *res);
```

**Paramètres**

| Paramètre | Explication |
|---|---|
| `res` | Pointeur de tête obtenu via `getaddrinfo` (ne pas passer un maillon intermédiaire). |

**Retour** : rien.

**Notes**
- À appeler **une seule fois** par `getaddrinfo` réussi.
- Ne pas utiliser `res` ni ses champs après l'appel : copie ce dont tu as besoin avant (par exemple `ai_addr` dans une `sockaddr_storage`).
- Si `getaddrinfo` a échoué, il n'y a rien à libérer.

**Exemple concret** : récupérer la première adresse, la copier, libérer.

```cpp
#include <sys/socket.h>
#include <netdb.h>
#include <cstring>
#include <iostream>

int main()
{
    struct addrinfo hints;
    struct addrinfo *res = NULL;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo("localhost", "8080", &hints, &res) != 0)
        return 1;

    struct sockaddr_storage copy;
    socklen_t copy_len = res->ai_addrlen;
    std::memcpy(&copy, res->ai_addr, res->ai_addrlen);

    freeaddrinfo(res);   // res et ses champs ne sont plus valides

    std::cout << "Adresse copiée (" << copy_len << " octets)" << std::endl;
    return 0;
}
```

---

## 5. Ordre des octets

Le réseau utilise le **big-endian** ("network byte order"), alors que beaucoup de CPU (x86) sont **little-endian** ("host byte order"). Ces quatre fonctions convertissent entre les deux. Sur une machine big-endian elles ne font rien, mais il faut toujours les appeler pour rester portable.

```cpp
#include <arpa/inet.h>   // ou <netinet/in.h>
uint16_t htons(uint16_t hostshort);   // host  -> network, 16 bits
uint32_t htonl(uint32_t hostlong);    // host  -> network, 32 bits
uint16_t ntohs(uint16_t netshort);    // network -> host, 16 bits
uint32_t ntohl(uint32_t netlong);     // network -> host, 32 bits
```

Mnémonique : **h** = host, **n** = network, **s** = short (16 bits), **l** = long (32 bits).

| Fonction | Paramètre | Usage typique |
|---|---|---|
| `htons` | valeur 16 bits dans l'ordre de la machine | écrire un **port** dans `sin_port` |
| `htonl` | valeur 32 bits dans l'ordre de la machine | écrire une **IPv4** dans `sin_addr.s_addr` (ex. `htonl(INADDR_ANY)`) |
| `ntohs` | valeur 16 bits au format réseau | lire un port reçu ou stocké dans une `sockaddr_in` |
| `ntohl` | valeur 32 bits au format réseau | lire une IPv4 / un entier reçu du réseau |

**Exemple concret** : écrire un en-tête binaire (version 16 bits, longueur 32 bits) dans un tampon, puis le relire.

```cpp
#include <arpa/inet.h>
#include <stdint.h>
#include <cstring>
#include <iostream>

int main()
{
    unsigned char packet[6];

    uint16_t version = 3;
    uint32_t length = 100000;

    uint16_t nv = htons(version);
    uint32_t nl = htonl(length);
    std::memcpy(packet, &nv, 2);
    std::memcpy(packet + 2, &nl, 4);

    // ... imaginons que 'packet' voyage sur le réseau ...

    uint16_t rv;
    uint32_t rl;
    std::memcpy(&rv, packet, 2);
    std::memcpy(&rl, packet + 2, 4);

    std::cout << "version = " << ntohs(rv) << std::endl;  // 3
    std::cout << "length  = " << ntohl(rl) << std::endl;  // 100000
    return 0;
}
```

---

## 6. Conversion d'adresses IP

### `inet_addr`

**Résumé** : convertit une IPv4 en notation pointée (`"192.168.1.10"`) en entier 32 bits **déjà en ordre réseau**. **Fonction ancienne** : renvoie `INADDR_NONE` (`(in_addr_t)-1`) en cas d'erreur, ce qui est aussi la valeur de `255.255.255.255` (ambiguïté). Préfère `inet_pton`.

```cpp
#include <arpa/inet.h>
in_addr_t inet_addr(const char *cp);
```

**Paramètres**

| Paramètre | Explication |
|---|---|
| `cp` | Chaîne IPv4 en notation pointée. |

**Retour** : adresse en ordre réseau, ou `INADDR_NONE` si invalide.

**Exemple concret**

```cpp
#include <arpa/inet.h>
#include <netinet/in.h>
#include <iostream>

int main()
{
    in_addr_t a = inet_addr("10.0.0.42");
    if (a == INADDR_NONE)
    {
        std::cerr << "adresse invalide" << std::endl;
        return 1;
    }

    struct sockaddr_in sa;
    sa.sin_addr.s_addr = a;     // déjà en ordre réseau : pas de htonl !
    std::cout << "ok : " << std::hex << ntohl(a) << std::endl;  // a00002a
    return 0;
}
```

---

### `inet_ntoa`

**Résumé** : convertit une IPv4 (struct `in_addr`) en chaîne pointée. **Non réentrante** : le résultat est dans un tampon statique écrasé à chaque appel.

```cpp
#include <arpa/inet.h>
char *inet_ntoa(struct in_addr in);
```

**Paramètres**

| Paramètre | Explication |
|---|---|
| `in` | Structure `in_addr` (**passée par valeur**) contenant l'IPv4 en ordre réseau. |

**Retour** : pointeur vers un tampon statique (ne pas `free`, copier si besoin de le garder).

**Piège classique** : deux appels dans la même expression écrasent le même tampon.

```cpp
// FAUX : les deux %s affichent la même chose
printf("%s %s", inet_ntoa(a), inet_ntoa(b));
```

**Exemple concret** : copier le résultat dans une `std::string` pour le conserver.

```cpp
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string>
#include <iostream>

int main()
{
    struct in_addr a;
    struct in_addr b;
    a.s_addr = inet_addr("192.168.0.1");
    b.s_addr = inet_addr("192.168.0.2");

    std::string sa(inet_ntoa(a));   // copie immédiate
    std::string sb(inet_ntoa(b));

    std::cout << sa << " puis " << sb << std::endl;
    return 0;
}
```

---

### `inet_ntop`

**Résumé** : convertit une adresse binaire (IPv4 **ou** IPv6) en chaîne lisible, dans un tampon que **tu** fournis (réentrant). Remplaçant moderne de `inet_ntoa`.

```cpp
#include <arpa/inet.h>
const char *inet_ntop(int af, const void *src, char *dst, socklen_t size);
```

**Paramètres**

| Paramètre | Explication |
|---|---|
| `af` | Famille : `AF_INET` ou `AF_INET6`. |
| `src` | Pointeur vers l'adresse binaire (`struct in_addr *` ou `struct in6_addr *`). |
| `dst` | Tampon de sortie fourni par l'appelant. |
| `size` | Taille du tampon. Constantes : `INET_ADDRSTRLEN` (16) pour IPv4, `INET6_ADDRSTRLEN` (46) pour IPv6. |

**Retour** : `dst` si OK, `NULL` sinon (`errno` = `ENOSPC` si tampon trop petit, `EAFNOSUPPORT` si `af` invalide).

(La fonction inverse, `inet_pton`, convertit texte → binaire ; elle n'est pas dans ta liste mais est utilisée dans les exemples de ce document.)

**Exemple concret**

```cpp
#include <arpa/inet.h>
#include <netinet/in.h>
#include <iostream>

int main()
{
    struct in_addr v4;
    inet_pton(AF_INET, "203.0.113.7", &v4);

    char buf[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &v4, buf, sizeof(buf)) != NULL)
        std::cout << "IPv4 : " << buf << std::endl;

    struct in6_addr v6;
    inet_pton(AF_INET6, "::1", &v6);

    char buf6[INET6_ADDRSTRLEN];
    if (inet_ntop(AF_INET6, &v6, buf6, sizeof(buf6)) != NULL)
        std::cout << "IPv6 : " << buf6 << std::endl;
    return 0;
}
```

---

## 7. Envoi / réception

### `send`

**Résumé** : envoie des octets sur une socket **connectée**. Peut n'envoyer qu'une **partie** des données : il faut vérifier la valeur de retour et boucler.

```cpp
#include <sys/socket.h>
ssize_t send(int sockfd, const void *buf, size_t len, int flags);
```

**Paramètres**

| Paramètre | Explication |
|---|---|
| `sockfd` | Socket connectée. |
| `buf` | Données à envoyer. |
| `len` | Nombre d'octets à envoyer. |
| `flags` | Options (0 = comportement par défaut, ou combinaison avec `\|`). |

**Retour** : nombre d'octets réellement envoyés (peut être < `len`), ou `-1` (`errno` : `EAGAIN`/`EWOULDBLOCK` en non bloquant et tampon plein, `EPIPE` si le pair a fermé, `ECONNRESET`...).

**Flags utiles**
- `MSG_NOSIGNAL` : n'envoie pas `SIGPIPE` si le pair a fermé la connexion, renvoie juste `EPIPE` (Linux ; sur macOS on utilise `SO_NOSIGPIPE` via `setsockopt`).
- `MSG_DONTWAIT` : appel non bloquant pour cet envoi uniquement.
- `MSG_MORE` : "d'autres données vont suivre" (Linux), permet de regrouper des petits envois.
- `MSG_OOB` : données "urgentes" hors bande (rarement utilisé).

**Exemple concret** : fonction qui envoie **tout** un tampon en bouclant sur les envois partiels.

```cpp
#include <sys/socket.h>
#include <cerrno>
#include <string>

// Renvoie true si tout a été envoyé.
bool sendAll(int fd, const std::string &data)
{
    size_t sent = 0;
    while (sent < data.size())
    {
        ssize_t n = send(fd, data.c_str() + sent, data.size() - sent,
                         MSG_NOSIGNAL);
        if (n == -1)
        {
            if (errno == EINTR)
                continue;           // interrompu par un signal : on réessaie
            return false;           // vraie erreur
        }
        sent += static_cast<size_t>(n);
    }
    return true;
}
```

---

### `recv`

**Résumé** : lit des octets depuis une socket. Ne garantit **pas** de renvoyer un "message complet" : TCP est un **flux** d'octets, un `recv` peut renvoyer un fragment, ou plusieurs messages collés.

```cpp
#include <sys/socket.h>
ssize_t recv(int sockfd, void *buf, size_t len, int flags);
```

**Paramètres**

| Paramètre | Explication |
|---|---|
| `sockfd` | Socket à lire. |
| `buf` | Tampon où écrire les données reçues. |
| `len` | Taille max à lire (taille du tampon). |
| `flags` | Options (0 = défaut). |

**Retour**
- `> 0` : nombre d'octets lus.
- `0` : le pair a fermé proprement la connexion (fin de flux).
- `-1` : erreur (`EAGAIN`/`EWOULDBLOCK` : rien à lire en non bloquant ; `EINTR` : interrompu ; `ECONNRESET` : connexion réinitialisée).

**Flags utiles**
- `MSG_PEEK` : lit sans retirer les données de la file (on pourra les relire).
- `MSG_DONTWAIT` : non bloquant pour cet appel uniquement.
- `MSG_WAITALL` : attend d'avoir reçu exactement `len` octets (peut quand même retourner moins sur signal/erreur/fermeture).
- `MSG_TRUNC` : (UDP/Linux) renvoie la vraie taille du datagramme même si tronqué.

**Exemple concret** : lire des lignes terminées par `\n` en accumulant dans un tampon (gère les messages coupés ou collés).

```cpp
#include <sys/socket.h>
#include <cerrno>
#include <string>
#include <iostream>

// Lit jusqu'à une ligne complète. Renvoie false si connexion fermée/erreur.
bool readLine(int fd, std::string &pending, std::string &line)
{
    for (;;)
    {
        std::string::size_type pos = pending.find('\n');
        if (pos != std::string::npos)
        {
            line = pending.substr(0, pos);
            pending.erase(0, pos + 1);
            return true;
        }

        char buf[256];
        ssize_t n = recv(fd, buf, sizeof(buf), 0);
        if (n == 0)
            return false;               // fermeture propre
        if (n == -1)
        {
            if (errno == EINTR)
                continue;
            return false;               // erreur
        }
        pending.append(buf, static_cast<size_t>(n));
    }
}
```

---

## 8. Signaux

Un **signal** est une notification asynchrone envoyée à un processus (Ctrl+C → `SIGINT`, `kill` → `SIGTERM`, écriture sur une connexion fermée → `SIGPIPE`...).

Dans un gestionnaire de signal, on ne peut appeler que des fonctions "async-signal-safe". En pratique, on se contente d'écrire dans une variable de type `volatile sig_atomic_t` et on traite l'événement dans la boucle principale (pas de `std::cout`, `new`, `malloc`, etc. dans le handler).

Signaux courants : `SIGINT` (Ctrl+C), `SIGTERM` (demande d'arrêt), `SIGQUIT` (Ctrl+\\), `SIGPIPE` (écriture dans un pipe/socket fermé), `SIGHUP`, `SIGUSR1`/`SIGUSR2` (usage libre), `SIGCHLD` (fils terminé). `SIGKILL` et `SIGSTOP` ne peuvent être ni interceptés ni ignorés.

### `signal`

**Résumé** : installe un gestionnaire pour un signal. Simple mais au comportement **variable selon les systèmes** (réinstallation automatique ou non du handler, redémarrage des appels système...). `sigaction` est préféré pour du code robuste.

```cpp
#include <csignal>
typedef void (*sighandler_t)(int);
sighandler_t signal(int signum, sighandler_t handler);
```

**Paramètres**

| Paramètre | Explication |
|---|---|
| `signum` | Numéro du signal (`SIGINT`, `SIGPIPE`...). |
| `handler` | Fonction `void f(int)` à appeler, ou `SIG_IGN` (ignorer), ou `SIG_DFL` (comportement par défaut). |

**Retour** : ancien gestionnaire, ou `SIG_ERR` en cas d'erreur.

**Valeurs spéciales utiles** : `SIG_IGN`, `SIG_DFL`.

**Exemple concret** : intercepter Ctrl+C pour terminer une boucle proprement.

```cpp
#include <csignal>
#include <unistd.h>
#include <iostream>

static volatile sig_atomic_t g_stop = 0;

static void onSigint(int)
{
    g_stop = 1;     // seulement poser un drapeau
}

int main()
{
    if (signal(SIGINT, onSigint) == SIG_ERR)
        return 1;

    int i = 0;
    while (!g_stop)
    {
        std::cout << "tick " << i++ << std::endl;
        sleep(1);
    }
    std::cout << "Arrêt propre." << std::endl;
    return 0;
}
```

---

### `sigaction`

**Résumé** : version complète et portable de `signal` : permet de choisir précisément le comportement (masque de signaux pendant le handler, flags, handler étendu).

```cpp
#include <csignal>
int sigaction(int signum, const struct sigaction *act,
              struct sigaction *oldact);
```

**Paramètres**

| Paramètre | Explication |
|---|---|
| `signum` | Signal concerné. |
| `act` | Nouvelle action à installer (ou `NULL` pour seulement lire l'actuelle). |
| `oldact` | (peut être `NULL`) reçoit l'ancienne action. |

**Retour** : `0` si OK, `-1` sinon.

**Structure `struct sigaction`**
- `sa_handler` : fonction `void f(int)` (ou `SIG_IGN` / `SIG_DFL`).
- `sa_sigaction` : handler étendu `void f(int, siginfo_t *, void *)` (si `SA_SIGINFO`).
- `sa_mask` : signaux **supplémentaires** à bloquer pendant l'exécution du handler.
- `sa_flags` : options (ci-dessous).

**Flags utiles (`sa_flags`)**
- `SA_RESTART` : relance automatiquement certains appels système interrompus par le signal (au lieu de `EINTR`).
- `SA_SIGINFO` : utilise le handler à 3 paramètres (infos sur l'émetteur, etc.).
- `SA_NOCLDSTOP` : `SIGCHLD` uniquement quand un fils termine (pas quand il est stoppé).
- `SA_RESETHAND` : remet le comportement par défaut après la première réception.
- `SA_NODEFER` : ne bloque pas le signal courant pendant son propre handler.

**Exemple concret** : gestion de `SIGINT` et `SIGTERM` avec `SA_RESTART`, et utilisation de `oldact`.

```cpp
#include <csignal>
#include <cstring>
#include <unistd.h>
#include <iostream>

static volatile sig_atomic_t g_signal = 0;

static void handler(int signum)
{
    g_signal = signum;
}

int main()
{
    struct sigaction sa;
    std::memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);       // aucun signal supplémentaire bloqué
    sa.sa_flags = SA_RESTART;

    if (sigaction(SIGINT, &sa, NULL) == -1)
        return 1;
    if (sigaction(SIGTERM, &sa, NULL) == -1)
        return 1;

    std::cout << "PID " << getpid() << " : Ctrl+C ou kill pour arrêter"
              << std::endl;
    while (g_signal == 0)
        pause();                    // attend un signal

    std::cout << "Signal reçu : " << static_cast<int>(g_signal) << std::endl;
    return 0;
}
```

---

### `sigemptyset`

**Résumé** : initialise un ensemble de signaux (`sigset_t`) **vide**. À appeler avant toute utilisation d'un `sigset_t` non initialisé.

```cpp
int sigemptyset(sigset_t *set);
```

| Paramètre | Explication |
|---|---|
| `set` | Ensemble à vider. |

**Retour** : `0` si OK, `-1` sinon.

---

### `sigfillset`

**Résumé** : initialise un ensemble de signaux (`sigset_t`) **plein** (tous les signaux).

```cpp
int sigfillset(sigset_t *set);
```

| Paramètre | Explication |
|---|---|
| `set` | Ensemble à remplir. |

**Retour** : `0` si OK, `-1` sinon.

---

### `sigaddset`

**Résumé** : ajoute un signal à un ensemble.

```cpp
int sigaddset(sigset_t *set, int signum);
```

| Paramètre | Explication |
|---|---|
| `set` | Ensemble déjà initialisé (`sigemptyset`/`sigfillset`). |
| `signum` | Signal à ajouter. |

**Retour** : `0` si OK, `-1` si `signum` invalide.

---

### `sigdelset`

**Résumé** : retire un signal d'un ensemble.

```cpp
int sigdelset(sigset_t *set, int signum);
```

| Paramètre | Explication |
|---|---|
| `set` | Ensemble initialisé. |
| `signum` | Signal à retirer. |

**Retour** : `0` si OK, `-1` si `signum` invalide.

---

### `sigismember`

**Résumé** : teste si un signal fait partie d'un ensemble.

```cpp
int sigismember(const sigset_t *set, int signum);
```

| Paramètre | Explication |
|---|---|
| `set` | Ensemble initialisé. |
| `signum` | Signal à tester. |

**Retour** : `1` s'il est dedans, `0` sinon, `-1` si `signum` invalide.

---

### Exemple concret commun aux fonctions `sig*set`

Construire un ensemble, le modifier, le tester, puis s'en servir pour **bloquer** des signaux temporairement avec `sigprocmask` (non listée, mais nécessaire pour appliquer un `sigset_t`).

```cpp
#include <csignal>
#include <unistd.h>
#include <iostream>

static void show(const char *name, const sigset_t *s, int sig)
{
    std::cout << name << " : "
              << (sigismember(s, sig) ? "dans l'ensemble" : "absent")
              << std::endl;
}

int main()
{
    sigset_t set;

    sigemptyset(&set);              // ensemble vide
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);
    show("SIGINT ", &set, SIGINT);  // dans l'ensemble
    show("SIGUSR1", &set, SIGUSR1); // absent

    sigdelset(&set, SIGTERM);
    show("SIGTERM", &set, SIGTERM); // absent

    sigfillset(&set);               // tous les signaux
    sigdelset(&set, SIGINT);        // ... sauf SIGINT
    show("SIGINT ", &set, SIGINT);  // absent
    show("SIGUSR1", &set, SIGUSR1); // dans l'ensemble

    // Application : section critique protégée de SIGINT pendant 5 s
    sigset_t block, old;
    sigemptyset(&block);
    sigaddset(&block, SIGINT);
    sigprocmask(SIG_BLOCK, &block, &old);
    std::cout << "Ctrl+C bloqué pendant 5 s..." << std::endl;
    sleep(5);
    sigprocmask(SIG_SETMASK, &old, NULL);   // le SIGINT en attente arrive ici
    return 0;
}
```

---

## 9. Fichiers et descripteurs

### `lseek`

**Résumé** : déplace la position de lecture/écriture courante d'un descripteur de fichier. Renvoie la nouvelle position (permet aussi de connaître la taille ou la position actuelle).

```cpp
#include <unistd.h>
off_t lseek(int fd, off_t offset, int whence);
```

**Paramètres**

| Paramètre | Explication |
|---|---|
| `fd` | Descripteur d'un fichier **seekable** (pas un socket, pas un pipe : erreur `ESPIPE`). |
| `offset` | Décalage en octets (peut être négatif). |
| `whence` | Point de référence (voir ci-dessous). |

**Valeurs de `whence`**
- `SEEK_SET` : depuis le début du fichier.
- `SEEK_CUR` : depuis la position courante.
- `SEEK_END` : depuis la fin du fichier.

**Retour** : nouvelle position (depuis le début), ou `-1` en cas d'erreur.

**Astuces**
- `lseek(fd, 0, SEEK_CUR)` : lire la position courante sans la changer.
- `lseek(fd, 0, SEEK_END)` : donne la taille du fichier (et place le curseur à la fin).

**Exemple concret** : lire les 10 derniers octets d'un fichier.

```cpp
#include <fcntl.h>
#include <unistd.h>
#include <iostream>

int main()
{
    int fd = open("/etc/passwd", O_RDONLY);
    if (fd == -1)
        return 1;

    off_t size = lseek(fd, 0, SEEK_END);
    std::cout << "Taille : " << size << " octets" << std::endl;

    off_t start = (size >= 10) ? size - 10 : 0;
    lseek(fd, start, SEEK_SET);

    char buf[11];
    ssize_t n = read(fd, buf, 10);
    if (n > 0)
    {
        buf[n] = '\0';
        std::cout << "Fin du fichier : [" << buf << "]" << std::endl;
    }
    close(fd);
    return 0;
}
```

---

### `fstat`

**Résumé** : remplit une structure `stat` avec les métadonnées d'un fichier **ouvert** (taille, type, droits, dates...).

```cpp
#include <sys/stat.h>
int fstat(int fd, struct stat *buf);
```

**Paramètres**

| Paramètre | Explication |
|---|---|
| `fd` | Descripteur ouvert. |
| `buf` | Structure `stat` à remplir. |

**Retour** : `0` si OK, `-1` sinon.

**Champs principaux de `struct stat`**
- `st_size` : taille en octets.
- `st_mode` : type + permissions.
- `st_mtime` / `st_atime` / `st_ctime` : dates de modification / accès / changement.
- `st_uid` / `st_gid` : propriétaire / groupe.
- `st_ino`, `st_dev`, `st_nlink`, `st_blksize`.

**Macros de test sur `st_mode`**
- `S_ISREG(m)` : fichier régulier.
- `S_ISDIR(m)` : dossier.
- `S_ISSOCK(m)` : socket.
- `S_ISFIFO(m)` : pipe/FIFO.
- `S_ISLNK(m)` : lien symbolique (surtout avec `lstat`).
- `S_ISCHR(m)` / `S_ISBLK(m)` : périphérique caractère / bloc.

**Exemple concret** : afficher le type, la taille et la dernière modification d'un fichier.

```cpp
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctime>
#include <iostream>

int main(int argc, char **argv)
{
    const char *path = (argc > 1) ? argv[1] : "/etc/hostname";
    int fd = open(path, O_RDONLY);
    if (fd == -1)
        return 1;

    struct stat st;
    if (fstat(fd, &st) == -1)
    {
        close(fd);
        return 1;
    }

    std::cout << path << " : ";
    if (S_ISREG(st.st_mode))
        std::cout << "fichier régulier, ";
    else if (S_ISDIR(st.st_mode))
        std::cout << "dossier, ";
    else
        std::cout << "autre type, ";
    std::cout << st.st_size << " octets, modifié le "
              << ctime(&st.st_mtime);      // ctime ajoute déjà un '\n'

    close(fd);
    return 0;
}
```

---

### `fcntl`

**Résumé** : "couteau suisse" pour manipuler un descripteur déjà ouvert : lire/modifier ses flags, le dupliquer, poser des verrous, etc. Le comportement dépend de `cmd`.

```cpp
#include <fcntl.h>
int fcntl(int fd, int cmd, ... /* arg */);
```

**Paramètres**

| Paramètre | Explication |
|---|---|
| `fd` | Descripteur à manipuler. |
| `cmd` | Commande à exécuter (voir ci-dessous). |
| `arg` | Argument variable, dépend de `cmd` (entier, pointeur vers `struct flock`...). Absent pour certaines commandes. |

**Retour** : dépend de la commande (valeur utile, souvent un entier >= 0), ou `-1` en cas d'erreur.

**Commandes (`cmd`) utiles**
- `F_GETFL` : lit les flags d'état du fichier (`O_NONBLOCK`, `O_APPEND`, ...). Retourne les flags.
- `F_SETFL` : modifie les flags d'état (`arg` = nouveaux flags, ex. `O_NONBLOCK`, `O_APPEND`, `O_ASYNC`).
- `F_GETFD` : lit les flags du descripteur (ex. `FD_CLOEXEC`).
- `F_SETFD` : modifie les flags du descripteur.
- `F_DUPFD` : duplique le descripteur vers le plus petit numéro libre >= `arg`.
- `F_DUPFD_CLOEXEC` : comme `F_DUPFD` en ajoutant `FD_CLOEXEC`.
- `F_GETLK` / `F_SETLK` / `F_SETLKW` : verrous d'enregistrements (avec `struct flock`).
- `F_GETOWN` / `F_SETOWN` : processus recevant `SIGIO`/`SIGURG`.

**Flags utiles**
- `O_NONBLOCK` : les opérations (`read`, `recv`, `accept`, `send`...) ne bloquent plus et renvoient `EAGAIN`/`EWOULDBLOCK` à la place.
- `O_APPEND` : chaque écriture se fait à la fin du fichier.
- `FD_CLOEXEC` : le descripteur est fermé automatiquement lors d'un `exec`.

**Bonne pratique** : pour modifier un flag, on **lit d'abord** avec `F_GETFL`, puis on ajoute/retire le bit avec `|` / `& ~`, puis on réécrit avec `F_SETFL` (pour ne pas écraser les autres flags).

**Exemple concret** : passer l'entrée standard en mode non bloquant et tester si une saisie est disponible.

```cpp
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <iostream>

static bool setNonBlocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
        return false;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1;
}

int main()
{
    if (!setNonBlocking(STDIN_FILENO))
        return 1;

    char buf[64];
    for (int i = 0; i < 5; ++i)
    {
        ssize_t n = read(STDIN_FILENO, buf, sizeof(buf) - 1);
        if (n > 0)
        {
            buf[n] = '\0';
            std::cout << "Lu : " << buf;
        }
        else if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
            std::cout << "rien à lire pour l'instant" << std::endl;
        else
            break;              // EOF ou vraie erreur
        sleep(1);
    }
    return 0;
}
```

---

## 10. Multiplexage

### `poll`

**Résumé** : surveille **plusieurs descripteurs à la fois** et attend qu'au moins un soit prêt (lecture, écriture, erreur) ou qu'un délai expire. Évite de bloquer sur un seul descripteur pendant que les autres attendent.

```cpp
#include <poll.h>
int poll(struct pollfd *fds, nfds_t nfds, int timeout);
```

**Paramètres**

| Paramètre | Explication |
|---|---|
| `fds` | Tableau de `struct pollfd` à surveiller. |
| `nfds` | Nombre d'éléments du tableau. |
| `timeout` | Délai max en **millisecondes**. `-1` = attendre indéfiniment, `0` = retour immédiat (simple test), `> 0` = délai. |

**Structure `struct pollfd`**
```cpp
struct pollfd {
    int   fd;       // descripteur à surveiller (un fd < 0 est ignoré)
    short events;   // événements demandés (rempli par toi)
    short revents;  // événements survenus (rempli par poll)
};
```

**Retour** : nombre de descripteurs ayant un `revents` non nul ; `0` si timeout ; `-1` en cas d'erreur (`EINTR` si interrompu par un signal).

**Flags utiles**
- Dans `events` **et** `revents` :
  - `POLLIN` : des données sont lisibles (ou une connexion à `accept`er, ou fin de flux).
  - `POLLOUT` : on peut écrire sans bloquer.
  - `POLLPRI` : données urgentes (hors bande).
- Seulement dans `revents` (toujours rapportés, inutile de les demander) :
  - `POLLERR` : erreur sur le descripteur.
  - `POLLHUP` : le pair a raccroché.
  - `POLLNVAL` : descripteur invalide (non ouvert).
- (Linux) `POLLRDHUP` : le pair a fermé son côté d'écriture (nécessite `_GNU_SOURCE`).

**Notes**
- Après l'appel, **teste `revents`** avec `&` (`if (fds[i].revents & POLLIN)`), et pas avec `==`.
- `events` n'est jamais modifié par `poll`, `revents` est écrasé à chaque appel.
- Pour retirer un descripteur : fermer + supprimer du tableau, ou mettre `fd = -1`.
- Avec `std::vector<struct pollfd>`, passe `&v[0]` (en C++98 pas de `.data()`) et `v.size()`.

**Exemple concret** : attendre soit une saisie clavier, soit 3 secondes d'inactivité, en boucle jusqu'à "quit".

```cpp
#include <poll.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <string>
#include <iostream>

int main()
{
    struct pollfd fds[1];
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;
    fds[0].revents = 0;

    std::cout << "Tape du texte ('quit' pour sortir)" << std::endl;
    for (;;)
    {
        int ready = poll(fds, 1, 3000);     // timeout : 3000 ms

        if (ready == -1)
        {
            if (errno == EINTR)
                continue;
            std::cerr << "poll: " << std::strerror(errno) << std::endl;
            return 1;
        }
        if (ready == 0)
        {
            std::cout << "(3 s sans activité)" << std::endl;
            continue;
        }

        if (fds[0].revents & (POLLERR | POLLNVAL))
            break;
        if (fds[0].revents & POLLIN)
        {
            char buf[128];
            ssize_t n = read(STDIN_FILENO, buf, sizeof(buf) - 1);
            if (n <= 0)
                break;                      // EOF (Ctrl+D)
            buf[n] = '\0';
            std::string line(buf);
            if (line == "quit\n")
                break;
            std::cout << "Reçu : " << line;
        }
        if (fds[0].revents & POLLHUP)
            break;
    }
    return 0;
}
```

---

## Annexe : récapitulatif des headers

| Fonction(s) | Header(s) |
|---|---|
| `socket`, `bind`, `listen`, `accept`, `connect`, `send`, `recv`, `setsockopt`, `getsockname` | `<sys/socket.h>` |
| `sockaddr_in`, `htons`, `htonl`, `ntohs`, `ntohl`, `INADDR_*` | `<netinet/in.h>`, `<arpa/inet.h>` |
| `inet_addr`, `inet_ntoa`, `inet_ntop`, `inet_pton` | `<arpa/inet.h>` |
| `getprotobyname`, `gethostbyname`, `getaddrinfo`, `freeaddrinfo`, `gai_strerror` | `<netdb.h>` |
| `close`, `lseek` | `<unistd.h>` |
| `fstat` | `<sys/stat.h>` |
| `fcntl`, `open`, `O_*` | `<fcntl.h>` |
| `poll`, `struct pollfd` | `<poll.h>` |
| `signal`, `sigaction`, `sigemptyset`, `sigfillset`, `sigaddset`, `sigdelset`, `sigismember` | `<csignal>` (ou `<signal.h>`) |
| `errno` / `strerror` | `<cerrno>` / `<cstring>` |

## Annexe : pièges fréquents

- Toujours initialiser les structures d'adresse avec `std::memset(&addr, 0, sizeof(addr))`.
- Initialiser `socklen_t` **avant** `accept` / `getsockname` (c'est un paramètre entrée/sortie).
- Ne jamais supposer qu'un `send` envoie tout, ni qu'un `recv` lit un message complet.
- Après `close(fd)`, ne plus réutiliser `fd`.
- `getaddrinfo` renvoie un code propre : l'afficher avec `gai_strerror`, pas `strerror(errno)`.
- Dans un handler de signal : seulement des opérations simples (drapeau `volatile sig_atomic_t`).
- Vérifier la valeur de retour de **chaque** appel système et gérer `EINTR` quand c'est pertinent.