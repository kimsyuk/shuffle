//
// Created by winch on 5/10/2023.
//

#ifndef SHUFFLE_INCLUDE_HASH_SUGGESTION_H_
#define SHUFFLE_INCLUDE_HASH_SUGGESTION_H_

#include <string>

using namespace std;

string callGptAI(const string &prompt);
string createHashSuggestion(const string& prompt);

#endif //SHUFFLE_INCLUDE_HASH_SUGGESTION_H_
