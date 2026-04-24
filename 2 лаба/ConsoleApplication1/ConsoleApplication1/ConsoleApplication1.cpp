#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cctype>

using namespace std;

struct Token {
    string type;
    string value;
};

bool isKeyword(string s) {
    string keywords[] = {
        "include", "using", "namespace", "int", "bool",
        "return", "if", "else", "for", "while"
    };

    for (int i = 0; i < 10; i++) {
        if (s == keywords[i]) return true;
    }

    return false;
}

bool isBoolConst(string s) {
    return s == "true" || s == "false";
}

bool isDelimiter(char c) {
    string delimiters = ";,(){}";

    for (int i = 0; i < delimiters.length(); i++) {
        if (c == delimiters[i]) return true;
    }

    return false;
}

bool isOneCharOperator(char c) {
    string operators = "#=+-*/<>";

    for (int i = 0; i < operators.length(); i++) {
        if (c == operators[i]) return true;
    }

    return false;
}

bool isTwoCharOperator(string s) {
    string operators[] = {
        "&&", "||", "<<", ">>", "==", "!=", "<=", ">="
    };

    for (int i = 0; i < 8; i++) {
        if (s == operators[i]) return true;
    }

    return false;
}

int main() {
	setlocale(LC_ALL, "Russian");
    ifstream file("test.cpp");

    if (!file.is_open()) {
        cout << "Ошибка: не удалось открыть файл test.cpp" << endl;
        return 1;
    }

    string code = "";
    string line;

    while (getline(file, line)) {
        code += line + '\n';
    }

    file.close();

    vector<Token> tokens;
    vector<string> errors;

    int i = 0;

    while (i < code.length()) {
        char c = code[i];

        if (isspace(c)) {
            i++;
        }

        else if (isalpha(c) || c == '_') {
            string word = "";

            while (i < code.length() && (isalnum(code[i]) || code[i] == '_')) {
                word += code[i];
                i++;
            }

            if (isKeyword(word)) {
                tokens.push_back({ "KEYWORD", word });
            }
            else if (isBoolConst(word)) {
                tokens.push_back({ "CONSTANT_BOOL", word });
            }
            else {
                tokens.push_back({ "IDENTIFIER", word });
            }
        }

        else if (isdigit(c)) {
            string number = "";
            bool hasDot = false;
            bool error = false;

            while (i < code.length() && (isdigit(code[i]) || code[i] == '.' || isalpha(code[i]))) {
                if (code[i] == '.') {
                    if (hasDot) {
                        error = true;
                    }
                    hasDot = true;
                }

                if (isalpha(code[i])) {
                    error = true;
                }

                number += code[i];
                i++;
            }

            if (error) {
                errors.push_back("Ошибка в числе: " + number);
            }
            else {
                if (hasDot) {
                    tokens.push_back({ "CONSTANT_REAL", number });
                }
                else {
                    tokens.push_back({ "CONSTANT_INT", number });
                }
            }
        }

        else if (c == '"') {
            string str = "";
            str += c;
            i++;

            bool closed = false;

            while (i < code.length()) {
                str += code[i];

                if (code[i] == '"') {
                    closed = true;
                    i++;
                    break;
                }

                i++;
            }

            if (closed) {
                tokens.push_back({ "CONSTANT_STRING", str });
            }
            else {
                errors.push_back("Незакрытая строковая константа: " + str);
            }
        }

        else if (i + 1 < code.length()) {
            string two = "";
            two += code[i];
            two += code[i + 1];

            if (isTwoCharOperator(two)) {
                tokens.push_back({ "OPERATOR", two });
                i += 2;
            }
            else if (isDelimiter(c)) {
                string s = "";
                s += c;
                tokens.push_back({ "DELIMITER", s });
                i++;
            }
            else if (isOneCharOperator(c)) {
                string s = "";
                s += c;
                tokens.push_back({ "OPERATOR", s });
                i++;
            }
            else {
                string s = "";
                s += c;
                errors.push_back("Недопустимый символ: " + s);
                i++;
            }
        }

        else {
            if (isDelimiter(c)) {
                string s = "";
                s += c;
                tokens.push_back({ "DELIMITER", s });
            }
            else if (isOneCharOperator(c)) {
                string s = "";
                s += c;
                tokens.push_back({ "OPERATOR", s });
            }
            else {
                string s = "";
                s += c;
                errors.push_back("Недопустимый символ: " + s);
            }

            i++;
        }
    }

    cout << "Лексема\t\tТип" << endl;
    cout << "-----------------------------" << endl;

    for (int j = 0; j < tokens.size(); j++) {
        cout << tokens[j].value << "\t\t" << tokens[j].type << endl;
    }

    cout << endl;
    cout << "[";

    for (int j = 0; j < tokens.size(); j++) {
        cout << "(" << tokens[j].type << ", " << tokens[j].value << ")";

        if (j != tokens.size() - 1) {
            cout << ", ";
        }
    }

    cout << "]" << endl << endl;

    if (errors.size() == 0) {
        cout << "Лексический анализ завершён успешно." << endl;
        cout << "Обнаружено токенов: " << tokens.size() << endl;
        cout << "Ошибок не найдено." << endl;
    }
    else {
        cout << "Лексический анализ завершён с ошибками." << endl;

        for (int j = 0; j < errors.size(); j++) {
            cout << errors[j] << endl;
        }
    }

    return 0;
}