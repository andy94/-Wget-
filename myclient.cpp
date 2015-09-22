/******************************************************************************
 * Tema        : 4 - PC                                                       *
 * Autor       : Andrei Ursache                                               *
 * Grupa       : 322 CA                                                       *
 * Data        : 17.05.2015                                                   *
 ******************************************************************************/

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/types.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <features.h>
#include <netinet/in.h>
#include <fcntl.h>

#include <iostream>
#include <fstream>
#include <algorithm>
#include <string>
#include <vector>

#include "aux.hpp"

#define BUFFLEN 1000
#define HTTP_PORT 80
#define MAX_LEVELS 5

using namespace std;

/* Retine numarul paginilor descarcate */
int page_counter = 0;

/* Funcite care trimite o cerere GET la server pentru pagina [page]
 * si salveaza pe disk cu numele caii.
 * Este folosita atat pentru pagini web cat si pentru fisiere
 * In [ok] se retine daca pagina a fost descarcata cu succes sau nu */
void get_file(string server_name, vector<GetFile>::iterator page,
        int server_socket, struct sockaddr_in server_addr, string &dir,
        string &page_path, bool &ok);

int main(int argc, char **argv) {

    bool recursive = false;
    bool everything = false;
    bool has_log = false;

    string log_file_name;
    string path;

    /* Gestionarea argumentelor */
    for (int i = 1; i < argc; ++i) {
        if (argv[i][0] == '-') {
            switch (argv[i][1]) {
            case 'r':
                recursive = true;
                break;
            case 'e':
                everything = true;
                break;
            case 'o':
                has_log = true;
                if (i == argc - 1) {
                    cerr << 
                      "Usage: ./myclient [-r] [-e] [-o <log_file_name>] path\n";
                    return -1;
                }
                log_file_name = argv[++i];
                break;
            default:
                cerr << 
                  "Usage: ./myclient [-r] [-e] [-o <log_file_name>] path\n";
                return -1;
            }
        } else {
            path = argv[i];
        }
    }

    if (path.empty() || path.substr(0, 7) != "http://") {
        cerr << "Usage: ./myclient [-r] [-e] [-o <log_file_name>] path\n";
        return -1;
    }

    if (has_log) {
        freopen(log_file_name.c_str(), "w", stderr);
    }

    /* Identificarea numelui serverului si a paginii de start */
    path.erase(0, 7);
    vector<string> path_components = split(path, '/');
    string server_name = path_components[0];
    string path_to_page = path.erase(0, server_name.size() + 1);

    /* Aflarea adresei IP a serverului */
    int server_socket = -1;
    struct sockaddr_in server_addr;
    struct hostent *host = NULL;
    host = gethostbyname(server_name.c_str());
    if (host == NULL) {
        error("Error: Server name unknown.");
    }

    /* Formarea adresei serverului */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(HTTP_PORT);
    memcpy(&server_addr.sin_addr, host->h_addr_list[0],
            sizeof(server_addr.sin_addr));

    /* Creearea buffer pentru paginile web */
    GetBuffer getBuffer(MAX_LEVELS);

    /* Introducerea primei pagini in buffer pe nivelul unu */
    GetFile firstPage(getFileName(path_to_page), getDirPath(path_to_page), "/");
    getBuffer.addPageOnNextLevel(firstPage);
    getBuffer.incrementCurrentLevel();

    /* Rulare program */
    printf("Starting...\n");

    while (!getBuffer.empty()) {

        /* Paginile de downloadat la nivelul actual */
        vector<GetFile> pages = getBuffer.getCurrentLevelBuffer();

        /* Procesare pagini de la nivelul curent */
        for (vector<GetFile>::iterator page = pages.begin();
                page != pages.end(); ++page) {

            /* Fisierele de pe pagina asta [link] */
            vector<string> files;

            /* Paginile de downloadat care se gasesc in aceasta pagina [link] */
            vector<string> next_pages;

            string dir;
            string page_path;

            bool ok = true;

            /* Se descarca pagina */
            get_file(server_name, page, server_socket, server_addr, dir,
                    page_path, ok);

            /* Daca s-a realizat cu succes */
            if (ok) {

                /* Se identifica linkurile */
                parse_links(page_path, files, next_pages, recursive,
                        everything);

                /* Procesare linkuri */
                vector<GetFile> pages_to_get_next_level;
                vector<GetFile> files_from_this_page;

                /* Se creeaza GetFile-urile */
                make_pages(pages_to_get_next_level, next_pages, *page);
                make_files(files_from_this_page, files, *page);

                /* Se adauga paginile gasite pe nivelul urmator (daca e cazul)*/
                for (unsigned int f = 0; f < pages_to_get_next_level.size();
                        ++f) {
                    getBuffer.addPageOnNextLevel(pages_to_get_next_level[f]);
                }

                /* Se descarca fisierele gasite pe aceasta pagina 
                 * Daca este cazul */
                for (vector<GetFile>::iterator f = files_from_this_page.begin();
                        f != files_from_this_page.end(); ++f) {

                    string dir_f;
                    string file_path;

                    bool ok = true;

                    /* Se descarca pagina */
                    get_file(server_name, f, server_socket, server_addr, dir_f,
                            file_path, ok);

                }
            }

        }

        /* Trece la next level */
        getBuffer.incrementCurrentLevel();

    }

    /* Exit */
    printf("\n\n%d pages downloaded\nClosing...\n", page_counter);

    return 0;
}

