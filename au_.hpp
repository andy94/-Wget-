/******************************************************************************
 * Tema        : 4 - PC                                                       *
 * Autor       : Andrei Ursache                                               *
 * Grupa       : 322 CA                                                       *
 * Data        : 17.05.2015                                                   *
 ******************************************************************************/

#ifndef _AUX_H
#define _AUX_H

#pragma once

#include <iostream>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <algorithm>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

using namespace std;

/* Verifica daca fisierul exista deja */
bool file_exists(string fileName) {
    ifstream f(fileName.c_str());
    return f.good();
}

/* Returneaza path-ul pana la fisier */
string getDirPath(string str) {
    unsigned found = str.find_last_of("/");

    return str.substr(0, found + 1);
}

/* Returneaza numele fisierului */
string getFileName(string str) {
    unsigned found = str.find_last_of("/");

    return str.substr(found + 1, str.size());
}

/* Functie de despartire dupa separator */
std::vector<std::string> &split(const std::string &s, char delim,
        std::vector<std::string> &elems) {
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}
std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> elems;
    split(s, delim, elems);
    return elems;
}

/* Returneaza extensia fisierului (daca exista si e valida */
string getFileExtension(string str) {
    size_t found = str.find_last_of(".");
    size_t st = str.find_last_of("/");

    if ((st > found && st != string::npos) || found == string::npos) {
        return "";
    }

    return str.substr(found + 1, str.size());
}

/* Verifica daca e un link valid:
 * -1 -> nu e valid
 *  0 -> pagina web
 *  1 -> fisier
 */
int chech_link(string link) {

    string ext = getFileExtension(link);

    if (ext == "" || link.find("//") != string::npos
            || link.find("#") != string::npos) {
        return -1;
    }

    if (ext == "htm" || ext == "html") {
        return 0;
    }

    if (ext.size() == 3 || ext.size() == 4) {
        return 1;
    }

    return -1;

}

/* Pune in files si next_pages linkurile valide din fisier
 * (daca e cazul) */
void parse_links(string file_path, vector<string> &files,
        vector<string> &next_pages, bool recursive, bool everything) {

    /* Daca nu e necesar */
    if (!recursive && !everything) {
        return;
    }

    ifstream in(file_path.c_str());
    string line;

    /* Pozitii in linie */
    size_t a;
    size_t af;
    size_t l;
    string current;
    string atag;

    while (getline(in, line)) {

        /* Pentru fiecare link de pe linie */
        while ((a = line.find("<a")) != string::npos) {
            line.erase(0, a + 3);

            if (((af = line.find("href=\"")) != string::npos)
                    || ((af = line.find("href='")) != string::npos)) {
                line.erase(0, af + 6);

                if (((l = line.find("\"")) != string::npos)
                        || ((l = line.find("'")) != string::npos)) {

                    string link = line.substr(0, l);
                    line.erase(0, l + 1);

                    /* Daca e link de pagina si trebuie adaugat */
                    if (chech_link(link) == 0 && recursive) {
                        next_pages.push_back(link);
                    }

                    /* Daca e link de file si trebuie adaugat */
                    if (chech_link(link) == 1 && everything) {
                        files.push_back(link);
                    }
                }
            }
        }
    }
}

/* Funcite pentru erori ce afecteaza fucntionarea programului */
void error(string msg) {
    perror(msg.c_str());
    exit(-1);
}

/* Conversie int->string */
string int_to_string(int val) {
    stringstream ss;
    ss << val;
    string str = ss.str();
    return str;

}

/* Clasa specifica unui fisier (sau pagina web) */
class GetFile {
private:
    /* Fie calea: "servername/dir1/dir2/filename" */

    string file_name; /*     "filename"      */
    string dir_path; /*      "/dir1/dir2"    */
    string current_dir; /*   "servername"    */
public:
    GetFile(string file_name, string dir_path, string current_dir) {
        this->file_name = file_name;
        this->dir_path = dir_path;
        this->current_dir = current_dir;
    }

