//
// Created by winch on 5/5/2023.
//

#ifndef SHUFFLE_INCLUDE_UTILS_H_
#define SHUFFLE_INCLUDE_UTILS_H_

#ifdef _WIN32
#define HOME string(getenv("APPDATA"))
#elif __linux__ || __APPLE__
#define HOME string(getenv("HOME"))
#endif
#define DOT_SHUFFLE (HOME + "/.shuffle")

#include <string>
#include <vector>
#include <regex>

using namespace std;

vector<string> split(const string &s, const regex &delimiter_regex);
string trim(const string &s);
int levenshteinDist(const string &str1, const string &str2);
string replace(string str, const string &from, const string &to);
string readFile(const string &path);
void writeFile(const string &path, const string& value);

#endif //SHUFFLE_INCLUDE_UTILS_H_