/* Funcite care trimite o cerere GET la server pentru pagina [page]
 * si salveaza pe disk cu numele caii.
 * Este folosita atat pentru pagini web cat si pentru fisiere
 * In ok se retine daca pagina a fost descarcata cu succes sau nu */
void get_file(string server_name, vector<GetFile>::iterator page,
        int server_socket, struct sockaddr_in server_addr, string &dir,
        string &page_path, bool &ok) {

    /* Buffer folosit la citire / scriere */
    char buffer[BUFFLEN];

    /* Creeare folder */
    dir = server_name + (*page).getCurrentDir() + (*page).getDirPath();
    page_path = dir + (*page).getFileName();
    string mkdir = "mkdir -p " + dir;

    /* Nu s-a putut creea */
    int res = system(mkdir.c_str());
    if (res != 0) {
        perror(("Error: mkdir - filed to create dir: " + dir + "\n").c_str());
        ok = false;
        return;
    }

    /* Daca exista deja */
    if (file_exists(page_path)) {
        perror(("Page [" + page_path + "] exists.").c_str());
        ok = false;
        return;
    }

    memset(buffer, 0, BUFFLEN);

    /* Creeare socket */
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        sprintf(buffer, "Error: Failed creating socket for page [%s].",
                (*page).getFileName().c_str());

        perror(buffer);
        ok = false;
        return;
    }

    /*  Conectare la server  */
    if (connect(server_socket, (struct sockaddr *) &server_addr,
            sizeof(server_addr)) < 0) {
        sprintf(buffer, "Error: Failed connecting the server for page [%s].",
                (*page).getFileName().c_str());

        perror(buffer);
        ok = false;
        return;
    }

    /* Formare cerere GET */
    memset(buffer, 0, BUFFLEN);
    sprintf(buffer, "GET %s HTTP/1.0\nHost: %s\nConnection: close\n\n",
            ((*page).getCurrentDir() + (*page).getDirPath()
                    + (*page).getFileName()).c_str(), server_name.c_str());

    printf("------------------------------  %d\nNext page:\n%s", page_counter,
            buffer);

    /* Trimitere comanda GET */
    send(server_socket, buffer, strlen(buffer), 0);
    int nrBytes;

    /* Primire raspuns*/
    memset(buffer, 0, BUFFLEN);
    nrBytes = recv(server_socket, buffer, BUFFLEN, 0);

    /* Verificare mesaj succes/eroare */
    string auxBuf(buffer, buffer + 255);
    std::size_t found = auxBuf.find('\n');

    /* Mesaj eronat */
    if (found == std::string::npos) {
        cerr << ("Error: receive page [" + page_path + "]").c_str();
        ok = false;
        return;
    } else {
        /* Daca nu este mesaj de success */
        auxBuf.erase(found, auxBuf.size());
        if (auxBuf[9] != '2' && auxBuf[9] != '1') {

            auxBuf.erase(0, 8);
            cerr << 
              ("Error: receive page [" + page_path + "] :" + auxBuf).c_str();
            ok = false;
            return;
        }
    }

    /* Daca totul este in regula */
    printf("===> OK\n");
    page_counter++;

    /* Parsare header */
    char *start = strstr(buffer, "\r\n\r\n");
    if (start == NULL) {
        cerr << ("Error: receive page [" + page_path + "]").c_str();
        ok = false;
        return;
    }

    start += 4;

    /* Creeare fisier */
    int file_o = creat(page_path.c_str(),  S_IRUSR | S_IWUSR | S_IXUSR |
    		S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
    int c = start - buffer;

    /* Afisarea primului buffer fara header */
    write(file_o, start, nrBytes - c);

    memset(buffer, 0, BUFFLEN);

    /* Primirea restului de fisier */
    while ((nrBytes = read(server_socket, buffer, BUFFLEN)) > 0) {
        /* Scrie in fisier */
        write(file_o, buffer, nrBytes);
        memset(buffer, 0, BUFFLEN);
    }

    close(file_o);
    close(server_socket);
}