    string getDirPath() {
        return dir_path;
    }

    string getFileName() {
        return file_name;
    }

    string getCurrentDir() {
        return this->current_dir;
    }

    friend bool operator==(GetFile &a, GetFile &f);
};

bool operator==(GetFile &a, GetFile &f) {
    return a.file_name == f.file_name && a.dir_path == f.dir_path
            && a.current_dir == f.current_dir;
}

/* Bufferul pentru nivelele de pagini web */
class GetBuffer {
private:
    /* Nivelele de pagini */
    vector<vector<GetFile> > *levels;

    /* Buffer de fisiere */
    vector<GetFile> *files;

    int current_level;
    int max_level;
    int pages_no;
public:
    GetBuffer(int max_level) {
        this->max_level = max_level;
        this->current_level = 0;
        this->levels = new vector<vector<GetFile> >(6, vector<GetFile>());
        this->files = new vector<GetFile>();
        pages_no = 0;
    }

    /* Ia paginile web de pe acest nivel si decrementeaza numarul lor */
    vector<GetFile>& getCurrentLevelBuffer() {
        pages_no -= (*levels)[current_level].size();
        return (*levels)[current_level];
    }

    /* Adaugarea unei paini pe nivelul urmator */
    void addPageOnNextLevel(GetFile page) {
        if (current_level < max_level) {
            (*levels)[current_level + 1].push_back(page);
            pages_no++;
        }
    }

    void addFile(GetFile file) {
        this->files->push_back(file);
    }

    vector<GetFile> getFiles(GetFile file) {
        return *this->files;
    }

    void clearFiles() {
        this->files->clear();
    }

    int getCurrentLevel() {
        return this->current_level;
    }

    /* Treci pe nivelul urmator */
    void incrementCurrentLevel() {
        this->current_level++;
    }

    bool empty() {
        return pages_no <= 0;
    }

    ~GetBuffer() {
        delete this->files;
        delete this->levels;
    }

};

/* Verifica daca un link e absolut */
bool isAbsoluteLink(string link) {
    if (link[0] == '/') {
        return true;
    }
    return false;
}

/* Din link-urile catre pagini, creeaza GetFile */
void make_pages(vector<GetFile> &pages_to_get_next_level,
        vector<string> &next_pages, GetFile page) {

    for (vector<string>::iterator link = next_pages.begin();
            link != next_pages.end(); ++link) {

        if (isAbsoluteLink(*link)) {
            (*link).erase((*link).begin());
            GetFile pg = GetFile(getFileName(*link), getDirPath(*link), "/");
            /* Verificare unicitate */
            for (unsigned j = 0; j < pages_to_get_next_level.size(); ++j) {
                if (pages_to_get_next_level[j] == pg) {
                    return;
                }
            }
            pages_to_get_next_level.push_back(pg);

        } else {
            pages_to_get_next_level.push_back(
                    GetFile(getFileName(*link), getDirPath(*link),
                            page.getCurrentDir() + page.getDirPath()));
        }
    }
}

/* Din link-urile catre files, creeaza GetFile */
void make_files(vector<GetFile> &files_from_this_page, vector<string> &files,
        GetFile page) {

    for (vector<string>::iterator link = files.begin(); link != files.end();
            ++link) {

        if (isAbsoluteLink(*link)) {
            (*link).erase((*link).begin());
            GetFile fp(getFileName(*link), getDirPath(*link), "/");
            /* Verificare unicitate */
            for (unsigned j = 0; j < files_from_this_page.size(); ++j) {
                if (files_from_this_page[j] == fp) {
                    return;
                }
            }
            files_from_this_page.push_back(fp);
        } else {
            files_from_this_page.push_back(
                    GetFile(getFileName(*link), getDirPath(*link),
                            page.getCurrentDir() + page.getDirPath()));
        }
    }
}

#endif
